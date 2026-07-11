# Valvane — Multi-Character Analog-Modeled Compressor

**By AJaudio**

A production-grade audio dynamics processor plugin featuring four distinct analog-modeled compressor topologies, each with mathematically unique detection, gain-reduction, and saturation characteristics.

## Formats & Platforms

| Format      | Windows | macOS | Linux |
|-------------|---------|-------|-------|
| **VST3**    | ✅      | ✅    | ✅    |
| **AU**      | —       | ✅    | —     |
| **Standalone** | ✅   | ✅    | ✅    |

> AAX is not included by default (requires a licensed Avid SDK). The code is structured so AAX can be added later by appending `AAX` to the `FORMATS` list in `CMakeLists.txt` and linking the AAX SDK.

---

## Build Instructions

### Prerequisites

- **CMake** 3.22 or later
- **C++20 compiler**: MSVC 2022+, Clang 14+, or GCC 12+
- **Git** (for JUCE FetchContent)
- **Platform SDKs**:
  - Windows: Windows SDK 10+
  - macOS: Xcode 14+ with Command Line Tools
  - Linux: `libasound2-dev`, `libcurl4-openssl-dev`, `libfreetype6-dev`, `libx11-dev`, `libxrandr-dev`, `libxinerama-dev`, `libxcursor-dev`, `libgl1-mesa-dev`, `webkit2gtk-4.0` (or `webkit2gtk-4.1`)

### Build Commands

```bash
# Clone or download this project
cd Valvane

# Configure (JUCE will be fetched automatically)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release

# The plugin binaries will be in:
#   build/Valvane_artefacts/Release/VST3/
#   build/Valvane_artefacts/Release/AU/          (macOS only)
#   build/Valvane_artefacts/Release/Standalone/
```

#### Platform-Specific Notes

**Windows (MSVC)**:
```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

**macOS (Xcode)**:
```bash
cmake -B build -G Xcode
cmake --build build --config Release
```

**Linux (Make)**:
```bash
sudo apt-get install -y libasound2-dev libfreetype6-dev libx11-dev \
    libxrandr-dev libxinerama-dev libxcursor-dev libgl1-mesa-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

---

## Character Modes

Valvane features four distinct compressor character modes, selectable via the top-panel selector. Each mode dynamically shows only its relevant controls.

### 1. Opto (LA-2A Style) — Default

Emulates an electro-optical leveling amplifier using a nonlinear ODE model of the EL panel + LDR (light-dependent resistor) pair.

**Controls:**
- **Gain** — Makeup gain (dB)
- **Peak Reduction** — Threshold-equivalent drive into the opto cell. This is the only compression control, matching real hardware.
- **Comp/Limit** — Toggle switch: Compress (~3:1 soft character) vs. Limit (aggressive high ratio). Changes the drive-to-gain-reduction mapping, not just a ratio number.

**Character:** Warm, smooth, program-dependent. Release timing depends on how hard and how long the compressor was driven (power-law decay, not simple exponential). Ideal for vocals, bass, and any source that benefits from transparent, gentle leveling.

### 2. FET (1176 Style)

Models a field-effect-transistor limiter with sub-millisecond attack, diode-bridge detector nonlinearity (Shockley equation), and level-dependent release.

**Controls:**
- **Input** — Drive level (dB). Pushes signal into the fixed threshold.
- **Output** — Output level (dB)
- **Attack** — 0.02–0.8 ms (sub-millisecond range)
- **Release** — 50–1200 ms (subtly speeds up as GR decreases)
- **Ratio** — 4:1, 8:1, 12:1, 20:1
- **All Buttons** — Toggle: engages all ratio buttons simultaneously for extreme, distorted compression with erratic gain reduction. A classic 1176 trick.

**Character:** Fast, aggressive, punchy. The detector's diode-bridge nonlinearity makes it increasingly sensitive at higher levels. Great for drums, vocals, guitars, and anything that needs transient control with harmonic excitement.

### 3. Vari-Mu (Fairchild Style)

Models a variable-mu tube compressor where gain reduction and harmonic distortion are coupled outputs of a single shared state variable (tube grid bias shift).

