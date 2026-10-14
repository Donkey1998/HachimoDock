# 第05课 · GPIO 与 ADC 输入：按键、编码器、摇杆

> 预计用时：2 小时 · 前置：[第04课](L04-启动序列与主循环.md)
> 路径约定：相对 `firmware/` 目录。本课精读 `main/pet_p4_input.c`（1561 行）的宏区与初始化，以及 `main/pet_p4_input_core.c`（312 行，全文精读——**全项目最适合第一个逐行读完的文件**）。

## 学习目标

1. 掌握 GPIO 输入的完整套路：配置 → 轮询 → 消抖 → 事件
2. 理解编码器正交解码和摇杆 ADC 迟滞
3. 体会"纯逻辑与硬件 IO 剥离"的可测试性设计

## 零基础补给站

| 术语 | 一句话解释 |
|---|---|
| 高/低电平 | 引脚上的电压约定：约 3.3V=高=逻辑 1，约 0V=低=逻辑 0。数字世界的全部词汇就这两个 |
| 上拉电阻 | 把引脚"默认轻轻拉向高电平"的电阻。按键一脚接 GPIO 一脚接地时，**没按下读到 1（靠上拉）、按下读到 0（被拉到地）**——所以代码里是"低电平=按下" |
| 消抖（debounce） | 按键按下的瞬间金属触点会弹跳几毫秒，电平 0/1 乱跳——不加处理一次按键会被认成十几次。软件对策=新电平稳定保持一段时间才承认 |
| ADC | 模数转换器：把引脚电压（0–3.3V 连续值）变成数字。**12 位=输出 0–4095**，好比把电压分成 4096 格刻度 |
| 衰减（attenuation） | ADC 前端的"量程扩展器"。加 12dB 衰减才能测到接近 3.3V 的满量程 |
| 迟滞（hysteresis） | "进入"和"退出"用两个不同阈值。恒温器：低于 18°C 开始制热、高于 22°C 才停——不会在 20°C 附近疯狂开关机 |
| 结构体 struct | 把几个变量打包成一个类型。`{int x; int y;}` ≈ Kotlin data class 的 C 版 |

**本课的 C 语法**：`pet_p4_input.c:44` 起的一串 `#define PET_P4_INPUT_SW1_GPIO GPIO_NUM_50`——宏给"魔法数字"起名字，全项目搜索宏名就能找到所有用到它的地方。

## 讲解

### 5.1 引脚与参数宏区（`main/pet_p4_input.c:44-64`）

先背这张表，后面所有练习围绕它：

```c
#define PET_P4_INPUT_SW1_GPIO            GPIO_NUM_50   // 按键1
#define PET_P4_INPUT_SW2_GPIO            GPIO_NUM_49   // 按键2
#define PET_P4_INPUT_SW3_GPIO            GPIO_NUM_5    // 按键3
#define PET_P4_INPUT_ENCODER_PRESS_GPIO  GPIO_NUM_4    // 编码器按压
#define PET_P4_INPUT_ENCODER_B_GPIO      GPIO_NUM_3    // 编码器 B 相
#define PET_P4_INPUT_ENCODER_A_GPIO      GPIO_NUM_2    // 编码器 A 相
#define PET_P4_INPUT_JOYSTICK_X_GPIO     GPIO_NUM_21   // 摇杆 X → ADC
#define PET_P4_INPUT_JOYSTICK_Y_GPIO     GPIO_NUM_20   // 摇杆 Y → ADC

#define PET_P4_INPUT_SAMPLE_MS           5    // 轮询周期
#define PET_P4_INPUT_DEBOUNCE_MS         25   // 消抖窗口
#define PET_P4_INPUT_LONG_PRESS_MS       700  // 长按阈值
#define PET_P4_INPUT_JOYSTICK_ACTIVATION_DELTA 900  // 摇杆激活阈值（ADC 原始值）
#define PET_P4_INPUT_JOYSTICK_RELEASE_DELTA     500 // 摇杆释放阈值
#define PET_P4_INPUT_JOYSTICK_REPEAT_DELAY_MS   350 // 推住后首次重复延迟
#define PET_P4_INPUT_JOYSTICK_REPEAT_INTERVAL_MS 140 // 重复间隔
#define PET_P4_INPUT_QUEUE_LENGTH        32   // 事件队列深度
```

（以上为该行号区宏的整理摘录，以源码为准。）

### 5.2 数字输入：按键与编码器

**轮询而非中断**：`pet_p4_input` 任务（优先级 9，`input.c:955` 创建）每 `SAMPLE_MS`(5ms) 读一次全部 GPIO。对 51 老手这很亲切——嵌入式里"轮询够了就不上中断"是合理保守主义：轮询天然消除了中断风暴和共享状态问题。

