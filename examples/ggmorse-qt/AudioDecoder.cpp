#include "AudioDecoder.h"

#include "ggmorse/ggmorse.h"

#include <QAudioFormat>
#include <QAudioSource>
#include <QIODevice>
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

constexpr int kSampleRate = 48000;
constexpr int kPollInterval_ms = 10;
constexpr float kIntensityScale = 30.0f;
constexpr float kRangeMargin_hz = 20.0f;

// black -> green -> light green -> white
QRgb spectrogramColor(float v) {
    static constexpr float colors[5][3] = {
        { 0.00f, 0.00f, 0.00f }, { 0.00f, 0.50f, 0.00f }, { 0.00f, 1.00f, 0.00f }, { 0.50f, 1.00f, 0.50f }, { 1.00f, 1.00f, 1.00f },
    };

    v = std::clamp(v, 0.0f, 1.0f)*4.0f;
    const int i0 = std::min(3, (int) v);
    const float t = v - i0;

    auto channel = [&](int c) { return (int) std::lround(255.0f*(colors[i0][c] + t*(colors[i0 + 1][c] - colors[i0][c]))); };
    return qRgb(channel(0), channel(1), channel(2));
}

}

AudioDecoder::AudioDecoder(QObject * parent) : QObject(parent) {
    m_preferredDeviceId = QSettings().value("audio/inputDeviceId").toByteArray();

    m_timer.setInterval(kPollInterval_ms);
    connect(&m_timer, &QTimer::timeout, this, &AudioDecoder::poll);
    connect(&m_mediaDevices, &QMediaDevices::audioInputsChanged, this, &AudioDecoder::refreshDevices);

    refreshDevices();
}

AudioDecoder::~AudioDecoder() {
    stop();
}

void AudioDecoder::refreshDevices() {
    m_devices = QMediaDevices::audioInputs();

    auto indexOf = [this](const QByteArray & id) {
        for (int i = 0; i < m_devices.size(); ++i) {
            if (m_devices[i].id() == id) return i;
        }
        return -1;
    };

    // prefer the remembered device, then the current selection, then the system default
    int index = m_preferredDeviceId.isEmpty() ? -1 : indexOf(m_preferredDeviceId);
    if (index < 0 && !m_currentDeviceId.isEmpty()) index = indexOf(m_currentDeviceId);
    if (index < 0) index = indexOf(m_mediaDevices.defaultAudioInput().id());
    if (index < 0) index = 0;

    m_deviceNames.clear();
    for (const auto & device : m_devices) {
        m_deviceNames << device.description();
    }

    emit devicesChanged();

    if (index != m_deviceIndex) {
        m_deviceIndex = index;
        emit deviceIndexChanged();
    }
    m_currentDeviceId = index < m_devices.size() ? m_devices[index].id() : QByteArray();
}

void AudioDecoder::setDeviceIndex(int index) {
    if (index == m_deviceIndex || index < 0 || index >= m_devices.size()) return;

    m_deviceIndex = index;
    m_currentDeviceId = m_devices[index].id();
    m_preferredDeviceId = m_currentDeviceId;
    QSettings().setValue("audio/inputDeviceId", m_preferredDeviceId);
    emit deviceIndexChanged();

    if (m_running) {
        start();
    }
}

void AudioDecoder::start() {
    stop();
    setError({});

    if (m_deviceIndex < 0 || m_deviceIndex >= m_devices.size()) {
        setError(tr("No audio input device found"));
        return;
    }

    const QAudioDevice & device = m_devices[m_deviceIndex];

    QAudioFormat format;
    format.setSampleRate(kSampleRate);
    format.setChannelCount(1);

    GGMorse::SampleFormat sampleFormat = GGMORSE_SAMPLE_FORMAT_UNDEFINED;
    for (const auto candidate : { QAudioFormat::Float, QAudioFormat::Int16 }) {
        format.setSampleFormat(candidate);
        if (device.isFormatSupported(format)) {
            sampleFormat = candidate == QAudioFormat::Float ? GGMORSE_SAMPLE_FORMAT_F32 : GGMORSE_SAMPLE_FORMAT_I16;
            break;
        }
    }

    if (sampleFormat == GGMORSE_SAMPLE_FORMAT_UNDEFINED) {
        setError(tr("'%1' does not support 48 kHz mono capture").arg(device.description()));
        return;
    }

    m_ggmorse = std::make_unique<GGMorse>(GGMorse::Parameters {
        (float) kSampleRate,
        (float) kSampleRate,
        GGMorse::kDefaultSamplesPerFrame,
        sampleFormat,
        GGMORSE_SAMPLE_FORMAT_I16,
    });
    applySettings();

    m_source = std::make_unique<QAudioSource>(device, format);
    m_io = m_source->start();
    if (m_io == nullptr || m_source->error() != QAudio::NoError) {
        setError(tr("Failed to open '%1'").arg(device.description()));
        m_source.reset();
        m_io = nullptr;
        m_ggmorse.reset();
        return;
    }

    m_buffer.clear();
    m_image = QImage();
    m_frame = 0;
    m_pitchPosition = -1;
    emit spectrogramChanged();
    m_timer.start();

    m_running = true;
    emit runningChanged();
}

void AudioDecoder::stop() {
    m_timer.stop();

    if (m_source) {
        m_source->stop();
    }
    m_source.reset();
    m_io = nullptr;
    m_ggmorse.reset();
    m_buffer.clear();

    if (m_running) {
        m_running = false;
        emit runningChanged();
    }
}

void AudioDecoder::clear() {
    m_text.clear();
    emit textChanged();
}

