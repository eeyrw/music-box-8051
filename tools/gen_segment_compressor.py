#!/usr/bin/env python3
"""Generate a minimax dBFS compressor for the 8051 synth ISR."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from math import ceil, inf, log10, pow
from pathlib import Path

try:
    import numpy as np
except ImportError:  # pragma: no cover - exercised only on minimal hosts
    np = None


@dataclass(frozen=True)
class Segment:
    lo: int
    hi: int
    shift: int
    slope_q8: int
    intercept: int
    error: float
    negate: bool = False


@dataclass(frozen=True)
class CompressorConfig:
    input_min: int
    input_max: int
    input_full_scale: int
    output_min: int
    output_max: int
    output_full_scale: int
    q8_slope_max: int


def clamp(v: float, lo: float, hi: float) -> float:
    return min(max(v, lo), hi)


def target_sample(
    x: int,
    input_full_scale: int,
    output_full_scale: int,
    threshold_db: float,
    ratio: float,
    makeup_db: float,
) -> float:
    mag = abs(x)
    if mag == 0:
        return 0.0

    in_db = 20.0 * log10(mag / input_full_scale)
    if in_db <= threshold_db:
        out_db = in_db
    else:
        out_db = threshold_db + (in_db - threshold_db) / ratio
    out_db += makeup_db

    y = output_full_scale * pow(10.0, out_db / 20.0)
    y = clamp(y, 0.0, output_full_scale)
    return -y if x < 0 else y


def compressed_db_at_full_scale(threshold_db: float, ratio: float) -> float:
    return threshold_db + (0.0 - threshold_db) / ratio


def resolve_compressor_params(args: argparse.Namespace) -> None:
    if args.threshold_db is None:
        args.threshold_db = -6.0
    if args.ratio is None:
        args.ratio = 4.0
    if args.ratio < 1.0:
        raise ValueError("ratio must be >= 1.0")

    if args.makeup_db is not None:
        return
    if args.preset == "safe":
        args.makeup_db = 0.0
    elif args.preset == "loud":
        args.makeup_db = -compressed_db_at_full_scale(args.threshold_db, args.ratio)
    else:
        args.makeup_db = 0.0


def compressor_eval(seg: Segment, x: int) -> int:
    if seg.negate:
        mag = -x
        if seg.slope_q8 >= 0:
            y = ((mag * seg.slope_q8) >> 8) + seg.intercept
        else:
            y = seg.intercept if seg.shift < 0 else (mag >> seg.shift) + seg.intercept
        return -y
    if seg.slope_q8 >= 0:
        return ((x * seg.slope_q8) >> 8) + seg.intercept
    if seg.shift < 0:
        return seg.intercept
    return (x >> seg.shift) + seg.intercept


def fit_interval(
    xs: list[int],
    residual_min: dict[int, float],
    residual_max: dict[int, float],
    lo: int,
    hi: int,
    shifts: list[int],
    out_min: int,
    out_max: int,
) -> Segment:
    best: Segment | None = None
    for shift in shifts:
        mid = (residual_min[shift] + residual_max[shift]) / 2.0
        candidates = {int(mid), int(mid + 0.5), int(mid - 0.5)}
        candidates.update(range(int(mid) - 2, int(mid) + 3))
        for intercept in candidates:
            if shift < 0:
                y_lo = y_hi = intercept
            else:
                y_lo = (xs[lo] >> shift) + intercept
                y_hi = (xs[hi] >> shift) + intercept
            if min(y_lo, y_hi) < out_min or max(y_lo, y_hi) > out_max:
                continue
            err = max(abs(residual_min[shift] - intercept), abs(residual_max[shift] - intercept))
            seg = Segment(xs[lo], xs[hi], shift, -1, intercept, err)
            if best is None or (seg.error, seg.shift, abs(seg.intercept)) < (
                best.error,
                best.shift,
                abs(best.intercept),
            ):
                best = seg
    if best is None:
        raise RuntimeError(f"no valid segment fit for {xs[lo]}..{xs[hi]}")
    return best


def precompute_segments(
    xs: list[int], ys: list[float], shifts: list[int], out_min: int, out_max: int
) -> list[list[Segment | None]]:
    n = len(xs)
    segs: list[list[Segment | None]] = [[None] * n for _ in range(n)]
    for lo in range(n):
        residual_min = {shift: inf for shift in shifts}
        residual_max = {shift: -inf for shift in shifts}
        for hi in range(lo, n):
            for shift in shifts:
                slope = 0.0 if shift < 0 else 1.0 / (1 << shift)
                residual = ys[hi] - slope * xs[hi]
                residual_min[shift] = min(residual_min[shift], residual)
                residual_max[shift] = max(residual_max[shift], residual)
            segs[lo][hi] = fit_interval(xs, residual_min, residual_max, lo, hi, shifts, out_min, out_max)
    return segs


def precompute_q8_segments(xs: list[int], ys: list[float], out_min: int, out_max: int,
                           slope_max: int, slope_step: int) -> list[list[Segment | None]]:
    if np is not None:
        return precompute_q8_segments_numpy(xs, ys, out_min, out_max, slope_max, slope_step)

    n = len(xs)
    yi = [int(round(y)) for y in ys]
    slopes = list(range(0, slope_max + 1, slope_step))
    slope_count = len(slopes)
    segs: list[list[Segment | None]] = [[None] * n for _ in range(n)]
    for lo in range(n):
        residual_min = [10_000] * slope_count
        residual_max = [-10_000] * slope_count
        x_lo = xs[lo]
        for hi in range(lo, n):
            x = xs[hi]
            y = yi[hi]
            best: Segment | None = None
            for idx, slope_q8 in enumerate(slopes):
                residual = y - ((x * slope_q8) >> 8)
                if residual < residual_min[idx]:
                    residual_min[idx] = residual
                if residual > residual_max[idx]:
                    residual_max[idx] = residual
                lo_res = residual_min[idx]
                hi_res = residual_max[idx]
                mid_floor = (lo_res + hi_res) // 2
                for intercept in (mid_floor, mid_floor + 1):
                    y_lo = ((x_lo * slope_q8) >> 8) + intercept
                    y_hi = ((x * slope_q8) >> 8) + intercept
                    if min(y_lo, y_hi) < out_min or max(y_lo, y_hi) > out_max:
                        continue
                    err = max(abs(lo_res - intercept), abs(hi_res - intercept))
                    seg = Segment(xs[lo], xs[hi], -1, slope_q8, intercept, float(err))
                    if best is None or (seg.error, abs(seg.intercept), -seg.slope_q8) < (
                        best.error,
                        abs(best.intercept),
                        -best.slope_q8,
                    ):
                        best = seg
            if best is None:
                raise RuntimeError(f"no valid q8 segment fit for {xs[lo]}..{xs[hi]}")
            segs[lo][hi] = best
    return segs


def precompute_q8_segments_numpy(xs: list[int], ys: list[float], out_min: int, out_max: int,
                                 slope_max: int, slope_step: int) -> list[list[Segment | None]]:
    n = len(xs)
    x_arr = np.asarray(xs, dtype=np.int32)
    y_arr = np.asarray(ys, dtype=np.float64)
    slopes = np.arange(0, slope_max + 1, slope_step, dtype=np.int32)
    slope_count = int(slopes.size)
    segs: list[list[Segment | None]] = [[None] * n for _ in range(n)]

    for lo in range(n):
        residual_min = np.full(slope_count, 10000.0, dtype=np.float64)
        residual_max = np.full(slope_count, -10000.0, dtype=np.float64)
        x_lo = int(x_arr[lo])
        y_lo_base = (x_lo * slopes) >> 8

        for hi in range(lo, n):
            x_hi = int(x_arr[hi])
            residual = float(y_arr[hi]) - ((x_hi * slopes) >> 8)
            np.minimum(residual_min, residual, out=residual_min)
            np.maximum(residual_max, residual, out=residual_max)

            mid = np.floor((residual_min + residual_max) / 2.0).astype(np.int32)
            y_hi_base = (x_hi * slopes) >> 8

            best_seg: Segment | None = None
            best_key: tuple[float, int, int] | None = None
            for intercepts in (mid, mid + 1):
                y_lo = y_lo_base + intercepts
                y_hi = y_hi_base + intercepts
                valid = (np.minimum(y_lo, y_hi) >= out_min) & (np.maximum(y_lo, y_hi) <= out_max)
                if not bool(np.any(valid)):
                    continue
                err = np.maximum(np.abs(residual_min - intercepts), np.abs(residual_max - intercepts))
                err = np.where(valid, err, 10000.0)
                idx = int(np.argmin(err))
                slope_q8 = int(slopes[idx])
                intercept = int(intercepts[idx])
                seg = Segment(xs[lo], xs[hi], -1, slope_q8, intercept, float(err[idx]))
                key = (seg.error, abs(seg.intercept), -seg.slope_q8)
                if best_key is None or key < best_key:
                    best_key = key
                    best_seg = seg
            if best_seg is None:
                raise RuntimeError(f"no valid q8 segment fit for {xs[lo]}..{xs[hi]}")
            segs[lo][hi] = best_seg
    return segs


def merge_adjacent(segments: list[Segment]) -> list[Segment]:
    merged: list[Segment] = []
    for seg in segments:
        if (
            merged
            and merged[-1].hi + 1 == seg.lo
            and merged[-1].shift == seg.shift
            and merged[-1].slope_q8 == seg.slope_q8
            and merged[-1].intercept == seg.intercept
            and merged[-1].negate == seg.negate
        ):
            prev = merged[-1]
            merged[-1] = Segment(prev.lo, seg.hi, prev.shift, prev.slope_q8, prev.intercept, max(prev.error, seg.error), prev.negate)
        else:
            merged.append(seg)
    return merged


def segment_error(seg: Segment, xs: list[int], ys: list[float]) -> float:
    lo = seg.lo - xs[0]
    hi = seg.hi - xs[0]
    return max(abs(compressor_eval(seg, xs[i]) - ys[i]) for i in range(lo, hi + 1))


def optimize_segments(
    xs: list[int], ys: list[float], count: int, shifts: list[int], out_min: int, out_max: int,
    slope_mode: str, q8_slope_max: int, q8_slope_step: int
) -> list[Segment]:
    n = len(xs)
    if count < 1 or count > n:
        raise ValueError("segments must be between 1 and the number of samples")

    if slope_mode == "q8mul":
        segs = precompute_q8_segments(xs, ys, out_min, out_max, q8_slope_max, q8_slope_step)
    else:
        segs = precompute_segments(xs, ys, shifts, out_min, out_max)
    dp = [[inf] * n for _ in range(count + 1)]
    prev = [[-1] * n for _ in range(count + 1)]

    for j in range(n):
        dp[1][j] = segs[0][j].error  # type: ignore[union-attr]

    for k in range(2, count + 1):
        for j in range(k - 1, n):
            best_cost = inf
            best_i = -1
            for i in range(k - 2, j):
                seg = segs[i + 1][j]
                cost = max(dp[k - 1][i], seg.error)  # type: ignore[union-attr]
                if cost < best_cost:
                    best_cost = cost
                    best_i = i
            dp[k][j] = best_cost
            prev[k][j] = best_i

    result: list[Segment] = []
    j = n - 1
    for k in range(count, 0, -1):
        i = prev[k][j]
        lo = 0 if k == 1 else i + 1
        result.append(segs[lo][j])  # type: ignore[arg-type]
        j = i
    result.reverse()
    return merge_adjacent(result)


def fit_single_segment(xs: list[int], ys: list[float], shifts: list[int], out_min: int, out_max: int) -> Segment:
    residual_min = {shift: inf for shift in shifts}
    residual_max = {shift: -inf for shift in shifts}
    for x, y in zip(xs, ys):
        for shift in shifts:
            slope = 0.0 if shift < 0 else 1.0 / (1 << shift)
            residual = y - slope * x
            residual_min[shift] = min(residual_min[shift], residual)
            residual_max[shift] = max(residual_max[shift], residual)
    return fit_interval(xs, residual_min, residual_max, 0, len(xs) - 1, shifts, out_min, out_max)


def refit_q8_segment(seg: Segment, xs: list[int], ys: list[float], out_min: int, out_max: int,
                     slope_max: int, slope_step: int) -> Segment:
    lo = seg.lo - xs[0]
    hi = seg.hi - xs[0]
    best: Segment | None = None
    for slope_q8 in range(0, slope_max + 1, slope_step):
        residuals = [ys[i] - ((xs[i] * slope_q8) >> 8) for i in range(lo, hi + 1)]
        lo_res = min(residuals)
        hi_res = max(residuals)
        mid = (lo_res + hi_res) / 2.0
        mid_floor = int(mid)
        for intercept in (mid_floor - 1, mid_floor, mid_floor + 1, mid_floor + 2):
            y_lo = ((seg.lo * slope_q8) >> 8) + intercept
            y_hi = ((seg.hi * slope_q8) >> 8) + intercept
            if min(y_lo, y_hi) < out_min or max(y_lo, y_hi) > out_max:
                continue
            err = max(abs(lo_res - intercept), abs(hi_res - intercept))
            candidate = Segment(seg.lo, seg.hi, -1, slope_q8, intercept, err)
            if best is None or (candidate.error, abs(candidate.intercept), -candidate.slope_q8) < (
                best.error,
                abs(best.intercept),
                -best.slope_q8,
            ):
                best = candidate
    if best is None:
        raise RuntimeError(f"no valid refined q8 segment fit for {seg.lo}..{seg.hi}")
    return best


def optimize_symmetric_segments(
    args: argparse.Namespace, cfg: CompressorConfig, shifts: list[int]
) -> tuple[list[Segment], list[int]]:
    mag_segment_count = args.segments // 2 if args.slope_mode == "q8mul" else (args.segments + 1) // 2
    mag_segment_count = max(1, mag_segment_count)
    mags = list(range(0, cfg.input_full_scale + 1))
    mag_targets = [
        abs(target_sample(m, cfg.input_full_scale, cfg.output_full_scale, args.threshold_db, args.ratio, args.makeup_db))
        for m in mags
    ]
    mag_segments = optimize_segments(mags, mag_targets, mag_segment_count, shifts, 0, cfg.output_full_scale,
                                     args.slope_mode, cfg.q8_slope_max, args.q8_slope_step)
    if args.slope_mode == "q8mul" and args.q8_refine_slope_step > 0:
        mag_segments = [
            refit_q8_segment(seg, mags, mag_targets, 0, cfg.output_full_scale,
                             cfg.q8_slope_max, args.q8_refine_slope_step)
            for seg in mag_segments
        ]

    targets = [
        int(round(target_sample(x, cfg.input_full_scale, cfg.output_full_scale, args.threshold_db, args.ratio, args.makeup_db)))
        for x in range(cfg.input_min, cfg.input_max + 1)
    ]

    signed_segments: list[Segment] = []
    for mag_seg in reversed(mag_segments):
        if mag_seg.lo == 0 and args.slope_mode != "q8mul":
            continue
        neg_hi = -max(1, mag_seg.lo)
        neg_lo = -mag_seg.hi
        if neg_lo > neg_hi:
            continue
        err = max(
            abs(
                compressor_eval(Segment(neg_lo, neg_hi, mag_seg.shift, mag_seg.slope_q8, mag_seg.intercept, 0.0, True), x)
                - target_sample(x, cfg.input_full_scale, cfg.output_full_scale, args.threshold_db, args.ratio, args.makeup_db)
            )
            for x in range(neg_lo, neg_hi + 1)
        )
        signed_segments.append(Segment(neg_lo, neg_hi, mag_seg.shift, mag_seg.slope_q8, mag_seg.intercept, err, True))

    for mag_seg in mag_segments:
        if args.slope_mode == "q8mul":
            pos_lo = mag_seg.lo
            pos_hi = min(mag_seg.hi, cfg.input_max)
            if pos_lo <= pos_hi:
                err = max(
                    abs(
                        compressor_eval(Segment(pos_lo, pos_hi, mag_seg.shift, mag_seg.slope_q8,
                                                mag_seg.intercept, 0.0, False), x)
                        - target_sample(x, cfg.input_full_scale, cfg.output_full_scale,
                                        args.threshold_db, args.ratio, args.makeup_db)
                    )
                    for x in range(pos_lo, pos_hi + 1)
                )
                signed_segments.append(Segment(pos_lo, pos_hi, mag_seg.shift, mag_seg.slope_q8,
                                               mag_seg.intercept, err, False))
            continue
        if mag_seg.lo == 0 and args.slope_mode != "q8mul":
            center_lo = -mag_seg.hi
            center_hi = min(mag_seg.hi, cfg.input_max)
            xs = list(range(center_lo, center_hi + 1))
            ys = [target_sample(x, cfg.input_full_scale, cfg.output_full_scale, args.threshold_db, args.ratio, args.makeup_db) for x in xs]
            signed_segments.append(fit_single_segment(xs, ys, shifts, cfg.output_min, cfg.output_max))
            continue
        pos_lo = mag_seg.lo
        pos_hi = min(mag_seg.hi, cfg.input_max)
        if pos_lo > pos_hi:
            continue
        xs = list(range(pos_lo, pos_hi + 1))
        ys = [target_sample(x, cfg.input_full_scale, cfg.output_full_scale, args.threshold_db, args.ratio, args.makeup_db) for x in xs]
        signed_segments.append(fit_single_segment(xs, ys, shifts, 0, cfg.output_max))

    return merge_adjacent(signed_segments), targets


def byte(v: int) -> int:
    return v & 0xFF


def word_hi(v: int) -> int:
    return (v >> 8) & 0xFF


def build_gain_table(args: argparse.Namespace, cfg: CompressorConfig) -> list[int]:
    table: list[int] = []
    env_shift = max(0, args.input_bits - 9)
    for env in range(256):
        mag = env << env_shift
        if mag == 0:
            gain = round(cfg.output_full_scale * 256.0 * pow(10.0, args.makeup_db / 20.0) /
                         cfg.input_full_scale)
        else:
            target = abs(target_sample(mag, cfg.input_full_scale, cfg.output_full_scale,
                                       args.threshold_db, args.ratio, args.makeup_db))
            gain = round(target * 256.0 / mag)
        table.append(max(0, min(255, int(gain))))
    for i in range(1, len(table)):
        if table[i] > table[i - 1]:
            table[i] = table[i - 1]
    return table


def gain_fast_shift(gain: int) -> int | None:
    if gain <= 0 or gain > 256:
        return None
    if 256 % gain != 0:
        return None
    divisor = 256 // gain
    if divisor & (divisor - 1):
        return None
    return divisor.bit_length() - 1


def emit_q8_compute(lines: list[str], slope_q8: int, intercept: int, suffix: int) -> None:
    lines.append(f"\t; y = ((x * {slope_q8}) >> 8) + {intercept}, x is unsigned magnitude")
    if slope_q8 == 0:
        lines.append("\tmov\tr5,#0")
        lines.append("\tmov\tr6,#0")
    else:
        lines.append("\tmov\ta,r6")
        lines.append("\tmov\tr0,a")
        lines.append("\tmov\ta,r5")
        lines.append(f"\tmov\ta,#{slope_q8 & 0xFF}")
        lines.append("\tmov\tb,a")
        lines.append("\tmov\ta,r5")
        lines.append("\tmul\tab")
        lines.append("\tmov\ta,b")
        lines.append("\tmov\tr5,a")
        lines.append("\tmov\ta,r0")
        lines.append(f"\tjz\tq8_high_done_{suffix}$")
        lines.append(f"\tmov\ta,#{slope_q8 & 0xFF}")
        lines.append("\tmov\tb,a")
        lines.append("\tmov\ta,r0")
        lines.append("\tmul\tab")
        lines.append("\tadd\ta,r5")
        lines.append("\tmov\tr5,a")
        lines.append(f"q8_high_done_{suffix}$:")
        lines.append("\tmov\tr6,#0")
    if intercept != 0:
        lines.append("\tmov\ta,r5")
        lines.append(f"\tadd\ta,#{byte(intercept)}")
        lines.append("\tmov\tr5,a")
        lines.append("\tmov\ta,r6")
        lines.append(f"\taddc\ta,#{word_hi(intercept)}")
        lines.append("\tmov\tr6,a")


def emit_dynamic_apply(lines: list[str], fast_gain: int, fast_shift: int | None) -> None:
    lines.append("\t; apply the gain prepared by SynthCompressorTick() to r6:r5")
    lines.append("\tmov\ta,(pSynth+pCompressorGain)")
    if fast_shift is not None:
        lines.append(f"\tcjne\ta,#{fast_gain},compress_apply_mul$")
        lines.append(f"\t; common no-compression gain: y = x >> {fast_shift}")
        for _ in range(fast_shift):
            lines.append("\tmov\ta,r6")
            lines.append("\tmov\tc,a.7")
            lines.append("\trrc\ta")
            lines.append("\tmov\tr6,a")
            lines.append("\tmov\ta,r5")
            lines.append("\trrc\ta")
            lines.append("\tmov\tr5,a")
        lines.append("\tsjmp\tcompress_done$")
    lines.append("compress_apply_mul$:")
    lines.append("\tmov\tr1,a")
    lines.append("\tmov\ta,r6")
    lines.append("\tjnb\tacc.7,compress_apply_positive$")
    lines.append("\tmov\ta,r5")
    lines.append("\tcpl\ta")
    lines.append("\tadd\ta,#1")
    lines.append("\tmov\tr5,a")
    lines.append("\tmov\ta,r6")
    lines.append("\tcpl\ta")
    lines.append("\taddc\ta,#0")
    lines.append("\tmov\tr6,a")
    emit_unsigned_mul_inline(lines, "neg")
    lines.append("\tmov\ta,r5")
    lines.append("\tcpl\ta")
    lines.append("\tadd\ta,#1")
    lines.append("\tmov\tr5,a")
    lines.append("\tmov\ta,r6")
    lines.append("\tcpl\ta")
    lines.append("\taddc\ta,#0")
    lines.append("\tmov\tr6,a")
    lines.append("\tsjmp\tcompress_done$")
    lines.append("compress_apply_positive$:")
    emit_unsigned_mul_inline(lines, "pos")
    lines.append("\tsjmp\tcompress_done$")


def emit_unsigned_mul_inline(lines: list[str], suffix: str) -> None:
    lines.append("\tmov\ta,r6")
    lines.append("\tmov\tr0,a")
    lines.append("\tmov\ta,r1")
    lines.append("\tmov\tb,a")
    lines.append("\tmov\ta,r5")
    lines.append("\tmul\tab")
    lines.append("\tmov\ta,b")
    lines.append("\tmov\tr5,a")
    lines.append("\tmov\ta,r0")
    lines.append(f"\tjz\tcompress_unsigned_high_done_{suffix}$")
    lines.append("\tmov\ta,r1")
    lines.append("\tmov\tb,a")
    lines.append("\tmov\ta,r0")
    lines.append("\tmul\tab")
    lines.append("\tadd\ta,r5")
    lines.append("\tmov\tr5,a")
    lines.append(f"compress_unsigned_high_done_{suffix}$:")
    lines.append("\tmov\tr6,#0")


def emit_dynamic_macro(lines: list[str], fast_gain: int, fast_shift: int | None) -> None:
    lines.append(".macro COMPRESS_R6R5_TO_R6R5")
    lines.append("\t; ISR only applies the gain prepared by SynthCompressorTick().")
    emit_dynamic_apply(lines, fast_gain, fast_shift)
    lines.append("compress_done$:")
    lines.append(".endm")
    lines.append("")


def emit_table(gain_table: list[int]) -> str:
    lines: list[str] = []
    lines.append("; Auto-generated by tools/gen_segment_compressor.py. Do not edit by hand.")
    lines.append("_CompressorGainTable::")
    for i in range(0, len(gain_table), 16):
        lines.append("\t.db " + ",".join(str(v) for v in gain_table[i:i + 16]))
    lines.append("")
    return "\n".join(lines)


def emit_asm(segments: list[Segment], args: argparse.Namespace, cfg: CompressorConfig) -> str:
    gain_table = build_gain_table(args, cfg)
    fast_gain = gain_table[0]
    fast_shift = gain_fast_shift(fast_gain)
    lines: list[str] = []
    lines.append("; Auto-generated by tools/gen_segment_compressor.py. Do not edit by hand.")
    lines.append(
        f"; dBFS compressor segments={len(segments)} input=[{cfg.input_min},{cfg.input_max}] "
        f"output=[{cfg.output_min},{cfg.output_max}]"
    )
    lines.append(
        f"; input_bits={args.input_bits} output_bits={args.output_bits} "
        f"threshold_db={args.threshold_db} ratio={args.ratio} makeup_db={args.makeup_db} "
        f"preset={args.preset} q8_slope_max={cfg.q8_slope_max} fast_gain={fast_gain}"
    )
    lines.append(f"; 0 dBFS is abs(input)=={cfg.input_full_scale}; output full-scale is {cfg.output_full_scale}")
    lines.append(";")
    for i, seg in enumerate(segments):
        if seg.slope_q8 >= 0:
            slope = f"{seg.slope_q8}/256"
            formula = f"((x*{seg.slope_q8})>>8)+{seg.intercept}"
        else:
            slope = "0" if seg.shift < 0 else f"1/{1 << seg.shift}"
            formula = f"(x>>{seg.shift})+{seg.intercept}"
        lines.append(
            f"; seg {i}: {seg.lo:5d}..{seg.hi:5d}  "
            f"negate={int(seg.negate)} y={formula} slope={slope} max_err={seg.error:.3f}"
        )
    lines.append("")
    lines.append("\t.globl _CompressorGainTable")
    emit_dynamic_macro(lines, fast_gain, fast_shift)
    return "\n".join(lines)


def emit_header(segments: list[Segment], targets: list[int], args: argparse.Namespace,
                cfg: CompressorConfig) -> str:
    gain_table = build_gain_table(args, cfg)
    max_error = max(
        abs(compressor_eval(seg, x) - targets[x - cfg.input_min])
        for seg in segments
        for x in range(seg.lo, seg.hi + 1)
    )
    lines: list[str] = []
    lines.append("/* Auto-generated by tools/gen_segment_compressor.py. Do not edit by hand. */")
    lines.append("#ifndef SYNTH_COMPRESSOR_GENERATED_H")
    lines.append("#define SYNTH_COMPRESSOR_GENERATED_H")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append(f"#define SYNTH_COMPRESSOR_SEGMENTS {len(segments)}")
    lines.append(f"#define SYNTH_COMPRESSOR_INPUT_MIN ({cfg.input_min})")
    lines.append(f"#define SYNTH_COMPRESSOR_INPUT_MAX ({cfg.input_max})")
    lines.append(f"#define SYNTH_COMPRESSOR_OUTPUT_MIN ({cfg.output_min})")
    lines.append(f"#define SYNTH_COMPRESSOR_OUTPUT_MAX ({cfg.output_max})")
    lines.append(f"#define SYNTH_COMPRESSOR_MAX_ERROR {max_error}")
    lines.append(f"#define SYNTH_COMPRESSOR_ENV_SHIFT {max(0, args.input_bits - 9)}")
    lines.append(f"#define SYNTH_COMPRESSOR_FAST_GAIN {gain_table[0]}")
    lines.append("")
    lines.append("typedef struct SynthCompressorSegment {")
    lines.append("\tint16_t lo;")
    lines.append("\tint16_t hi;")
    lines.append("\tint8_t shift;")
    lines.append("\tint16_t slope_q8;")
    lines.append("\tint16_t intercept;")
    lines.append("\tuint8_t negate;")
    lines.append("} SynthCompressorSegment;")
    lines.append("")
    lines.append("static const SynthCompressorSegment SynthCompressorSegments[SYNTH_COMPRESSOR_SEGMENTS] = {")
    for seg in segments:
        lines.append(f"\t{{ {seg.lo}, {seg.hi}, {seg.shift}, {seg.slope_q8}, {seg.intercept}, {1 if seg.negate else 0} }},")
    lines.append("};")
    lines.append("")
    lines.append("static const int8_t SynthCompressorTarget[SYNTH_COMPRESSOR_INPUT_MAX - SYNTH_COMPRESSOR_INPUT_MIN + 1] = {")
    for i in range(0, len(targets), 16):
        lines.append("\t" + ", ".join(str(v) for v in targets[i:i + 16]) + ",")
    lines.append("};")
    lines.append("")
    lines.append("static const uint8_t SynthCompressorGainTable[256] = {")
    for i in range(0, len(gain_table), 16):
        lines.append("\t" + ", ".join(str(v) for v in gain_table[i:i + 16]) + ",")
    lines.append("};")
    lines.append("")
    lines.append("#endif")
    lines.append("")
    return "\n".join(lines)


def parse_shifts(value: str) -> list[int]:
    shifts: list[int] = []
    for part in value.split(","):
        part = part.strip()
        if part:
            shifts.append(-1 if part == "flat" else int(part))
    return shifts


def derive_config(args: argparse.Namespace) -> CompressorConfig:
    if args.input_bits < 2:
        raise ValueError("input-bits must be at least 2")
    if args.output_bits < 2 and (args.output_min is None or args.output_max is None):
        raise ValueError("output-bits must be at least 2 when output range is not explicit")

    input_full_scale = 1 << (args.input_bits - 1)
    input_min = -input_full_scale
    input_max = input_full_scale - 1

    if args.output_min is None and args.output_max is None:
        output_max = (1 << (args.output_bits - 1)) - 1
        output_min = -output_max
    elif args.output_min is not None and args.output_max is not None:
        output_min = args.output_min
        output_max = args.output_max
    else:
        raise ValueError("output-min and output-max must be specified together")

    if output_min >= 0 or output_max <= 0:
        raise ValueError("output range must cross zero")
    output_full_scale = min(-output_min, output_max)
    if output_full_scale < 1:
        raise ValueError("output range is too small")

    q8_slope_max = args.q8_slope_max
    if q8_slope_max is None:
        makeup_gain = pow(10.0, args.makeup_db / 20.0)
        pass_through_q8 = output_full_scale * 256.0 * makeup_gain / input_full_scale
        q8_slope_max = min(256, max(16, int(ceil(pass_through_q8 * 2.0))))

    return CompressorConfig(
        input_min=input_min,
        input_max=input_max,
        input_full_scale=input_full_scale,
        output_min=output_min,
        output_max=output_max,
        output_full_scale=output_full_scale,
        q8_slope_max=q8_slope_max,
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--segments", type=int, default=9)
    parser.add_argument("--input-bits", type=int, default=10)
    parser.add_argument("--output-bits", type=int, default=8)
    parser.add_argument("--output-min", type=int)
    parser.add_argument("--output-max", type=int)
    parser.add_argument("--preset", choices=("safe", "loud", "custom"), default="safe")
    parser.add_argument("--threshold-db", type=float)
    parser.add_argument("--ratio", type=float)
    parser.add_argument("--makeup-db", type=float)
    parser.add_argument("--slope-mode", choices=("shift", "q8mul"), default="q8mul")
    parser.add_argument("--q8-slope-max", type=int)
    parser.add_argument("--q8-slope-step", type=int, default=1)
    parser.add_argument("--q8-refine-slope-step", type=int, default=0)
    parser.add_argument("--out-asm", type=Path, default=Path("Synthesizer/CompressorGenerated.inc"))
    parser.add_argument("--out-table", type=Path, default=Path("Synthesizer/CompressorTableGenerated.inc"))
    parser.add_argument("--out-header", type=Path, default=Path("Synthesizer/CompressorGenerated.h"))
    parser.add_argument("--shifts", default="0,1,2,3,4,flat")
    args = parser.parse_args()

    resolve_compressor_params(args)
    cfg = derive_config(args)
    shifts = parse_shifts(args.shifts)
    segments, targets = optimize_symmetric_segments(args, cfg, shifts)

    gain_table = build_gain_table(args, cfg)
    args.out_asm.write_text(emit_asm(segments, args, cfg), encoding="ascii")
    args.out_table.write_text(emit_table(gain_table), encoding="ascii")
    args.out_header.write_text(emit_header(segments, targets, args, cfg), encoding="ascii")

    max_error = max(seg.error for seg in segments)
    print(
        f"generated {len(segments)} segments, max error {max_error:.3f}, "
        f"target=[{cfg.output_min},{cfg.output_max}], q8_slope_max={cfg.q8_slope_max}"
    )
    for i, seg in enumerate(segments):
        slope = f"q8={seg.slope_q8:3d}" if seg.slope_q8 >= 0 else f"shift={seg.shift:2d}"
        print(f"{i:2d}: {seg.lo:5d}..{seg.hi:5d} {slope} intercept={seg.intercept:5d} negate={int(seg.negate)} err={seg.error:.3f}")


if __name__ == "__main__":
    main()
