# Third-party notices

Ridgeline Drums contains code from other people's projects. Their licence
notices are reproduced here, as those licences require.

## Peaks drum models and stmlib — MIT licence

The four drum voices (bass drum, snare drum, hi-hat, FM drum), their lookup
tables, and the small support library they depend on come from the open-source
firmware of the **Peaks** Eurorack module, designed by **Emilie Gillet** of
Mutable Instruments.

- Source: <https://github.com/pichenettes/eurorack> (`peaks/` and `stmlib/`)
- Upstream commits used: see `third_party/peaks/UPSTREAM_COMMIT.txt`
- Files are in `third_party/peaks/` and keep their original copyright headers.

```
Copyright 2013 Emilie Gillet (Peaks), Copyright 2012 Emilie Gillet (stmlib).

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

The upstream `stmlib/LICENSE` file (kept at `third_party/peaks/stmlib/LICENSE`)
also describes two directories of stmlib that are **not** included in this
project: `third_party/STM` (STMicroelectronics firmware) and
`programming/serial` (a GPL serial loader). Neither is used or distributed here.

Only files carrying the MIT notice were copied. The upstream desktop test
program (`peaks/test/peaks_test.cc`) is GPL-licensed and was deliberately **not**
used; `tests/engine_test.cpp` is written from scratch.

### Changes made to the upstream files

- `stmlib/utils/random.h` and `random.cc`: the random-number state is now
  `thread_local`, so several plugin instances running on different audio threads
  do not share one generator. No other change to the algorithm.

Everything else under `third_party/peaks/` is unmodified. (`Source/msvc_compat.h`
handles Microsoft-compiler compatibility without editing the upstream files.)

## Trademarks and affiliation

"Mutable Instruments" is a registered trademark. Following the upstream
guidelines for derivative works, this project does not use that name for itself
or its product. Ridgeline Drums is an independent project and is **not made,
endorsed or supported by Mutable Instruments or Emilie Gillet.** Please do not
ask them for support with this plugin.

"VST" is a trademark of Steinberg Media Technologies GmbH.

## JUCE — AGPLv3 (or commercial licence)

The plugin framework is [JUCE](https://juce.com), used here under its
**AGPLv3** option. This is why Ridgeline Drums' own code (everything outside
`third_party/`) is released under the AGPLv3 — see `LICENSE`. If you distribute
the plugin you must make the corresponding source code available under the same
terms. JUCE's own licence notes (including its bundled third-party components)
are in its repository's `LICENSE.md`; the plugin build downloads JUCE 8.0.16
automatically (see `CMakeLists.txt`). The VST3 SDK that ships inside that JUCE
version is listed there as MIT-licensed.
