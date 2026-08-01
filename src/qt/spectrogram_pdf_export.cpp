#include "qt/spectrogram_pdf_export.h"

#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>

#include <algorithm>
#include <cmath>

namespace {
QString durationLabel(double seconds) {
    const int total = std::max(
        0, static_cast<int>(std::round(seconds)));
    const int hours = total / 3600;
    const int minutes = (total % 3600) / 60;
    const int remaining = total % 60;
    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(remaining, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2")
        .arg(minutes)
        .arg(remaining, 2, 10, QLatin1Char('0'));
}

QString frequencyLabel(double frequency) {
    if (frequency >= 1000.0) {
        return QStringLiteral("%1 kHz")
            .arg(frequency / 1000.0, 0, 'f', 1);
    }
    return QStringLiteral("%1 Hz")
        .arg(frequency, 0, 'f', 0);
}

QString groupedInteger(qsizetype value) {
    return QStringLiteral("%L1").arg(value);
}
}  // namespace

bool exportSpectrogramLandscapePdf(
    const QString &path,
    const QImage &spectrogram,
    const SpectrogramPdfMetadata &metadata) {
    if (path.isEmpty() || spectrogram.isNull() ||
        metadata.durationSeconds <= 0.0 ||
        metadata.maximumFrequency <= 0.0) {
        return false;
    }

    QPdfWriter writer(path);
    writer.setCreator(QStringLiteral("Spectra - Fourier Audio Lab"));
    writer.setTitle(QStringLiteral("Spectrogram - %1")
                        .arg(metadata.sourceName));
    writer.setResolution(144);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageOrientation(QPageLayout::Landscape);
    writer.setPageMargins(
        QMarginsF(12.0, 12.0, 12.0, 12.0),
        QPageLayout::Millimeter);

    QPainter painter(&writer);
    if (!painter.isActive()) {
        return false;
    }
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QColor ink(28, 31, 39);
    const QColor muted(92, 99, 113);
    const QColor border(205, 210, 219);
    const QColor accent(36, 132, 242);
    const QRectF page(0, 0, writer.width(), writer.height());
    painter.fillRect(page, Qt::white);

    QFont heading(QStringLiteral("Segoe UI"));
    heading.setPointSizeF(21.0);
    heading.setWeight(QFont::DemiBold);
    painter.setFont(heading);
    painter.setPen(ink);
    painter.drawText(
        QRectF(0, 0, page.width() * 0.62, 54),
        Qt::AlignLeft | Qt::AlignVCenter,
        QStringLiteral("Spectrogram"));

    QFont body(QStringLiteral("Segoe UI"));
    body.setPointSizeF(9.0);
    painter.setFont(body);
    painter.setPen(muted);
    const QString source = metadata.sourceName.isEmpty()
        ? QStringLiteral("Untitled audio")
        : metadata.sourceName;
    const QString sourceLabel = QFontMetrics(body).elidedText(
        source,
        Qt::ElideMiddle,
        static_cast<int>(page.width() * 0.58));
    painter.drawText(
        QRectF(0, 48, page.width() * 0.62, 30),
        Qt::AlignLeft | Qt::AlignVCenter,
        sourceLabel);

    const QString sourceDetails = QStringLiteral(
        "%1  |  %2 Hz  |  %3")
        .arg(durationLabel(metadata.durationSeconds))
        .arg(metadata.sampleRate)
        .arg(metadata.channelCount == 1
            ? QStringLiteral("Mono")
            : QStringLiteral("%1 channels")
                .arg(metadata.channelCount));
    painter.drawText(
        QRectF(page.width() * 0.58, 10,
               page.width() * 0.42, 44),
        Qt::AlignRight | Qt::AlignVCenter,
        sourceDetails);
    painter.setPen(accent);
    painter.drawLine(QPointF(0, 84), QPointF(page.width(), 84));

    const qreal leftAxis = 86;
    const qreal rightInset = 24;
    const qreal plotTop = 118;
    const qreal plotBottom = page.height() - 248;
    const QRectF plot(
        leftAxis,
        plotTop,
        page.width() - leftAxis - rightInset,
        plotBottom - plotTop);
    painter.fillRect(plot, QColor(14, 16, 28));
    painter.drawImage(plot, spectrogram);

    painter.save();
    painter.setClipRect(plot);
    QPen gridPen(QColor(255, 255, 255, 52));
    gridPen.setWidthF(1.0);
    painter.setPen(gridPen);
    for (int index = 1; index < 4; ++index) {
        const qreal fraction = static_cast<qreal>(index) / 4.0;
        const qreal x = plot.left() + plot.width() * fraction;
        const qreal y = plot.top() + plot.height() * fraction;
        painter.drawLine(QPointF(x, plot.top()),
                         QPointF(x, plot.bottom()));
        painter.drawLine(QPointF(plot.left(), y),
                         QPointF(plot.right(), y));
    }
    painter.restore();
    painter.setPen(QPen(QColor(72, 78, 91), 1.2));
    painter.drawRect(plot);

    QFont axisFont(QStringLiteral("Segoe UI"));
    axisFont.setPointSizeF(8.0);
    painter.setFont(axisFont);
    painter.setPen(muted);
    for (int index = 0; index <= 4; ++index) {
        const qreal fraction = static_cast<qreal>(index) / 4.0;
        const qreal x = plot.left() + plot.width() * fraction;
        const qreal y = plot.bottom() - plot.height() * fraction;
        painter.drawText(
            QRectF(x - 62, plot.bottom() + 10, 124, 24),
            Qt::AlignHCenter | Qt::AlignTop,
            durationLabel(metadata.durationSeconds * fraction));
        painter.drawText(
            QRectF(0, y - 12, leftAxis - 12, 24),
            Qt::AlignRight | Qt::AlignVCenter,
            frequencyLabel(metadata.maximumFrequency * fraction));
    }

    QFont labelFont(QStringLiteral("Segoe UI"));
    labelFont.setPointSizeF(9.0);
    labelFont.setWeight(QFont::DemiBold);
    painter.setFont(labelFont);
    painter.setPen(ink);
    painter.drawText(
        QRectF(plot.left(), plot.bottom() + 40,
               plot.width(), 28),
        Qt::AlignHCenter | Qt::AlignVCenter,
        QStringLiteral("Time"));
    painter.save();
    painter.translate(18, plot.center().y());
    painter.rotate(-90);
    painter.drawText(
        QRectF(-plot.height() / 2.0, -15,
               plot.height(), 30),
        Qt::AlignCenter,
        QStringLiteral("Frequency"));
    painter.restore();

    const QRectF legend(
        plot.left(), plot.bottom() + 82, 260, 16);
    QLinearGradient gradient(legend.topLeft(), legend.topRight());
    gradient.setColorAt(0.00, QColor(14, 16, 28));
    gradient.setColorAt(0.25, QColor(34, 72, 144));
    gradient.setColorAt(0.50, QColor(114, 69, 173));
    gradient.setColorAt(0.75, QColor(221, 87, 77));
    gradient.setColorAt(1.00, QColor(250, 207, 83));
    painter.fillRect(legend, gradient);
    painter.setPen(border);
    painter.drawRect(legend);
    painter.setFont(axisFont);
    painter.setPen(muted);
    painter.drawText(
        QRectF(legend.left(), legend.bottom() + 4, 70, 20),
        Qt::AlignLeft | Qt::AlignTop,
        QStringLiteral("-100 dB"));
    painter.drawText(
        QRectF(legend.right() - 70, legend.bottom() + 4, 70, 20),
        Qt::AlignRight | Qt::AlignTop,
        QStringLiteral("0 dB"));

    const QString analysisDetails = QStringLiteral(
        "Window %1  |  Hop %2  |  Frames %3  |  Maximum %4")
        .arg(metadata.windowSize)
        .arg(metadata.hopSize)
        .arg(groupedInteger(metadata.frameCount))
        .arg(frequencyLabel(metadata.maximumFrequency));
    painter.setFont(body);
    painter.setPen(ink);
    painter.drawText(
        QRectF(legend.right() + 40, plot.bottom() + 76,
               page.width() - legend.right() - 40, 32),
        Qt::AlignRight | Qt::AlignVCenter,
        analysisDetails);

    painter.setPen(border);
    painter.drawLine(
        QPointF(0, page.height() - 44),
        QPointF(page.width(), page.height() - 44));
    painter.setFont(axisFont);
    painter.setPen(muted);
    painter.drawText(
        QRectF(0, page.height() - 34, page.width() * 0.5, 24),
        Qt::AlignLeft | Qt::AlignVCenter,
        QStringLiteral("Spectra - Fourier Audio Lab"));
    painter.drawText(
        QRectF(page.width() * 0.5, page.height() - 34,
               page.width() * 0.5, 24),
        Qt::AlignRight | Qt::AlignVCenter,
        QStringLiteral("Page 1"));

    painter.end();
    return QFileInfo(path).exists() && QFileInfo(path).size() > 0;
}
