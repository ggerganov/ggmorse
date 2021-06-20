#ifndef GGMORSE_H
#define GGMORSE_H

#ifdef GGMORSE_SHARED
#    ifdef _WIN32
#        ifdef GGMORSE_BUILD
#            define GGMORSE_API __declspec(dllexport)
#        else
#            define GGMORSE_API __declspec(dllimport)
#        endif
#    else
#        define GGMORSE_API __attribute__ ((visibility ("default")))
#    endif
#else
#    define GGMORSE_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

    //
    // C interface
    //

    // Data format of the audio samples
    typedef enum {
        GGMORSE_SAMPLE_FORMAT_UNDEFINED,
        GGMORSE_SAMPLE_FORMAT_U8,
        GGMORSE_SAMPLE_FORMAT_I8,
        GGMORSE_SAMPLE_FORMAT_U16,
        GGMORSE_SAMPLE_FORMAT_I16,
        GGMORSE_SAMPLE_FORMAT_F32,
    } ggmorse_SampleFormat;

    typedef struct {
        float sampleRateInp;                    // capture sample rate
        float sampleRateOut;                    // playback sample rate
        int samplesPerFrame;                    // number of samples per audio frame
        ggmorse_SampleFormat sampleFormatInp;   // format of the captured audio samples
        ggmorse_SampleFormat sampleFormatOut;   // format of the playback audio samples
    } ggmorse_Parameters;

    typedef struct {
        float frequency_hz;
        float speed_wpm;
    } ggmorse_ParametersDecode;

    typedef struct {
        float volume;
        float frequency_hz;
        float speedCharacters_wpm;
        float speedFarnsworth_wpm;
    } ggmorse_ParametersEncode;

    typedef struct {
        float timeResample_ms;
        float timePitchDetection_ms;
        float timeGoertzel_ms;
        float timeFrameAnalysis_ms;
        float estimatedPitch_Hz;
        float estimatedSpeed_wpm;
        float signalThreshold;
    } ggmorse_Statistics;

#ifdef __cplusplus
}

//
// C++ interface
//

#include <memory>
#include <vector>
#include <functional>

class GGMorse {
public:
    static constexpr auto kBaseSampleRate = 8000.0f;
    static constexpr auto kDefaultSamplesPerFrame = 512;
    static constexpr auto kMaxSamplesPerFrame = 2048;
    static constexpr auto kDefaultVolume = 10;
    static constexpr auto kMaxWindowToAnalyze_s = 3.0f;
    static constexpr auto kMaxTxLength = 256;

    using Parameters        = ggmorse_Parameters;
    using ParametersDecode  = ggmorse_ParametersDecode;
    using ParametersEncode  = ggmorse_ParametersEncode;
    using Statistics        = ggmorse_Statistics;
    using SampleFormat      = ggmorse_SampleFormat;

    using WaveformF   = std::vector<float>;
    using WaveformI16 = std::vector<int16_t>;
    using TxRx        = std::vector<std::uint8_t>;
    using Spectrogram = std::vector<std::vector<float>>;
    using SignalF     = std::vector<float>;

    using CBWaveformOut = std::function<void(const void * data, uint32_t nBytes)>;
    using CBWaveformInp = std::function<uint32_t(void * data, uint32_t nMaxBytes)>;

    GGMorse(const Parameters & parameters);
    ~GGMorse();

    static const Parameters & getDefaultParameters();
    static const ParametersDecode & getDefaultParametersDecode();
    static const ParametersEncode & getDefaultParametersEncode();

    bool init(int dataSize, const char * dataBuffer);

    bool setParametersDecode(const ParametersDecode & parameters);
    bool setParametersEncode(const ParametersEncode & parameters);

    uint32_t encodeSize_bytes() const;
    uint32_t encodeSize_samples() const;

    bool encode(const CBWaveformOut & cbWaveformOut);
    bool decode(const CBWaveformInp & cbWaveformInp);

    // instance state
    const bool & hasTxData() const;
    const bool & lastDecodeResult() const;

    const int & getSamplesPerFrame() const;
    const int & getSampleSizeBytesInp() const;
    const int & getSampleSizeBytesOut() const;

    const float & getSampleRateInp() const;
    const float & getSampleRateOut() const;
    const SampleFormat & getSampleFormatInp() const;
    const SampleFormat & getSampleFormatOut() const;

    const TxRx & getRxData() const;

    int takeRxData(TxRx & dst);
    int takeSignalF(SignalF & dst);

    const Statistics & getStatistics() const;
    const Spectrogram getSpectrogram() const;

private:
    void decode_float();

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif

#endif
