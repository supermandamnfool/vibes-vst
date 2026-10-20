# Vibes

VST3 (and AU on macOS) port of the Mutable Instruments Beads firmware DSP, wrapped with JUCE.
The granular engine is the original firmware code (MIT, Emilie Gillet), vendored in `third_party/`.
Unofficial; not affiliated with Mutable Instruments.

## Build (Windows / macOS)
Needs CMake 3.22+ and a C++17 compiler. JUCE 8.0.12 is downloaded automatically.

    # Windows (ClangCL toolset is required, MSVC can't compile the DSP)
    cmake -S . -B build -G "Visual Studio 17 2022" -T ClangCL -A x64
    cmake --build build --config Release --target Vibes_VST3

    # macOS (universal)
    cmake -S . -B build -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
    cmake --build build --config Release

Or push to GitHub: `.github/workflows/build.yml` builds both and uploads the plugins as artifacts.

Install: copy `Vibes.vst3` to `C:\Program Files\Common Files\VST3` (Windows) or
`~/Library/Audio/Plug-Ins/VST3` (macOS), then rescan in Ableton (Preferences > Plug-ins).
Put it on an audio track as an effect; MIDI input drives the seed (grain trigger) and pitch.
The SYNTH button switches to the module's wavetable granular mode (no audio input; Feedback picks the bank).

## Credits and licences
* DSP: Beads firmware and stmlib, (c) Emilie Gillet, MIT licence (see `third_party/mutable/`).
* Fonts: Barlow, Barlow Condensed and IBM Plex Mono, SIL Open Font License 1.1 (see `Assets/OFL-*.txt`). They are compiled into the plugin.
* Unofficial; not affiliated with Mutable Instruments.
