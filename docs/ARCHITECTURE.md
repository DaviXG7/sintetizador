# Firmware Architecture

This document explains **how the synthesizer firmware works** internally. It is a
companion to the `README.md` and focuses on the code in `src/main.cpp`.

It is written for readers who already know the basics of Arduino and want to
understand the scanning, debouncing, frequency math and mode logic.

---

## 1. High-level flow

The firmware follows the classic Arduino `setup()` / `loop()` structure:

```
setup()
  ├── Start Serial (115200 baud)
  ├── Pre-compute the pitch-bend lookup table
  ├── Configure the pins (buzzer, mode button, matrix)
  └── Play the startup jingle (Do Re Mi Fa Sol La Si Do)

loop()   (runs forever)
  ├── Read + debounce the mode button -> may cycle the mode
  ├── Depending on the mode, compute a frequency:
  │     ├── POTENTIOMETER : map(pot, 0..1023 -> 110..880 Hz)
  │     ├── PIANO         : matrix note, shifted by the selected octave
  │     └── MIX           : matrix note, bent by the pot (semitones)
  ├── Drive the buzzer only if the frequency changed (setTone)
  └── Print the debug state to Serial every ~250 ms
```

---

## 2. Pin configuration

| Group     | Direction       | Notes                                        |
|-----------|-----------------|----------------------------------------------|
| Columns   | `INPUT_PULLUP`  | Idle HIGH; a pressed key pulls them LOW.     |
| Rows      | `OUTPUT`        | Idle HIGH; driven LOW one at a time to scan. |
| Buzzer    | `OUTPUT`        | Driven with `tone()` / `noTone()`.           |
| Mode btn  | `INPUT_PULLUP`  | Idle HIGH; pressed = LOW.                    |
| Potentiom.| `ANALOG INPUT`  | `A0`, 10-bit result 0..1023.                 |

---

## 3. Matrix scanning (`readMatrixKey`)

The 4x4 matrix is read by **row-by-row scanning**:

1. Drive one row LOW and keep the others HIGH.
2. Read every column. Because the columns use `INPUT_PULLUP`, a column reads
   LOW only when the key at (this row, that column) closes the circuit.
3. If a LOW column is found, the code re-reads the same column after a short
   delay as a **noise confirmation** before accepting the key.
4. The row is returned to HIGH and the next row is scanned.

The returned value is the **physical index**: `rowIndex * 4 + columnIndex`.
If no key is pressed, the function returns `-1`.

---

## 4. Physical vs logical index

The last row of the keypad is physically reversed, so a raw physical index does
not match the printed button number. The `physicalToLogical[]` table fixes that:

```
[1] [2] [3] [4]      R1
[5] [6] [7] [8]      R2
[9] [10][11][12]     R3
[15][14][13][16]     R4  <- inverted
```

All the note/octave logic works on the **logical** index, obtained with:

```cpp
int logicalKey = (stableKey >= 0) ? physicalToLogical[stableKey] : -1;
```

---

## 5. Debouncing

Both the keypad and the mode button are de-bounced in software (no external RC
filter needed). The idea is a simple state machine:

```
raw value changes ──► remember the time (…ChangedAt)
                         │
        after 25 ms with no new change ──► accept it as the "stable" value
```

- `rawKey` / `rawKey` timestamps track the unstable reading.
- `stableKey` is the value the instrument reacts to.
- The mode button uses the same pattern and **advances the mode on the falling
  edge** (HIGH -> LOW), i.e. once per press.

This prevents a single press from registering as multiple notes/mode changes due
to mechanical contact bounce.

---

## 6. Frequencies and octaves

Notes are stored as frequencies in Hz in the physical order of the keys
(`noteFrequencies[]`, indices 0..11):

```
D4  C#4 C4  D#4 F#4 F4  E4  G4  A#4 A4  G#4 B4
294 277 262 311 370 349 330 392 466 440 415 494
```

An octave doubles the frequency, so shifting by octave is a cheap **bit shift**:

```cpp
base = noteFrequencies[noteIndex] << shift;  // shift = 0,1,2,3  => x1,x2,x4,x8
```

The octave buttons set `currentOctave` (0..3) which selects the shift from
`octaveShifts[]`.

---

## 7. Pitch bend (MIX mode)

In MIX mode the potentiometer bends the note instead of changing octave. The
pitch shift runs from 0 to `PITCH_MAX_SEMITONES` (24) semitones, and each
semitone multiplies the frequency by the equal-temperament ratio `2^(1/12)`.

At startup the lookup table is pre-computed once:

```cpp
pitchBendTable[i] = pow(2.0, i / 12.0);   // i = 0..24
```

Then the final frequency is just a multiply and a rounding:

```cpp
frequency = (uint16_t)(base * pitchBendTable[pitchShift] + 0.5);
```

Pre-computing avoids calling `pow()` (slow, and uses floating point) inside the
main loop.

---

## 8. Buzzer output (`setTone`)

The buzzer is only re-triggered when the frequency actually changes:

- `frequency == 0`      -> `noTone()` and silence.
- `frequency != last`   -> `tone(pin, frequency)`.
- `frequency == last`   -> do nothing (keeps the tone playing).

This avoids audible clicks and lets the tone sound continuously while a key is
held.

---

## 9. Analog smoothing (`readSmoothedPotentiometer`)

Raw `analogRead()` values from `A0` are noisy. The firmware averages 20 samples
with ~1 ms between them (~20 ms total). That window is about one cycle of the
mains supply (50/60 Hz), which helps cancel electrical interference picked up by
the wiring.

---

## 10. State variables

| Variable              | Purpose                                              |
|-----------------------|------------------------------------------------------|
| `currentMode`         | Active mode (0 PIANO, 1 POT, 2 MIX).                 |
| `currentOctave`       | Octave shift 0..3 for PIANO mode.                    |
| `rawKey`/`stableKey`  | Debounced matrix key (physical domain).              |
| `previousStableKey`   | Used to detect the octave-button press edge.         |
| `lastToneFrequency`   | Last frequency sent to the buzzer.                   |
| `*ChangedAt`          | Timestamps for the debounce logic.                   |

---

## 11. Extending the firmware

- **Change the sound engine**: replace `tone()` with a DAC + envelope for ADSR.
- **Add a display**: print `currentMode`, `logicalKey` and `frequency` to an LCD.
- **Add polyphony**: scan the whole matrix into a list of active keys instead of
  returning a single key.
- **Make it configurable**: parse Serial commands to change modes, octave base,
  or the note map at runtime.
