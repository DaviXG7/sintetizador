#include <Arduino.h>

// ============================================================================
//  ARDUINO SYNTHESIZER (PlatformIO) - 4x4 button matrix - 3 MODES
// ----------------------------------------------------------------------------
//  Operating modes (the button on D11 CYCLES through them on every press):
//    Order: PIANO -> POTENTIOMETER -> MIX -> PIANO ...
//    0 = PIANO/KEYBOARD : the 4x4 matrix plays notes; buttons 13..16 = octaves
//    1 = POTENTIOMETER  : the pot (A0) directly controls the beep frequency
//    2 = MIX            : matrix plays the note + pot does "pitch bend" by
//                        semitones (the pot does NOT change octaves, it raises
//                        the note one half-tone at a time; e.g. the C key
//                        becomes C#, D, ...)
//
//  4x4 matrix wiring:
//    Columns: C1 -> D2 | C2 -> D3 | C3 -> D4 | C4 -> D8
//    Rows   : R1 -> D5 | R2 -> D6 | R3 -> D7 | R4 -> D9
//
//  Matrix index map (rowIndex * 4 + columnIndex) - PHYSICAL order:
//
//         C1(D2)  C2(D3)  C3(D4)  C4(D8)
//  R1(D5)  D4      C#4     C4      D#4      (indices 0..3)
//  R2(D6)  F#4     F4      E4      G4       (indices 4..7)
//  R3(D7)  A#4     A4      G#4     B4       (indices 8..11)
//  R4(D9)  oct1    oct2    oct3    oct4     (indices 12..15 = octaves)
//
//    - Indices 0..11 : 12 notes in the physical order of the buttons
//    - Indices 12..15: 4 octave buttons (KEYBOARD and MIX modes)
// ============================================================================

constexpr uint8_t columnPins[] = {2, 3, 4, 8};
constexpr uint8_t rowPins[] = {5, 6, 7, 9};
constexpr uint8_t NUM_COLS = 4;
constexpr uint8_t NUM_ROWS = 4;

constexpr uint8_t buzzerPin = 12;
constexpr uint8_t modeButtonPin = 11;
constexpr uint8_t potentiometerPin = A0;
constexpr unsigned long debounceMs = 25;

// Frequencies in the PHYSICAL order of the matrix buttons (indices 0..11),
// according to the real keyboard layout:
//   D4   C#4  C4   D#4  F#4  F4   E4   G4   A#4  A4   G#4  B4
constexpr uint16_t noteFrequencies[] = {
    294, 277, 262, 311, 370, 349,
    330, 392, 466, 440, 415, 494
};
constexpr uint8_t NUM_NOTES = sizeof(noteFrequencies) / sizeof(noteFrequencies[0]);

// MIX mode table: all 16 buttons are notes (0..15), continuing the chromatic
// sequence after the B (index 11). This mode has NO octave buttons.
//   idx 12=C5  13=C#5  14=D5  15=D#5  (the pot does the "pitch bend" by semitones)
constexpr uint16_t mixFrequencies[16] = {
    294, 277, 262, 311, 370, 349, 330, 392,   // 0..7  (D4 C#4 C4 D#4 F#4 F4 E4 G4)
    466, 440, 415, 494, 523, 554, 587, 622    // 8..15 (A#4 A4 G#4 B4 C5 C#5 D5 D#5)
};
constexpr uint8_t NUM_MIX_NOTES = 16;

// Maps the PHYSICAL index of the matrix (how it was wired) to the LOGICAL button
// number (index 0 = button 1). The last row is physically inverted:
//   [1] [2] [3] [4]      R1
//   [5] [6] [7] [8]      R2
//   [9] [10][11][12]     R3
//   [15][14][13][16]     R4  <- inverted
// Physical index 12->button15, 13->14, 14->13, 15->16.
constexpr uint8_t physicalToLogical[16] = {
    0, 1, 2, 3, 4, 5, 6, 7,
    8, 9, 10, 11, 14, 13, 12, 15
};

// 4 octave buttons: indices 12, 13, 14 and 15 of the matrix (buttons 13..16).
// Shift as a power of 2: x1, x2, x4, x8 ("full" octaves, audible range).
constexpr uint8_t firstOctaveKey = 12;
constexpr uint8_t NUM_OCTAVES = 4;
constexpr int8_t octaveShifts[NUM_OCTAVES] = {0, 1, 2, 3};

