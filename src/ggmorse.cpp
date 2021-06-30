#include "ggmorse/ggmorse.h"

#include "stfft.h"
#include "filter.h"
#include "goertzel.h"
#include "resampler.h"

#include <chrono>
#include <string>
#include <unordered_map>

//
// C++ implementation
//

namespace {

float lendot_ms(float speed_wpm) {
    return 60000.0f/(50.0f*speed_wpm);
}

char toUpper(char c) {
    if (c >= 'a' && c <= 'z') return c -= 'a' - 'A';
    return c;
}

// 0 - dot
// 1 - dash
const std::unordered_map<std::string, char> kMorseCode = {
    { "01",     'A', },
    { "1000",   'B', },
    { "1010",   'C', },
    { "100",    'D', },
    { "0",      'E', },
    { "0010",   'F', },
    { "110",    'G', },
    { "0000",   'H', },
    { "00",     'I', },
    { "0111",   'J', },
    { "101",    'K', },
    { "0100",   'L', },
    { "11",     'M', },
    { "10",     'N', },
    { "111",    'O', },
    { "0110",   'P', },
    { "1101",   'Q', },
    { "010",    'R', },
    { "000",    'S', },
    { "1",      'T', },
    { "001",    'U', },
    { "0001",   'V', },
    { "011",    'W', },
    { "1001",   'X', },
    { "1011",   'Y', },
    { "1100",   'Z', },
    { "01111",  '1', },
    { "00111",  '2', },
    { "00011",  '3', },
    { "00001",  '4', },
    { "00000",  '5', },
    { "10000",  '6', },
    { "11000",  '7', },
    { "11100",  '8', },
    { "11110",  '9', },
    { "11111",  '0', },
    { "010101", '.', },
    { "110011", ',', },
    { "001100", '?', },
};

uint64_t t_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); // duh ..
}

float dt_ms(uint64_t tStart_us) {
    return 1e-3*(t_us() - tStart_us);
}

int bytesForSampleFormat(GGMorse::SampleFormat sampleFormat) {
    switch (sampleFormat) {
        case GGMORSE_SAMPLE_FORMAT_UNDEFINED:    return 0;                   break;
        case GGMORSE_SAMPLE_FORMAT_U8:           return sizeof(uint8_t);     break;
        case GGMORSE_SAMPLE_FORMAT_I8:           return sizeof(int8_t);      break;
        case GGMORSE_SAMPLE_FORMAT_U16:          return sizeof(uint16_t);    break;
        case GGMORSE_SAMPLE_FORMAT_I16:          return sizeof(int16_t);     break;
        case GGMORSE_SAMPLE_FORMAT_F32:          return sizeof(float);       break;
    };

    fprintf(stderr, "Invalid sample format: %d\n", (int) sampleFormat);

    return 0;
}

struct Interval {
    int signal = 0;
    int start = 0;
    int end = 0;
    float avg = 0.0f;
    float com = 0.0f;
    float len = 0.0f;
    int type = 0; // 0 - dot, 1 - dah
};

}

struct GGMorse::Impl {
    const float sampleRateInp;
    const float sampleRateOut;
    const int samplesPerFrame;
    const int sampleSizeBytesInp;
    const int sampleSizeBytesOut;
    const SampleFormat sampleFormatInp;
    const SampleFormat sampleFormatOut;

    int samplesNeeded;
    int framesProcessed = 0;
    int txDataLength = 0;

    bool hasNewTxData = false;
    bool hasNewWaveform = false;
    bool hasNewSpectrogram = false;
    bool receivingData = false;
    bool lastDecodeResult = false;

    ParametersDecode parametersDecode = getDefaultParametersDecode();
    ParametersEncode parametersEncode = getDefaultParametersEncode();

    Statistics statistics = {};

    Interval lastInterval = {};
    std::string curLetter = "";

    WaveformF waveform = WaveformF(2*kMaxSamplesPerFrame + 128);
    WaveformF waveformResampled = WaveformF(2*kMaxSamplesPerFrame + 128);
    TxRx waveformTmp = TxRx((2*kMaxSamplesPerFrame + 128)*sampleSizeBytesInp);
    Spectrogram spectrogram = Spectrogram(0);

    TxRx rxData = {};
    TxRx txData = {};
    SignalF signalF = {};
    WaveformI16 txWaveformI16 = {};

    TxRx outputBlockTmp = {};
    WaveformF outputBlockF = {};
    WaveformI16 outputBlockI16 = {};

    // todo : refactor
    std::vector<std::vector<std::vector<Interval>>> intervalsAll = {};

