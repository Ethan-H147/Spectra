#ifndef SPECTRA_QT_SPECTROGRAM_PDF_EXPORT_H
#define SPECTRA_QT_SPECTROGRAM_PDF_EXPORT_H

#include <QImage>
#include <QString>

struct SpectrogramPdfMetadata {
    QString sourceName;
    double durationSeconds = 0.0;
    int sampleRate = 0;
    int channelCount = 0;
    double maximumFrequency = 0.0;
    int windowSize = 0;
    int hopSize = 0;
    qsizetype frameCount = 0;
};

bool exportSpectrogramLandscapePdf(
    const QString &path,
    const QImage &spectrogram,
    const SpectrogramPdfMetadata &metadata);

#endif
