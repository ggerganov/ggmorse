# ggmorse

[![Actions Status](https://github.com/ggerganov/ggmorse/workflows/CI/badge.svg)](https://github.com/ggerganov/ggmorse/actions)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![ggmorse badge][changelog-badge]][changelog]

Morse code decoding library.

*Work in progress* ...

Demo: https://ggerganov.com/ggmorse-gui/

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