// ----- Modes (the D11 button cycles through them in this order) -----
// Cycle order: PIANO -> POTENTIOMETER -> MIX -> PIANO ...
constexpr uint8_t MODE_KEYBOARD = 0;
constexpr uint8_t MODE_POT = 1;
constexpr uint8_t MODE_MIX = 2;
constexpr uint8_t NUM_MODES = 3;

// MIX mode pitch bend: the pot adds from 0 to PITCH_MAX_SEMITONES half-tones
// to the chosen note (each semitone = frequency x 2^(1/12)).
constexpr uint8_t PITCH_MAX_SEMITONES = 24;
float pitchBendTable[PITCH_MAX_SEMITONES + 1];

int currentMode = MODE_KEYBOARD;  // starts in PIANO (KEYBOARD) mode
int currentOctave = 0;  // 0..3, set by the octave buttons (KEYBOARD/MIX modes)
bool modeButtonRaw = HIGH;
bool modeButtonStable = HIGH;
int rawKey = -1;
int stableKey = -1;
int previousStableKey = -1;
unsigned long modeButtonChangedAt = 0;
unsigned long keyChangedAt = 0;
uint16_t lastToneFrequency = 0;
unsigned long lastDebugAt = 0;

int readMatrixKey() {
    for (uint8_t rowIndex = 0; rowIndex < NUM_ROWS; ++rowIndex) {
        digitalWrite(rowPins[rowIndex], LOW);
        delayMicroseconds(10);  // let the row settle before reading

        int found = -1;
        for (uint8_t columnIndex = 0; columnIndex < NUM_COLS; ++columnIndex) {
            if (digitalRead(columnPins[columnIndex]) == LOW) {
                found = rowIndex * NUM_COLS + columnIndex;
                break;
            }
        }

        if (found >= 0) {
            // Confirmation: read the SAME column again with the row still LOW,
            // to discard false readings caused by noise.
            delayMicroseconds(20);
            uint8_t col = found % NUM_COLS;
            if (digitalRead(columnPins[col]) == LOW) {
                digitalWrite(rowPins[rowIndex], HIGH);
                return found;
            }
        }

        digitalWrite(rowPins[rowIndex], HIGH);
    }

    return -1;
}

void setTone(uint16_t frequency) {
    if (frequency == 0) {
        if (lastToneFrequency != 0) {
            noTone(buzzerPin);
            lastToneFrequency = 0;
        }
        return;
    }

    if (frequency != lastToneFrequency) {
        tone(buzzerPin, frequency);
        lastToneFrequency = frequency;
    }
}

int readSmoothedPotentiometer() {
    // Averages several readings to remove the noise ("hiss") from A0.
    // 20 samples x ~1ms = ~20ms ~= 1 mains cycle (50/60Hz),
    // which helps cancel the interference from the power outlet.
    long sum = 0;
    const uint8_t samples = 20;
    for (uint8_t i = 0; i < samples; ++i) {
        sum += analogRead(potentiometerPin);
        delay(1);
    }
    return (int)(sum / samples);
}

void playStartupJingle() {
    // Startup jingle: Do Re Mi Fa Sol La Si Do (major scale).
    static const uint16_t scale[] = {262, 294, 330, 349, 392, 440, 494, 523};
    static const uint16_t durations[] = {150, 150, 150, 150, 150, 150, 150, 300};

    for (uint8_t i = 0; i < 8; ++i) {
        tone(buzzerPin, scale[i]);
        delay(durations[i]);
    }
    noTone(buzzerPin);
}

void setup() {
    Serial.begin(115200);
    // Pre-computes the pitch bend table: each semitone = frequency x 2^(1/12).
    for (uint8_t i = 0; i <= PITCH_MAX_SEMITONES; ++i) {
        pitchBendTable[i] = pow(2.0, (double)i / 12.0);
    }

    Serial.println(F("Synthesizer started (4x4 matrix). Mode: PIANO (KEYBOARD)"));
    pinMode(buzzerPin, OUTPUT);
    pinMode(modeButtonPin, INPUT_PULLUP);

    for (uint8_t columnIndex = 0; columnIndex < NUM_COLS; ++columnIndex) {
        pinMode(columnPins[columnIndex], INPUT_PULLUP);
    }

    for (uint8_t rowIndex = 0; rowIndex < NUM_ROWS; ++rowIndex) {
        pinMode(rowPins[rowIndex], OUTPUT);
        digitalWrite(rowPins[rowIndex], HIGH);
    }

    // Plays the "Do Re Mi Fa Sol La Si Do" jingle to signal it has started.
    playStartupJingle();
}

