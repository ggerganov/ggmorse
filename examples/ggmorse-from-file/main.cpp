#include "ggmorse/ggmorse.h"

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#include "ggmorse-common.h"

#include <cstdio>
#include <cstring>
#include <iostream>

int main(int argc, char** argv) {
    fprintf(stderr, "Usage: %s audio.wav [-fN] [-wN]\n", argv[0]);
    fprintf(stderr, "    -fN - frequency of the sound in HZ, N in [200, 1200], (default: auto)\n");
    fprintf(stderr, "    -wN - speed of the transmission in words-per-minute, N in [5, 55], (default: auto)\n");
    fprintf(stderr, "\n");

    if (argc < 2) {
        return -1;
    }

    auto argm = parseCmdArguments(argc, argv);

    if (argm.find("h") != argm.end()) {
        return 0;
    }

    float frequency_hz = argm["f"].empty() ? -1.0 : std::stof(argm["f"]);
    float speed_wpm = argm["w"].empty() ? -1.0 : std::stof(argm["w"]);

    if (frequency_hz > 0.0f && (frequency_hz < 100 || frequency_hz > GGMorse::kBaseSampleRate/2 + 1)) {
        fprintf(stderr, "Invalid frequency\n");
        return -1;
    }

    if (speed_wpm > 0.0f && (speed_wpm < 5.0f || speed_wpm > 140.0f)) {
        fprintf(stderr, "Invalid speed\n");
        return -1;
    }

    drwav wav;
    if (!drwav_init_file(&wav, argv[1], nullptr)) {
        fprintf(stderr, "Failed to open WAV file\n");
        return -4;
    }

    if (wav.channels != 1) {
        fprintf(stderr, "Only mono WAV files are supported\n");
        return -5;
    }

    // Read WAV samples into a buffer
    // Add 3 seconds of silence at the end
    const size_t samplesSilence = 3*wav.sampleRate;
    const size_t samplesCount = wav.totalPCMFrameCount;
    const size_t samplesSize = wav.bitsPerSample/8;
    const size_t samplesTotal = samplesCount + samplesSilence;
    std::vector<uint8_t> samples(samplesTotal*samplesSize*wav.channels, 0);

    printf("[+] Number of channels: %d\n", wav.channels);
    printf("[+] Sample rate: %d\n", wav.sampleRate);
    printf("[+] Bits per sample: %d\n", wav.bitsPerSample);
    printf("[+] Total samples: %zu\n", samplesCount);

    printf("[+] Decoding: \n\n");

    auto parameters = GGMorse::Parameters {
        (float) wav.sampleRate,
        (float) wav.sampleRate,
        GGMorse::kDefaultSamplesPerFrame,
        GGMORSE_SAMPLE_FORMAT_I16,
        GGMORSE_SAMPLE_FORMAT_I16
    };

    switch (wav.bitsPerSample) {
        case 16:
            drwav_read_pcm_frames_s16(&wav, samplesCount, reinterpret_cast<int16_t*>(samples.data()));

            if (wav.channels > 1) {
                for (size_t i = 0; i < samplesCount; ++i) {
                    int16_t sample = 0;
                    for (size_t j = 0; j < wav.channels; ++j) {
                        sample += reinterpret_cast<int16_t*>(samples.data())[i*wav.channels + j];
                    }
                    reinterpret_cast<int16_t*>(samples.data())[i] = sample / wav.channels;
                }
            }

            parameters.sampleFormatInp = GGMORSE_SAMPLE_FORMAT_I16;

            break;
        case 32:
            drwav_read_pcm_frames_f32(&wav, samplesCount, reinterpret_cast<float*>(samples.data()));

            if (wav.channels > 1) {
                for (size_t i = 0; i < samplesCount; ++i) {
                    float sample = 0.0f;
                    for (size_t j = 0; j < wav.channels; ++j) {
                        sample += reinterpret_cast<float*>(samples.data())[i*wav.channels + j];
                    }
                    reinterpret_cast<float*>(samples.data())[i] = sample / wav.channels;
                }
            }

            parameters.sampleFormatInp = GGMORSE_SAMPLE_FORMAT_F32;

            break;
        default:
            fprintf(stderr, "Unsupported WAV format\n");
            return -6;
    }

    GGMorse ggMorse(parameters);

    {
        auto parametersDecode = ggMorse.getDefaultParametersDecode();
        parametersDecode.frequency_hz = frequency_hz;
        parametersDecode.speed_wpm = speed_wpm;
        ggMorse.setParametersDecode(parametersDecode);
    }

    uint32_t nSamplesHave = samplesTotal;
    GGMorse::CBWaveformInp cbWaveformInp = [&](void * data, uint32_t nMaxBytes) {
        if (nSamplesHave*samplesSize < nMaxBytes) {
            return 0;
        }

        nSamplesHave -= nMaxBytes/samplesSize;
        memcpy(data, samples.data() + (samplesTotal - nSamplesHave)*samplesSize, nMaxBytes);

        return (int) nMaxBytes;
    };

    ggMorse.decode(cbWaveformInp);

    printf("\n\n[+] Done\n");

    return 0;
}
