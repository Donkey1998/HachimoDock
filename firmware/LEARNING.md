# HachimoDock 固件层学习课程 · 总目录

> **面向读者**：安卓高级开发、熟悉 Python/JS、有 C 语言基础、早年做过 51 单片机、ESP32/ESP-IDF/FreeRTOS 零经验、手上有实体硬件可烧录。
>
> **课程目标**：框架级理解本固件——ESP32-P4 如何驱动各个外设、整体架构思路是什么。不追求逐行精读。
>
> 代码位置以 `文件:行号` 标注，路径均相对 `firmware/` 目录；行号基于 0.7.72-p4 版本，代码变动后以函数名搜索为准。

---

## 课程路线图

按顺序学习，每课约 1.5–2.5 小时（L12 选修），全程约 4–5 周（每周 4–6 小时）。

| 课 | 主题 | 用时 | 前置 | 状态 |
|---|---|---|---|---|
| [第00课](learning/L00-51单片机复习.md) | 51 单片机复习（可选热身） | 2h | 无 | ☐ |
| [第01课](learning/L01-项目全景与环境搭建.md) | 项目全景与环境搭建 | 1.5h | L00（可选） | ☐ |
| [第02课](learning/L02-ESP-IDF与组件体系.md) | ESP-IDF 与组件体系 | 1.5h | L01 | ☐ |
| [第03课](learning/L03-FreeRTOS最小概念集.md) | FreeRTOS 最小概念集 | 2h | L02 | ☐ |
| [第04课](learning/L04-启动序列与主循环.md) | 启动序列与主循环 | 1.5h | L03 | ☐ |
| [第05课](learning/L05-GPIO与ADC输入.md) | GPIO 与 ADC 输入 | 2h | L04 | ☐ |
| [第06课](learning/L06-I2C与触摸GT911.md) | I2C 与触摸 GT911 | 2h | L05 | ☐ |
| [第07课](learning/L07-显示与渲染管线.md) | 显示与渲染管线 | 2.5h | L06 | ☐ |
| [第08课](learning/L08-音频与codec.md) | 音频与 codec | 2h | L07 | ☐ |
| [第09课](learning/L09-USB三通道.md) | USB 三通道 | 2h | L08 | ☐ |
| [第10课](learning/L10-协议与数据流.md) | 协议与数据流 | 2h | L09 | ☐ |
| [第11课](learning/L11-架构思想与综合演练.md) | 架构思想与综合演练 | 1.5h | L10 | ☐ |
| [第12课](learning/L12-进阶专题导读.md) | 进阶专题导读（选修） | 1h+ | L11 | ☐ |

课程分四个梯段：**L00 热身**（51 基础复习，可选但推荐——唤醒寄存器/中断/定时器的直觉并建立"51→ESP32 翻译表"），**L01–L04 打地基**（环境、工程体系、RTOS、启动流程），**L05–L10 外设与协议实战**（本课程核心，按由易到难过五类外设+协议），**L11–L12 收官**（架构原则蒸馏+选修专题）。

## 每课的固定结构

```
学习目标 → 零基础补给站（术语卡 + 前置概念，读讲解前先过一遍）
→ 讲解（带 文件:行号）→ 练习（📍阅读 / 🔍追踪 / 🔧动手 / 💭思考）
→ 自测与参考答案 → 完成标志 → 下一课
```

练习共 62 道（第00课为纸笔题，其余动手题基于已核实的宏名，如 `PET_P4_INPUT_DEBOUNCE_MS`、`PET_P4_TOUCH_SAMPLE_MS`，改了就能在硬件上看到效果）。

## 学习心法（开始前读一遍）

1. **先 `.h` 后 `.c`**：头文件是模块的公开接口清单，本项目头文件普遍只有 20–60 行。
2. **先读文件头部注释**：大文件多带 `[Input]/[Output]/[Pos]/[Sync]` 结构化设计注释，先读它再进函数。有优质头注释：protocol.c、renderer.c、miniapp.c、game.c；**没有**头注释（风格密集）：audio.c、media.c。
3. **grep 跟踪调用链**：`grep -rn "函数名" main/ components/`，比顺序翻文件快得多。
4. **两个禁读文件**：`main/pet_p4_font_cn.c`（约 15 万行纯字形数据）、`main/pet_p4_builtin_components_data.c`（约 2500 行纯组件数据）。知道存在即可，永不打开。

## 配套文档