void loop() {
    unsigned long now = millis();

    // ----- Mode button: CYCLES through the 3 modes on every press -----
    bool currentModeButton = digitalRead(modeButtonPin);
    if (currentModeButton != modeButtonRaw) {
        modeButtonRaw = currentModeButton;
        modeButtonChangedAt = now;
    }

    if (now - modeButtonChangedAt >= debounceMs &&
        currentModeButton != modeButtonStable) {
        modeButtonStable = currentModeButton;
        if (modeButtonStable == LOW) {
            currentMode = (currentMode + 1) % NUM_MODES;
            setTone(0);
        }
    }

    uint16_t frequency = 0;

    if (currentMode == MODE_POT) {
        // POTENTIOMETER mode: the pot directly controls the beep frequency.
        int potentiometerValue = readSmoothedPotentiometer();
        frequency = map(potentiometerValue, 0, 1023, 110, 880);
    } else {
        // PIANO and MIX modes: read the button matrix.
        int currentKey = readMatrixKey();
        if (currentKey != rawKey) {
            rawKey = currentKey;
            keyChangedAt = now;
        }

        if (now - keyChangedAt >= debounceMs && currentKey != stableKey) {
            previousStableKey = stableKey;
            stableKey = currentKey;  // stays in the PHYSICAL domain for debounce
        }

        // LOGICAL index of the button (corrects the inverted last row).
        int logicalKey = (stableKey >= 0) ? physicalToLogical[stableKey] : -1;
        int previousLogicalKey =
            (previousStableKey >= 0) ? physicalToLogical[previousStableKey] : -1;

        // Only in PIANO mode: buttons 13..16 (logical 12..15) = octaves.
        // (in MIX mode these buttons are notes, they do not change octave)
        if (currentMode == MODE_KEYBOARD &&
            logicalKey >= firstOctaveKey &&
            logicalKey < firstOctaveKey + NUM_OCTAVES &&
            previousLogicalKey < firstOctaveKey) {
            currentOctave = logicalKey - firstOctaveKey;
        }

        if (currentMode == MODE_MIX) {
            // MIX mode: all 16 buttons are notes (0..15) and the pot pitch bends.
            if (logicalKey >= 0 && logicalKey < NUM_MIX_NOTES) {
                uint16_t base = mixFrequencies[logicalKey];
                int potValue = readSmoothedPotentiometer();
                int pitchShift = map(potValue, 0, 1023, 0, PITCH_MAX_SEMITONES);
                frequency = (uint16_t)(base * pitchBendTable[pitchShift] + 0.5);
            }
        } else {
            // PIANO mode: buttons 0..11 are notes; 12..15 are the octaves.
            if (logicalKey >= 0 && logicalKey < firstOctaveKey) {
                uint8_t noteIndex = logicalKey % NUM_NOTES;
                int shift = octaveShifts[currentOctave];
                uint16_t base;
                if (shift >= 0) {
                    base = noteFrequencies[noteIndex] << shift;
                } else {
                    base = noteFrequencies[noteIndex] >> (-shift);
                }
                frequency = base;
            }
        }
    }

    setTone(frequency);

    if (now - lastDebugAt >= 250) {
        lastDebugAt = now;
        Serial.print(F("mode="));
        if (currentMode == MODE_POT) Serial.print(F("POT"));
        else if (currentMode == MODE_KEYBOARD) Serial.print(F("KEYBOARD"));
        else Serial.print(F("MIX"));
        Serial.print(F(" | btn="));
        Serial.print(modeButtonStable);
        Serial.print(F(" | pot="));
        Serial.print(analogRead(potentiometerPin));
        Serial.print(F(" | key="));
        Serial.print(stableKey);
        Serial.print(F(" | oct="));
        Serial.print(currentOctave);
        Serial.print(F(" | freq="));
        Serial.println(frequency);
    }
}
