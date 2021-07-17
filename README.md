# ggmorse

[![Actions Status](https://github.com/ggerganov/ggmorse/workflows/CI/badge.svg)](https://github.com/ggerganov/ggmorse/actions)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![ggmorse badge][changelog-badge]][changelog]

Morse code decoding library.

https://user-images.githubusercontent.com/1991296/126041448-6fd3fe52-c137-4448-ad40-ff4fef186682.mp4

https://user-images.githubusercontent.com/1991296/126041433-201ed39b-bd62-4abf-981b-240106b2f1a9.mp4

https://user-images.githubusercontent.com/1991296/126041445-93e8c683-ba03-4a7c-9d8a-15794df0a11f.mp4

Online Demo: https://ggerganov.com/ggmorse-gui/

## Details

The library decodes Morse code transmission in real-time from raw audio captured via microphone.

- Automatic pitch detection: `[0.2, 1.2] kHz`
- Automatic speed detection: `[5, 55] WPM`

## Building

### Dependencies for SDL-based examples

    [Ubuntu]
    $ sudo apt install libsdl2-dev

    [Mac OS with brew]
    $ brew install sdl2

    [MSYS2]
    $ pacman -S git cmake make mingw-w64-x86_64-dlfcn mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2

### Linux, Mac, Windows (MSYS2)

```bash
# build
git clone https://github.com/ggerganov/ggmorse --recursive
cd ggmorse && mkdir build && cd build
cmake ..
make

# running
./bin/ggmorse-gui
```

### Emscripten

```bash
git clone https://github.com/ggerganov/ggmorse --recursive
cd ggmorse
mkdir build && cd build
emcmake cmake ..
make
```

[changelog]: ./CHANGELOG.md
[changelog-badge]: https://img.shields.io/badge/changelog-ggmorse%20v0.1.0-dummy
[license]: ./LICENSE