    STFFT stfft = {};
    Filter filterHighPass200Hz = {};
    Filter filterLowPass2kHz = {};
    Resampler resampler = {};
    GoertzelRunningFIR goertzelFilter = {};
};

const GGMorse::Parameters & GGMorse::getDefaultParameters() {
    static ggmorse_Parameters result {
        kBaseSampleRate,
        kBaseSampleRate,
        kDefaultSamplesPerFrame,
        GGMORSE_SAMPLE_FORMAT_F32,
        GGMORSE_SAMPLE_FORMAT_F32,
    };

    return result;
}

const GGMorse::ParametersDecode & GGMorse::getDefaultParametersDecode() {
    static ggmorse_ParametersDecode result {
        -1.0f,
        -1.0f,
    };

    return result;
}

const GGMorse::ParametersEncode & GGMorse::getDefaultParametersEncode() {
    static ggmorse_ParametersEncode result {
        10,
        550.0f,
        25.0f,
        25.0f,
    };

    return result;
}

GGMorse::GGMorse(const Parameters & parameters)
    : m_impl(new Impl({
        parameters.sampleRateInp,
        parameters.sampleRateOut,
        parameters.samplesPerFrame,
        bytesForSampleFormat(parameters.sampleFormatInp),
        bytesForSampleFormat(parameters.sampleFormatOut),
        parameters.sampleFormatInp,
        parameters.sampleFormatOut,
        parameters.samplesPerFrame,
    })) {

    m_impl->intervalsAll.resize(100);
    for (auto & intervals : m_impl->intervalsAll) {
        intervals.resize(100);
        for (auto & x : intervals) {
            x.reserve(100);
        }
    }

    m_impl->rxData.reserve(1024);

    int pow2For10Hz = 1;
    while (pow2For10Hz < kBaseSampleRate/10) pow2For10Hz *= 2;

    int pow2For50Hz = 1;
    while (pow2For50Hz < kBaseSampleRate/50) pow2For50Hz *= 2;

    m_impl->stfft.init(kBaseSampleRate, pow2For10Hz, parameters.samplesPerFrame, kMaxWindowToAnalyze_s);
    m_impl->filterHighPass200Hz.init(Filter::FirstOrderHighPass, 200.0f, kBaseSampleRate);
    m_impl->filterLowPass2kHz.init(Filter::FirstOrderLowPass, 2000.0f, parameters.sampleRateInp);
    m_impl->goertzelFilter.init(kBaseSampleRate, pow2For50Hz, kMaxWindowToAnalyze_s);
}

GGMorse::~GGMorse() {
}

bool GGMorse::setParametersDecode(const ParametersDecode & parameters) {
    // todo : validate parameters

    m_impl->parametersDecode = parameters;

    return true;
}

bool GGMorse::setParametersEncode(const ParametersEncode & parameters) {
    // todo : validate parameters

    if (parameters.volume < 0.0f || parameters.volume > 1.0f) {
        fprintf(stderr, "Invalid volume: %g\n", parameters.volume);
        return false;
    }

    m_impl->parametersEncode = parameters;

    return true;
}

bool GGMorse::init(int dataSize, const char * dataBuffer) {
    if (dataSize < 0) {
        fprintf(stderr, "Negative data size: %d\n", dataSize);
        return false;
    }

    if (dataSize > kMaxTxLength) {
        fprintf(stderr, "Truncating data from %d to %d bytes\n", dataSize, kMaxTxLength);
        dataSize = kMaxTxLength;
    }

    m_impl->txDataLength = dataSize;

    const uint8_t * text = reinterpret_cast<const uint8_t *>(dataBuffer);

    m_impl->hasNewTxData = false;
    m_impl->txData.resize(m_impl->txDataLength);

    if (m_impl->txDataLength > 0) {
        for (int i = 0; i < m_impl->txDataLength; ++i) m_impl->txData[i] = text[i];

        m_impl->hasNewTxData = true;
    }

    return true;
}

