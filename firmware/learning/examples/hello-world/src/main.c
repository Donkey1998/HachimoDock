/* ESP32-P4 串口 Hello World
 *
 * 对应课程:L01(环境搭建)、L04(启动序列与主循环)
 * 现象:固件启动后每秒通过 UART0 打印一行递增的 Hello World,
 *       PC 端经 CH343 串口桥、以 115200 波特率在串口监视器里看到输出。
 */
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ESP-IDF 的入口函数:启动代码跑完后调用一次,相当于固件的 "main"。
 * 类比 Android:app_main() ≈ Application.onCreate(),
 * 区别是这里没有 Activity 生命周期,想持续做事就要自己写主循环。
 */
void app_main(void)
{
    int count = 0;
    while (1) {
        printf("Hello, World! #%d\n", count++);
        /* 阻塞延时 1000ms。vTaskDelay 是 FreeRTOS 的"让出 CPU":
         * 类比 Thread.sleep(),但单位是系统 tick,
         * pdMS_TO_TICKS() 负责把毫秒换算成 tick 数。 */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
