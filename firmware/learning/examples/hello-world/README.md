# ESP32-P4 串口 Hello World(PlatformIO 版)

对应课程:L01《项目全景与环境搭建》、L04《启动序列与主循环》。

**目标现象**:烧录后,板子每秒通过串口打印一行 `Hello, World! #N`,在 PC 的串口监视器里能看到 N 递增。

## 目录结构

```
hello-world/
├── platformio.ini                    # 工程配置,类比 Android 的 build.gradle
├── boards/
│   └── esp32-p4-wlk2802-32mb.json    # 板级描述(芯片型号、Flash 大小),从主工程复制
└── src/
    └── main.c                        # 全部业务代码,只有一个入口 app_main()
```

这是一个**独立、自包含**的 PlatformIO 工程:整个文件夹拷到任何装了 PlatformIO 的机器上都能跑。

## 跑起来:三步

前提:VSCode 已装 PlatformIO 插件,且已按 L01 完成环境搭建。

**方式 A:VSCode 图形界面(推荐第一次用)**

1. 用 VSCode 打开 **`E:\HachimoDock\firmware`** 文件夹(注意:必须从这个无空格路径打开,
   原因见"常见问题"第 1 条)。PlatformIO 会同时识别主工程和本示例;
   在底部状态栏把环境切到 **hello_world**。
2. 点左下角 **✓**(Build)编译。
3. 板子插 USB(串口桥那根),点 **→**(Upload)烧录,完成后点 **插头图标**(Serial Monitor)看输出。

**方式 B:命令行**

```bash
C:/Users/L1600/.platformio/penv/Scripts/pio.exe run -d E:/HachimoDock/firmware/learning/examples/hello-world              # 编译
C:/Users/L1600/.platformio/penv/Scripts/pio.exe run -d E:/HachimoDock/firmware/learning/examples/hello-world -t upload    # 烧录
C:/Users/L1600/.platformio/penv/Scripts/pio.exe run -d E:/HachimoDock/firmware/learning/examples/hello-world -t monitor   # 看串口输出(Ctrl+C 退出)
```

> 命令行用的是 PIO 自带的 penv 版本(6.1.18,平台包已全部缓存,离线可编译)。
> 系统 Python 里的 6.2.0 会尝试重新下载 tool-scons,网络不稳时容易失败,不建议用它编本示例。

## 预期输出

```
Hello, World! #0
Hello, World! #1
Hello, World! #2
...
```

## 讲解

### 1. printf 打出来的字,去了哪里?

```
printf("...") → ESP-IDF 控制台 → UART0(板上固定引脚)→ CH343 串口桥 → USB 线 → PC 串口监视器
```

类比 Android:`printf` ≈ `Log.d()`,串口监视器 ≈ Logcat。区别是这里没有日志缓冲区的历史记录——监视器必须**开着**才能看到,错过了就错过了(除非板子再打一遍)。

### 2. app_main() 是什么?

固件上电后:ROM 引导 → bootloader → 启动代码(STARTUP)→ **调用 `app_main()`** → 我们的代码开始跑。
`app_main()` 由启动代码**只调用一次**,返回后 FreeRTOS 调度器继续运行,所以想持续干活就要像本例一样自己写 `while(1)` 主循环 + `vTaskDelay()` 让出 CPU。

类比:`app_main()` ≈ `Application.onCreate()`,但没人替你管理生命周期,主循环自己负责。

### 3. platformio.ini 每行在说什么

| 配置 | 作用 | Android 类比 |
|---|---|---|
| `platform` | 拉取哪个芯片平台包(P4 用 pioarduino 社区平台) | gradle 插件版本 |
| `board` | 用哪个板级描述文件,决定芯片型号、Flash 大小 | 设备硬件档案(device profile) |
| `framework = espidf` | 用 ESP-IDF 而不是 Arduino 框架 | 选 framework 还是裸 SDK |
| `monitor_speed` | 串口监视器波特率,双方必须一致 | 无对应,串口特有约定 |
| `upload_speed` | 烧录时 USB 传输速度 | adb push 的速度参数 |

**波特率陷阱**:本示例用 ESP-IDF 默认的 115200。主工程固件会把 UART0 重配到 4,000,000,所以从主固件换到本示例(或反过来)时,记得监视器波特率要跟着切(115200 ↔ 4000000),否则满屏乱码。

## 练习

1. **改节奏**:把 1000ms 改成 500ms,并在每行打印"已运行秒数"(提示:维护一个 `seconds` 变量)。
2. **换日志 API**:用 `ESP_LOGI("HELLO", "count=%d", count)` 替换 `printf`(头部加 `#include "esp_log.h"`),观察输出多了时间戳、级别、彩色标签——这才是正式项目的日志姿势。
3. **打印系统信息**:在循环外打印一次 `esp_get_free_heap_size()`(单位字节)和芯片信息 `esp_chip_info()`,感受"固件里也能查手机内存"。

## 常见问题

- **报错 `Detected a whitespace character in project paths`**:仓库在 `E:\StudySpace\AI Hardware\...`,
  路径里的空格会让 PlatformIO 的 ESP-IDF 构建直接拒绝。解决办法是走无空格的目录链接(junction)
  `E:\HachimoDock`(已建好,指向仓库根目录,主工程编译同样要用它)。如果不慎删掉了,在 PowerShell 重建:

  ```powershell
  New-Item -ItemType Junction -Path 'E:\HachimoDock' -Target 'E:\StudySpace\AI Hardware\HachimoDock'
  ```

  删除链接本身不影响仓库:`(Get-Item E:\HachimoDock).Delete()`(只删链接,不删文件)。
- **烧录时报找不到串口**:先装 CH343 驱动;多串口时在 `platformio.ini` 加 `upload_port = COMx`(设备管理器里确认编号)。
- **监视器乱码**:九成是波特率不一致,核对 `monitor_speed` 与上文的波特率陷阱。
- **按了复位键没输出**:检查监视器是否还开着、选的串口对不对;本示例上电即打印,不需要按任何键。
