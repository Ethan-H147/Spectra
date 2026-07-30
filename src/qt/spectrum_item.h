#ifndef SPECTRA_QT_SPECTRUM_ITEM_H
#define SPECTRA_QT_SPECTRUM_ITEM_H

#include "qt/spectrum_lod.h"

#include <QColor>
#include <QQuickItem>
#include <QVariantList>

#include <vector>

class SpectrumItem : public QQuickItem {
    Q_OBJECT

    Q_PROPERTY(QVariantList spectrum READ spectrum WRITE setSpectrum NOTIFY spectrumChanged)
    Q_PROPERTY(QVariantList peaks READ peaks WRITE setPeaks NOTIFY peaksChanged)
    Q_PROPERTY(qreal spectrumMaximumFrequency READ spectrumMaximumFrequency WRITE setSpectrumMaximumFrequency NOTIFY viewChanged)
    Q_PROPERTY(qreal minimumFrequency READ minimumFrequency WRITE setMinimumFrequency NOTIFY viewChanged)
    Q_PROPERTY(qreal maximumFrequency READ maximumFrequency WRITE setMaximumFrequency NOTIFY viewChanged)
    Q_PROPERTY(qreal minimumDb READ minimumDb WRITE setMinimumDb NOTIFY viewChanged)
    Q_PROPERTY(qreal maximumDb READ maximumDb WRITE setMaximumDb NOTIFY viewChanged)
    Q_PROPERTY(qreal plotX READ plotX WRITE setPlotX NOTIFY plotChanged)
    Q_PROPERTY(qreal plotY READ plotY WRITE setPlotY NOTIFY plotChanged)
    Q_PROPERTY(qreal plotWidth READ plotWidth WRITE setPlotWidth NOTIFY plotChanged)
    Q_PROPERTY(qreal plotHeight READ plotHeight WRITE setPlotHeight NOTIFY plotChanged)
    Q_PROPERTY(QColor lineColor READ lineColor WRITE setLineColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor peakColor READ peakColor WRITE setPeakColor NOTIFY colorsChanged)
    Q_PROPERTY(qulonglong cacheBytes READ cacheBytes NOTIFY spectrumChanged)

public:
    explicit SpectrumItem(QQuickItem *parent = nullptr);

    QVariantList spectrum() const;
    void setSpectrum(const QVariantList &spectrum);
    QVariantList peaks() const;
    void setPeaks(const QVariantList &peaks);

    qreal spectrumMaximumFrequency() const;
    void setSpectrumMaximumFrequency(qreal frequency);
    qreal minimumFrequency() const;
    void setMinimumFrequency(qreal frequency);
    qreal maximumFrequency() const;
    void setMaximumFrequency(qreal frequency);
    qreal minimumDb() const;
    void setMinimumDb(qreal db);
    qreal maximumDb() const;
    void setMaximumDb(qreal db);

    qreal plotX() const;
    void setPlotX(qreal value);
    qreal plotY() const;
    void setPlotY(qreal value);
    qreal plotWidth() const;
    void setPlotWidth(qreal value);
    qreal plotHeight() const;
    void setPlotHeight(qreal value);

    QColor lineColor() const;
    void setLineColor(const QColor &color);
    QColor peakColor() const;
    void setPeakColor(const QColor &color);

    qulonglong cacheBytes() const;

signals:
    void spectrumChanged();
    void peaksChanged();
    void viewChanged();
    void plotChanged();
    void colorsChanged();

protected:
    QSGNode *updatePaintNode(
        QSGNode *oldNode,
        UpdatePaintNodeData *data) override;

private:
    struct PeakPoint {
        float frequency = 0.0f;
        float db = 0.0f;
    };

    void updateViewValue(qreal &target, qreal value);
    QPointF pointForIndex(std::size_t index, float db) const;
    QPointF pointForFrequency(qreal frequency, qreal db) const;

    SpectrumLodCache cache_;
    std::vector<PeakPoint> peaks_;
    qreal spectrumMaximumFrequency_ = 1.0;
    qreal minimumFrequency_ = 0.0;
    qreal maximumFrequency_ = 1.0;
    qreal minimumDb_ = -100.0;
    qreal maximumDb_ = 0.0;
    qreal plotX_ = 0.0;
    qreal plotY_ = 0.0;
    qreal plotWidth_ = 1.0;
    qreal plotHeight_ = 1.0;
    QColor lineColor_ = QColor(QStringLiteral("#2E8BFF"));
    QColor peakColor_ = QColor(QStringLiteral("#F05D55"));
};

#endif