**消抖（debounce）**：机械按键按下/弹起会在几 ms 内抖动多次。软件对策：新电平必须**稳定保持 DEBOUNCE_MS(25ms)** 才被承认。5ms 采样 × 连续 5 次一致 ≈ 25ms，正好。

**长按**：按下持续时间超过 700ms 触发长按事件（和短按区分，一个键当两个用）。

**编码器正交解码**：旋转编码器有 A/B 两相，相位差 90°。读 A/B 的边沿序列就能算出方向和步数——`input_core.c` 里是纯查表/状态机实现，无堆分配。

### 5.3 模拟输入：摇杆的 ADC + 迟滞

摇杆是两个电位器（X/Y），走 **ADC1** 单次采样模式（`esp_adc/adc_oneshot.h`，12dB 衰减扩展量程），通道由 GPIO21/20 决定（宏 `ADC1_GPIO21_CHANNEL`）。开机时做**居中校准**：记录松手时的 ADC 值作为零点。

**迟滞（hysteresis）**是最值得学的细节：推过零点 ±900 才算"激活"，但激活后要回落到 ±500 以内才算"释放"。两个阈值不等的意义：如果激活/释放用同一阈值，摇杆停在阈值附近时微小抖动会导致事件疯狂开关——这和恒温器的启停滞环是同一个物理直觉。

**重复率**：推住不放，先等 350ms，然后每 140ms 连发一次"方向键"——这就是摇杆模拟方向键连按的手感来源（和你手机键盘长按连打一个思路）。

### 5.4 可测试性设计：input_core 的"无堆"纯逻辑

`main/pet_p4_input_core.c`（312 行）实现了按键/编码器/摇杆的**解码器**：输入是"当前电平/ADC 值 + 时间戳"，输出是"事件"，**零内部依赖、零 malloc、不碰任何硬件寄存器**。于是它可以在 PC 上直接编译测试：`tests/p4_input_logic_test.py` 把真实源文件拉进主机编译并断言行为。

这就是"**把纯逻辑从硬件 IO 里剥出来**"的教科书做法——嵌入式代码可测试性的关键一步。对比：如果消抖逻辑写死在读取 GPIO 的函数里，就没法离线测试了。

### 5.5 事件去向

解码出的事件进 32 深队列 → 主循环 `pet_p4_input_process` 消费 → 按**可配置的功能映射表**转换（映射表持久化在 NVS 命名空间 `pet_input`，PC 可经 `input/config` 协议改）→ 产出 `input/event` JSON 上行。

## 练习

1. **📍阅读**：精读 `main/pet_p4_input_core.c` 全文，找到按键消抖状态机的核心函数，写下它区分"短按/长按"的判断条件。
2. **🔍追踪**：在 `main/pet_p4_input.c` 中找到摇杆居中校准的代码（`pet_p4_input_init`，:906-975 附近），回答：校准值存在哪个变量？为什么要在开机时校准而不是写死？
3. **🔧动手**：把 `PET_P4_INPUT_DEBOUNCE_MS` 从 25 改成 100，重新构建烧录。描述按键手感变化。再改回 25，把 `PET_P4_INPUT_LONG_PRESS_MS` 改成 2000，感受长按变"迟钝"。
4. **🔧动手**：在仓库根目录跑 `python -m pytest firmware/tests/p4_input_logic_test.py -q`，挑一个测试用例读它的断言，说明它验证了哪条解码行为。
5. **💭思考**：摇杆重复率参数（350/140ms）如果对调成 140/350，手感会变成什么样？哪个场景下你会想调这两个值？

## 自测与参考答案

- **练习2**：开机校准捕捉的是"当前这颗摇杆的机械零点+当前温度下的电位器漂移"，写死的话换一颗摇杆或老化后零点就偏了。
- **练习3**：100ms 消抖会感觉按键"迟滞"、快速连按会被吃掉；2000ms 长按要按得非常久才触发，接近不可用——这些体感就是参数的物理意义。
- **练习5**：对调后变成"一点推就立刻连发"（像按键重复太快），350 延迟变成重复间隔会让连发变慢。列表滚动想跟手就调小两个值，防误触就调大。

## 完成标志

- [ ] 精读完 `input_core.c` 312 行（第一个全文精读成就达成）
- [ ] 完成练习3的改参-烧录-验证闭环
- [ ] 能向别人解释迟滞为什么需要两个不同阈值

## 下一课

[第06课 · I2C与触摸GT911](L06-I2C与触摸GT911.md)——第一次接触总线型外设：两根线挂三个设备，按地址说话。
