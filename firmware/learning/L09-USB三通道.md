# 第09课 · USB 三通道

> 预计用时：2 小时 · 前置：[第08课](L08-音频与codec.md)
> 路径约定：相对 `firmware/` 目录。精读 `main/pet_p4_usb_native.c`（716 行）的帧协议部分与 `main/pet_p4_usb_descriptors.c`（139 行，可全读）。

## 学习目标

1. 分清三种 USB 形态：串口桥、Serial-JTAG、TinyUSB 自定义设备
2. 精读 P4BU 二进制帧协议的宏区
3. 讲清"三通道汇流"架构：为什么三条路进一个队列

## 零基础补给站

| 术语 | 一句话解释 |
|---|---|
| 主机 / 设备 | USB 是主从协议：PC 是主机（发号施令），小屏固件是设备（应答）。插上电脑枚举你的就是主机——和 I2C 的主从同构 |
| 描述符（descriptor） | 设备上电时递给主机的"自我介绍表"：我叫什么（VID/PID）、我有哪些功能（接口/端点）。主机照表加载驱动 |
| VID / PID | 厂商 ID + 产品 ID。VID 0x303A 是乐鑫的注册号，PID 厂商自定——USB 世界的"身份证号" |
| 端点（endpoint） | 主机和设备之间的"编号管道"，数据只沿管道流动。OUT=主机→设备，IN=设备→主机（方向永远以主机视角定义） |
| 四类传输 | control（枚举配置必用）、bulk（大块数据、不保时、保完整——文件传输用）、interrupt（小数据、定期——键盘鼠标）、isochronous（保时不保全——摄像头/扬声器）。本课的 vendor bulk 走 bulk 类 |
| 免驱原理 | Windows 对标准类（U盘/HID）自带驱动；非标设备要么发安装包，要么像本项目这样内置 **WinUSB 兼容描述符**让系统自动绑定通用驱动 |
| TinyUSB | 开源 USB 设备协议栈（固件侧实现 USB 协议的库），乐鑫芯片内置支持 |

## 讲解

### 9.1 一片芯片的三种 USB 人格

| 通道 | 本质 | 固件侧代码 | 特点 |
|---|---|---|---|
| USB-UART（CH343 桥） | 外置桥芯片把 UART0 转 USB 串口 | `init_uart()`（`main/pet_p4_main.c:520-545`）+ `pet_p4_uart_rx` 任务（:569-588） | 最简单通用，4M 波特；板 Type-C 默认路由 |
| USB-Serial-JTAG | P4 **片内**固定功能外设 | `init_usb()`（:511-518）+ `pet_p4_usj_rx` 任务（:547-567） | 零配置免驱，调试口 |
| 原生 USB HS OTG | TinyUSB 协议栈自定义设备 | `pet_p4_native_usb_init()`（`main/pet_p4_usb_native.c:635-682`） | 完全自定义描述符，vendor bulk 大带宽；Type-C 跳线短接时启用 |

**板上跳线**决定 PC 实际连到哪条；固件**三条同时监听**不挑食（第04课启动序列步骤 8/15 都建了任务）。

### 9.2 原生 USB：描述符即身份

USB 设备靠**描述符**告诉主机"我是谁"（`main/pet_p4_usb_descriptors.c`）：

- 设备/厂商 ID：VID `0x303A`（乐鑫）PID `0x4040`（:6-18 区）
- 单个 vendor 接口，端点 OUT 0x01 / IN 0x81（枚举在 :16-17），512 字节包，High-Speed（配置描述符 :43 起；`main/tusb_config.h`）
- **MS OS 2.0 WinUSB 兼容 ID 描述符**（:65-110）：让 Windows 免驱绑定 WinUSB——不加这个，Windows 用户就得手装驱动

TinyUSB 配置的接法有个工程技巧：`main/CMakeLists.txt:94-107` 把 main 目录注入 tinyusb 组件的 include 路径，让 `usb_descriptors.c` 留在 main 里编译、提供 `tud_descriptor_*` 回调。

### 9.3 P4BU 帧协议（`main/pet_p4_usb_native.c:35-50`，全文背下来不过分）

