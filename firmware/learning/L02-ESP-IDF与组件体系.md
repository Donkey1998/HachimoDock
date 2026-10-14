# 第02课 · ESP-IDF 与组件体系

> 预计用时：1.5 小时 · 前置：[第01课](L01-项目全景与环境搭建.md)
> 路径约定：相对 `firmware/` 目录。

## 学习目标

1. 读懂工程配置五件套，知道每份文件"管什么"
2. 理解组件三层结构（托管 / 本地 override / main 组件）
3. 学到"空壳 stub + 编译宏"两步剔除不想要的厂商代码的技巧

## 零基础补给站

| 术语 | 一句话解释 |
|---|---|
| 构建系统 | 把源代码变成可执行文件的自动化流水线。老前辈是 make，C++ 世界用 CMake，本项目是 PlatformIO 包着 CMake——**角色等价于安卓的 Gradle** |
| 编译四阶段 | 预处理（展开 #include 和宏）→ 编译（C 变汇编）→ 汇编（变机器码）→ 链接（拼上库，生成最终文件） |
| `#include "x.h"` | 把头文件内容原样粘贴进来。`.h` 是"说明书"（声明有哪些函数），`.c` 是"实现" |
| 宏 `#define` | 编译前的文本替换。`#define N 5` 之后代码里的 `N` 全部变成 `5`——没有类型、零运行时开销 |
| 条件编译 `#if` | 让某些代码**根本不参与编译**（不是运行时 if）。第 2.3 节的 stub 技巧全靠它 |
| 组件（component） | ESP-IDF 世界对"库"的叫法：一个目录=一个组件，对外暴露 .h 接口。≈ Maven/Gradle 里的一个 dependency |
| 依赖解析 / lock | 声明"我要 A 1.2 版"，工具自动下载 A 和 A 的依赖们；lock 文件锁定实际版本保证可复现——和 `package-lock.json` / Gradle lockfile 完全同构 |

**C 基础唤醒**：如果 `#if` / `#define` 已经陌生，现在花 10 分钟找任何一个"C 预处理器"教程过一遍——本课和后续所有课程都建立在这两个语法上。

## 讲解

### 2.1 工程配置五件套

用安卓类比：`build.gradle` + `AndroidManifest` 的角色在这里被拆成五份文件：

| 文件 | 安卓世界对应 | 关键点 |
|---|---|---|
| `platformio.ini` | CI 构建脚本 + productFlavors | 两个环境：`esp32_p4_evboard`（v1 芯片）与 `esp32_p4_evboard_v3`，**镜像不可互刷**；全局宏 `BSP_CONFIG_NO_GRAPHIC_LIB=1`（:9） |
| `CMakeLists.txt` | 工程级 build.gradle | `PROJECT_VER 0.7.72-p4`、协议 schema 版本 7、UART 波特率 4000000（:31）、buildId=版本+git sha |
| `main/idf_component.yml` | dependencies 块 | 声明托管依赖：`espressif/esp-sr 2.5.3`、`espressif/esp_h264 ^1.3.6`、`espressif/tinyusb`；以及 `override_path` 指向本地组件（:4-10） |
| `partitions.csv` | （无对应，系统概念） | 第01课已读：8 个分区，注意 `otadata` 是 A/B 槽切换指针 |
| `sdkconfig.defaults` | gradle.properties + proguard | 编译期配置默认值：PSRAM 200MHz+XIP（:14-18）、FreeRTOS 1000Hz（:19）、H.264 解码 IRAM 加速（:26-31） |

`dependencies.lock` 是自动生成的版本锁定文件（对应 `gradle lockfile`），改动依赖后重新解析。

### 2.2 组件三层结构（驱动接入总纲）

ESP-IDF 的世界由**组件（component）**拼成，每个组件 = 一个目录 + `CMakeLists.txt` + 源码，对外暴露 `.h` 接口。本项目的组件来自三层：

**第一层：ESP-IDF 内置组件**——芯片厂商提供的驱动，如 `esp_driver_gpio`、`esp_lcd`、`nvs_flash`、`spiffs`。`main/CMakeLists.txt` 的 `REQUIRES` 段按名字点名要链接哪些（第04课会看到 :43-44 的 `esp_driver_jpeg/ppa`）。

**第二层：托管组件**——写在 `main/idf_component.yml` 里，组件管理器从乐鑫 registry 自动拉取，如 GT911 触摸驱动 `esp_lcd_touch_gt911 1.2.1`、音频抽象 `esp_codec_dev`、LVGL 9.4.0。完全对应 Gradle 拉 Maven 依赖。

**第三层：本地 override 组件**——`components/` 目录下共 3 个，用 `override_path` 把**同名组件**指向本地源码，registry 版本被顶掉：