- [README.md](README.md) 项目总览、硬件方案、快速开始
- [BUILD.md](BUILD.md) 构建与烧录、工具链版本矩阵
- [protocol.md](protocol.md) USB 协议规范（59KB，按第10课的选读法使用）

---

## 附录 A：模块依赖图（阅读顺序依据）

内部 `#include "pet_p4_*"` 依赖：

```
叶子层（零内部依赖，先读）:
  input_core ← touch_core ← game ← behavior ← conversation ← audio_frontend
  media_codec ← widget_data ← stats ← lcd ← assets ← raw_assets ← system_cues

中层（依赖叶子，被枢纽依赖）:
  view ← media ← audio（被依赖最多，7 处） ← ota ← touch ← input ← miniapp

枢纽层（依赖一切，最后读）:
  diagnostics ← usb_native ← renderer ← protocol（13 个依赖） ← main（14 个依赖）
```

**推荐精读顺序**：`input_core.c`（312 行）→ `touch.c`（317 行）→ `lcd.c`（269 行）→ `usb_native.c`（716 行）→ `main.c`（855 行）→ `protocol.c`（只读头注释）。

## 附录 B：文件行数速查（main/ 目录）

| 区间 | 文件 | 处理方式 |
|---|---|---|
| <100 行 | 各 .h、tusb_config.h、media_codec.c 等 | 随用随读 |
| 100–400 行 | lcd / touch / input_core / touch_core / view / conversation / media / usb_descriptors / stats / behavior / diagnostics | 精读主力区 |
| 400–1100 行 | usb_native / ota / audio / game / input / assets / raw_assets | 框架级阅读 |
| >2400 行 | protocol(2600) / builtin_components_data(2484) / miniapp(4317) / renderer(4563) | 只读头部注释+函数目录 |
| 152381 行 | **font_cn.c** | **禁读** |

## 附录 C：常用命令

```powershell
# Windows 下的构建/烧录/测试入口（在 firmware/ 目录）
.\tools\p4.ps1          # 查看子命令
.\tools\p4.ps1 test     # 主机侧测试

# 仓库根目录
python -m pytest firmware/tests -q                  # 全部主机测试
python firmware/tests/protocol_contract_test.py     # 协议契约测试
python firmware/tests/p4_input_logic_test.py        # 输入逻辑测试（第05课）
```

## 附录 D：术语表

| 术语 | 含义 |
|---|---|
| BSP | Board Support Package，板级支持包（引脚/时序/外设初始化） |
| NVS | 非易失键值存储（Flash 上的"SharedPreferences"） |
| SPIFFS | 嵌入式文件系统（存组件包、音效） |
| A/B OTA | 双分区轮流升级、可回滚的 OTA 方案 |
| MIPI-DSI / DPI | 显示接口的命令通道 / 像素通道 |
| PPA | Pixel Processing Accelerator，P4 片上 2D 加速器（旋转用） |
| AEC / NS / VAD | 回声消除 / 降噪 / 语音活动检测 |
| codec（ES8311/ES7210） | 音频编解码芯片（播放 / 采集） |
| JSONL | 每行一条 JSON 的流式协议 |
| chipRevision v1/v3 | P4 芯片修订版（1–199 / 300–399），镜像不通用 |
| P4BU | 原生 USB 通道的自定义二进制帧协议（16 字节头） |
| buildId | 固件版本 + git sha 组成的构建标识 |

## 附录 E：延伸阅读（ESP-IDF 官方文档）

入口：ESP-IDF 编程指南（中文版，docs.espressif.com），选 ESP32-P4 目标芯片：

- **Get Started**：构建系统、烧录、monitor（对应第01–02课）
- **API 参考 → 系统API → FreeRTOS**：任务/队列/信号量完整 API（第03课的展开）
- **API 参考 → 外设 API**：GPIO、I2C、I2S、UART、LEDC、ADC 各章（对应第05–09课，官方例程可单独跑）
- **API 参考 → 存储API**：NVS、分区表、SPIFFS（第02课）
- **API 参考 → USB**：TinyUSB 设备栈（第09课）

官方例程（esp-idf 仓库 examples/peripherals/）是"最小可跑"的外设样例，和本项目代码对照阅读效果最好。

---

*本课程基于固件 0.7.72-p4 / 桌面端 0.1.101 的代码调研生成（2026-10）。行号会随版本漂移，以函数名搜索为准；代码才是最终裁判。*