```c
#define PET_P4_NATIVE_MAGIC    "P4BU"        // 帧头魔数
#define PET_P4_NATIVE_VERSION  1             // 协议版本
#define PET_P4_NATIVE_HEADER_LEN 16          // 帧头定长 16 字节
#define PET_P4_NATIVE_MAX_PAYLOAD (64 * 1024) // 单帧负载上限
#define PET_P4_NATIVE_PATH_MAX 96            // 文件路径字段上限
#define PET_P4_NATIVE_TRANSFER_MAX 64        // 并发传输数上限
#define PET_P4_NATIVE_RX_QUEUE_DEPTH 4       // RX 帧队列深度

enum {
  PET_P4_NATIVE_KIND_JSON = 1,       // 帧=一条 JSON 消息（和 UART 的 jsonl 同构！）
  PET_P4_NATIVE_KIND_FILE_BEGIN = 2, // 文件传输开始
  PET_P4_NATIVE_KIND_FILE_DATA = 3,  // 文件数据块
  PET_P4_NATIVE_KIND_FILE_END = 4,   // 文件传输结束
  PET_P4_NATIVE_KIND_COMMIT = 5,     // 提交（原子生效）
  PET_P4_NATIVE_KIND_PING = 6,       // 保活/探测
};
```

注意 `KIND_JSON=1`：**原生通道上的小消息就是 UART 上的 JSON 行**，帧头只是外包装——所以 protocol 层不用关心消息从哪条路来（这是三通道能汇流的前提）。`FILE_BEGIN/DATA/END + COMMIT` 四段式是**断点可控、可原子提交**的文件传输设计，大文件（形象包 ~10MB）走它。

收帧入口 `tud_vendor_rx_cb`（:606-614）：TinyUSB 收到数据回调 → 校验魔数/版本/长度 → 入 RX 队列 → `pet_p4_usb_exec` 任务慢慢处理。**回调快进快出，重活留给任务**——和第06课触摸"ISR 只发通知"同一纪律。

### 9.4 汇流架构

```
UART0 ──── pet_p4_uart_rx ─┐
USB-JTAG ── pet_p4_usj_rx ─┼──> g_protocol_queue ──> pet_p4_protocol 任务（持状态锁统一处理）
原生USB ── pet_p4_usb_exec ─┘                              │
                                            回复按来源路由（BROADCAST/UART/USJ/原生，
                                            main/pet_p4_main.c:199-228）
```

原生 USB 挂载成功时 `tud_mount_cb` 会主动发 `hello` 握手——PC 一插上就知道设备版本、芯片修订版等信息。

**为什么汇成一个队列**：消费者只有一个（协议分发要持 `g_state_mutex`，多处消费=锁竞争+顺序不定）；生产者三个（通道物理上并行）。经典的生产者多、消费者一。

## 练习

1. **📍阅读**：读 `main/pet_p4_usb_descriptors.c:6-46`，抄下 VID/PID、端点号、端点大小各是什么。
2. **📍阅读**：对照 9.3 的注释逐行读 `usb_native.c:35-50`，然后回答：为什么 `MAX_PAYLOAD` 定 64KB 而不是更大？（想想 RAM 和 96 字节路径字段暗示的用途）
3. **🔍追踪**：追踪一条 JSON 消息的完整路径：UART0 硬件 → `uart_rx_task`（:569-588）→ 队列 → `protocol_worker_task`（:406 起）。每步的函数名和所在文件写下来。
4. **🔧动手**：把设备用两种方式连 PC（跳线切换 CH343 / 原生 USB），在桌面端各连一次，观察两种方式下传输一个形象包的速度差异（如果跳线不可用，只做 CH343 一种并说明）。
5. **💭思考**：UART 上的 JSON Lines（文本）和原生 USB 的 P4BU（二进制帧）各有什么优缺点？为什么不留一种就够？（提示：调试可见性 vs 带宽/校验）

## 自测与参考答案

- **练习2**：64KB 负载 × 4 深 RX 队列已是可观 RAM 占用；大文件本来就走 FILE_* 分段流式传，单帧不必大。96 字节路径字段说明帧头里带目标路径（如形象槽文件名）。
- **练习5**：JSONL 文本肉眼可读、串口终端直接调试（第10课练习就要手写 JSON 行）；但文本解析慢、无结构校验，10MB 级文件传输低效。P4BU 二进制紧凑、定长头好校验、分段+提交原子性好；但不可读。所以：控制面走 JSON，数据面走二进制——**双协议不是冗余，是分层**。

## 完成标志

- [ ] 能背 P4BU 宏区的关键数字（16 字节头、六种 kind、64KB 上限）
- [ ] 完成练习3 的消息路径追踪
- [ ] 能解释"控制面 JSON / 数据面二进制"的分层思想

## 下一课

[第10课 · 协议与数据流](L10-协议与数据流.md)——汇流之后的消息世界：topic 分发、ACK/NACK、requestId，以及亲手用串口指挥宠物说话。
