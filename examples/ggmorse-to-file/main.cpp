#include "ggmorse/ggmorse.h"

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#include "ggmorse-common.h"

#include <cstdio>
#include <cstring>
#include <iostream>

int main(int argc, char** argv) {
    fprintf(stderr, "Usage: %s [-fN] [-wN] [-vN] [-sN]\n", argv[0]);
    fprintf(stderr, "    -fN - frequency of the generated signal, N in [100, sampleRate/2], (default: 550)\n");
    fprintf(stderr, "    -wN - speed of the transmission in words-per-minute, N in [5, 140], (default: 25)\n");
    fprintf(stderr, "    -vN - output volume, N in (0, 100], (default: 50)\n");
    fprintf(stderr, "    -sN - output sample rate, N in [%d, %d], (default: %d)\n", (int) 4000, (int) 96000, (int) GGMorse::kBaseSampleRate);
    fprintf(stderr, "\n");

    if (argc < 1) {
        return -1;
    }

    auto argm = parseCmdArguments(argc, argv);

    if (argm.find("h") != argm.end()) {
        return 0;
    }

    float frequency_hz = argm["f"].empty() ? 550.0f : std::stof(argm["f"]);
    float speed_wpm = argm["w"].empty() ? 25.0f : std::stof(argm["w"]);
    int volume = argm["v"].empty() ? 50 : std::stoi(argm["v"]);
    float sampleRateOut = argm["s"].empty() ? GGMorse::kBaseSampleRate : std::stof(argm["s"]);

    if (frequency_hz < 100 || frequency_hz > sampleRateOut/2 + 1) {
        fprintf(stderr, "Invalid frequency\n");
        return -1;
    }

    if (speed_wpm < 5 || speed_wpm > 140) {
        fprintf(stderr, "Invalid speed\n");
        return -1;
    }

    if (volume <= 0 || volume > 100) {
        fprintf(stderr, "Invalid volume\n");
        return -1;
    }

    if (sampleRateOut < 4000 || sampleRateOut > 96000) {
        fprintf(stderr, "Invalid sample rate: %g\n", sampleRateOut);
        return -1;
    }

    fprintf(stderr, "Enter a text message:\n");

    std::string message;
    std::getline(std::cin, message);

    if (message.size() == 0) {
        fprintf(stderr, "Invalid message: size = 0\n");
        return -2;
    }

    if (message.size() > 256) {
        fprintf(stderr, "Invalid message: size > 256\n");
        return -3;
    }

    fprintf(stderr, "Generating waveform for message '%s' ...\n", message.c_str());

    GGMorse ggMorse({ GGMorse::kBaseSampleRate, sampleRateOut, GGMorse::kDefaultSamplesPerFrame, GGMORSE_SAMPLE_FORMAT_F32, GGMORSE_SAMPLE_FORMAT_I16 });

    ggMorse.setParametersEncode({ 0.01f*volume, frequency_hz, speed_wpm, speed_wpm });
    ggMorse.init(message.size(), message.data());

    std::vector<char> bufferPCM;
    GGMorse::CBWaveformOut cbWaveformOut = [&](const void * data, uint32_t nBytes) {
        bufferPCM.resize(nBytes);
        std::memcpy(bufferPCM.data(), data, nBytes);
    };

    if (ggMorse.encode(cbWaveformOut) == false) {
        fprintf(stderr, "Failed to generate waveform!\n");
        return -4;
    }

    fprintf(stderr, "Output size = %d bytes\n", (int) bufferPCM.size());

    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_PCM;
    format.channels = 1;
    format.sampleRate = sampleRateOut;
    format.bitsPerSample = 16;

    fprintf(stderr, "Writing WAV data ...\n");

    drwav wav;
    drwav_init_file_write(&wav, "/dev/stdout", &format, NULL);
    drwav_uint64 framesWritten = drwav_write_pcm_frames(&wav, bufferPCM.size()/2, bufferPCM.data());

    fprintf(stderr, "WAV frames written = %d\n", (int) framesWritten);

    drwav_uninit(&wav);

    return 0;
}
