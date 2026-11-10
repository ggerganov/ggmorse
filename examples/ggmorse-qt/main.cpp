#include "AudioDecoder.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickImageProvider>
#include <QQuickStyle>

class SpectrogramProvider : public QQuickImageProvider {
public:
    explicit SpectrogramProvider(const AudioDecoder & decoder)
        : QQuickImageProvider(QQuickImageProvider::Image), m_decoder(decoder) {}

    QImage requestImage(const QString &, QSize * size, const QSize &) override {
        const QImage image = m_decoder.spectrogramImage();
        if (size) *size = image.size();
        return image;
    }

private:
    const AudioDecoder & m_decoder;
};

int main(int argc, char * argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName("GGMorse");
    app.setOrganizationName("ggerganov");

    QQuickStyle::setStyle("Material");

    AudioDecoder decoder;

    QQmlApplicationEngine engine;
    engine.addImageProvider("spectrogram", new SpectrogramProvider(decoder));
    engine.rootContext()->setContextProperty("decoder", &decoder);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("GGMorse", "Main");

    return app.exec();
}
