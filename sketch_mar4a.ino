/*
 * NES Controller Bluetooth Gamepad - Xbox XInput 模式
 * Beta 版本 - 方向键抗干扰优化
 * 增加 ESP32 深度睡眠支持
 */

#include <Arduino.h>
#include <BleCompositeHID.h>
#include <XboxGamepadDevice.h>
#include <esp_sleep.h>

// NES 控制器引脚
#define NES_CLOCK  2
#define NES_LATCH  4
#define NES_DATA   5
#define LED_PIN 2

// Xbox 游戏手柄设备
XboxGamepadDevice* gamepad;
BleCompositeHID compositeHID("NES Controller", "ESP32", 100);

// 参数
#define READ_SAMPLES 7
#define MAJORITY_THRESHOLD 5
#define SAMPLE_INTERVAL 200
#define SEND_INTERVAL 50

// 自动休眠参数
#define AUTO_SLEEP_TIME_MS 300000  // 5 分钟无按键自动休眠

// 调试开关
#define DEBUG_ENABLED 0  // 1=开启调试输出，0=关闭调试输出

// 调试宏定义
#if DEBUG_ENABLED
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTF(x, ...) Serial.printf(x, ##__VA_ARGS__)
  #define DEBUG_PRINT_HEX(x) do { Serial.print("0x"); Serial.print(x, HEX); } while(0)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(x, ...)
  #define DEBUG_PRINT_HEX(x)
#endif

// 按钮位定义
#define BTN_A       0x01
#define BTN_B       0x02
#define BTN_SELECT  0x04
#define BTN_START   0x08
#define BTN_UP      0x10
#define BTN_DOWN    0x20
#define BTN_LEFT    0x40
#define BTN_RIGHT   0x80

// 状态
uint8_t currentButtons = 0;
unsigned long lastSendTime = 0;

// 自动休眠状态
unsigned long lastActivityTime = 0;

// 方向键保护机制 - 防止 Turbo 干扰导致方向键误释放
#define DIRECTION_RELEASE_THRESHOLD 2  // 需要连续检测到释放才接受
uint8_t directionReleaseCounter = 0;
uint8_t lastDirection = 0;  // 记录上一次的方向键状态

// RTC 内存 - 保存唤醒原因
RTC_DATA_ATTR bool wokeFromSleep = false;

void setup() {
  Serial.begin(115200);
  
  DEBUG_PRINTLN("NES Controller Starting...");
  
  // 首先初始化 GPIO 引脚（必须在唤醒检测之前）
  pinMode(NES_CLOCK, OUTPUT);
  pinMode(NES_LATCH, OUTPUT);
  pinMode(NES_DATA, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // 检查唤醒原因
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  
  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
    DEBUG_PRINTLN("=== Woke from deep sleep (TIMER) ===");
    wokeFromSleep = true;
    
    // 从睡眠唤醒，快速检测是否有按键
    uint8_t buttons = readNESMajority();
    if (buttons == 0) {
      // 没有按键，继续睡眠
      DEBUG_PRINTLN("No buttons pressed, going back to sleep...");
      Serial.flush();
      delay(100);
      enterDeepSleep();
      return;  // 不会执行到这里
    }
    
    // 有按键按下，完全唤醒
    DEBUG_PRINT("Button detected (");
    DEBUG_PRINT_HEX(buttons);
    DEBUG_PRINTLN("), waking up completely!");
    
    // 点亮 LED
    digitalWrite(LED_PIN, HIGH);
    
    // 重置按键状态，避免唤醒时的按键导致状态错乱
    currentButtons = 0;
  } else {
    DEBUG_PRINTLN("NES Controller Ready! (Cold boot)");
    wokeFromSleep = false;
    currentButtons = 0;
  }
  
  // 初始化休眠状态
  lastActivityTime = millis();
  
  // 初始化蓝牙 HID
  XboxSeriesXControllerDeviceConfiguration* config = new XboxSeriesXControllerDeviceConfiguration();
  BLEHostConfiguration hostConfig = config->getIdealHostConfiguration();
  gamepad = new XboxGamepadDevice(config);
  compositeHID.addDevice(gamepad);
  compositeHID.begin(hostConfig);
  
  DEBUG_PRINTLN("Controller initialized, waiting for connection...");
  
  // 等待蓝牙设备准备好
  delay(200);
  
  // 发送一个空的手柄报告，确保手柄状态清零
  sendButtons(0);
  
  Serial.flush();
  delay(300);
}

void loop() {
  // 检查是否需要进入休眠
  unsigned long idleTime = millis() - lastActivityTime;
  if (idleTime >= AUTO_SLEEP_TIME_MS) {
    enterDeepSleep();
  }
  
  if (!compositeHID.isConnected()) {
    digitalWrite(LED_PIN, (millis() / 500) % 2);
    delay(10);
    return;
  }
  
  uint8_t result = readNESMajority();
  
  // 应用方向键保护机制 - 只延迟释放，不延迟按下
  uint8_t direction = result & (BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT);
  uint8_t nonDirection = result & ~(BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT);
  
  if (direction != 0) {
    // 有方向键按下，立即重置计数器并更新方向
    directionReleaseCounter = 0;
    lastDirection = direction;
  } else if (lastDirection != 0) {
    // 方向键看似释放，需要连续检测到才接受
    directionReleaseCounter++;
    if (directionReleaseCounter >= DIRECTION_RELEASE_THRESHOLD) {
      // 真正释放
      lastDirection = 0;
      directionReleaseCounter = 0;
    }
    // 否则保持上一次的方向
    direction = lastDirection;
  } else {
    // 本来就是释放状态
    direction = 0;
  }
  
  // 合并方向键和非方向键
  result = nonDirection | direction;
  
  // 更新最后活动时间
  if (result != 0) {
    lastActivityTime = millis();
  }
  
  unsigned long now = millis();
  if (result != currentButtons && (now - lastSendTime >= SEND_INTERVAL)) {
    currentButtons = result;
    sendButtons(currentButtons);
    lastSendTime = now;
  }
  
  digitalWrite(LED_PIN, currentButtons ? HIGH : LOW);
  delay(5);
}

uint8_t readNESMajority() {
  uint8_t voteCount[8] = {0};
  
  for (int sample = 0; sample < READ_SAMPLES; sample++) {
    uint8_t raw = readNESRaw();
    for (int bit = 0; bit < 8; bit++) {
      if (raw & (1 << bit)) {
        voteCount[bit]++;
      }
    }
    delayMicroseconds(SAMPLE_INTERVAL);
  }
  
  uint8_t result = 0;
  for (int bit = 0; bit < 8; bit++) {
    if (voteCount[bit] >= MAJORITY_THRESHOLD) {
      result |= (1 << bit);
    }
  }
  
  return result;
}

uint8_t readNESRaw() {
  uint8_t result = 0;
  
  digitalWrite(NES_LATCH, HIGH);
  delayMicroseconds(20);
  digitalWrite(NES_LATCH, LOW);
  delayMicroseconds(10);
  
  for (int i = 0; i < 8; i++) {
    delayMicroseconds(5);
    if (!digitalRead(NES_DATA)) {
      result |= (1 << i);
    }
    if (i < 7) {
      digitalWrite(NES_CLOCK, HIGH);
      delayMicroseconds(10);
      digitalWrite(NES_CLOCK, LOW);
      delayMicroseconds(10);
    }
  }
  
  return result;
}

// 进入深度睡眠模式
void enterDeepSleep() {
  DEBUG_PRINTLN("=== Entering deep sleep (no activity for 5 min) ===");
  DEBUG_PRINTLN("Shutting down peripherals...");
  
  // 熄灭 LED
  digitalWrite(LED_PIN, LOW);
  
  DEBUG_PRINTLN("Deinitializing Bluetooth...");
  // esp_deep_sleep_start() 会自动关闭所有外设包括蓝牙
  
  DEBUG_PRINTLN("Configuring wake-up sources...");
  
  // 简化方案：使用 timer 周期性唤醒检测
  // 每 2 秒唤醒一次，检测是否有按键，没有则继续睡眠
  
  DEBUG_PRINTLN("Using timer wake-up (2 second interval)...");
  
  // 配置 2 秒定时器唤醒
  esp_sleep_enable_timer_wakeup(2000000);  // 2000000 微秒 = 2 秒
  
  DEBUG_PRINTLN("All peripherals off. Entering deep sleep...");
  DEBUG_PRINTLN("Will wake every 2s to check for button press");
  Serial.flush();
  
  delay(100);
  
  // 进入深度睡眠 - 这会关闭 CPU、蓝牙、WiFi 等所有外设
  esp_deep_sleep_start();
  
  // 程序不会执行到这里 - ESP32 会重启
}

void sendButtons(uint8_t buttons) {
  bool a = buttons & BTN_A;
  bool b = buttons & BTN_B;
  bool sel = buttons & BTN_SELECT;
  bool start = buttons & BTN_START;
  bool up = buttons & BTN_UP;
  bool down = buttons & BTN_DOWN;
  bool left = buttons & BTN_LEFT;
  bool right = buttons & BTN_RIGHT;
  
  gamepad->release(XBOX_BUTTON_A);
  gamepad->release(XBOX_BUTTON_B);
  gamepad->release(XBOX_BUTTON_SELECT);
  gamepad->release(XBOX_BUTTON_START);
  gamepad->releaseDPad();
  
  if (a) gamepad->press(XBOX_BUTTON_A);
  if (b) gamepad->press(XBOX_BUTTON_B);
  if (sel) gamepad->press(XBOX_BUTTON_SELECT);
  if (start) gamepad->press(XBOX_BUTTON_START);
  
  if (up && right) gamepad->pressDPadDirectionFlag((XboxDpadFlags)(XboxDpadFlags::NORTH | XboxDpadFlags::EAST));
  else if (down && right) gamepad->pressDPadDirectionFlag((XboxDpadFlags)(XboxDpadFlags::SOUTH | XboxDpadFlags::EAST));
  else if (down && left) gamepad->pressDPadDirectionFlag((XboxDpadFlags)(XboxDpadFlags::SOUTH | XboxDpadFlags::WEST));
  else if (up && left) gamepad->pressDPadDirectionFlag((XboxDpadFlags)(XboxDpadFlags::NORTH | XboxDpadFlags::WEST));
  else if (up) gamepad->pressDPadDirectionFlag(XboxDpadFlags::NORTH);
  else if (down) gamepad->pressDPadDirectionFlag(XboxDpadFlags::SOUTH);
  else if (left) gamepad->pressDPadDirectionFlag(XboxDpadFlags::WEST);
  else if (right) gamepad->pressDPadDirectionFlag(XboxDpadFlags::EAST);
  
  gamepad->sendGamepadReport();
}