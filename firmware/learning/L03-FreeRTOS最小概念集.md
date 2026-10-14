# 第03课 · FreeRTOS 最小概念集

> 预计用时：2 小时 · 前置：[第02课](L02-ESP-IDF与组件体系.md)
> 路径约定：相对 `firmware/` 目录。本课只讲项目里**实际出现**的 RTOS 用法，不做百科式展开。

## 学习目标

1. 掌握 6 个 FreeRTOS 概念：任务、优先级、核绑定、队列、互斥量、任务通知
2. 记住本固件的 10 任务架构表，能解释优先级设计的语义
3. 建立"安卓概念 → FreeRTOS 概念"的翻译能力

## 零基础补给站

| 术语 | 一句话解释 |
|---|---|
| 裸机（bare-metal） | 没有操作系统，一个 while(1) 轮询一切——第00课的 51 就是裸机 |
| RTOS | 实时操作系统：内核很小，但提供任务/队列/信号量这些"多线程积木"。FreeRTOS 是嵌入式界事实标准（安卓的祖宗 Linux 之一系太大，塞不进芯片） |
| 任务（task） | ≈ 线程：独立的执行流，有自己的函数和栈。`xTaskCreate(函数, 名字, 栈大小, 参数, 优先级, 句柄)` 六个参数一次看清 |
| 栈 | 函数的局部变量和调用记录存放地。每个任务独享一块；局部变量太大或递归太深会**栈溢出**——嵌入式最经典崩溃 |
| 调度器 / 抢占 | 决定"现在 CPU 给谁用"的裁判。**抢占式**=高优先级任务可以随时打断低优先级，不用等它主动让出 |
| 上下文切换 | 任务换人时，CPU 把当前任务的寄存器现场保存、恢复下一个任务的现场——看似并行的真相是高速轮换 |
| 阻塞 / 非阻塞 | 阻塞=原地等待（等队列、等延时）；非阻塞=等不到就立刻返回继续干别的。UI 卡顿的经典原因就是在不该阻塞的地方阻塞了 |
| 死锁 | 任务 A 拿着锁 1 等锁 2，任务 B 拿着锁 2 等锁 1，俩人永远等下去。解法：全员按相同顺序拿锁 |
| ISR | 中断服务程序：硬件事件（数据到达）触发的一段代码。铁律见第00课——越短越好 |

**本课会用到的 C 语法**：`xTaskCreate` 的第一个参数是**函数指针**（把函数当变量传给别人，安卓里 `View.OnClickListener` 的底层同类物）；第四个参数 `void*` 是"任意类型的指针"，用来给任务传自定义参数。

**volatile 关键字**（嵌入式高频坑）：告诉编译器"这个变量随时会被你看不见的力量改变（ISR/别的核），别自作聪明缓存它的值"。多个任务共享的标志变量若不加 volatile，可能读到旧值——看到代码里的 `volatile` 不要删。

## 讲解

### 3.1 概念速览（每个都配项目真实位置）

**任务（Task）**：独立函数 + 私有栈 + 优先级，创建后永远跑自己的 `while(true)`。创建用 `xTaskCreatePinnedToCore()`（可绑核）或 `xTaskCreate()`（任意核）。`app_main` 本身也是任务。示例：`xTaskCreate(touch_task, "pet_p4_touch", 4096, NULL, 8, NULL)`（`main/pet_p4_touch.c:128`）——参数依次是函数、名字、栈大小、参数、优先级、句柄。

**优先级**：**数字越大优先级越高**（注意：和安卓 `Process.THREAD_PRIORITY_*` 数字越小越高相反）。高优先级任务不睡就一直霸占 CPU，所以每个任务循环里必有 `vTaskDelay()` 主动让出。

**核绑定**：双核各自跑独立的调度器，`core` 参数 0/1 指定钉死哪个核。跨核共享数据必须加锁，本项目的音频任务对绑 core 0（采集和播放之间强交互），H.264 解码钉 core 1（和渲染抢不同的核）。

**队列（Queue）**：线程安全 FIFO，`xQueueSend/xQueueReceive`，生产者-消费者解耦的标准件。项目最核心的队列 `g_protocol_queue`：UART 和 USB-JTAG 的 RX 任务直接往里塞，原生 USB 的帧经 `pet_p4_usb_exec` 解包后入队（生产者共三路），`protocol_worker_task` 统一消费（`main/pet_p4_main.c:647` 创建）。

**互斥量（Mutex）**：跨任务共享数据的锁，`xSemaphoreTake/Give`。项目 4 把锁各有分工：`g_render_mutex`（渲染期间状态不被改）、`g_state_mutex`（运行时状态）、`g_transport_mutex`（发送通道）。启动时统一创建（`main/pet_p4_main.c:607-643`）。

**任务通知（Task Notification）**：最轻量的"喂，醒醒"信号，`ulTaskNotifyGive/xTaskNotifyTake`。音频采集任务平时挂起等通知，按下语音键才被唤醒干活，不空转。

