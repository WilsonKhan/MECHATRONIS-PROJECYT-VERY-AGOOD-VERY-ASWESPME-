# BUZZ-IN: Game Show Buzzer System

MTRX2700 Major Project — Semester 1, 2026

## Scope

This project was completed individually rather than in the intended team of 5. The scope has been adjusted accordingly. The following mandated components are implemented:

- Actuator (DC motor / roulette wheel)
- Two sensors (LDR via ADC, onboard IMU via I2C/SPI)
- UART communication (3 channels: 2x Arduino pods, 1x PC)
- Additional serial interface (I2C/SPI to onboard IMU)
- ADC input (LDR light sensor)
- PWM output (motor speed control)
- Computer interface (Python scoreboard + sound effects)

Not implemented: communication between two STM32 boards (only one STM32 available for solo work). The architecture is documented such that a second STM32 could replace an Arduino pod without restructuring.

## System Overview

A two-player game show buzzer system. Players compete to buzz in first on trivia questions. A master console (STM32F3 Discovery) arbitrates the lockout, tracks scores, and runs a roulette bonus round. A PC displays the scoreboard and plays sound effects. First to 3 points wins.

## Game Flow

1. Host presses ARM button on the master console
2. Both contestant pods are armed and can now buzz in
3. First player to press their buzz button locks out the other player
4. Host reads the trivia question from the PowerPoint
5. Player answers verbally
6. Host presses CORRECT (point awarded) or WRONG (round re-arms for other player)
7. Scores update on pod LEDs and PC scoreboard
8. Every 3rd question: optional roulette bonus round with spinning wheel
9. First player to reach 3 points wins

## Hardware

### Components

- 1x STM32F3 Discovery board (master console)
- 2x Arduino Nano (contestant pods)
- 3x tactile buttons on master console (correct, wrong, roulette)
- 2x buzz buttons (one per pod)
- 6x LEDs with 220 ohm resistors (3 score LEDs per pod)
- 1x LDR + 10k ohm resistor (light sensor, voltage divider)
- 1x DC motor + motor driver (roulette wheel)
- 1k ohm + 2.2k ohm resistors for Arduino-to-STM32 voltage dividers

### Pin Assignments — STM32F3 Discovery

| Pin | Function | Connected To |
|-----|----------|-------------|
| PA0 | User button (onboard) | ARM / cancel round |
| PA1 | ADC1 CH2 | LDR voltage divider |
| PA2 | USART2 TX | Pod 1 Arduino RX |
| PA3 | USART2 RX | Pod 1 Arduino TX (via voltage divider) |
| PA8 | TIM1 CH1 (PWM) | Motor driver IN |
| PA9 | USART1 TX | PC (via ST-Link VCP) |
| PA10 | USART1 RX | PC |
| PB4 | GPIO input, pull-up | CORRECT button |
| PB5 | GPIO input, pull-up | WRONG button |
| PB6 | GPIO input, pull-up | ROULETTE button |
| PB10 | USART3 TX | Pod 2 Arduino RX |
| PB11 | USART3 RX | Pod 2 Arduino TX (via voltage divider) |
| PE8-PE15 | GPIO output | Onboard LEDs |

### Pin Assignments — Arduino Nano (per pod)

| Pin | Function |
|-----|----------|
| 0 (RX) | From STM32 USART TX |
| 1 (TX) | To STM32 USART RX (via voltage divider) |
| 2 | Buzz button (INPUT_PULLUP, active LOW) |
| 5 | Score LED 1 |
| 6 | Score LED 2 |
| 7 | Score LED 3 |
| 13 | Onboard LED (locked-in indicator) |

### LED Mapping on STM32 (PE8-PE15, led_num 0-7)

| LED | Meaning |
|-----|---------|
| 0 (PE8) | Pod 1 locked in |
| 1 (PE9) | Pod 2 locked in |
| 2 (PE10) | Round armed |
| 3 (PE11) | Roulette spinning |
| 7 (PE15) | Game active heartbeat |

## UART Protocol

Custom framed binary protocol used identically across all three communication links (STM32 to pods, STM32 to PC).

### Frame Format

```
[0xAA start] [src_id 1B] [msg_type 1B] [len 1B] [payload 0-4B] [checksum 1B]
```

Checksum: XOR of src_id, msg_type, len, and all payload bytes.

### Device IDs

| ID | Device |
|----|--------|
| 0x00 | Master (STM32) |
| 0x01 | Pod 1 (Arduino) |
| 0x02 | Pod 2 (Arduino) |
| 0xFF | PC (Python) |

### Message Types

| Code | Name | Direction | Payload | Description |
|------|------|-----------|---------|-------------|
| 0x01 | BUZZ | pod to master | none | Player pressed buzz button |
| 0x02 | BUZZ_ACK | master to pod | none | You won the buzz, light up |
| 0x03 | BUZZ_REJECT | master to pod | none | Too late, stay dark |
| 0x04 | SET_SCORE | master to pod | 1B score | Update score LEDs (0-3) |
| 0x05 | ARM | master to pod | none | New round, you can buzz |
| 0x06 | DISARM | master to pod | none | Round over, ignore buzzes |
| 0x07 | ROULETTE_START | master to PC | none | Roulette wheel spinning |
| 0x08 | ROULETTE_STOP | master to PC | none | Roulette wheel stopped |
| 0x10 | PLAY_BUZZ | master to PC | 1B pod_id | Play buzz sound for this pod |
| 0x11 | UPDATE_STATE | master to PC | 4B | state, p1_score, p2_score, locked_pod |

### Lockout Mechanism

