#pragma once

#include <QAudioDevice>
#include <QByteArray>
#include <QImage>
#include <QList>
#include <QMediaDevices>
#include <QObject>
#include <QStringList>
#include <QTimer>

#include <memory>

class GGMorse;
class QAudioSource;
class QIODevice;

// Bridges the ggmorse decoder with Qt Multimedia audio capture and exposes its state to QML
class AudioDecoder : public QObject {
    Q_OBJECT

    Q_PROPERTY(QStringList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(int deviceIndex READ deviceIndex WRITE setDeviceIndex NOTIFY deviceIndexChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QString text READ text NOTIFY textChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(int frame READ frame NOTIFY spectrogramChanged)
    Q_PROPERTY(qreal pitchPosition READ pitchPosition NOTIFY spectrogramChanged)
    Q_PROPERTY(qreal pitch READ pitch NOTIFY statsChanged)
    Q_PROPERTY(qreal speed READ speed NOTIFY statsChanged)

    // Decoder settings. A frequency or wpm <= 0 means "detect automatically"
    Q_PROPERTY(qreal frequency READ frequency WRITE setFrequency NOTIFY settingsChanged)
    Q_PROPERTY(qreal wpm READ wpm WRITE setWpm NOTIFY settingsChanged)
    Q_PROPERTY(qreal rangeMin READ rangeMin WRITE setRangeMin NOTIFY settingsChanged)
    Q_PROPERTY(qreal rangeMax READ rangeMax WRITE setRangeMax NOTIFY settingsChanged)
    Q_PROPERTY(bool highPass READ highPass WRITE setHighPass NOTIFY settingsChanged)
    Q_PROPERTY(bool lowPass READ lowPass WRITE setLowPass NOTIFY settingsChanged)

public:
    explicit AudioDecoder(QObject * parent = nullptr);
    ~AudioDecoder() override;

    QStringList devices() const { return m_deviceNames; }
    int deviceIndex() const { return m_deviceIndex; }
    bool running() const { return m_running; }
    QString text() const { return m_text; }
    QString error() const { return m_error; }
    int frame() const { return m_frame; }
    qreal pitchPosition() const { return m_pitchPosition; }
    QImage spectrogramImage() const { return m_image; }
    qreal pitch() const { return m_pitch; }
    qreal speed() const { return m_speed; }

    qreal frequency() const { return m_frequency; }
    qreal wpm() const { return m_wpm; }
    qreal rangeMin() const { return m_rangeMin; }
    qreal rangeMax() const { return m_rangeMax; }
    bool highPass() const { return m_highPass; }
    bool lowPass() const { return m_lowPass; }

    void setDeviceIndex(int index);
    void setFrequency(qreal value);
    void setWpm(qreal value);
    void setRangeMin(qreal value);
    void setRangeMax(qreal value);
    void setHighPass(bool value);
    void setLowPass(bool value);

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void clear();
    Q_INVOKABLE void refreshDevices();

signals:
    void devicesChanged();
    void deviceIndexChanged();
    void runningChanged();
    void textChanged();
    void errorChanged();
    void spectrogramChanged();
    void statsChanged();
    void settingsChanged();

private:
    void poll();
    void applySettings();
    void updateSpectrogram();
    void setError(const QString & error);

    static constexpr int kMaxTextLength = 20000;

    QMediaDevices m_mediaDevices;
    QList<QAudioDevice> m_devices;
    QStringList m_deviceNames;
    int m_deviceIndex = 0;
    QByteArray m_currentDeviceId;
    QByteArray m_preferredDeviceId;

    std::unique_ptr<QAudioSource> m_source;
    QIODevice * m_io = nullptr;
    std::unique_ptr<GGMorse> m_ggmorse;
    QTimer m_timer;
    QByteArray m_buffer;

    bool m_running = false;
    QString m_text;
    QString m_error;
    QImage m_image;
    int m_frame = 0;
    qreal m_pitchPosition = -1;
    qreal m_pitch = 0;
    qreal m_speed = 0;

    qreal m_frequency = -1;
    qreal m_wpm = -1;
    qreal m_rangeMin = 200;
    qreal m_rangeMax = 1200;
    bool m_highPass = true;
    bool m_lowPass = true;
};
