# NES Controller Bluetooth Adapter

[中文文档](README_zh_CN.md)

## Overview

This project is an **ESP32-based NES (Nintendo Entertainment System) controller to Bluetooth HID gamepad adapter**. It allows you to connect classic NES controllers to modern devices (PC, smartphone, etc.) via Bluetooth HID protocol, emulating an Xbox gamepad device.

### Key Features

- **NES Controller Input Reading**: Reads 8 button states via standard NES controller protocol
- **Bluetooth HID Xbox Output**: Emulates a standard Xbox gamepad with D-Pad and multiple buttons
- **Signal Filtering**: Uses majority voting algorithm to eliminate button bounce and improve stability
- **LED Status Indicator**: Shows connection status and button press states
- **Auto Sleep Mode**: Automatically enters deep sleep after 5 minutes of inactivity to save power (press any button to wake up)

---

## Hardware Specifications

### Development Board

- **ESP32** (development board with Bluetooth BLE HID support)

### Pin Definitions

| Pin | Function | Description |
|-----|----------|-------------|
| D2 | NES_CLOCK | NES controller clock signal |
| D4 | NES_LATCH | NES controller latch signal |
| D5 | NES_DATA | NES controller data input |
| D2 | LED_PIN | Status indicator LED |

### NES Controller Interface

```
NES Controller (Male Connector)
┌─────────────┐
│ 1 - GND     │─────── GND
│ 2 - CLOCK   │─────── D2 (NES_CLOCK)
│ 3 - LATCH   │─────── D4 (NES_LATCH)
│ 4 - DATA    │─────── D5 (NES_DATA)
│ 5 - VCC     │─────── 5V or 3.3V
└─────────────┘
```

---

## Technical Architecture

### Library Dependencies

- `Arduino.h` - Arduino core library
- `BleCompositeHID.h` - Bluetooth HID composite device library
- `XboxGamepadDevice.h` - Xbox gamepad device library
- `esp_sleep.h` - ESP32 deep sleep library

### NES Controller Protocol

NES controller uses serial protocol communication:
1. Pull **LATCH** signal HIGH (20µs)
2. Pull **LATCH** signal LOW (10µs)
3. Send 8 consecutive clock pulses, read **DATA** pin after each pulse
4. Button reading order: A → B → Select → Start → Up → Down → Left → Right

### Signal Filtering Algorithm

Uses **Majority Voting** algorithm to eliminate button bounce:
- Sample 7 times consecutively (`READ_SAMPLES = 7`)
- Count HIGH states for each button bit
- Determine as pressed if count exceeds 5 (`MAJORITY_THRESHOLD = 5`)
- Sample interval: 200µs (`SAMPLE_INTERVAL = 200`)

### Auto Sleep Feature

- **Sleep timeout**: 5 minutes of no button activity
- **Sleep mode**: ESP32 deep sleep (consumes ~10µA)
- **Wake-up**: Timer wakes every 2 seconds to check for button press
- **Full wake-up**: Any button press triggers full system wake-up
- **Power saving**: Bluetooth and peripherals are shut down during deep sleep

---

## Button Mapping

| NES Button | Xbox Button | Function |
|------------|-------------|----------|
| A | XBOX_BUTTON_A | Main attack button |
| B | XBOX_BUTTON_B | Secondary attack button |
| Select | XBOX_BUTTON_SELECT | Select button |
| Start | XBOX_BUTTON_START | Start button |
| Up | D-Pad North | Direction Up |
| Down | D-Pad South | Direction Down |
| Left | D-Pad West | Direction Left |
| Right | D-Pad East | Direction Right |

### D-Pad Combinations

Supports diagonal input:
- ↑ + → → Northeast
- ↓ + → → Southeast
- ↓ + ← → Southwest
- ↑ + ← → Northwest

---

## Configuration Parameters

| Parameter | Default Value | Description |
|-----------|---------------|-------------|
| READ_SAMPLES | 7 | Number of samples for majority voting |
| MAJORITY_THRESHOLD | 5 | Threshold to determine button press |
| SAMPLE_INTERVAL | 200 | Sample interval (microseconds) |
| SEND_INTERVAL | 50 | Button report send interval (milliseconds) |
| AUTO_SLEEP_TIME_MS | 300000 | Auto sleep timeout (5 minutes, milliseconds) |
| DEBUG_ENABLED | 0 | Debug output switch (1=enabled, 0=disabled) |

---

## Build and Run

### Hardware Connection

1. Connect 5 wires from NES controller to corresponding ESP32 pins
2. Ensure common GND connection
3. Connect VCC to 5V or 3.3V (depending on NES controller model)

### Development Environment

- **IDE**: Arduino IDE
- **Board**: ESP32 (requires ESP32 core installation)
- **Library Manager**: Install the following libraries via Arduino IDE Library Manager:
  - `BleCompositeHID`
  - `XboxGamepadDevice`

### Serial Debugging

- **Baud Rate**: 115200
- Startup outputs "NES Controller Ready!"
- Debug information can be enabled by setting `DEBUG_ENABLED = 1`

---

## Program Flow

### Initialization Flow (`setup()`)

1. Initialize serial debug (115200 baud)
2. Configure NES controller pins (CLOCK, LATCH as output, DATA as input with pull-up)
3. Configure LED pin
4. Create Xbox controller configuration
5. Initialize Bluetooth HID composite device
6. Start advertising

### Main Loop (`loop()`)

```
┌─────────────────────────────────────┐
│ Check sleep timeout                 │
│   └─ 5 min idle → enter deep sleep  │
├─────────────────────────────────────┤
│ Check Bluetooth connection status   │
│   ├─ Not connected: LED blink       │
│   └─ Connected: Continue            │
├─────────────────────────────────────┤
│ Read NES controller state           │
│   ├─ Majority voting filter         │
│   └─ Raw reading                    │
├─────────────────────────────────────┤
│ Detect button changes               │
│   ├─ Changed and interval elapsed   │
│   └─ Send Xbox gamepad report       │
├─────────────────────────────────────┤
│ LED status update                   │
│   └─ Light on when button pressed   │
└─────────────────────────────────────┘
```

---

## LED Status Indicators

| Status | LED Behavior | Description |
|--------|--------------|-------------|
| Not Connected | Blink every 500ms | Waiting for Bluetooth pairing |
| Connected | Lights on button press | Normal operation |
| Deep Sleep | OFF | Power saving mode |

---

## Extended Features

### Optional Extensions

1. **Turbo Function**: Add Turbo A/B button support
2. **Multi-Controller Support**: Support for 2 or more NES controllers
3. **Configuration Storage**: Use EEPROM to save settings
4. **Custom Mapping**: Support button remapping

---

## Notes

- This project uses Bluetooth BLE HID, requires devices supporting Bluetooth 4.0+
- Verify NES controller wiring, as some third-party compatible controllers may have different pin definitions
- ESP32 power supply should be stable, recommend using independent power source
- Device name shows as "NES Controller" during first pairing
- Deep sleep mode significantly reduces power consumption (~10µA vs ~50-100mA active)

---

## File Structure

```
sketch_mar4a/
├── sketch_mar4a.ino    # Main program file with all functionality
├── README.md           # English documentation (this file)
└── README_zh_CN.md     # Chinese documentation
```

---

## License

This project is open source. Feel free to modify and distribute.
