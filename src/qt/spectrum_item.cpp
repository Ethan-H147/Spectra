#include "qt/spectrum_item.h"

#include <QSGClipNode>
#include <QSGFlatColorMaterial>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr int kPeakSegments = 12;
constexpr float kPeakRadius = 4.5f;
constexpr float kPi = 3.14159265358979323846f;

QSGGeometryNode *geometryNode(
    const std::vector<QPointF> &points,
    QSGGeometry::DrawingMode mode,
    const QColor &color,
    float lineWidth = 1.0f) {
    if (points.empty()) {
        return nullptr;
    }

    auto *geometry = new QSGGeometry(
        QSGGeometry::defaultAttributes_Point2D(),
        static_cast<int>(points.size()));
    geometry->setDrawingMode(mode);
    geometry->setLineWidth(lineWidth);
    QSGGeometry::Point2D *vertices = geometry->vertexDataAsPoint2D();
    for (std::size_t index = 0U; index < points.size(); ++index) {
        vertices[index].set(
            static_cast<float>(points[index].x()),
            static_cast<float>(points[index].y()));
    }

    auto *material = new QSGFlatColorMaterial();
    material->setColor(color);
    auto *node = new QSGGeometryNode();
    node->setGeometry(geometry);
    node->setFlag(QSGNode::OwnsGeometry);
    node->setMaterial(material);
    node->setFlag(QSGNode::OwnsMaterial);
    return node;
}

}  // namespace

SpectrumItem::SpectrumItem(QQuickItem *parent)
    : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
}

QVariantList SpectrumItem::spectrum() const {
    QVariantList result;
    result.reserve(static_cast<qsizetype>(cache_.values().size()));
    for (float value : cache_.values()) {
        result.append(value);
    }
    return result;
}

void SpectrumItem::setSpectrum(const QVariantList &spectrum) {
    std::vector<float> values;
    values.reserve(static_cast<std::size_t>(spectrum.size()));
    for (const QVariant &value : spectrum) {
        values.push_back(value.toFloat());
    }
    if (values == cache_.values()) {
        return;
    }
    cache_.build(values);
    emit spectrumChanged();
    update();
}

QVariantList SpectrumItem::peaks() const {
    QVariantList result;
    result.reserve(static_cast<qsizetype>(peaks_.size()));
    for (const PeakPoint &peak : peaks_) {
        QVariantMap value;
        value.insert(QStringLiteral("frequency"), peak.frequency);
        value.insert(QStringLiteral("db"), peak.db);
        result.append(value);
    }
    return result;
}

void SpectrumItem::setPeaks(const QVariantList &peaks) {
    std::vector<PeakPoint> parsed;
    parsed.reserve(static_cast<std::size_t>(peaks.size()));
    for (const QVariant &value : peaks) {
        const QVariantMap peak = value.toMap();
        parsed.push_back({
            peak.value(QStringLiteral("frequency")).toFloat(),
            peak.value(QStringLiteral("db")).toFloat(),
        });
    }
    peaks_ = std::move(parsed);
    emit peaksChanged();
    update();
}

qreal SpectrumItem::spectrumMaximumFrequency() const {
    return spectrumMaximumFrequency_;
}

void SpectrumItem::setSpectrumMaximumFrequency(qreal frequency) {
    updateViewValue(spectrumMaximumFrequency_, frequency);
}

qreal SpectrumItem::minimumFrequency() const {
    return minimumFrequency_;
}

void SpectrumItem::setMinimumFrequency(qreal frequency) {
    updateViewValue(minimumFrequency_, frequency);
}

qreal SpectrumItem::maximumFrequency() const {
    return maximumFrequency_;
}

void SpectrumItem::setMaximumFrequency(qreal frequency) {
    updateViewValue(maximumFrequency_, frequency);
}

qreal SpectrumItem::minimumDb() const {
    return minimumDb_;
}

void SpectrumItem::setMinimumDb(qreal db) {
    updateViewValue(minimumDb_, db);
}

qreal SpectrumItem::maximumDb() const {
    return maximumDb_;
}

void SpectrumItem::setMaximumDb(qreal db) {
    updateViewValue(maximumDb_, db);
}

qreal SpectrumItem::plotX() const {
    return plotX_;
}

