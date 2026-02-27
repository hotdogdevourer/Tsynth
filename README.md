# Tsynth

**A simple, lightweight speech synthesizer in C.**  

Converts ARPAbet phoneme sequences into synthetic speech using formant synthesis.

This project is a fun, educational take on classic TTS techniques in raw C. It’s ideal for embedded use, learning DSP-style synthesis, or just playing around with phoneme-to-wave conversion.

---

## Features

-  **Text → Speech via ARPAbet** phoneme processing  
-  **Formant-based synthesis**
-  Clean, minimal C code (sort of)
-  Generates waveform output suitable for further audio processing

---

### Clone the repo

```sh
git clone https://github.com/hotdogdevourer/Tsynth.git
cd Tsynth
```
# Build

Tsynth is written in standard C and compiles with gcc:
`gcc -std=c99 -O3 -lm -o tsynth synth.c`

Or alternatively for the compacted code:
`gcc -std=c99 -O3 -lm -o compact_tsynth compacted_synth.c`

# Run
`./tsynth -o output.wav -i input_phonemes.txt`

`input_phonemes.txt` Should contain your phonemes

Or you can use:

`./compact_tsynth -o output.wav -i input_phonemes.txt`

If you run `./tsynth -h` it should show this help menu:

```text
Formant Synthesizer

Usage: ./tsynth [options] "phoneme text"
   or: ./tsynth [options] -i input.txt

Options:
  -o FILE    Output WAV file (default: output.wav)
  -i FILE    Read phoneme text from file
  -f FREQ    Pitch in Hz (default: 100, range: 50-500)
  -v DB      Volume in dB (default: 6.0, range: -120 to +120)
  -V         Verbose output
  -h         Show this help
```

Same goes for `./compact_tsynth -h` but the usage shows `./compact_tsynth` instead of `./tsynth`

# What's inside
- `synth.c` - Core synthesizer implementation
- `compacted_synth.c` - Compacted synthesizer implementation
- `LICENSE` - MIT License

# How it works (High level)
Tsynth uses classic **formant synthesis**, with:
- 5-formant resonator model using biquad filters (direct form II) with time-varying center frequencies
- Voicing control: blends periodic "buzz" (sawtooth oscillator) with noise "hiss" based on phoneme voicing values
- Envelope shaping: attack/sustain/release per phoneme + crossfading between segments
- Soft clipping + normalization: prevents distortion while maximizing output level
