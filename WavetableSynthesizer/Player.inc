;====================================================================
; Player.inc — Player 结构体文档 (不再编译, 仅供参考)
;
; Player 由 SDCC 自动分配到 XRAM (large 模型默认)。
;
; ===================================================================
; 结构体布局 (~31 字节, 无 padding):
;
;   SSCR_Player (17 字节):
;     [0-2]   data            __code uint8_t *  事件数据指针 (3 字节)
;     [3-6]   length          uint32_t           事件数据长度
;     [7-10]  position        uint32_t           当前读取偏移
;     [11-14] nextEventTick   uint32_t           下一事件触发 tick
;     [15]    flags           uint8_t            SSCR header flags
;     [16]    totalTranspose  int8_t             编码移调值
;     [17]    status          uint8_t            STATUS_STOP / STATUS_DECODING
;
;   PlayScheduler (14 字节):
;     [18]    schedulerMode    uint8_t           播放模式
;     [19-22] currentScoreIndex int32_t          当前曲目编号 (-1 = 初始)
;     [23-26] maxScoreNum       uint32_t         曲目总数
;     [27-29] ssplData          __code uint8_t *  完整 SSPL 数据指针 (3 字节)
;     [30]    status            uint8_t           调度器状态
;     [31]    switchDirect      uint8_t           切歌方向
;
; 播放模式:
;   MODE_ORDER_PLAY  (0) — 循环无限播放
;   MODE_LIST_ONCE   (1) — 全部播完一轮停止
;   MODE_SINGLE_SONG (2) — 只播当前曲目, 播完停止 (UART 可手动切歌)
;
; SSPL 容器: Score[] = "SSPL" header + entry table + SSCR data[]
; SSCR 乐谱: 13-byte header + 6-bit delta chunks + event bytes
;====================================================================