void SpectrumItem::setPlotX(qreal value) {
    if (!qFuzzyCompare(plotX_, value)) {
        plotX_ = value;
        emit plotChanged();
        update();
    }
}

qreal SpectrumItem::plotY() const {
    return plotY_;
}

void SpectrumItem::setPlotY(qreal value) {
    if (!qFuzzyCompare(plotY_, value)) {
        plotY_ = value;
        emit plotChanged();
        update();
    }
}

qreal SpectrumItem::plotWidth() const {
    return plotWidth_;
}

void SpectrumItem::setPlotWidth(qreal value) {
    if (!qFuzzyCompare(plotWidth_, value)) {
        plotWidth_ = value;
        emit plotChanged();
        update();
    }
}

qreal SpectrumItem::plotHeight() const {
    return plotHeight_;
}

void SpectrumItem::setPlotHeight(qreal value) {
    if (!qFuzzyCompare(plotHeight_, value)) {
        plotHeight_ = value;
        emit plotChanged();
        update();
    }
}

QColor SpectrumItem::lineColor() const {
    return lineColor_;
}

void SpectrumItem::setLineColor(const QColor &color) {
    if (lineColor_ != color) {
        lineColor_ = color;
        emit colorsChanged();
        update();
    }
}

QColor SpectrumItem::peakColor() const {
    return peakColor_;
}

void SpectrumItem::setPeakColor(const QColor &color) {
    if (peakColor_ != color) {
        peakColor_ = color;
        emit colorsChanged();
        update();
    }
}

qulonglong SpectrumItem::cacheBytes() const {
    return static_cast<qulonglong>(cache_.estimatedBytes());
}