**信号量（Semaphore，二值）**：当"完成事件"用。LCD 的 DMA 传帧完成后 `on_refresh_done` 回调释放信号量，渲染循环等它——效果类似"垂直同步"（`main/pet_p4_lcd.c:69-82`，第07课细讲）。

### 3.2 安卓概念映射表

| 安卓 | FreeRTOS（本项目实例） |
|---|---|
| Handler/Looper/MessageQueue | 队列：`g_protocol_queue` |
| `synchronized` | 互斥量：`g_render_mutex` |
| 线程优先级（数字小=高） | 任务优先级（**数字大=高**） |
| big.LITTLE 大小核感知 | `xTaskCreatePinnedToCore` 绑核 |
| `SharedPreferences` | NVS（键值进 Flash，如命名空间 `pet_input`） |
| HAL/AIDL 接口分层 | 组件 `.h` 接口 / `.c` 实现 |
| APK + 系统级 OTA | 固件镜像 + A/B 双分区 OTA |
| Gradle 依赖 | idf_component.yml + dependencies.lock |

### 3.3 本固件 10 任务架构表

| 任务名 | 创建位置 | 优先级 | 核 | 职责 |
|---|---|---|---|---|
| `pet_p4_audio` | `main/pet_p4_audio.c:720` | 18 | 0 | 麦克风采集 |
| `pet_p4_playback` | `main/pet_p4_audio.c:744` | 18 | 0 | 扬声器播放 |
| `h264-pack` | `main/pet_p4_renderer.c:4360` | 16 | 1 | H.264 解码喂帧 |
| `pet_p4_protocol` | `main/pet_p4_main.c:647` | 11 | 任意 | 消费协议队列，分发 JSON |
| `pet_p4_uart_rx` | `main/pet_p4_main.c:718` | 10 | 任意 | UART0 接收 → 入协议队列 |
| `pet_p4_usj_rx` | `main/pet_p4_main.c:726` | 10 | 任意 | USB-Serial-JTAG 接收 |
| `pet_p4_input` | `main/pet_p4_input.c:955` | 9 | 任意 | 按键/编码器/摇杆轮询 |
| `pet_p4_usb_exec` | `main/pet_p4_usb_native.c:666` | 9 | 任意 | 原生 USB 帧处理 |
| `pet_p4_touch` | `main/pet_p4_touch.c:128` | 8 | 任意 | 触摸轮询（15ms） |
| `pet_p4_tusb` | `main/pet_p4_usb_native.c:676` | 8 | 任意 | TinyUSB 协议栈任务 |

加上 `app_main` 主循环共 11 个执行流。优先级表直接编码了产品价值观：**音频实时性（18）> 协议处理（11）> 人机输入（9）> 触摸/协议栈（8）**——音频断流人耳立刻听得出，触摸慢 10ms 没人在意。

## 练习

1. **📍阅读**：打开 `main/pet_p4_main.c`，找到 4 个互斥量的创建代码（约 :607-643），抄下变量名，猜各自保护什么。
2. **🔍追踪**：`g_protocol_queue` 有几个生产者、几个消费者？用 `grep -n "g_protocol_queue" main/*.c` 数出来，写出每个生产者的任务名。
3. **🔍追踪**：在 `main/pet_p4_renderer.c` 或 `main/pet_p4_main.c` 中找 `g_render_mutex` 的 `xSemaphoreTake` 和 `xSemaphoreGive` 调用，数一数是否配对（Take 几次 Give 几次）。
4. **💭思考**：如果给 `pet_p4_touch` 任务升到优先级 19（超过音频），会发生什么？如果把它降到 5 呢？
5. **💭思考**：为什么 `pet_p4_audio` 和 `pet_p4_playback` 必须绑同一个核，而 `pet_p4_uart_rx` 绑哪个核无所谓？

## 自测与参考答案

- **练习2**：生产者 3 个（`pet_p4_uart_rx`、`pet_p4_usj_rx`，原生 USB 路径经 `pet_p4_usb_exec` 间接入队），消费者 1 个（`pet_p4_protocol`）。
- **练习4**：升到 19：触摸轮询 15ms 一次、每次占用时间很短，单看优先级不会饿死音频，但**违背了"音频最高"的语义约定**，后续维护者会误判；降到 5：触摸采样周期抖动增大，长按/滑动的判定可能不准。教训：优先级是团队约定的"重要性契约"。
- **练习5**：采集/播放共享同一套 I2S/codec 状态，同核可以避免跨核锁开销和调度时序漂移（音频对时序敏感）；UART RX 只是搬运字节进队列，无共享状态、无实时性要求。

## 完成标志

- [ ] 能默写 10 任务表中的前 5 行（名字+优先级+职责）
- [ ] 能说出队列、互斥量、通知、信号量在本项目的至少各一个用例
- [ ] 做安卓→FreeRTOS 概念翻译不再卡壳

## 下一课

[第04课 · 启动序列与主循环](L04-启动序列与主循环.md)——从上电到宠物出现在屏幕上，中间的 17 步每一步都是设计决定。
