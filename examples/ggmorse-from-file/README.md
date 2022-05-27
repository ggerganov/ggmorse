## ggmorse-from-file

Decode Morse Code from an input WAV file

```
Usage: ./bin/ggmorse-from-file [-fN] [-wN]
    -fN - frequency of the sound in HZ, N in [200, 1200], (default: auto)
    -wN - speed of the transmission in words-per-minute, N in [5, 55], (default: auto)
```

### Examples

- Basic usage with auto-detection of frequency and speed:

  ```bash
  echo "Hello world" | ./bin/ggmorse-to-file > example.wav
  ./bin/ggmorse-from-file example.wav

  Usage: ./bin/ggmorse-from-file audio.wav [-fN] [-wN]
      -fN - frequency of the sound in HZ, N in [200, 1200], (default: auto)
      -wN - speed of the transmission in words-per-minute, N in [5, 55], (default: auto)

  [+] Number of channels: 1
  [+] Sample rate: 4000
  [+] Bits per sample: 16
  [+] Total samples: 21696
  [+] Decoding:

  HELLO WORLD

  [+] Done
  ```

- Decoding at speicific frequency and/or speed:

  ```bash
  ./bin/ggmorse-from-file example.wav -f550 -w25
  ```
