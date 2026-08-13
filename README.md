## Gameplay Football

Football game. This repository is a **fork of [vi3itor/GameplayFootball](https://github.com/vi3itor/GameplayFootball)**,
itself a fork of the discontinued [GameplayFootball](https://github.com/BazkieBumpercar/GameplayFootball)
written by [Bastiaan Konings Schuiling](http://www.properlydecent.com/).

In 2019, Google Brain picked up the game and created a Reinforcement Learning environment based on it —
[Google Research Football](https://github.com/google-research/football). They improved the game and
updated the libraries, but threw away everything (menus, audio, HUD) that was not necessary for their RL task.

### What this fork adds

- Modern build: CMake 3.16+, C++17, a single static `blunted2` engine library, `sources.cmake` removed.
- SDL2 → SDL3 (SDL3_image/SDL3_ttf; SDL_gfx dropped).
- Renderer migrated to **OpenGL 3.2 core profile**: legacy fixed-function pipeline
  (`glBegin`/`glEnd`, `glLightfv`, matrix stack) removed; rendering runs through the shader
  pipeline (`#version 150`, VAO/VBO). This is a prerequisite for future OpenGL ES
  (mobile) support.
- Builds and runs on Windows (MSVC + vcpkg), Linux (gcc) and macOS (verified on MacBook Air M2,
  2026-08-13; rendering runs on the main thread as AppKit requires, the scheduler on a helper thread).
- **Gamepad input reworked** (SDL3 `SDL_Gamepad`): semantic `SDL_GAMEPAD_BUTTON_*`/`AXIS_*` indices
  instead of raw joystick numbers (this fixes Xbox Series and any modern controller), PES/FIFA layout
  presets switched on the controller-select screen (LB/RB), menu navigation from stick and D-pad,
  and hot-plug (plug/unplug mid-match pauses and opens controller select).
- Determinism tooling: `tools/determinism` runs the match headless and fingerprints the simulation
  (SHA-1) to catch unintended gameplay changes. Platform references live in `tools/determinism/`.
- Project documentation lives in the wiki: `docs/wiki/index.md`.

## Building from source

### Linux

Install required dependencies (SDL3 apt packages exist since Ubuntu 26.04 LTS):
```bash
sudo apt-get install git cmake build-essential libgl1-mesa-dev libsdl3-dev \
libsdl3-image-dev libsdl3-ttf-dev libopenal-dev libboost-all-dev \
libsqlite3-dev
```

Build and run:
```bash
# Clone the repository
git clone https://github.com/Churikov0112/GameplayFootball.git
cd GameplayFootball

# Configure, build, and copy the data next to the binaries
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cp -R data/. build/

# Run from the build directory (the game uses relative data paths)
cd build
./gameplayfootball
```

### macOS

**Status**: verified on MacBook Air M2 (2026-08-13) — window, menu, match, textures and UI render,
clean exit. Window/GL-context creation and the SDL event pump live on the main thread (required by
AppKit); on low-RAM machines build with a single job (`cmake --build build --parallel 1`).

```bash
# Install dependencies (requires brew)
brew install git cmake sdl3 sdl3_image sdl3_ttf boost openal-soft

# Clone and build
git clone https://github.com/Churikov0112/GameplayFootball.git
cd GameplayFootball
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix)"
cmake --build build --parallel 1

# Copy the data next to the binary, then run from the build directory
# (the game uses relative data paths)
cp -R data/. build/
cd build
./gameplayfootball
```

### Windows

Download and install:
- [Visual Studio](https://visualstudio.microsoft.com/downloads/) (2019 or newer),
- [Git](https://git-scm.com/download/win),
- [CMake](https://cmake.org/download/) (make sure to add it to the system PATH).

Install [`vcpkg`](https://github.com/microsoft/vcpkg) as explained in the
[Quick Start Guide](https://github.com/microsoft/vcpkg#quick-start-windows). Install the dependencies
(all triplets **must be `x86-windows`**):
```bat
.\vcpkg.exe install --triplet x86-windows boost:x86-windows sdl3 sdl3-image[jpeg,png] sdl3-ttf openal-soft sqlite3
```

Build and run:
```bat
git clone https://github.com/Churikov0112/GameplayFootball.git
cd GameplayFootball

cmake -B build -A Win32 -DCMAKE_TOOLCHAIN_FILE=C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_WINDOWS_EXPORT_ALL_SYMBOLS=TRUE
cmake --build build --config Release --parallel

xcopy /e /i data build\Release
```

Run `gameplayfootball.exe` inside `build\Release` (the game uses relative data paths).

## Releases

Prebuilt binaries for Windows (x86/x64), Linux and macOS are published on the
[Releases](https://github.com/Churikov0112/GameplayFootball/releases) page —
self-contained archives, no installation required. Packaging scripts live in
`tools/release/` (see `tools/release/README.md`).

## Problems?

If you have any problems, please open an issue.

### Donate

If you want to thank Bastiaan for his great work, consider a donation to his Bitcoin address 1JHnTe2QQj8RL281fXFiyvK9igj2VhPh2t