bool GGMorse::encode(const CBWaveformOut & cbWaveformOut) {
    if (m_impl->hasNewTxData == false) {
        return false;
    }

    m_impl->hasNewTxData = false;

    int nSamplesTotal = 0;

    float lendot0_samples = m_impl->sampleRateOut*(1e-3*lendot_ms(m_impl->parametersEncode.speedCharacters_wpm));
    float lendot1_samples = m_impl->sampleRateOut*(1e-3*lendot_ms(m_impl->parametersEncode.speedFarnsworth_wpm));

    float lenLetterSpace_samples = 3.0f*lendot1_samples;
    float lenWordSpace_samples = 7.0f*lendot1_samples;

    // 0 - dot
    // 1 - dash
    // 2 - pause between symbols
    // 3 - pause between letters
    // 4 - pause between words
    std::string symbols0;
    std::string symbols1;

    for (int i = 0; i < m_impl->txDataLength; ++i) {
        for (const auto & l : kMorseCode) {
            if (l.second == toUpper(m_impl->txData[i])) {
                for (int k = 0; k < (int) l.first.size(); ++k) {
                    if (l.first[k] == '0') {
                        nSamplesTotal += 1*lendot0_samples;
                        symbols0 += "0";
                        symbols1 += ".";
                    }
                    if (l.first[k] == '1') {
                        nSamplesTotal += 3*lendot0_samples;
                        symbols0 += "1";
                        symbols1 += "-";
                    }
                    if (k < (int) l.first.size() - 1) {
                        nSamplesTotal += lendot1_samples;
                        symbols0 += "2";
                        symbols1 += "";
                    }
                }
                break;
            }
        }

        if (i < m_impl->txDataLength - 1) {
            if (m_impl->txData[i + 1] != ' ') {
                nSamplesTotal += lenLetterSpace_samples;
                symbols0 += "3";
                symbols1 += " ";
            } else {
                nSamplesTotal += lenWordSpace_samples;
                symbols0 += "4";
                symbols1 += " / ";
            }
        }
    }

    m_impl->outputBlockF.resize(nSamplesTotal);

    int idx = 0;
    float factorCur = 0.0;
    float factorDst = 0.0;
    const auto dampFactor = 1.0f/std::max(1.0f, 0.1f*lendot0_samples);
    const auto & volume = m_impl->parametersEncode.volume;
    const auto & frequency_hz = m_impl->parametersEncode.frequency_hz;
    for (const char & s : symbols0) {
        if (s == '0') {
            factorDst = 1.0f;
            for (int i = 0; i < lendot0_samples; ++i) {
                m_impl->outputBlockF[idx] = factorCur*volume*std::sin((2.0*M_PI)*(idx*frequency_hz/m_impl->sampleRateOut));
                factorCur = std::min(1.0f, factorCur + dampFactor);
                ++idx;
            }
        }
        if (s == '1') {
            factorDst = 1.0f;
            for (int i = 0; i < 3*lendot0_samples; ++i) {
                m_impl->outputBlockF[idx] = factorCur*volume*std::sin((2.0*M_PI)*(idx*frequency_hz/m_impl->sampleRateOut));
                factorCur = std::min(1.0f, factorCur + dampFactor);
                ++idx;
            }
        }
        if (s == '2') {
            factorDst = 0.0f;
            for (int i = 0; i < lendot1_samples; ++i) {
                m_impl->outputBlockF[idx] = factorCur*volume*std::sin((2.0*M_PI)*(idx*frequency_hz/m_impl->sampleRateOut));
                factorCur = std::max(0.0f, factorCur - dampFactor);
                ++idx;
            }
        }
        if (s == '3') {
            factorDst = 0.0f;
            for (int i = 0; i < lenLetterSpace_samples; ++i) {
                m_impl->outputBlockF[idx] = factorCur*volume*std::sin((2.0*M_PI)*(idx*frequency_hz/m_impl->sampleRateOut));
                factorCur = std::max(0.0f, factorCur - dampFactor);
                ++idx;
            }
        }
        if (s == '4') {
            factorDst = 0.0f;
            for (int i = 0; i < lenWordSpace_samples; ++i) {
                m_impl->outputBlockF[idx] = factorCur*volume*std::sin((2.0*M_PI)*(idx*frequency_hz/m_impl->sampleRateOut));
                factorCur = std::max(0.0f, factorCur - dampFactor);
                ++idx;
            }
        }
    }

    // default output is in 16-bit signed int so we always compute it
    m_impl->outputBlockI16.resize(nSamplesTotal);
    for (int i = 0; i < nSamplesTotal; ++i) {
        m_impl->outputBlockI16[i] = 32768*m_impl->outputBlockF[i];
    }

    // convert from 32-bit float
    m_impl->outputBlockTmp.resize(nSamplesTotal*m_impl->sampleSizeBytesOut);
    switch (m_impl->sampleFormatOut) {
        case GGMORSE_SAMPLE_FORMAT_UNDEFINED:
            {
                return false;
            } break;
        case GGMORSE_SAMPLE_FORMAT_U8:
            {
                auto p = reinterpret_cast<uint8_t *>(m_impl->outputBlockTmp.data());
                for (int i = 0; i < nSamplesTotal; ++i) {
                    p[i] = 128*(m_impl->outputBlockF[i] + 1.0f);
                }
            } break;
        case GGMORSE_SAMPLE_FORMAT_I8:
            {
                auto p = reinterpret_cast<uint8_t *>(m_impl->outputBlockTmp.data());
                for (int i = 0; i < nSamplesTotal; ++i) {
                    p[i] = 128*m_impl->outputBlockF[i];
                }
            } break;
        case GGMORSE_SAMPLE_FORMAT_U16:
            {
                auto p = reinterpret_cast<uint16_t *>(m_impl->outputBlockTmp.data());
                for (int i = 0; i < nSamplesTotal; ++i) {
                    p[i] = 32768*(m_impl->outputBlockF[i] + 1.0f);
                }
            } break;
        case GGMORSE_SAMPLE_FORMAT_I16:
            {
                // skip because we already have the data in m_impl->outputBlockI16
                //auto p = reinterpret_cast<uint16_t *>(m_impl->outputBlockTmp.data());
                //for (int i = 0; i < nSamplesTotal; ++i) {
                //    p[i] = 32768*m_impl->outputBlockF[i];
                //}
            } break;
        case GGMORSE_SAMPLE_FORMAT_F32:
            {
                auto p = reinterpret_cast<float *>(m_impl->outputBlockTmp.data());
                for (int i = 0; i < nSamplesTotal; ++i) {
                    p[i] = m_impl->outputBlockF[i];
                }
            } break;
    }

    // output generated data via the provided callback
    switch (m_impl->sampleFormatOut) {
        case GGMORSE_SAMPLE_FORMAT_UNDEFINED:
            {
                return false;
            } break;
        case GGMORSE_SAMPLE_FORMAT_I16:
            {
                cbWaveformOut(m_impl->outputBlockI16.data(), nSamplesTotal*m_impl->sampleSizeBytesOut);
            } break;
        case GGMORSE_SAMPLE_FORMAT_U8:
        case GGMORSE_SAMPLE_FORMAT_I8:
        case GGMORSE_SAMPLE_FORMAT_U16:
        case GGMORSE_SAMPLE_FORMAT_F32:
            {
                cbWaveformOut(m_impl->outputBlockTmp.data(), nSamplesTotal*m_impl->sampleSizeBytesOut);
            } break;
    }

    m_impl->txWaveformI16.resize(nSamplesTotal);
    for (int i = 0; i < nSamplesTotal; ++i) {
        m_impl->txWaveformI16[i] = m_impl->outputBlockI16[i];
    }

    return true;
}

