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

    WaveformF waveform = WaveformF(kMaxSamplesPerFrame + 128);
    WaveformF waveformResampled = WaveformF(kMaxSamplesPerFrame + 128);
    TxRx waveformTmp = TxRx((kMaxSamplesPerFrame + 128)*sampleSizeBytesInp);
    Spectrogram spectrogram = Spectrogram(0);

    TxRx rxData = {};
    SignalF signalF = {};

    // todo : refactor
    std::vector<std::vector<std::vector<Interval>>> intervalsAll = {};

    STFFT stfft = {};
    Filter filter = {};
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
        550.0f,
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
    m_impl->filter.init(Filter::FirstOrderHighPass, 200.0f, kBaseSampleRate);
    m_impl->goertzelFilter.init(kBaseSampleRate, pow2For50Hz, kMaxWindowToAnalyze_s);
}

GGMorse::~GGMorse() {
}

bool GGMorse::setParametersDecode(const ParametersDecode & parameters) {
    m_impl->parametersDecode = parameters;

    return true;
}

bool GGMorse::setParametersEncode(const ParametersEncode & parameters) {
    m_impl->parametersEncode = parameters;

    return true;
}

bool GGMorse::encode(const CBWaveformOut & /*cbWaveformOut*/) {
    // todo : to be implemented ..

    return true;
}

bool GGMorse::decode(const CBWaveformInp & cbWaveformInp) {
    bool result = false;
    while (m_impl->hasNewTxData == false) {
        auto tStart_us = t_us();

        // read capture data
        float factor = m_impl->sampleRateInp/kBaseSampleRate;
        uint32_t nBytesNeeded = m_impl->samplesNeeded*m_impl->sampleSizeBytesInp;

        if (m_impl->sampleRateInp != kBaseSampleRate) {
            // note : predict 4 extra samples just to make sure we have enough data
            nBytesNeeded = (m_impl->resampler.resample(1.0f/factor,
                                                       m_impl->samplesNeeded,
                                                       m_impl->waveformResampled.data(),
                                                       nullptr) + 4)*m_impl->sampleSizeBytesInp;
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

        //printf("recorded %d samples\n", nSamplesRecorded);

        if (nSamplesRecorded == 0) {
            break;
        }

        uint32_t offset = m_impl->samplesPerFrame - m_impl->samplesNeeded;

        if (m_impl->sampleRateInp != kBaseSampleRate) {
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

            int nSamplesResampled = offset + m_impl->resampler.resample(factor, nSamplesRecorded, m_impl->waveformResampled.data(), m_impl->waveform.data() + offset);
            nSamplesRecorded = nSamplesResampled;
        } else {
            for (int i = 0; i < nSamplesRecorded; ++i) {
                m_impl->waveform[offset + i] = m_impl->waveformResampled[i];
            }
        }

        // we have enough bytes to do analysis
        if (nSamplesRecorded >= m_impl->samplesPerFrame) {
            m_impl->hasNewWaveform = true;
            m_impl->statistics.timeResample_ms = dt_ms(tStart_us);

            decode_float();
            result = true;

            int nExtraSamples = nSamplesRecorded - m_impl->samplesPerFrame;
            for (int i = 0; i < nExtraSamples; ++i) {
                m_impl->waveform[i] = m_impl->waveform[m_impl->samplesPerFrame + i];
            }

            m_impl->samplesNeeded = m_impl->samplesPerFrame - nExtraSamples;
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

    m_impl->filter.process(m_impl->waveform.data(), m_impl->samplesPerFrame);
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

    int nSamples = filteredF.size();
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
            float lendot_ms = 60000.0f/(50.0f*(5 + s));
            float lendot_samples = kBaseSampleRate*(1e-3*lendot_ms)/nDownsample;

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

                int nIntervals = intervals.size();

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

    return dst.size();
}

int GGMorse::takeSignalF(SignalF & dst) {
    if (m_impl->signalF.size() == 0) return 0;

    dst = std::move(m_impl->signalF);

    return dst.size();
}

const GGMorse::Statistics & GGMorse::getStatistics() const { return m_impl->statistics; }
const GGMorse::Spectrogram GGMorse::getSpectrogram() const { return m_impl->stfft.spectrogram(); }