When the round is ARMED, the master accepts the first BUZZ packet it receives. It sends BUZZ_ACK to the winner and BUZZ_REJECT to the loser. All subsequent BUZZ packets during LOCKED state are rejected. UART packet arrival order is the tiebreaker for simultaneous presses.

## State Machine

```
IDLE ──(host ARM button)──> ARMED
ARMED ──(BUZZ received)──> LOCKED
LOCKED ──(CORRECT button)──> score++, ──> IDLE
LOCKED ──(WRONG button)──> ARMED (other player can still buzz)
ARMED|LOCKED ──(user button)──> IDLE (cancel)
IDLE ──(ROULETTE button, 3+ questions)──> ROULETTE_SPIN
ROULETTE_SPIN ──(ROULETTE button)──> motor stops ──> IDLE
```

## Software Modules

| Module | File(s) | Description |
|--------|---------|-------------|
| Protocol | protocol.h, protocol.c | Packet parser and frame builder. Byte-at-a-time state machine with XOR checksum. Same logic shared across STM32, Arduino, and Python. |
| UART | uart.h, uart.c | Bare-metal three-channel UART driver. USART1 at 115200 for PC, USART2/3 at 9600 for pods. |
| GPIO | gpio_pins.h, gpio_pins.c | LED control (PE8-15), button reads with debounce, SysTick millisecond counter. |
| Motor | motor.h, motor.c | PWM motor control via TIM1 CH1 on PA8. 1 kHz PWM frequency, 0-999 speed range. |
| Sensors | sensors.h, sensors.c | ADC1 single-conversion read of LDR on PA1 channel 2. 12-bit resolution. |
| Main | main.c | Game state machine with four states. Polls UART and buttons in main loop, dispatches to state handlers. |
| Pod Firmware | pod_firmware.ino | Arduino sketch for contestant pods. Handles button input, score LED display, and protocol parsing. |
| Scoreboard | scoreboard.py | Python PC application using Tkinter for display and pygame for sound generation. Receives and parses protocol packets over serial. |

## Design Decisions

### Byte-at-a-time parser
The protocol parser is implemented as a state machine that processes one byte at a time. This design works identically regardless of how bytes are delivered (interrupt-driven, polled, or buffered), making the same parser logic portable across STM32 (C), Arduino (C++), and PC (Python).

### Per-channel parser context
Each UART channel maintains its own independent parser struct. There is no shared state between channels, eliminating any possibility of cross-talk between the pod and PC data streams.

### Fixed maximum payload
The 4-byte payload cap means all frame buffers are statically sized. There is no dynamic memory allocation anywhere in the embedded code, which improves reliability and predictability.

### XOR checksum
The checksum is accumulated byte-by-byte during parsing rather than computed as a separate pass over the complete frame. This minimizes memory usage and catches corruption from electrical noise on the UART lines.

## Build Instructions

### STM32 Master Console (STM32CubeIDE)

1. Open the project in STM32CubeIDE
2. Ensure CMSIS headers are in the Drivers/ folder (see below)
3. Project settings: add `../Drivers` to include paths, add `STM32F303xC` to preprocessor defines
4. Build: Project > Build All
5. Flash: Run > Run As > STM32 C/C++ Application

CMSIS headers required (download from GitHub if not present):
- https://github.com/STMicroelectronics/cmsis-device-f3 (Include/ folder contents)
- https://github.com/STMicroelectronics/cmsis_core (CMSIS/Core/Include/ folder contents)

### Arduino Pod Firmware (Arduino IDE)

1. Open pod_firmware.ino in Arduino IDE
2. Set MY_POD_ID to 0x01 for Pod 1 or 0x02 for Pod 2
3. Select board: Arduino Nano, Processor: ATmega328P (Old Bootloader if needed)
4. Upload

### Python Scoreboard

```
pip install pyserial pygame numpy
python scoreboard.py <COM_PORT>
```

## Testing

### Test 1: STM32 LEDs and Button
Flash firmware. LED 7 (PE15) should blink every 500ms. Press blue button: LED 2 (PE10) lights solid (ARMED). Press again: back to blinking (IDLE).

### Test 2: STM32 to PC UART
Open Putty at 115200 baud on the STM32 COM port. Press blue button. Garbage characters should appear (binary protocol data).

### Test 3: Python Scoreboard
Close Putty. Run scoreboard.py with the COM port. Press blue button. Scoreboard should show state changes.

### Test 4: Arduino Pod Standalone
Upload pod firmware. Open Serial Monitor at 9600. Press buzz button (D2 to GND). Garbage characters should appear.

### Test 5: Full System
Wire Arduino to STM32 UART. Run scoreboard. ARM round. Press buzz button on pod. Scoreboard should show buzz-in and play sound.

## Clock Configuration

The firmware runs on the default 8 MHz HSI oscillator (no PLL configured). All baud rates, SysTick, and PWM timings are calculated from 8 MHz. This is sufficient for all project functionality. To upgrade to 72 MHz, update the BRR divisors in uart.c, the SysTick config in gpio_pins.c, and the TIM1 prescaler in motor.c.

## Wiring Notes

Arduino Nanos operate at 5V logic. The STM32F3 Discovery uses 3.3V. A voltage divider is required on each Arduino TX to STM32 RX line to prevent damage:

```
Arduino TX ── 1k ohm ──┬── 2.2k ohm ── GND
                        |
                     STM32 RX
```

STM32 TX to Arduino RX requires no divider (Arduino reads 3.3V as logic HIGH).

All boards must share a common ground connection.

## Work Log

See WORKLOG.md for dated entries of work performed.
