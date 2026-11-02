# Ridgeline Drums

A free VST3 drum instrument built on the drum models from the open-source
firmware of the **Peaks** Eurorack module (by Emilie Gillet, MIT licence).

Four voices, each triggered by a MIDI note:

| Voice | Default note | Knobs |
|---|---|---|
| Bass drum | C1 (36) | Frequency, Punch, Tone, Decay |
| Snare | D1 (38) | Frequency, Tone, Snappy, Decay |
| Hi-hat | F#1 (42) | *(none — fixed in the original)* |
| FM drum | A1 (45) | Frequency, FM Amount, Decay, Noise/Drive |

Every voice also has **Level**, **Pan** and a **MIDI note** selector (so you can
put the drums wherever your pad controller or drum rack expects them), plus
master **Level** and **Velocity** sensitivity (0 % = every hit the same volume,
100 % = full velocity response).

Notes on the knobs:

- **Frequency** on the bass drum and snare shifts the pitch up to ±7 semitones.
- **Noise/Drive** on the FM drum: above 50 % it mixes in noise, below 50 % it
  overdrives the sound. 50 % is clean.
- Double-click any knob to return it to its default.

## What is *not* included

Peaks is a dual-channel module with many functions. This plugin only includes
the **sound-making drum models**. The envelopes, LFOs, pulse shaper, bouncing
ball, sequencer and "number station" functions output control voltages that
have no equivalent inside a VST3, so they were left out.

## Getting the plugin without installing any programming tools

GitHub can build it for you, for free, for Windows, macOS and Linux.

1. Make a free account at <https://github.com> and click **New repository**.
   Give it any name (for example `ridgeline-drums`). Choose **Public** if you
   want to share the plugin; AGPL licensing means the source has to be
   available to anyone you give the plugin to.
2. On the new repository page click **uploading an existing file**, then drag in
   **everything inside this folder** (all files and sub-folders, including the
   hidden `.github` folder), and click **Commit changes**.
   - If the hidden `.github` folder didn't upload: click **Add file ▸ Create new
     file**, type `.github/workflows/build.yml` as the name (typing the slashes
     creates the folders), paste in the contents of that file, and commit.
3. Click the **Actions** tab. A build named "Build VST3" starts automatically
   (about 10–20 minutes the first time).
4. When it finishes (green tick), open it and scroll to **Artifacts** at the
   bottom. Download the one for your system, unzip it, and you'll find
   `Ridgeline Drums.vst3`.
5. If a build shows a red cross, open it, copy the red error text, and send it
   to me. Build errors on one operating system are normal for a first run and
   are usually quick to fix.

### Installing

- **Windows:** copy `Ridgeline Drums.vst3` into `C:\Program Files\Common Files\VST3`
- **macOS:** copy it into `~/Library/Audio/Plug-Ins/VST3`. Because the plugin
  isn't signed with an Apple developer certificate, macOS may block it. If
  your DAW can't load it, open Terminal and run
  `xattr -dr com.apple.quarantine "~/Library/Audio/Plug-Ins/VST3/Ridgeline Drums.vst3"`
- **Linux:** copy it into `~/.vst3`

Then rescan plugins in your DAW and load **Ridgeline Drums** as an instrument.

## Building it yourself (optional)

You need CMake 3.22+ and a C++17 compiler (Visual Studio 2022 on Windows, Xcode
command-line tools on macOS, `build-essential` plus the JUCE Linux dependencies
listed in `.github/workflows/build.yml` on Linux).

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release     # runs the engine test
```

JUCE is downloaded automatically. The finished plugin is under
`build/RidgelineDrums_artefacts/Release/VST3/`.

## How it works

The Peaks drum code is written for a fixed 48 kHz sample rate. The plugin runs
each drum at 48 kHz internally and converts to your DAW's rate with a cubic
interpolator; at 48 kHz it is a bit-exact pass-through of the original code.
`tests/engine_test.cpp` checks that every voice makes sound, stays silent before
its trigger, decays in the same time at 44.1/48/88.2/96 kHz, and matches the
original code exactly at 48 kHz.

## Licence

- Ridgeline Drums' own code: **AGPL-3.0** (`LICENSE`), because it uses JUCE under
  its AGPLv3 option.
- Peaks drum models and stmlib (`third_party/peaks`): **MIT**, © Emilie Gillet.
- Full details and required notices: `THIRD_PARTY_NOTICES.md`.

This project is independent and is not affiliated with or endorsed by Mutable
Instruments.
