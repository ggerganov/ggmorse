import sys
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

result = ggmorse("Hello world!")

sys.stdout.buffer.write(result.content)