void AudioDecoder::poll() {
    if (m_io == nullptr || !m_ggmorse) return;

    if (m_source->error() != QAudio::NoError) {
        setError(tr("Audio capture failed"));
        stop();
        return;
    }

    m_buffer.append(m_io->readAll());

    // drop stale audio if processing falls behind
    const qsizetype maxBuffered = 32LL*m_ggmorse->getSamplesPerFrame()*m_ggmorse->getSampleSizeBytesInp()*(kSampleRate/(int) GGMorse::kBaseSampleRate);
    if (m_buffer.size() > 2*maxBuffered) {
        m_buffer.remove(0, m_buffer.size() - maxBuffered);
    }

    GGMorse::CBWaveformInp callback = [this](void * data, uint32_t nMaxBytes) -> uint32_t {
        if (m_buffer.size() < (qsizetype) nMaxBytes) return 0;
        std::memcpy(data, m_buffer.constData(), nMaxBytes);
        m_buffer.remove(0, nMaxBytes);
        return nMaxBytes;
    };

    bool newText = false;
    bool newSpectrogram = false;
    GGMorse::TxRx rx;

    while (true) {
        const auto before = m_buffer.size();
        m_ggmorse->decode(callback);
        if (m_buffer.size() == before) break;

        newSpectrogram = true;

        if (m_ggmorse->takeRxData(rx) > 0) {
            m_text.append(QString::fromLatin1(reinterpret_cast<const char *>(rx.data()), (qsizetype) rx.size()));
            newText = true;
        }
    }

    if (newText) {
        if (m_text.size() > kMaxTextLength) {
            m_text.remove(0, m_text.size() - kMaxTextLength);
        }
        emit textChanged();
    }

    if (newSpectrogram) {
        updateSpectrogram();
    }

    const auto & stats = m_ggmorse->getStatistics();
    if (stats.estimatedPitch_Hz != m_pitch || stats.estimatedSpeed_wpm != m_speed) {
        m_pitch = stats.estimatedPitch_Hz;
        m_speed = stats.estimatedSpeed_wpm;
        emit statsChanged();
    }
}

// time runs along x, frequency along y (low frequencies on top), limited to the decoder range
void AudioDecoder::updateSpectrogram() {
    const auto spectrogram = m_ggmorse->getSpectrogram();
    if (spectrogram.empty() || spectrogram[0].size() < 4) return;

    const int nx = (int) spectrogram.size();
    const int nBins = (int) spectrogram[0].size()/2;
    const float df = 0.5f*GGMorse::kBaseSampleRate/nBins;

    const int binMin = std::clamp((int) (std::max(0.0f, (float) m_rangeMin - kRangeMargin_hz)/df), 0, nBins - 2);
    const int binMax = std::clamp((int) (((float) m_rangeMax + kRangeMargin_hz)/df), binMin + 1, (int) spectrogram[0].size());
    const int ny = binMax - binMin;

    float mean = 0.0f;
    for (int i = 0; i < nx; ++i) {
        for (int j = 0; j < ny; ++j) {
            mean += spectrogram[i][binMin + j];
        }
    }
    mean /= (float) (nx*ny);
    if (mean == 0.0f) mean = 1.0f;
    const float scale = 1.0f/(kIntensityScale*mean);

    QImage image(nx, ny, QImage::Format_RGB32);
    for (int j = 0; j < ny; ++j) {
        auto * row = reinterpret_cast<QRgb *>(image.scanLine(j));
        for (int i = 0; i < nx; ++i) {
            const float v = spectrogram[i][binMin + j]*scale;
            row[i] = spectrogramColor(v > 0.05f ? v : 0.0f);
        }
    }

    m_image = std::move(image);
    m_pitchPosition = -1;
    const float pitch = m_ggmorse->getStatistics().estimatedPitch_Hz;
    if (pitch/df >= binMin && pitch/df < binMax) {
        m_pitchPosition = (pitch/df - binMin)/ny;
    }

    ++m_frame;
    emit spectrogramChanged();
}

void AudioDecoder::applySettings() {
    if (!m_ggmorse) return;

    GGMorse::ParametersDecode parameters = GGMorse::getDefaultParametersDecode();
    parameters.frequency_hz = m_frequency > 0 ? (float) m_frequency : -1.0f;
    parameters.speed_wpm = m_wpm > 0 ? (float) m_wpm : -1.0f;
    parameters.frequencyRangeMin_hz = (float) m_rangeMin;
    parameters.frequencyRangeMax_hz = (float) m_rangeMax;
    parameters.applyFilterHighPass = m_highPass;
    parameters.applyFilterLowPass = m_lowPass;

    m_ggmorse->setParametersDecode(parameters);
}

void AudioDecoder::setError(const QString & error) {
    if (error == m_error) return;
    m_error = error;
    emit errorChanged();
}

void AudioDecoder::setFrequency(qreal value) {
    if (value == m_frequency) return;
    m_frequency = value;
    applySettings();
    emit settingsChanged();
}

void AudioDecoder::setWpm(qreal value) {
    if (value == m_wpm) return;
    m_wpm = value;
    applySettings();
    emit settingsChanged();
}

void AudioDecoder::setRangeMin(qreal value) {
    if (value == m_rangeMin) return;
    m_rangeMin = value;
    applySettings();
    emit settingsChanged();
}

void AudioDecoder::setRangeMax(qreal value) {
    if (value == m_rangeMax) return;
    m_rangeMax = value;
    applySettings();
    emit settingsChanged();
}

void AudioDecoder::setHighPass(bool value) {
    if (value == m_highPass) return;
    m_highPass = value;
    applySettings();
    emit settingsChanged();
}

void AudioDecoder::setLowPass(bool value) {
    if (value == m_lowPass) return;
    m_lowPass = value;
    applySettings();
    emit settingsChanged();
}
