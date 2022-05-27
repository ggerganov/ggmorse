## ggmorse-to-file

Output a generated waveform to an uncompressed WAV file.

```
Usage: ./bin/ggmorse-to-file [-fN] [-wN] [-vN] [-sN]
    -fN - frequency of the generated signal, N in [200, 1200], (default: 550)
    -wN - speed of the transmission in words-per-minute, N in [5, 55], (default: 25)
    -vN - output volume, N in (0, 100], (default: 50)
    -sN - output sample rate, N in [4000, 96000], (default: 4000)
```

### Examples

- Generate waveform with default parameters

  ```bash
  echo "Hello world" | ./bin/ggmorse-to-file > example.wav
  ```

- Generate waveform at 24 kHz sample rate

  ```bash
  echo "Hello world" | ./bin/ggmorse-to-file -s24000 > example.wav
  ```

- Generate waveform at 800 Hz and speed 35 WPM

  ```bash
  echo "Hello world" | ./bin/ggmorse-to-file -f800 -w35 > example.wav
  ```


## HTTP service

Based on this tool, there is an HTTP service available on the following link:

https://ggmorse-to-file.ggerganov.com/

You can use it to query audio waveforms by specifying the text message as a GET parameter to the HTTP request. Here are a few examples:

### terminal

```bash
curl -sS 'https://ggmorse-to-file.ggerganov.com/?m=Hello world' --output hello.wav
```

### browser

-  https://ggmorse-to-file.ggerganov.com/?m=Hello%20world%21


### python

```python
import requests

def ggmorse(message: str, frequency_hz: int = 550, speed_wpm: int = 25, sampleRate: float = 4000, volume: int = 50):

    url = 'https://ggmorse-to-file.ggerganov.com/'

    params = {
        'm': message,       # message to encode
        'f': frequency_hz,  # frequency of the generated signal
        'w': speed_wpm,     # transmission speed in words-per-minute
        's': sampleRate,    # output sample rate
        'v': volume,        # output volume
    }

    response = requests.get(url, params=params)

    if response == '':
        raise SyntaxError('Request failed')

    return response

```

...

```python
import sys

# query waveform from server
result = ggmorse("Hello world")

# dump wav file to stdout
sys.stdout.buffer.write(result.content)

```
