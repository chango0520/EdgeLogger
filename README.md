# EdgeLogger

## 项目简介

本项目为一个工业级标准的物联网边缘数据采集节点，核心主控采用基于 ARM 32-bit Cortex™-M4 CPU 的 STM32F407VET6 芯片。该微控制器最高工作频率可达 168MHz，内部集成 FPU 浮点运算单元，能够高效处理边缘端的数据采集与协议封装任务。节点当前通过 I2C1 总线轮询读取 SHT30-DIS 传感器的温湿度数据，并利用搭载 MQTT 协议的 ESP8266 模块通过 USART1 将数据上报至 OneNET 云平台，为环境感知与数据存储提供可靠的硬件基础。  



## 硬件架构

- **核心控制器**：STM32F407VET6

  - 核心架构：ARM Cortex-M4 32b MCU+FPU  
  - 存储资源：192+4 Kbytes SRAM，支持最高 1MB Flash  
  - 工作频率：最高 168 MHz  

- **传感器模块**：SHT30-DIS（温湿度采集）

  - 接口配置：I2C1（PB6 SCL, PB7 SDA）

- **无线通信模块**：ESP8266（Wi-Fi）

  - 接口配置：USART1（PA9 TX, PA10 RX）

- **调试模块**：CH340（USB 转 TTL）

  - 接口配置：USART2（PA2 TX, PA3 RX）

    

## 软件架构

项目基于 STM32CubeMX 工具进行底层硬件初始化配置，并在 Keil MDK-ARM 环境下进行应用层开发。驱动层与应用层严格解耦，模块化设计便于后续向 FreeRTOS 等实时操作系统进行平滑移植与扩展。

代码目录结构如下：

- `Core/`：系统核心初始化代码及 `main.c`（由 STM32CubeMX 自动生成）。

- `Drivers/`：STM32 HAL 库及 CMSIS 核心外设访问层驱动文件。

- `MDK-ARM/`：Keil 工程编译输出与工程管理目录。

- `Middlewares/`：STM32CubeMX生成的FreeRTOS移植代码。

- `bsp/`：板级支持包（Board Support Package），负责底层硬件设备的驱动封装。

  - `inc/` & `src/`
  - `sht30.c / sht30.h`：SHT30-DIS 传感器 I2C 读写驱动。
  - `esp8266.c / esp8266.h`：ESP8266 模块 AT 指令集及串口收发底层驱动。

- `app/`：应用层逻辑，实现业务解耦。

  - `inc/` & `src/`

  - `app_sensor.c / app_sensor.h`：传感器数据轮询与异常数据过滤任务。

  - `app_mqtt.c / app_mqtt.h`：OneNET 平台 MQTT 协议连接、主题发布与心跳维持业务。

    

## 开发环境

- **配置工具**：STM32CubeMX

- **编译环境**：Keil MDK-ARM (使用 MDK-ARM 工具链)

- **驱动外设库**：STM32 HAL 库

- **版本控制**：Git

  

## 快速上手与调试

1. **硬件连接**：

   - 确认 SHT30-DIS 正确连接至开发板 PB6、PB7 引脚，若模块未集成 I2C 上拉电阻，需外部硬件接 4.7kΩ 上拉至 3.3V。
   - 确认 ESP8266 连接至 PA9、PA10 引脚，由于其发射瞬态电流较大，推荐提供充沛的独立 3.3V 稳压电源。
   - 通过 USB 线连接 CH340 调试接口（PA2、PA3），并在 PC 端开启串口调试终端。

2. **软件编译**：

   - 使用 Keil5 打开 `MDK-ARM` 目录下的工程文件。
   - 编译工程并使用 SWD/JTAG 调试器下载至 STM32F407VET6 开发板。

3. **运行与验证**：

   - 系统复位后，外设将自动初始化，并通过 USART2 打印启动日志。

   - 观察串口输出，验证 SHT30 温湿度数据读取状态及 ESP8266 连网与 MQTT 握手情况。

     

## 更新日志

### v2.0.0（2026/5/30）

- 移植FreeRTOS

- 全面采用FreeRTOS任务代替裸机轮询

  

### v1.1.0（2026/5/14）

- 升级接收 ESP8266 数据为 DMA+IDLE+Ringbuffer。

- 解决了 ESP8266  噪声造成的错误关闭 USART1。




### v1.0.0（2026/5/4）

- 确立项目基础分层架构（建立 `Core`, `Drivers`, `bsp`, `app` 独立结构）。
- 实现 SHT30-DIS 传感器温湿度基础轮询采集功能。
- 实现 ESP8266 MQTT上传OneNET平台功能（DMA+IDLE）
- 实现基于 USART1 的 `printf` 重定向功能，支持实时串行输出调试数据。