| 组件 | 性质 | 为什么要 override |
|---|---|---|
| `esp_lcd_st7701` | 官方屏幕驱动的真分支 | 加了 `skip_mipi_id_read` 等适配改动（配合只写 DSI 链路） |
| `esp32_p4_wifi6_touch_lcd_4_3` | Waveshare BSP 本地适配版 | 改了 480×640 时序、DMA 描述符数 |
| `esp_lvgl_adapter` | **0.0.0 空壳 stub** | 见 2.3，本课主角 |

### 2.3 精妙技巧：用空壳 stub"关掉"BSP 的图形栈

问题：BSP 依赖声明里要求 `esp_lvgl_adapter`（LVGL 适配器），但本项目不想用 BSP 的 LVGL 显示管线，想要自研渲染循环。

直接删依赖？不行，BSP 的 yml 不是我们说了算。解法分两步，**缺一不可**：

1. **空壳满足链接**：放一个 `components/esp_lvgl_adapter`，里面只有 CMakeLists 注册 include 目录、零源码。依赖解析通过，但没有任何 LVGL 适配代码被编进来。
2. **编译宏剔除调用**：BSP 源码里所有用到 LVGL 的分支都被 `#if` 包着，`platformio.ini:9` 的全局宏 `BSP_CONFIG_NO_GRAPHIC_LIB=1` 让这些分支整体不参与编译。

结果：LVGL 9.4.0 仍在依赖树里，但**只被当字体引擎用**（`lv_font_get_glyph_dsc` 取 16px 中文字形，`main/pet_p4_renderer.c:789-943`），UI 框架完全是自研的。

### 2.4 BSP 的正确用法：当"硬件 bring-up 库"，不当框架

BSP（Board Support Package，板级支持包）的真正价值在**引脚定义和时序参数**（看 `components/esp32_p4_wifi6_touch_lcd_4_3/include/bsp/` 下的头文件，背光 GPIO26、I2C 引脚 7/8 全在这里），不在它的软件框架。

main 代码对 BSP 的调用总共只有 12 处设备级 API：`bsp_display_new()`（拿面板句柄）、`bsp_i2c_init()`、`bsp_audio_codec_speaker/microphone_init()`。BSP 的 SD 卡、USB Host、SPIFFS、LVGL 封装全部绕过——SPIFFS 是 main 自己挂载的。

## 练习

1. **📍阅读**：通读 `platformio.ini`，回答：v1 和 v3 环境各自的 PlatformIO 平台版本是什么？`custom_p4_chip_family` 在 v3 环境里设成了什么？
2. **📍阅读**：打开 `main/idf_component.yml`（约 10 行），列出所有托管依赖的名字和版本约束，指出哪些条目带了 `override_path`。
3. **🔍追踪**：GT911 触摸驱动不在 `components/` 也不在 main 里。打开 `dependencies.lock`，搜索 `esp_lcd_touch_gt911`，记下它的版本号和它被哪个组件声明引入（提示：不是 main）。
4. **💭思考**：如果只要空壳 stub、不要编译宏（或反过来），各会发生什么？（提示：一个是链接期问题，一个是编译期问题）
5. **💭思考**：为什么本项目要费力 override `esp_lcd_st7701` 而不是把改动直接写进 BSP？两种放置方式各适合什么改动？

## 自测与参考答案

- **练习1**：v1 = 55.03.32（对应 ESP-IDF 5.5.1），v3 = 55.03.38-1（ESP-IDF 5.5.4，400MHz）；v3 的 `custom_p4_chip_family = v3`。
- **练习3**：1.2.1，由 BSP（esp32_p4_wifi6_touch_lcd_4_3）的 idf_component.yml 声明——依赖是传递的，main 不需要直接声明它。
- **练习4**：只要 stub 不要宏：BSP 里对不存在符号的调用（`esp_lv_adapter_*`）编译失败；只要宏不要 stub：依赖解析阶段就找不到 `esp_lvgl_adapter` 组件，直接报错。
- **练习5**：改的是"这颗屏幕芯片"的驱动逻辑→放驱动组件；改的是"这块板"的引脚/外设组合→放 BSP。`skip_mipi_id_read` 属于 ST7701S 驱动行为，所以 override 驱动组件。

## 完成标志

- [ ] 能说出五件套各自管什么
- [ ] 能向别人解释 stub + 宏两步剔除的原理
- [ ] 知道去哪个文件查一个组件"从哪来、什么版本"

## 下一课

[第03课 · FreeRTOS最小概念集](L03-FreeRTOS最小概念集.md)——固件的"多线程世界"：10 个任务、3 种同步原语，每个概念都配项目里的真实代码。