bool GGMorse::decode(const CBWaveformInp & cbWaveformInp) {
    bool result = false;
    while (m_impl->hasNewTxData == false) {
        const auto tStart_us = t_us();

        if (m_impl->samplesNeeded < m_impl->samplesPerFrame) {
            m_impl->samplesNeeded += m_impl->samplesPerFrame;
        }

        // read capture data
        const float factor = m_impl->sampleRateInp/kBaseSampleRate;
        uint32_t nBytesNeeded = m_impl->samplesNeeded*m_impl->sampleSizeBytesInp;

        bool resampleSimple = false;
        if (m_impl->sampleRateInp != kBaseSampleRate) {
            if (int(m_impl->sampleRateInp) % int(kBaseSampleRate) == 0) {
                nBytesNeeded *= factor;
                resampleSimple = true;
            } else {
                nBytesNeeded = (m_impl->resampler.resample(1.0f/factor,
                                                           m_impl->samplesNeeded,
                                                           m_impl->waveformResampled.data(),
                                                           nullptr))*m_impl->sampleSizeBytesInp;
            }
        }

        uint32_t nBytesRecorded = 0;

        switch (m_impl->sampleFormatInp) {
            case GGMORSE_SAMPLE_FORMAT_UNDEFINED: break;
            case GGMORSE_SAMPLE_FORMAT_U8:
            case GGMORSE_SAMPLE_FORMAT_I8:
            case GGMORSE_SAMPLE_FORMAT_U16:
            case GGMORSE_SAMPLE_FORMAT_I16:
                {
                    nBytesRecorded = cbWaveformInp(m_impl->waveformTmp.data(), nBytesNeeded);
                } break;
            case GGMORSE_SAMPLE_FORMAT_F32:
                {
                    nBytesRecorded = cbWaveformInp(m_impl->waveformResampled.data(), nBytesNeeded);
                } break;
        }

        if (nBytesRecorded == 0) {
            break;
        }

        if (nBytesRecorded % m_impl->sampleSizeBytesInp != 0) {
            fprintf(stderr, "Failure during capture - provided bytes (%d) are not multiple of sample size (%d)\n",
                    nBytesRecorded, m_impl->sampleSizeBytesInp);
            m_impl->samplesNeeded = m_impl->samplesPerFrame;
            break;
        }

        if (nBytesRecorded > 0 && nBytesRecorded < nBytesNeeded) {
            fprintf(stderr, "Failure during capture - less samples were provided (%d) than requested (%d)\n",
                    nBytesRecorded/m_impl->sampleSizeBytesInp, nBytesNeeded/m_impl->sampleSizeBytesInp);
            m_impl->samplesNeeded = m_impl->samplesPerFrame;
            break;
        }

        if (nBytesRecorded > nBytesNeeded) {
            fprintf(stderr, "Failure during capture - more samples were provided (%d) than requested (%d)\n",
                    nBytesRecorded/m_impl->sampleSizeBytesInp, nBytesNeeded/m_impl->sampleSizeBytesInp);
            m_impl->samplesNeeded = m_impl->samplesPerFrame;
            break;
        }

        // convert to 32-bit float
        int nSamplesRecorded = nBytesRecorded/m_impl->sampleSizeBytesInp;
        switch (m_impl->sampleFormatInp) {
            case GGMORSE_SAMPLE_FORMAT_UNDEFINED: break;
            case GGMORSE_SAMPLE_FORMAT_U8:
                {
                    constexpr float scale = 1.0f/128;
                    auto p = reinterpret_cast<uint8_t *>(m_impl->waveformTmp.data());
                    for (int i = 0; i < nSamplesRecorded; ++i) {
                        m_impl->waveformResampled[i] = float(int16_t(*(p + i)) - 128)*scale;
                    }
                } break;
            case GGMORSE_SAMPLE_FORMAT_I8:
                {
                    constexpr float scale = 1.0f/128;
                    auto p = reinterpret_cast<int8_t *>(m_impl->waveformTmp.data());
                    for (int i = 0; i < nSamplesRecorded; ++i) {
                        m_impl->waveformResampled[i] = float(*(p + i))*scale;
                    }
                } break;
            case GGMORSE_SAMPLE_FORMAT_U16:
                {
                    constexpr float scale = 1.0f/32768;
                    auto p = reinterpret_cast<uint16_t *>(m_impl->waveformTmp.data());
                    for (int i = 0; i < nSamplesRecorded; ++i) {
                        m_impl->waveformResampled[i] = float(int32_t(*(p + i)) - 32768)*scale;
                    }
                } break;
            case GGMORSE_SAMPLE_FORMAT_I16:
                {
                    constexpr float scale = 1.0f/32768;
                    auto p = reinterpret_cast<int16_t *>(m_impl->waveformTmp.data());
                    for (int i = 0; i < nSamplesRecorded; ++i) {
                        m_impl->waveformResampled[i] = float(*(p + i))*scale;
                    }
                } break;
            case GGMORSE_SAMPLE_FORMAT_F32: break;
        }

        if (nSamplesRecorded == 0) {
            break;
        }

        uint32_t offset = m_impl->samplesNeeded > m_impl->samplesPerFrame ? 2*m_impl->samplesPerFrame - m_impl->samplesNeeded : 0;

        if (m_impl->sampleRateInp != kBaseSampleRate) {
            if (resampleSimple) {
                m_impl->filterLowPass2kHz.process(m_impl->waveformResampled.data(), nSamplesRecorded);

                int ds = int(factor);
                int nSamplesResampled = 0;
                for (int i = 0; i < nSamplesRecorded; i += ds) {
                    m_impl->waveform[offset + nSamplesResampled] = m_impl->waveformResampled[i];
                    ++nSamplesResampled;
                }
                nSamplesRecorded = offset + nSamplesResampled;
            } else {
                if (nSamplesRecorded <= 2*Resampler::kWidth) {
                    fprintf(stderr, "Failure to resample data - provided samples (%d) are less than the allowed minimum (%d)\n",
                            nSamplesRecorded, 2*Resampler::kWidth);
                    m_impl->samplesNeeded = m_impl->samplesPerFrame;
                    break;
                }

                // reset resampler state every minute
                if (!m_impl->receivingData && m_impl->resampler.nSamplesTotal() > 60.0f*factor*kBaseSampleRate) {
                    m_impl->resampler.reset();
                }

                int nSamplesResampled = m_impl->resampler.resample(factor, nSamplesRecorded, m_impl->waveformResampled.data(), m_impl->waveform.data() + offset);
                nSamplesRecorded = offset + nSamplesResampled;
            }
        } else {
            for (int i = 0; i < nSamplesRecorded; ++i) {
                m_impl->waveform[offset + i] = m_impl->waveformResampled[i];
            }
        }

        // we have enough bytes to do analysis
        if (nSamplesRecorded >= m_impl->samplesPerFrame) {
            m_impl->statistics.timeResample_ms = dt_ms(tStart_us);

            while (nSamplesRecorded >= m_impl->samplesPerFrame) {
                m_impl->hasNewWaveform = true;

                decode_float();
                result = true;

                int nExtraSamples = nSamplesRecorded - m_impl->samplesPerFrame;
                for (int i = 0; i < nExtraSamples; ++i) {
                    m_impl->waveform[i] = m_impl->waveform[m_impl->samplesPerFrame + i];
                }

                m_impl->samplesNeeded = m_impl->samplesPerFrame - nExtraSamples;
                nSamplesRecorded -= m_impl->samplesPerFrame;
            }
        } else {
            m_impl->samplesNeeded = m_impl->samplesPerFrame - nSamplesRecorded;
            break;
        }
    }

    m_impl->lastDecodeResult = result;

    return result;
}