**Controls:**
- **Input** — Drive level (dB)
- **Output** — Output level (dB)
- **Threshold** — Compression threshold (dB)
- **Time Constant** — Single macro control blending attack/release, like the real hardware's multi-position switch (0 = fastest, 100 = slowest)

**Character:** The slowest, warmest, most musical of the four modes. Long program-dependent time constants with a logarithmic knee. As compression deepens, 2nd and 3rd harmonic content increases proportionally through the same nonlinear function — not a separate saturation stage. Perfect for bus glue, mastering, and adding lush tube warmth.

### 4. VCA (SSL-Bus Style)

A clean, precise voltage-controlled amplifier compressor with no coupling between detection and saturation. This is the "transparent/modern" option.

**Controls:**
- **Threshold** — -60 to 0 dB
- **Ratio** — 1:1 to 20:1 (continuous)
- **Attack** — 0.01–100 ms
- **Release** — 10–2000 ms
- **Auto Release** — Toggle: program-dependent release (fast for transients, slow for sustained material, using a dual time-constant approach)
- **Knee** — 0–24 dB width (0 = hard knee, higher = gradual onset)
- **Makeup** — -12 to +36 dB

**Character:** Clean, fast, precise. Full manual control over every parameter. The gain computation uses the Giannoulis/Reiss/Rice soft-knee curve, guaranteeing C1 continuity at knee boundaries. Ideal for surgical dynamics control, mix bus compression, and mastering where transparency is paramount.

---

## Shared Features

- **Mix** — 0–100% dry/wet for parallel ("New York style") compression
- **Sidechain HPF** — 20–500 Hz high-pass filter on the detector path (not the audible output) to prevent bass-driven pumping
- **External Sidechain** — When a DAW routes a sidechain signal, the detector reads from that instead of the main input
- **Stereo Link** — Detector uses max(L,R) for linked stereo compression
- **Detector Type** — Peak, RMS, or Blend (crossfade between both)
- **Auto Makeup Gain** — Compensates for average gain reduction
- **Soft Clip** — Output ceiling using smooth tanh saturation
- **A/B Compare** — Two full parameter snapshots with one-click toggle
- **Bypass** — True bypass (audio routed around all processing)
- **Presets** — 12+ factory presets (3 per mode) plus user save/load/browse
- **VU Meter** — Analog-style needle meter with real ballistic physics (spring-mass-damper model)

---

## Architecture Overview

```
Source/
├── PluginProcessor.h/cpp    — Main audio processor, parameter tree, routing
├── PluginEditor.h/cpp       — Vintage hardware UI, dynamic mode visibility
├── PresetManager.h/cpp      — Factory/user preset management
├── DSP/
│   ├── CompressorEngine.h   — Abstract base class + parameter ID constants
│   ├── EnvelopeDetector.h/cpp — Peak/RMS/Blend envelope follower
│   ├── GainComputer.h/cpp   — Soft-knee gain curve (Giannoulis/Reiss/Rice)
│   ├── SaturationStage.h/cpp — 2x oversampled waveshaping + pre/de-emphasis
│   ├── OptoEngine.h/cpp     — LA-2A-style opto engine
│   ├── FETEngine.h/cpp      — 1176-style FET engine
│   ├── VariMuEngine.h/cpp   — Fairchild-style vari-mu engine
│   └── VCAEngine.h/cpp      — SSL-bus-style VCA engine
└── UI/
    ├── CustomLookAndFeel.h/cpp — Brushed metal, knurled knobs, toggle switches
    └── VUMeterComponent.h/cpp  — Analog needle meter with spring-damper physics
```

---

## DSP References

- Giannoulis, D., Massberg, M., & Reiss, J. D. (2012). "Digital Dynamic Range Compressor Design — A Tutorial and Analysis." *Journal of the Audio Engineering Society*, 60(6), 399-408.
- Shockley diode equation for FET detector modeling
- CdS photocell response modeling for Opto engine
- Second-order damped spring system for VU meter ballistics

---

## License

Copyright © AJaudio. All rights reserved.
