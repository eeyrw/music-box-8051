;====================================================================
; Player.inc — Player 结构体文档 (不再编译, 仅供参考)
;
; Player 由 SDCC 自动分配到 XRAM (large 模型默认)。
;
; ===================================================================
; 结构体布局 (22 字节, 无 padding):
;
;   ScoreDecoder (8 字节):
;     [0-3]   lastScoreTick   uint32_t   下一音符触发时刻
;     [4]     status          uint8_t    STATUS_STOP / STATUS_DECODING
;     [5-7]   scorePointer    __code *   乐谱当前读位置 (3 字节)
;
;   PlayScheduler (14 字节):
;     [8]     schedulerMode    uint8_t    MODE_ORDER / LIST_ONCE / SINGLE_SONG
;     [9-12]  currentScoreIndex int32_t  当前曲目编号 (-1 = 初始)
;     [13-16] maxScoreNum       uint32_t  曲目总数
;     [17-19] scoreListHeader   __code *  指向 SCRE 头部 (3 字节)
;     [20]    status            uint8_t   调度器状态
;     [21]    switchDirect      uint8_t   切歌方向
;
; 播放模式:
;   MODE_ORDER_PLAY  (0) — 循环无限播放
;   MODE_LIST_ONCE   (1) — 全部播完一轮停止
;   MODE_SINGLE_SONG (2) — 只播当前曲目, 播完停止 (UART 可手动切歌)
;====================================================================