void GGMorse::decode_float() {
    auto tStart_us = t_us();

    m_impl->filterHighPass200Hz.process(m_impl->waveform.data(), m_impl->samplesPerFrame);
    m_impl->stfft.process(m_impl->waveform.data(), m_impl->samplesPerFrame);

    auto frequency_hz = m_impl->parametersDecode.frequency_hz;
    auto speed_wpm = m_impl->parametersDecode.speed_wpm;

    if (frequency_hz <= 0.0f) {
        frequency_hz = m_impl->stfft.pitch(200.0f, 1200.0f);
    }

    if (std::fabs(frequency_hz - m_impl->statistics.estimatedPitch_Hz) > 100.0) {
        m_impl->goertzelFilter.clear();
        m_impl->rxData.push_back('\n');
    }

    m_impl->statistics.timePitchDetection_ms = dt_ms(tStart_us);
    m_impl->statistics.estimatedPitch_Hz = frequency_hz;

    tStart_us = t_us();

    m_impl->goertzelFilter.process(m_impl->waveform.data(), m_impl->samplesPerFrame, frequency_hz);

    // todo : this is a copy
    auto filteredF = m_impl->goertzelFilter.filtered();

    // experimental filtering:
    // noise below 200 Hz is eliminated
    //auto filteredF = m_impl->goertzelFilter.filtered_min(kBaseSampleRate/200.0f);

    int nSamples = (int) filteredF.size();
    int windowToAnalyze_samples = kMaxWindowToAnalyze_s*kBaseSampleRate;
    int nFramesInWindow = windowToAnalyze_samples/m_impl->samplesPerFrame;

    int nDownsample = 1;
    while ((nSamples % 2 == 0) && (windowToAnalyze_samples > 500*kMaxWindowToAnalyze_s)) {
        nDownsample *= 2;
        nSamples /= 2;
        windowToAnalyze_samples /= 2;
    }

    double mean = 0.0;
    for (int i = 0; i < nSamples; ++i) {
        float sum = 0.0;
        for (int j = 0; j < nDownsample; ++j) {
            sum += filteredF[i*nDownsample + j];
        }
        sum /= nDownsample;
        filteredF[i] = sum;
        mean += filteredF[i];
    }
    mean /= nSamples;
    filteredF.resize(nSamples);

    m_impl->statistics.timeGoertzel_ms = dt_ms(tStart_us);

    tStart_us = t_us();

    float bestCost = 1e6;
    int bestLevelIdx = 0;
    int bestSpeedIdx = 0;

    int s0 = 0;
    int s1 = 50;
    int ds = 10;
    int nModes = 2;

    if (speed_wpm > 0.0f && speed_wpm < 100.0f) {
        s0 = s1 = std::round(speed_wpm - 5.0f);
        nModes = 1;
    }

    for (int mode = 0; mode < nModes; ++mode) {
        if (mode == 1) {
            s0 = std::min(std::max(0.0f, std::round(m_impl->statistics.estimatedSpeed_wpm - 5.0f - 2.0f)), 50.0f);
            s1 = std::min(std::max(0.0f, std::round(m_impl->statistics.estimatedSpeed_wpm - 5.0f + 2.0f)), 50.0f);
            ds = 1;
        }

        int lOld = std::min(std::max(20.0f, 100.0f*m_impl->statistics.signalThreshold), 80.0f);
        int l0 = (mode == 0) ? 10 : lOld - 10;
        int l1 = (mode == 0) ? 90 : lOld + 10;
        int dl = (mode == 0) ? 20 : 2;

        for (int s = s0; s <= s1 && s < 55; s += ds) {
            float lendot_samples = kBaseSampleRate*(1e-3*lendot_ms(5 + s))/nDownsample;

            for (int l = l0; l <= l1; l += dl) {
                float level = (0.01*mean)*l;
                int lastSignal = filteredF[0] > level ? 1 : 0;

                Interval curInterval;
                curInterval.signal = lastSignal;
                curInterval.start = 0;
                curInterval.avg = filteredF[0];

                auto & intervals = m_impl->intervalsAll[s][l];
                intervals.clear();

                int nOnIntervals = 0;
                float avgOnLength = 0.0f;

                for (int i = 1; i < nSamples; ++i) {
                    int curSignal = filteredF[i] > level ? 1 : 0;
                    if (curSignal != lastSignal) {
                        curInterval.end = i;
                        curInterval.avg /= (i - curInterval.start);
                        curInterval.len = float(curInterval.end - curInterval.start)/lendot_samples;
                        intervals.push_back(curInterval);

                        if (curInterval.signal == 1) {
                            nOnIntervals++;
                            avgOnLength += curInterval.len;
                        }

                        curInterval.signal = curSignal;
                        curInterval.start = i;
                        curInterval.avg = filteredF[i];
                        lastSignal = curSignal;
                    } else {
                        curInterval.avg += filteredF[i];
                    }
                }

                avgOnLength /= nOnIntervals;

                curInterval.end = nSamples;
                intervals.push_back(curInterval);

                int nIntervals = (int) intervals.size();

                for (int i = 0; i < nIntervals; ++i) {
                    if (intervals[i].signal == 0) {
                        intervals[i].type = 0;
                        continue;
                    }

                    intervals[i].type = intervals[i].len > 2 ? 1 : 0;
                }

                float curCost = 0.0f;

                int nDots = 0;
                float avgDotLength = 0.0f;

                int nDahs = 0;
                float avgDahLength = 0.0f;

                for (int i = 1; i < nIntervals - 1; ++i) {
                    const auto & curInterval = intervals[i];
                    if (curInterval.signal == 0) continue;

                    if (curInterval.type == 0) {
                        nDots++;
                        avgDotLength += curInterval.len;
                    }

                    if (curInterval.type == 1) {
                        nDahs++;
                        avgDahLength += curInterval.len;
                    }
                }

                if (nDots > 0) avgDotLength /= nDots; else avgDotLength = 1.0f;
                if (nDahs > 0) avgDahLength /= nDahs; else avgDahLength = 3.0f;

                for (int i = 1; i < nIntervals - 1; ++i) {
                    auto & curInterval = intervals[i];
                    if (curInterval.signal == 0) {
                        continue;
                    }

                    float mid = 0.5f*(curInterval.start + curInterval.end);
                    if (curInterval.type == 0) {
                        curInterval.len *= 1.0f/avgDotLength;
                    } else {
                        curInterval.len *= 3.0f/avgDahLength;
                    }

                    intervals[i - 1].end = curInterval.start = mid - 0.5f*curInterval.len*lendot_samples;
                    intervals[i - 1].len = float(intervals[i - 1].end - intervals[i - 1].start)/lendot_samples;
                    intervals[i + 1].start = curInterval.end = mid + 0.5f*curInterval.len*lendot_samples;
                    intervals[i + 1].len = float(intervals[i + 1].end - intervals[i + 1].start)/lendot_samples;
                }

                nDots = 0;
                float costDots = 0.0f;
                nDahs = 0;
                float costDahs = 0.0f;

                int nSpaces = 0;
                float costSpaces = 0.0f;

                for (int i = 1; i < nIntervals - 1; ++i) {
                    auto & curInterval = intervals[i];
                    if (curInterval.signal == 0) {
                        curInterval.type = 0;

                        if (curInterval.len < 8.0) {
                            float c1 = std::pow(curInterval.len - 1.0, 2);
                            float c3 = std::pow(curInterval.len - 3.0, 2);
                            float c7 = std::pow(curInterval.len - 7.0, 2);

                            if (c1 < c3 && c1 < c7) {
                                curInterval.type = 1;
                                costSpaces += std::min(std::min(c1, c3), c7);
                                ++nSpaces;
                            } else if (c3 < c1 && c3 < c7) {
                                curInterval.type = 2;
                            } else if (c7 < c1 && c7 < c3) {
                                curInterval.type = 3;
                            }
                        }

                        continue;
                    }

                    if (curInterval.type == 0) {
                        nDots++;
                        costDots += std::pow(curInterval.len - 1.0, 2);
                    }

                    if (curInterval.type == 1) {
                        nDahs++;
                        costDahs += std::pow(curInterval.len - 3.0, 2);
                    }
                }

                if (nSpaces == 0) { nSpaces = 1; costSpaces = 100.0f; }
                if (nDots < 1) { nDots = 1; costDots = 100.0f; }
                if (nDahs < 1) { nDahs = 1; costDahs = 100.0f; }

                curCost = costDots/nDots + costDahs/nDahs + costSpaces/nSpaces;

                if (avgDahLength/avgDotLength < 2.5 || avgDahLength/avgDotLength > 3.5) curCost += 100.0f;

                if (curCost < bestCost) {
                    bestCost = curCost;
                    bestLevelIdx = l;
                    bestSpeedIdx = s;
                }
            }
        }
    }

    m_impl->statistics.timeFrameAnalysis_ms = dt_ms(tStart_us);

    {
        const auto & intervals = m_impl->intervalsAll[bestSpeedIdx][bestLevelIdx];

        m_impl->statistics.estimatedSpeed_wpm = 5 + bestSpeedIdx;
        m_impl->statistics.signalThreshold = 0.01*bestLevelIdx;

        int j = 0;
        for (int i = 0; i < m_impl->samplesPerFrame/nDownsample; ++i) {
            int s = (nFramesInWindow/2)*m_impl->samplesPerFrame/nDownsample + i;

            while (s >= intervals[j].end) ++j;

            if (m_impl->lastInterval.signal != intervals[j].signal) {
                if (intervals[j].signal == 1) {
                    m_impl->curLetter += intervals[j].type == 1 ? "1" : "0";
                } else {
                    if (intervals[j].type == 0 ||
                        intervals[j].type == 2 ||
                        intervals[j].type == 3) {
                        if (auto let = kMorseCode.find(m_impl->curLetter); let != kMorseCode.end()) {
                            m_impl->rxData.push_back(let->second);
                            printf("%c", let->second);
                        } else {
                            m_impl->rxData.push_back('?');
                            printf("?");
                        }
                        fflush(stdout);
                        m_impl->curLetter = "";
                    }
                    {
                        std::string tmp = intervals[j].type == 2 ? "" : intervals[j].type == 3 ? " " : intervals[j].type == 1 ? "" : " ";
                        if (tmp.size()) {
                            m_impl->rxData.push_back(tmp[0]);
                        }
                        printf("%s", tmp.c_str());
                    }
                }
                m_impl->lastInterval = intervals[j];
            }
        }
    }

    m_impl->signalF = filteredF;

    ++m_impl->framesProcessed;
}

