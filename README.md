# ggmorse

[![Actions Status](https://github.com/ggerganov/ggmorse/workflows/CI/badge.svg)](https://github.com/ggerganov/ggmorse/actions)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![ggmorse badge][changelog-badge]][changelog]

**Morse code decoding library**

https://user-images.githubusercontent.com/1991296/126041448-6fd3fe52-c137-4448-ad40-ff4fef186682.mp4

https://user-images.githubusercontent.com/1991296/168162178-9265d701-9262-4008-be9d-30d5e6ae1e96.mp4

https://user-images.githubusercontent.com/1991296/168162207-806d564e-2722-4a3a-9e64-196d0602d477.mp4

## Try it out

You can easily test the library using the free **GGMorse** application which is available on the following platforms:

<a href="https://apps.apple.com/us/app/ggmorse/id1573531678?itsct=apps_box_badge&amp;itscg=30200&platform=iphone" style="display: inline-block; overflow: hidden; border-radius: 13px; width: 250px; height: 83px;"><img src="https://tools.applemediaservices.com/api/badges/download-on-the-app-store/white/en-us?size=250x83&amp;releaseDate=1625097600&h=d674545a41868f8632e5c37c3c33bfc8" alt="Download on the App Store" style="border-radius: 13px; width: 250px; height: 83px;" height="60px"></a>
<a href='https://play.google.com/store/apps/details?id=com.ggerganov.GGMorse&pcampaignid=pcampaignidMKT-Other-global-all-co-prtnr-py-PartBadge-Mar2515-1'><img alt='Get it on Google Play' src='https://i.imgur.com/BKDCbKv.png' height="60px"/></a>

Simply start the application and place your phone near speakers or radio that plays some Morse code.
The speed and frequency of the transmission will be detected automatically by the application and you should be able to see
the decoded text in real-time.

Browser Demo: https://ggmorse.ggerganov.com/

## Details

The library decodes Morse code transmission in real-time from raw audio captured via microphone.

- Automatic pitch detection: `[0.2, 1.2] kHz`
- Automatic speed detection: `[5, 55] WPM`

## Todo

The current library implementation is not very user-friendly when it comes to using it in external projects.
Still, if you want to try using it in your project and have trouble in getting it to work, let me know and I can try to help you.

The next version of the library would be in a much better state.
These are the things I want to improve before releasing v0.2.0:

- Improve the C and C++ interface
- Add tests
- Add examples
- Reduce memory allocations
- Clean-up the algorithmic part

Depending on the level of interest this gets, I can also provide various language bindings, similar to [ggwave](https://github.com/ggerganov/ggwave).

## Examples

The [examples](https://github.com/ggerganov/ggmorse/blob/master/examples/) folder contains several sample applications of the library:


| Example | Description | Audio |
| ------- | ----------- | ----- |
| [ggmorse-to-file](https://github.com/ggerganov/ggmorse/blob/master/examples/ggmorse-to-file) | Output a generated waveform to an uncompressed WAV file | - |
| [ggmorse-gui](https://github.com/ggerganov/ggmorse/blob/master/examples/ggmorse-gui) | GUI application for decoding Morse code | SDL |

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
