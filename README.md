# Arduino Synthesizer

> A pocket monophonic synthesizer built with an **Arduino Uno**, a **4x4 button matrix** and a **passive buzzer**, using **PlatformIO**.

This project turns a 4x4 matrix keypad into a small playable keyboard. It has three operating modes (piano, potentiometer and mix), plays real note frequencies through a buzzer, and includes octave selection and pitch-bend via a potentiometer. It was made for learning and for fun with electronics and embedded programming.

---

## Circuit diagram

<img width="994" height="779" alt="image" src="https://github.com/user-attachments/assets/eea5a352-d20e-4640-b24c-99ef5232b9a7" />

![Circuit diagram](https://github.com/user-attachments/files/33412738/Shiny.Amberis-Curcan.pdf)
---

## About project

This is a simple **monophonic synthesizer** developed with **Arduino Uno** and **PlatformIO**. The system uses a 4x4 button matrix as the keyboard, a passive buzzer as the sound output, a potentiometer for frequency/pitch control and a push button to switch between modes.

The whole instrument is played by pressing the matrix keys: each of the 12 keys maps to a musical note (C4 .. B4), while the last row of the matrix selects the octave. A potentiometer can either sweep the pitch freely or bend the current note by semitones.

## Overview

The synthesizer starts in **PIANO (KEYBOARD)** mode and plays a short "Do Re Mi Fa Sol La Si Do" jingle on power-up. A push button on **D11** cycles through the three modes on every press. The selected key/note is turned into a frequency that drives the buzzer, and the current state is printed to the Serial Monitor for debugging.

## Features

- 3 operating modes selectable with a single button (PIANO, POTENTIOMETER, MIX)
- 12-note chromatic keyboard mapped on a 4x4 matrix (C4 .. B4)
- 4 octave selector buttons (x1, x2, x4, x8) using the last matrix row
- Direct frequency control through a potentiometer (110 Hz .. 880 Hz)
- Pitch-bend by semitones using equal temperament (2^(1/12))
- Passive buzzer output with startup jingle
- Software debounce for the keypad and the mode button
- Smoothed (averaged) potentiometer readings to remove analog noise
- Live debug output on the Serial Monitor (115200 baud)
- Ready-to-use PlatformIO project structure

## Hardware used

- Arduino Uno
- 4x4 matrix keypad
- Passive buzzer
- 10k potentiometer
- Momentary push button (mode switch)
- Resistors, wires and protoboard

## Pin mapping

### 4x4 matrix

| Function  | Arduino pin |
|-----------|-------------|
| Column C1 | D2          |
| Column C2 | D3          |
| Column C3 | D4          |
| Column C4 | D8          |
| Row R1    | D5          |
| Row R2    | D6          |
| Row R3    | D7          |
| Row R4    | D9          |

### Other components

| Function             | Arduino pin |
|----------------------|-------------|
| Buzzer               | D12         |
| Mode button          | D11         |
| Potentiometer (wiper)| A0          |

## Note map

The matrix index is computed as `rowIndex * 4 + columnIndex` in PHYSICAL order:

```
        C1(D2)  C2(D3)  C3(D4)  C4(D8)
R1(D5)   D4      C#4     C4      D#4     (indices 0..3)
R2(D6)   F#4     F4      E4      G4      (indices 4..7)
R3(D7)   A#4     A4      G#4     B4      (indices 8..11)
R4(D9)   oct1    oct2    oct3    oct4    (indices 12..15 = octaves)
```

- Indices **0..11**: the 12 notes in the physical order of the keys.
- Indices **12..15**: the 4 octave buttons (only in PIANO/KEYBOARD mode).

> The last row is physically inverted on the keypad, so the firmware maps the
> physical index to a logical button number with the `physicalToLogical[]` table.

## Modes

| Value | Name        | Description                                                        |
|-------|-------------|--------------------------------------------------------------------|
| 0     | PIANO       | Matrix keys play notes; last row selects the octave.               |
| 1     | POTENTIOMETER | The pot (A0) directly controls the buzzer frequency (110..880 Hz).|
| 2     | MIX         | Matrix keys play a note and the pot bends the pitch by semitones.  |

The mode button on **D11** cycles the modes on every press, in this order:

```
PIANO -> POTENTIOMETER -> MIX -> PIANO ...
```

## Controls

- **D11 button**: cycles through the 3 modes.
- **Matrix keys (indices 0..11)**: play the notes C4 .. B4.
- **Matrix keys (indices 12..15)**: select the octave (PIANO mode only).
- **Potentiometer (A0)**: controls frequency (POT mode) or pitch bend (MIX mode).

## Project structure

```
sintetizador/
├── docs/
│   └── ARCHITECTURE.md     # How the firmware works (deeper explanation)
├── src/
│   └── main.cpp            # Main source code
├── platformio.ini          # PlatformIO configuration
├── .gitignore              # Ignored files (build output, editor config)
└── README.md               # Project documentation
```

## Dependencies

This project only depends on the **Arduino framework** that PlatformIO installs
automatically for the `uno` board. There are no external libraries.

The configuration is already set in `platformio.ini`.

## Build instructions

With [PlatformIO](https://platformio.org/) installed, run:

```bash
# Compile the firmware
pio run

# Upload to the Arduino
pio run -t upload

# Open the Serial Monitor (115200 baud)
pio device monitor
```

## How to use

1. Wire the components according to the pin mapping.
2. Build and upload the firmware to the Arduino Uno.
3. Open the Serial Monitor at **115200** baud to see the live debug output.
4. The buzzer plays a startup jingle and the device starts in **PIANO** mode.
5. Press the matrix keys to play notes and use the last row to change octave.
6. Press **D11** to switch between PIANO, POTENTIOMETER and MIX.

## Serial debug output

Every ~250 ms the firmware prints the current state, for example:

```
mode=KEYBOARD | btn=1 | pot=512 | key=6 | oct=0 | freq=349
```

## Notes

- The firmware is designed for an **Arduino Uno**; all pins are defined at the top
  of `src/main.cpp` and can be changed freely.
- The keypad and mode button use **software debounce** (~25 ms).
- The potentiometer is sampled 20 times and averaged (~20 ms) to cancel mains
  hum (50/60 Hz) and analog noise.
- Pitch bend uses the equal-temperament ratio `2^(1/12)` per semitone, pre-computed
  into a lookup table at startup.
- The buzzer only re-triggers when the frequency actually changes, which avoids
  audible clicks and wasted CPU cycles.

## Possible improvements

- Add an audio amplifier / low-pass filter for a cleaner sound
- Add volume control (PWM envelope / ADSR)
- Add an OLED/LCD to display the current mode, note and octave
- Support polyphony (multiple notes at once)
- Add recording / looping of short sequences
- Make the note map and modes configurable at runtime via Serial

## License

This project was created for educational and learning purposes in electronics
and Arduino development.

---

## Videos


- Demo video 1 — 

https://youtube.com/shorts/qpnLH1EZp3U?si=4ItmB6pUPz2T5x15

- Demo video 2 —

https://github.com/user-attachments/assets/dae4a3d9-f57d-4694-9a06-74e83407adac