const bool & GGMorse::hasTxData() const { return m_impl->hasNewTxData; }
const bool & GGMorse::lastDecodeResult() const { return m_impl->lastDecodeResult; }

const int & GGMorse::getSamplesPerFrame() const { return m_impl->samplesPerFrame; }
const int & GGMorse::getSampleSizeBytesInp() const { return m_impl->sampleSizeBytesInp; }
const int & GGMorse::getSampleSizeBytesOut() const { return m_impl->sampleSizeBytesOut; }

const float & GGMorse::getSampleRateInp() const { return m_impl->sampleRateInp; }
const float & GGMorse::getSampleRateOut() const { return m_impl->sampleRateOut; }
const GGMorse::SampleFormat & GGMorse::getSampleFormatInp() const { return m_impl->sampleFormatInp; }
const GGMorse::SampleFormat & GGMorse::getSampleFormatOut() const { return m_impl->sampleFormatOut; }

const GGMorse::TxRx & GGMorse::getRxData() const {
    return m_impl->rxData;
}

int GGMorse::takeRxData(TxRx & dst) {
    if (m_impl->rxData.size() == 0) return 0;

    dst = std::move(m_impl->rxData);

    return (int) dst.size();
}

int GGMorse::takeSignalF(SignalF & dst) {
    if (m_impl->signalF.size() == 0) return 0;

    dst = std::move(m_impl->signalF);

    return (int) dst.size();
}

int GGMorse::takeTxWaveformI16(WaveformI16 & dst) {
    if (m_impl->txWaveformI16.size() == 0) return false;

    dst = std::move(m_impl->txWaveformI16);

    return (int) dst.size();
}

const GGMorse::Statistics & GGMorse::getStatistics() const { return m_impl->statistics; }
const GGMorse::Spectrogram GGMorse::getSpectrogram() const { return m_impl->stfft.spectrogram(); }