QSGNode *SpectrumItem::updatePaintNode(
    QSGNode *oldNode,
    UpdatePaintNodeData *data) {
    Q_UNUSED(data)
    delete oldNode;

    auto *root = new QSGNode();
    const std::vector<float> &values = cache_.values();
    if (values.size() < 2U || plotWidth_ <= 0.0 ||
        plotHeight_ <= 0.0 ||
        maximumFrequency_ <= minimumFrequency_ ||
        maximumDb_ <= minimumDb_) {
        return root;
    }

    auto *clip = new QSGClipNode();
    clip->setIsRectangular(true);
    clip->setClipRect(
        QRectF(plotX_, plotY_, plotWidth_, plotHeight_));
    root->appendChildNode(clip);

    const qreal safeSpectrumMaximum =
        std::max<qreal>(spectrumMaximumFrequency_, 0.0001);
    const qreal firstPosition = std::clamp(
        minimumFrequency_ / safeSpectrumMaximum,
        0.0,
        1.0);
    const qreal lastPosition = std::clamp(
        maximumFrequency_ / safeSpectrumMaximum,
        0.0,
        1.0);
    const std::size_t lastValueIndex = values.size() - 1U;
    const std::size_t firstIndex = std::min(
        lastValueIndex,
        static_cast<std::size_t>(
            std::floor(firstPosition * static_cast<qreal>(values.size()))));
    const std::size_t lastIndex = std::min(
        lastValueIndex,
        static_cast<std::size_t>(
            std::ceil(lastPosition * static_cast<qreal>(values.size()))));
    const std::size_t visibleCount =
        lastIndex >= firstIndex ? lastIndex - firstIndex + 1U : 0U;

    std::vector<QPointF> spectrumPoints;
    QSGGeometry::DrawingMode spectrumMode = QSGGeometry::DrawLineStrip;
    const SpectrumLodLevel *level = cache_.levelForView(
        visibleCount,
        static_cast<std::size_t>(std::max<qreal>(1.0, plotWidth_)));
    if (level == nullptr) {
        spectrumPoints.reserve(visibleCount);
        for (std::size_t index = firstIndex;
             index <= lastIndex;
             ++index) {
            spectrumPoints.push_back(
                pointForIndex(index, values[index]));
        }
    } else {
        const std::size_t firstBlock = firstIndex / level->blockSize;
        const std::size_t lastBlock = lastIndex / level->blockSize;
        spectrumPoints.reserve((lastBlock - firstBlock + 1U) * 2U);
        for (std::size_t block = firstBlock;
             block <= lastBlock && block < level->envelopes.size();
             ++block) {
            const std::size_t blockStart = block * level->blockSize;
            const std::size_t blockEnd = std::min(
                blockStart + level->blockSize,
                values.size());
            const std::size_t includedStart =
                std::max(blockStart, firstIndex);
            const std::size_t includedEnd =
                std::min(blockEnd, lastIndex + 1U);
            if (includedStart >= includedEnd) {
                continue;
            }

            float minimum = level->envelopes[block].minimum;
            float maximum = level->envelopes[block].maximum;
            if (includedStart != blockStart || includedEnd != blockEnd) {
                minimum = values[includedStart];
                maximum = values[includedStart];
                for (std::size_t index = includedStart + 1U;
                     index < includedEnd;
                     ++index) {
                    minimum = std::min(minimum, values[index]);
                    maximum = std::max(maximum, values[index]);
                }
            }

            std::size_t minimumIndex = includedStart;
            std::size_t maximumIndex = includedStart;
            for (std::size_t index = includedStart;
                 index < includedEnd;
                 ++index) {
                if (values[index] == minimum) {
                    minimumIndex = index;
                }
                if (values[index] == maximum) {
                    maximumIndex = index;
                }
            }
            if (minimumIndex <= maximumIndex) {
                spectrumPoints.push_back(
                    pointForIndex(minimumIndex, minimum));
                spectrumPoints.push_back(
                    pointForIndex(maximumIndex, maximum));
            } else {
                spectrumPoints.push_back(
                    pointForIndex(maximumIndex, maximum));
                spectrumPoints.push_back(
                    pointForIndex(minimumIndex, minimum));
            }
        }
    }

    if (QSGGeometryNode *line = geometryNode(
            spectrumPoints,
            spectrumMode,
            lineColor_,
            1.5f)) {
        clip->appendChildNode(line);
    }

    std::vector<QPointF> peakTriangles;
    for (const PeakPoint &peak : peaks_) {
        if (peak.frequency < minimumFrequency_ ||
            peak.frequency > maximumFrequency_ ||
            peak.db < minimumDb_ || peak.db > maximumDb_) {
            continue;
        }
        const QPointF center =
            pointForFrequency(peak.frequency, peak.db);
        for (int segment = 0; segment < kPeakSegments; ++segment) {
            const float firstAngle =
                2.0f * kPi * static_cast<float>(segment) /
                static_cast<float>(kPeakSegments);
            const float secondAngle =
                2.0f * kPi * static_cast<float>(segment + 1) /
                static_cast<float>(kPeakSegments);
            peakTriangles.push_back(center);
            peakTriangles.emplace_back(
                center.x() + std::cos(firstAngle) * kPeakRadius,
                center.y() + std::sin(firstAngle) * kPeakRadius);
            peakTriangles.emplace_back(
                center.x() + std::cos(secondAngle) * kPeakRadius,
                center.y() + std::sin(secondAngle) * kPeakRadius);
        }
    }
    if (QSGGeometryNode *peakNode = geometryNode(
            peakTriangles,
            QSGGeometry::DrawTriangles,
            peakColor_)) {
        clip->appendChildNode(peakNode);
    }

    return root;
}

void SpectrumItem::updateViewValue(qreal &target, qreal value) {
    if (!qFuzzyCompare(target, value)) {
        target = value;
        emit viewChanged();
        update();
    }
}

QPointF SpectrumItem::pointForIndex(
    std::size_t index,
    float db) const {
    const std::size_t count = cache_.values().size();
    const qreal frequency =
        count > 0U
            ? static_cast<qreal>(index) /
                static_cast<qreal>(count) *
                spectrumMaximumFrequency_
            : 0.0;
    return pointForFrequency(frequency, static_cast<qreal>(db));
}

QPointF SpectrumItem::pointForFrequency(
    qreal frequency,
    qreal db) const {
    const qreal frequencySpan =
        std::max<qreal>(0.0001, maximumFrequency_ - minimumFrequency_);
    const qreal dbSpan =
        std::max<qreal>(0.0001, maximumDb_ - minimumDb_);
    return {
        plotX_ +
            (frequency - minimumFrequency_) /
                frequencySpan * plotWidth_,
        plotY_ +
            (maximumDb_ - db) /
                dbSpan * plotHeight_,
    };
}
