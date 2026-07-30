#include "qt/spectra_controller.h"
#include "qt/spectrum_item.h"

extern "C" {
#include "platform/windows_app.h"
}

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <qqml.h>
#include <QQuickStyle>

int main(int argc, char *argv[]) {
    windows_app_prepare_process();
    QGuiApplication::setApplicationName(QStringLiteral("Spectra"));
    QGuiApplication::setOrganizationName(QStringLiteral("Fourier Audio Lab"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QGuiApplication application(argc, argv);
    application.setWindowIcon(QIcon(QStringLiteral(":/qt/qml/Spectra/assets/spectra-icon.png")));
    qmlRegisterType<SpectrumItem>(
        "Spectra.Native",
        1,
        0,
        "SpectrumItem");

    SpectraController controller;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("spectra"), &controller);
    engine.loadFromModule(QStringLiteral("Spectra"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    return application.exec();
}
