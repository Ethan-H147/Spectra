#include "qt/spectra_controller.h"
#include "qt/spectrum_item.h"

extern "C" {
#include "audio/audio_engine.h"
#include "dsp/dsp_types.h"
}

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QUrl>
#include <qqml.h>

#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

#define CHECK(condition, message) \
    do { \
        if (!(condition)) { \
            std::fprintf(stderr, "FAIL: %s\n", message); \
            return 1; \
        } \
    } while (0)

namespace {

constexpr double kPi = 3.14159265358979323846;

template <typename Predicate>
bool waitFor(Predicate predicate, int timeoutMilliseconds = 30000) {
    QElapsedTimer timer;
    timer.start();
    while (!predicate()) {
        if (timer.elapsed() > timeoutMilliseconds) {
            return false;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    return true;
}

bool writeStereoTone(const QString &path) {
    const unsigned int sampleRate = 44100U;
    const size_t frameCount = sampleRate * 2U;
    std::vector<float> samples(frameCount * 2U);
    for (size_t frame = 0U; frame < frameCount; ++frame) {
        const double time =
            static_cast<double>(frame) / sampleRate;
        samples[frame * 2U] =
            static_cast<float>(0.5 * std::sin(2.0 * kPi * 440.0 * time));
        samples[frame * 2U + 1U] =
            static_cast<float>(0.3 * std::sin(2.0 * kPi * 1000.0 * time));
    }
    InterleavedBuffer buffer = {};
    buffer.samples = samples.data();
    buffer.frame_count = frameCount;
    buffer.sample_rate = sampleRate;
    buffer.channel_count = 2U;
    return export_interleaved_to_wav(
        path.toUtf8().constData(), &buffer);
}

const char *kBindingQml = R"(
import QtQuick
import Spectra.Native

Item {
    property alias original: originalItem
    property alias transformed: transformedItem
    property alias analysis: analysisItem
    readonly property int originalLength: spectra.eqOriginalSpectrum.length
    readonly property real secondValue:
        spectra.eqOriginalSpectrum.length > 1
            ? spectra.eqOriginalSpectrum[1] : 0

    SpectrumItem { id: originalItem; spectrum: spectra.eqOriginalSpectrum }
    SpectrumItem { id: transformedItem; spectrum: spectra.eqTransformedSpectrum }
    SpectrumItem { id: analysisItem; spectrum: spectra.analysisSpectrum }
}
)";

}  // namespace

int main(int argc, char **argv) {
    QGuiApplication application(argc, argv);
    // Keep the developer's real application settings untouched.
    QCoreApplication::setOrganizationName(
        QStringLiteral("Spectra Tests"));
    QCoreApplication::setApplicationName(
        QStringLiteral("controller_tests"));
    qmlRegisterType<SpectrumItem>(
        "Spectra.Native", 1, 0, "SpectrumItem");

    QTemporaryDir directory;
    CHECK(directory.isValid(), "test should create a temporary directory");
    const QString wavPath = directory.filePath(QStringLiteral("tone.wav"));
    CHECK(writeStereoTone(wavPath), "test should write a stereo WAV");

    SpectraController controller;
    QQmlEngine engine;
    engine.rootContext()->setContextProperty(
        QStringLiteral("spectra"), &controller);
    QQmlComponent component(&engine);
    component.setData(kBindingQml, QUrl());
    std::unique_ptr<QObject> root(component.create());
    if (root == nullptr) {
        std::fprintf(
            stderr,
            "%s\n",
            component.errorString().toUtf8().constData());
    }
    CHECK(root != nullptr, "binding component should load");

    controller.importAudioFile(QUrl::fromLocalFile(wavPath));
    CHECK(
        waitFor([&] {
            return controller.sourceLoaded() &&
                   !controller.sourceImporting() &&
                   !controller.eqOriginalSpectrum().isEmpty() &&
                   !controller.analysisSpectrum().isEmpty();
        }),
        "import should load the source and both spectra");
    CHECK(controller.sourceChannels() == 2,
          "import should preserve the stereo layout");

    const QList<qreal> original = controller.eqOriginalSpectrum();
    CHECK(root->property("originalLength").toInt() == original.size(),
          "QML should see the whole spectrum sequence");
    CHECK(
        std::abs(root->property("secondValue").toDouble() - original[1]) <
            1.0e-9,
        "QML indexing should read spectrum values");

    auto *originalItem =
        root->property("original").value<SpectrumItem *>();
    auto *transformedItem =
        root->property("transformed").value<SpectrumItem *>();
    auto *analysisItem =
        root->property("analysis").value<SpectrumItem *>();
    CHECK(originalItem != nullptr && transformedItem != nullptr &&
              analysisItem != nullptr,
          "spectrum items should be reachable");
    CHECK(originalItem->spectrum() == original &&
              originalItem->cacheBytes() > 0U,
          "bound spectrum item should receive and cache the spectrum");
    CHECK(analysisItem->spectrum() == controller.analysisSpectrum() &&
              analysisItem->cacheBytes() > 0U,
          "analysis spectrum should reach its item");

    controller.addEqBand(800.0, 1500.0, -18.0);
    const QList<qreal> preview = controller.eqTransformedSpectrum();
    CHECK(preview.size() == original.size(),
          "EQ preview should match the source spectrum size");
    CHECK(transformedItem->spectrum() == preview,
          "EQ preview should reach its bound item immediately");
    const qsizetype cutBin = static_cast<qsizetype>(
        1000.0 / controller.eqSpectrumMaximumFrequency() *
        static_cast<double>(original.size() - 1));
    CHECK(preview[cutBin] < original[cutBin] - 6.0,
          "EQ preview should cut the band it covers");

    controller.setEffectMode(1);
    controller.setEffectSemitones(12.0);
    controller.buildEffect();
    CHECK(
        waitFor([&] {
            return !controller.effectProcessing() &&
                   controller.effectReady();
        }),
        "phase-vocoder pitch shift should finish through the controller");
    CHECK(std::abs(controller.effectOutputDuration() - 2.0) < 0.01 &&
              controller.effectOutputChannels() == 2,
          "pitch shift should keep the duration and channels");

    controller.buildEq();
    CHECK(
        waitFor([&] {
            return !controller.eqProcessing() && controller.eqReady();
        }),
        "EQ render should finish through the controller");

    std::puts("All controller tests passed.");
    return 0;
}
