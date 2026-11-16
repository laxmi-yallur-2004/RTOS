# RTOS on STM32F4 (Arduino Framework)

A simple cooperative, priority-based RTOS built from scratch on an STM32F4 board. It demonstrates task scheduling, a message queue, a mutex, and live status output on a 128x64 graphic LCD (ST7920, serial mode) and the serial monitor.

## Features

- **Priority-based scheduler** – picks the highest-priority ready task each loop (periodic or always-ready).
- **Message queue** – 32-slot circular buffer (interrupt-safe) passing sample IDs between tasks.
- **Mutex** – protects the serial port so tasks don't print over each other.
- **Stress test** – a CPU-heavy task whose load can be switched at runtime.
- **GLCD display** – bit-banged serial driver showing live system status.

## Hardware

- **Board:** STM32F401CCU6 Black Pill
- **Display:** GLCD 12864B (ST7920 controller) in **serial mode**
- **Status LED:** PA5
- **Serial (Serial1):** 9600 baud

### GLCD Wiring

Only the wires below are used, nothing else:

| GLCD 12864B Pin | STM32 Black Pill |
|-----------------|------------------|
| GND | GND |
| VCC | 5V |
| RS / CS | PB12 |
| RW / SID | PB15 |
| E / SCLK | PB13 |
| PSB | GND (selects serial mode) |
| RST | 5V |
| BLA (backlight +) | 5V |
| BLK (backlight -) | GND |

## Tasks

| Task | Priority | Period | Description |
|------|----------|--------|-------------|
| SamplingTask | 4 | 500 ms | Toggles LED, increments sample ID, pushes it to the queue |
| CommsTask | 3 | 10 ms | Reads queue, prints `COMMS: ID=n OK`, handles serial commands |
| LoggingTask | 2 | 1000 ms | Prints ID, missed count, queue level and PASS/FAIL |
| DisplayTask | 2 | 1000 ms | Updates the GLCD |
| StressTask | 1 | always | Busy loop to load the CPU |

## Serial Commands

Send via the Serial Monitor (9600 baud):

- `H` / `h` – High stress (loop count 30000)
- `L` / `l` – Low stress (loop count 3000, default)

## Sample Output

Serial monitor:

```
COMMS: ID=46 OK
COMMS: ID=47 OK
LOG: ID=47 Missed=0 QueueLeft=0 RESULT=PASS
COMMS: ID=48 OK
COMMS: ID=49 OK
LOG: ID=49 Missed=0 QueueLeft=0 RESULT=PASS
```

GLCD:

```
ID:      47
Missed:  0
RESULT:  PASS
Queue:   0
```

**Meaning:** every sample produced was received (Missed = 0, Queue empty), so the real-time test passes. `RESULT=FAIL` appears if the queue ever overflows and a sample is missed.

## How to Run

1. Open the project in PlatformIO / Arduino IDE with STM32 support (Generic STM32F4 series).
2. Wire the LED and GLCD as per the pin table.
3. Upload the code.
4. Open the Serial Monitor at 9600 baud.
5. Watch the GLCD and serial logs; send `H` to stress the system and `L` to relax it.

## Notes

- The scheduler is cooperative (no preemption): a task runs to completion before the next is chosen.
- Priorities are 1–4; a higher number means higher priority.
- `MAX_TASKS` is 5; increase it to add more tasks.
