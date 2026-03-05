# NES 控制器蓝牙适配器项目

## 项目概述

本项目是一个基于 **ESP32** 的 **NES（任天堂红白机）控制器转蓝牙 HID 游戏手柄适配器**。它允许将经典的 NES 控制器连接到现代设备（PC、手机等），通过蓝牙 BLE HID 协议模拟为 Xbox 手柄设备。

### 主要功能

- **NES 控制器输入读取**：通过标准 NES 控制器协议读取 8 个按钮状态
- **蓝牙 HID Xbox 手柄输出**：模拟标准 Xbox 游戏手柄，支持 D-Pad 方向键和多个按钮
- **信号滤波**：采用多数投票算法（Majority Voting），消除按钮抖动，提高稳定性
- **LED 状态指示**：显示蓝牙连接状态和按钮按下状态

## 技术架构

### 目标平台

- **硬件**：ESP32 开发板（支持蓝牙 BLE HID）
- **开发环境**：Arduino IDE

### 核心库依赖

- `Arduino.h` - Arduino 核心库
- `BleCompositeHID.h` - 蓝牙 HID 复合设备库
- `XboxGamepadDevice.h` - Xbox 游戏手柄设备库

### 引脚定义

| 引脚 | 功能 | 说明 |
|------|------|------|
| D2 | NES_CLOCK | NES 控制器时钟信号 |
| D4 | NES_LATCH | NES 控制器锁存信号 |
| D5 | NES_DATA | NES 控制器数据输入 |
| D2 | LED_PIN | 状态指示 LED |

### NES 控制器协议

NES 控制器使用串行协议通信：
1. 拉高 **LATCH** 信号（20µs）
2. 拉低 **LATCH** 信号（10µs）
3. 连续发送 8 个时钟脉冲，每次脉冲后读取 **DATA** 引脚
4. 按钮读取顺序：A → B → Select → Start → Up → Down → Left → Right

### 信号滤波算法

采用**多数投票算法**（Majority Voting）消除按钮抖动：
- 连续采样 7 次（`READ_SAMPLES = 7`）
- 每个按钮位统计高电平次数
- 超过 5 次（`MAJORITY_THRESHOLD = 5`）则判定为按下
- 采样间隔 200µs（`SAMPLE_INTERVAL = 200`）

### 按钮映射

| NES 按钮 | Xbox 按钮 | 功能说明 |
|----------|-----------|----------|
| A | XBOX_BUTTON_A | 主攻击键 |
| B | XBOX_BUTTON_B | 副攻击键 |
| Select | XBOX_BUTTON_SELECT | 选择键 |
| Start | XBOX_BUTTON_START | 开始键 |
| Up | D-Pad North | 方向上 |
| Down | D-Pad South | 方向下 |
| Left | D-Pad West | 方向左 |
| Right | D-Pad East | 方向右 |

### 方向键组合

支持斜方向输入：
- ↑ + → → 右上
- ↓ + → → 右下
- ↓ + ← → 左下
- ↑ + ← → 左上

### 配置参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| READ_SAMPLES | 7 | 多数投票采样次数 |
| MAJORITY_THRESHOLD | 5 | 判定按钮按下的阈值 |
| SAMPLE_INTERVAL | 200 | 采样间隔（微秒） |
| SEND_INTERVAL | 50 | 按钮发送间隔（毫秒） |

## 构建和运行

### 硬件连接

```
NES Controller (公头)
┌─────────────┐
│ 1 - GND     │─────── GND
│ 2 - CLOCK   │─────── D2 (NES_CLOCK)
│ 3 - LATCH   │─────── D4 (NES_LATCH)
│ 4 - DATA    │─────── D5 (NES_DATA)
│ 5 - VCC     │─────── 5V 或 3.3V
└─────────────┘
```

### 开发环境

在 Arduino IDE 中：
1. 选择正确的开发板（**ESP32** 系列）
2. 安装所需库：
   - `BleCompositeHID`
   - `XboxGamepadDevice`
3. 选择正确的串口端口
4. 点击 **上传** 按钮

### 串口调试

- **波特率**：115200
- 启动时输出 "NES Controller Ready!"

### 运行流程

1. ESP32 上电初始化
2. 配置 NES 控制器引脚
3. 初始化蓝牙 HID 设备
4. 开始广播，等待设备连接
5. 进入主循环：
   - 检查蓝牙连接状态
   - 读取 NES 控制器状态（多数投票滤波）
   - 检测按钮变化并发送 Xbox 手柄报告
   - 更新 LED 状态

## 开发约定

### 代码风格

- 使用 C++ 基本类型和函数
- 函数命名采用驼峰命名法（如 `readNESMajority`）
- 常量使用 `#define` 宏定义，全大写命名
- 适当的注释说明关键逻辑

### 时序控制

- 主循环延迟：5ms
- NES 协议时序：
  - LATCH 高电平：20µs
  - LATCH 低电平后延迟：10µs
  - CLOCK 高电平：10µs
  - CLOCK 低电平：10µs

### 调试输出

- 通过串口输出初始化状态
- 可扩展添加更多调试信息

## 文件结构

```
sketch_mar4a/
├── sketch_mar4a.ino    # 主程序文件，包含所有功能代码
└── README.md           # 项目说明文档
```

## 扩展建议

1. **Turbo 连发功能**：添加 Turbo A/B 按键支持
2. **多控制器支持**：扩展支持 2 个或更多 NES 控制器
3. **配置存储**：使用 EEPROM 保存用户配置
4. **自定义映射**：支持按键重映射

## 注意事项

- 本项目使用蓝牙 BLE HID，需要配对的设备支持 Bluetooth 4.0+
- 某些国产 NES 控制器可能引脚定义不同，需要确认接线
- ESP32 供电需稳定，建议使用独立电源
- 首次配对时设备显示名称为 "NES Controller"
