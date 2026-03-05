/*
 * NES Controller Bluetooth Gamepad - Xbox XInput 模式
 * 最终版本 - 兼容 QuickNES 核心
 */

#include <Arduino.h>
#include <BleCompositeHID.h>
#include <XboxGamepadDevice.h>

// NES控制器引脚
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

void setup() {
  Serial.begin(115200);
  Serial.println("NES Controller Ready!");
  
  pinMode(NES_CLOCK, OUTPUT);
  pinMode(NES_LATCH, OUTPUT);
  pinMode(NES_DATA, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  XboxSeriesXControllerDeviceConfiguration* config = new XboxSeriesXControllerDeviceConfiguration();
  BLEHostConfiguration hostConfig = config->getIdealHostConfiguration();
  gamepad = new XboxGamepadDevice(config);
  compositeHID.addDevice(gamepad);
  compositeHID.begin(hostConfig);
}

void loop() {
  if (!compositeHID.isConnected()) {
    digitalWrite(LED_PIN, (millis() / 500) % 2);
    delay(10);
    return;
  }
  
  uint8_t result = readNESMajority();
  
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