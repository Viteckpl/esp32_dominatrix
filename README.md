# esp32_dominatrix

A simple, solderless ESP32 project built as a standalone device that reacts after a random amount of time, requires physical presence inside a defined zone, and expects confirmation through a button press. The current version is intentionally designed as a practical learning prototype on a breadboard, focused on testing, iteration, and future expansion rather than on enclosure design or mobile power.

## Project goals

The device works as a small state-driven control box with the following states: `IDLE`, `CALLING`, `PRESENCE`, `PUNISH`, and `SLEEP_MODE`. In `IDLE`, it waits for a random amount of time; then it calls the user with an audible signal, expects a button press within a defined timeout, and finally requires the user to stay within a distance threshold measured by an HC-SR04 sensor. Failure to react in time or leaving the monitored zone triggers punishment mode, which blocks all other actions until the punishment is acknowledged. 

## Hardware used

The current working prototype uses only the components that are already integrated into the project.

| Component | Role in the project | Notes |
|---|---|---|
| ESP32 ESP-32S / ESP32 Dev Module | Main microcontroller | Programmed from Arduino IDE as `ESP32 Dev Module`. |
| HC-SR04 | Presence detection through distance measurement | In this prototype version it is powered from 3.3V for simpler and safer GPIO handling. |
| TM1637 4-digit 7-segment display | Countdown display | Controlled with the `TM1637Display` library. |
| Arcade push button | Acknowledge reaction, acknowledge punishment, enter/exit sleep mode | Wired using `INPUT_PULLUP` logic. |
| Red LED | Punishment indication and sleep mode indication | Requires a series resistor. |
| Passive buzzer | Call signal and short confirmation signals | Requires a driven signal from GPIO. |
| 830-point breadboard | Solderless assembly | All connections are made with Dupont wires. |
| 150Ω resistor | LED current limiting | Used in series with the red LED. |
| Dupont jumper wires | Electrical connections | No soldering required. |
| Micro USB cable | Power and programming | Powers the full prototype from USB. |

## Pin mapping

The project was intentionally laid out so that it can use pins from only one side of the ESP32 board. The final mapping is: 

| Signal | ESP32 pin | Notes |
|---|---|---|
| HC-SR04 Trig | D4 | Output trigger pulse.  |
| HC-SR04 Echo | D5 | Distance input signal.  |
| TM1637 CLK | D18 | Display clock line.  |
| TM1637 DIO | D19 | Display data line.  |
| Button | D21 | `INPUT_PULLUP`, active when shorted to GND.  |
| Red LED | D22 | Digital output through a series resistor.  |
| Passive buzzer | D23 | Digital output used for tones.  |
| Module power | 3V3, GND | Shared power rails for the full prototype.  |

## Exact breadboard wiring

### 1. ESP32 placement

Insert the ESP32 across the central gap of the breadboard so that the module body bridges the split between both halves. Keep the USB connector facing outward for easy power and programming access.

### 2. Power rails

- Connect `3V3` from the ESP32 to the positive power rail of the breadboard.
- Connect `GND` from the ESP32 to the negative power rail of the breadboard.
- All modules share the same 3.3V and GND rails. 

### 3. Red LED

- LED anode (longer leg) -> 150Ω or 220Ω resistor -> `D22` on the ESP32.
- LED cathode (shorter leg) -> `GND` rail.
- The resistor can be placed on either side of the LED, as long as it remains in series.

### 4. Arcade button

The final version uses the ESP32 internal pull-up resistor, so no external pull-up or pull-down resistor is needed. The button only has to short the input pin to ground when pressed.

- One side of the button -> `D21` on the ESP32.
- The other side of the button -> `GND` rail.
- Unpressed state reads as logical `1`; pressed state reads as logical `0`.

If the button has 4 pins, it should be placed so that it crosses the breadboard center gap. Typically, the two pins on one side are internally connected, and the two pins on the other side are also internally connected; pressing the button bridges both sides.

### 5. Passive buzzer

- `+` pin of the buzzer -> `D23` on the ESP32.
- `-` pin of the buzzer -> `GND` rail.
- Because this is a passive buzzer, it is driven by generated tones from the microcontroller rather than by constant DC power alone.

### 6. TM1637 display

Check the silkscreen labels on the actual module, because the order of pins can vary between manufacturers. The logical wiring, however, stays the same.

- `VCC` -> `3V3`
- `GND` -> `GND`
- `CLK` -> `D18`
- `DIO` -> `D19`

