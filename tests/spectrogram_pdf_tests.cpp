#include "qt/spectrogram_pdf_export.h"

#include <QGuiApplication>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>

#include <cmath>
#include <cstdio>

namespace {
QColor colorForLevel(double level) {
    const QColor stops[] = {
        QColor(14, 16, 28),
        QColor(34, 72, 144),
        QColor(114, 69, 173),
        QColor(221, 87, 77),
        QColor(250, 207, 83),
    };
    const double scaled = std::max(0.0, std::min(1.0, level)) * 4.0;
    const int first = std::min(3, static_cast<int>(scaled));
    const double fraction = scaled - static_cast<double>(first);
    return QColor(
        static_cast<int>(stops[first].red() * (1.0 - fraction)
            + stops[first + 1].red() * fraction),
        static_cast<int>(stops[first].green() * (1.0 - fraction)
            + stops[first + 1].green() * fraction),
        static_cast<int>(stops[first].blue() * (1.0 - fraction)
            + stops[first + 1].blue() * fraction));
}
}

int main(int argc, char *argv[]) {
    QGuiApplication application(argc, argv);
    QTemporaryDir temporary;
    if (!temporary.isValid()) {
        std::fprintf(stderr, "Could not create a temporary directory\n");
        return 1;
    }
    const QString outputPath = argc > 1
        ? QString::fromLocal8Bit(argv[1])
        : temporary.filePath(QStringLiteral("spectrogram.pdf"));

    QImage image(256, 128, QImage::Format_RGB32);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const double ridge = 0.72
                + 0.12 * std::sin(static_cast<double>(x) * 0.08);
            const double normalizedY = 1.0
                - static_cast<double>(y) / image.height();
            const double distance = std::abs(normalizedY - ridge);
            const double level = std::max(
                0.02,
                std::exp(-distance * 34.0)
                    * (0.55 + 0.45 * std::sin(x * 0.15)
                        * std::sin(x * 0.15)));
            image.setPixelColor(x, y, colorForLevel(level));
        }
    }

    const SpectrogramPdfMetadata metadata = {
        QStringLiteral("Andante spianato et grande polonaise brillante.wav"),
        840.07,
        48000,
        2,
        12000.0,
        2048,
        512,
        1576,
    };
    if (!exportSpectrogramLandscapePdf(
            outputPath, image, metadata)) {
        std::fprintf(stderr, "PDF export failed\n");
        return 1;
    }

    QFile pdf(outputPath);
    if (!pdf.open(QIODevice::ReadOnly)) {
        std::fprintf(stderr, "Could not reopen exported PDF\n");
        return 1;
    }
    const QByteArray prefix = pdf.read(5);
    if (prefix != QByteArrayLiteral("%PDF-") || pdf.size() < 5000) {
        std::fprintf(stderr, "Exported file is not a valid PDF\n");
        return 1;
    }
    std::printf("spectrogram PDF export test passed: %s\n",
                qPrintable(outputPath));
    return 0;
}