### 7. HC-SR04 sensor

In standard usage, the HC-SR04 is usually powered from 5V, and its Echo line can output 5V, which requires extra care when used with ESP32 GPIO. In this prototype, a simpler test-oriented approach is used: the sensor is powered from 3.3V so the setup remains easier and safer during early learning and breadboard experimentation.

- `VCC` -> `3V3`
- `GND` -> `GND`
- `Trig` -> `D4`
- `Echo` -> `D5`

This approach is convenient for a learning prototype, but it can reduce range and stability, which is why software filtering is used in the sketch.

## Current configuration

The project currently uses the following configuration block:

```cpp
// ===== CONFIG =====
const unsigned long WAIT_MIN_MS = 600000;       // 10 min
const unsigned long WAIT_MAX_MS = 10800000;     // 3 h
const unsigned long CALL_TIMEOUT_MS = 30000;    // 30 s to press the button after the call
const unsigned long PRESENCE_MIN_MS = 10000;    // 10 s
const unsigned long PRESENCE_MAX_MS = 600000;   // 10 min
const unsigned long SLEEP_HOLD_MS = 2000;       // 2 s button hold
const unsigned long PUNISH_BLINK_MS = 400;      // punishment LED blink tempo
const unsigned long CALL_BEEP_INTERVAL_MS = 2000;
const unsigned long CALL_BEEP_DURATION_MS = 200;
const int CALL_BEEP_FREQ = 2000;
const unsigned long SUCCESS_BEEP_DURATION_MS = 200;
const int SUCCESS_BEEP_FREQ = 2500;
const int DISTANCE_THRESHOLD_CM = 60;
const int MAX_MISSES = 5;
const int SLEEP_ON_BLINKS = 3;
const int SLEEP_OFF_BLINKS = 5;
const unsigned long SLEEP_SIGNAL_ON_MS = 180;
const unsigned long SLEEP_SIGNAL_OFF_MS = 180;
// ==================
```

This means the device waits for a random duration between 10 minutes and 3 hours, gives 30 seconds to react after the call begins, and then requires presence in front of the sensor for a random duration between 10 seconds and 10 minutes. Presence detection uses a 60 cm threshold and tolerates up to 5 consecutive failed readings to compensate for the imperfect stability of the HC-SR04 in this simplified 3.3V setup. 

## State machine

### IDLE

The device stays quiet and shows nothing on the TM1637 display while waiting for the next randomly scheduled call. The timer still runs internally, but it is intentionally hidden from the user. 

### CALLING

After the random waiting period expires, the buzzer starts calling the user at regular intervals. From that moment, the user has 30 seconds to reach the device and press the button once. 

### PRESENCE

After a valid button press, the display starts showing the required presence countdown. The HC-SR04 continuously measures distance, and isolated invalid readings such as `-1` are not immediately treated as failure if the last valid reading still confirmed presence within the accepted range. 

### PUNISH

If the button is not pressed in time or if the user leaves the accepted zone during the presence countdown, the device enters punishment mode. In this state, the red LED blinks continuously and the device stays blocked until punishment is acknowledged with a single button press. 

### SLEEP_MODE

Holding the button for the configured duration enters sleep mode. Entering sleep mode is signaled by 3 red LED blinks, exiting sleep mode is signaled by 5 red LED blinks, and while asleep the device does not call the user or continue its normal cycle. 

## Upload and run

1. Open the project in Arduino IDE.
2. Select `ESP32 Dev Module` as the board.
3. Make sure the `TM1637Display` library is installed.
4. Upload the sketch to the ESP32.
5. Open the Serial Monitor at 115200 baud if diagnostic logs are needed. 

## Known limitations

The main limitation of the current prototype is the use of the HC-SR04 as a simple presence detector. The HC-SR04 is fundamentally a distance sensor, not a dedicated occupancy sensor, so it requires software filtering and does not provide the same confidence as a sensor specifically designed for reliable presence detection.

In addition, running the HC-SR04 from 3.3V is a conscious prototype compromise: it simplifies early-stage learning and breadboard safety, but it can reduce measurement stability. Future versions may benefit from improved power handling or from replacing the sensor with one that is better suited to presence sensing.

## Suggested repository structure

A minimal repository structure for the current version is:

```text
.
├── README.md
└── esp32_dominatrix.ino
```

As the project grows, it will make sense to add folders for images, wiring diagrams, and archived sketch versions.
