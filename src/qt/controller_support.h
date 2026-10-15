#ifndef SPECTRA_QT_CONTROLLER_SUPPORT_H
#define SPECTRA_QT_CONTROLLER_SUPPORT_H

// Constants and helpers shared by the SpectraController translation units.
// Internal to the controller implementation; not part of its interface.

#include "qt/spectra_controller.h"

#include <QBuffer>
#include <QClipboard>
#include <QColor>
#include <QCoreApplication>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QPointF>
#include <QSettings>
#include <QStringList>
#include <QSysInfo>
#include <QVariantMap>
#include <QtMath>

extern "C" {
#include "dsp/additive_synth.h"
#include "dsp/channel_mix.h"
#include "dsp/fft.h"
#include "dsp/harmonic_analysis.h"
#include "dsp/harmonic_resynthesis.h"
#include "dsp/signal_utils.h"
#include "dsp/waveform_summary.h"
#include "raylib.h"
}

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <new>
#include <utility>
#include <vector>

namespace spectra_controller_detail {
inline constexpr unsigned int kSampleRate = 44100U;
inline constexpr int kHarmonicCount = 16;
inline constexpr int kMaximumPeaks = 12;
inline constexpr int kWaveformPoints = 512;
inline constexpr int kSourceWaveformPoints = 1024;
inline constexpr unsigned int kFullFileWindowSize = 2048U;
inline constexpr unsigned int kFullFileHopSize = 512U;
inline constexpr unsigned int kEffectWindowSize = 2048U;
inline constexpr unsigned int kEffectHopSize = 512U;
inline constexpr double kInstantPreviewSeconds = 5.0;
inline constexpr double kInstantPreviewFadeSeconds = 0.012;
inline constexpr int kInstantPreviewDebounceMilliseconds = 160;
inline constexpr float kEffectSpectrumFloorDb = -100.0f;
inline constexpr size_t kMaximumEqBands = 16U;
inline constexpr int kEqPresetCount = 12;
inline constexpr unsigned int kSpectrogramTimeBins = 256U;
inline constexpr unsigned int kSpectrogramFrequencyBins = 128U;
inline constexpr float kSpectrogramMaximumFrequency = 12000.0f;
inline constexpr size_t kGlobalOperationsPerBatch = 65536U;
inline constexpr size_t kStftFramesPerBatch = 2U;
inline constexpr size_t kSpectrogramFramesPerBatch = 32U;
inline constexpr size_t kReconstructionCacheLimit =
    256U * 1024U * 1024U;
inline constexpr ADSREnvelope kDefaultEnvelope = {0.015f, 0.08f, 0.75f, 0.12f};
inline constexpr double kMinimumTextScale = 0.90;
inline constexpr double kMaximumTextScale = 1.40;
inline constexpr qulonglong kMinimumModelMemoryBytes =
    128ULL * 1024ULL * 1024ULL;
inline constexpr qulonglong kMaximumModelMemoryBytes =
    4ULL * 1024ULL * 1024ULL * 1024ULL;

inline unsigned int threadLimitForPerformanceMode(int mode) {
    const unsigned int hardware =
        spectra_hardware_thread_count();
    switch (mode) {
        case 1:
            return hardware;
        case 2:
            return std::max(1U, hardware / 2U);
        case 3:
            return 1U;
        default:
            return 0U;
    }
}

inline const char *const kPresetNames[] = {
    "Sine",
    "Square-like",
    "Saw-like",
    "Clarinet-like",
    "Bright string",
};

inline const char *const kEqPresetNames[kEqPresetCount] = {
    "Flat",
    "Balanced",
    "Bass Boost",
    "Deep Bass",
    "Warm",
    "Vocal Clarity",
    "Treble Boost",
    "Bright",
    "Loudness",
    "Reduce Rumble",
    "Reduce Harshness",
    "Podcast / Speech",
};

inline const char *const kEqBandShapeNames[
    SPECTRAL_EQ_SHAPE_COUNT] = {
    "Range",
    "Bell",
    "Low shelf",
    "High shelf",
};

inline double clampValue(double value, double minimum, double maximum) {
    return std::max(minimum, std::min(value, maximum));
}

inline void applyInstantPreviewFade(InterleavedBuffer *buffer) {
    if (buffer == nullptr || buffer->samples == nullptr ||
        buffer->frame_count < 2U ||
        buffer->sample_rate == 0U ||
        buffer->channel_count == 0U) {
        return;
    }

    const size_t requestedFrames = static_cast<size_t>(
        std::ceil(
            static_cast<double>(buffer->sample_rate) *
            kInstantPreviewFadeSeconds));
    const size_t fadeFrames = std::min(
        requestedFrames,
        buffer->frame_count / 2U);
    constexpr double pi = 3.14159265358979323846;
    for (size_t frame = 0U; frame < fadeFrames; ++frame) {
        const double phase =
            static_cast<double>(frame + 1U) /
            static_cast<double>(fadeFrames + 1U);
        const float gain = static_cast<float>(
            0.5 - 0.5 * std::cos(pi * phase));
        const size_t endFrame =
            buffer->frame_count - 1U - frame;
        for (unsigned int channel = 0U;
             channel < buffer->channel_count;
             ++channel) {
            buffer->samples[
                frame * buffer->channel_count + channel] *= gain;
            buffer->samples[
                endFrame * buffer->channel_count + channel] *= gain;
        }
    }
}

inline void appendEqPresetBand(
    std::vector<SpectralEqBand> &bands,
    double maximumFrequency,
    double lowHz,
    double highHz,
    double gainDb,
    SpectralEqBandShape shape) {
    if (bands.size() >= kMaximumEqBands ||
        maximumFrequency <= 0.0 ||
        lowHz >= maximumFrequency) {
        return;
    }
    const double low =
        clampValue(lowHz, 0.0, maximumFrequency);
    const double high =
        clampValue(highHz, 0.0, maximumFrequency);
    const double minimumWidth =
        std::min(20.0, maximumFrequency);
    if (high - low < minimumWidth) {
        return;
    }
    bands.push_back(SpectralEqBand{
        static_cast<float>(low),
        static_cast<float>(high),
        static_cast<float>(
            clampValue(gainDb, -24.0, 24.0)),
        true,
        shape,
    });
}

inline std::vector<SpectralEqBand> buildEqPresetBands(
    int preset,
    double maximumFrequency) {
    std::vector<SpectralEqBand> bands;
    const auto add =
        [&bands, maximumFrequency](
            double lowHz,
            double highHz,
            double gainDb,
            SpectralEqBandShape shape) {
            appendEqPresetBand(
                bands,
                maximumFrequency,
                lowHz,
                highHz,
                gainDb,
                shape);
        };

    const auto range = [&add](
        double lowHz,
        double highHz,
        double gainDb) {
        add(
            lowHz,
            highHz,
            gainDb,
            SPECTRAL_EQ_SHAPE_RANGE);
    };
    const auto bell = [&add](
        double lowHz,
        double highHz,
        double gainDb) {
        add(
            lowHz,
            highHz,
            gainDb,
            SPECTRAL_EQ_SHAPE_BELL);
    };
    const auto lowShelf = [&add](
        double lowHz,
        double highHz,
        double gainDb) {
        add(
            lowHz,
            highHz,
            gainDb,
            SPECTRAL_EQ_SHAPE_LOW_SHELF);
    };
    const auto highShelf = [&add](
        double lowHz,
        double highHz,
        double gainDb) {
        add(
            lowHz,
            highHz,
            gainDb,
            SPECTRAL_EQ_SHAPE_HIGH_SHELF);
    };

    switch (preset) {
    case 0:
        range(0.0, maximumFrequency, 0.0);
        break;
    case 1:
        lowShelf(70.0, 320.0, 1.0);
        bell(250.0, 900.0, -0.75);
        bell(1600.0, 4800.0, 1.0);
        highShelf(7000.0, 14000.0, 1.0);
        break;
    case 2:
        lowShelf(70.0, 280.0, 4.0);
        bell(120.0, 450.0, 1.0);
        break;
    case 3:
        lowShelf(35.0, 160.0, 5.0);
        bell(60.0, 180.0, 1.5);
        break;
    case 4:
        lowShelf(90.0, 400.0, 2.0);
        bell(180.0, 650.0, 1.25);
        highShelf(6000.0, 12000.0, -1.5);
        break;
    case 5:
        lowShelf(70.0, 250.0, -2.5);
        bell(180.0, 600.0, -1.5);
        bell(1200.0, 4500.0, 2.5);
        highShelf(6000.0, 12000.0, 1.0);
        break;
    case 6:
        bell(2500.0, 7000.0, 1.5);
        highShelf(5500.0, 11000.0, 4.0);
        break;
    case 7:
        bell(1800.0, 6000.0, 1.5);
        highShelf(6500.0, 14000.0, 2.5);
        break;
    case 8:
        lowShelf(60.0, 300.0, 3.5);
        bell(350.0, 2500.0, -1.0);
        highShelf(5000.0, 12000.0, 3.0);
        break;
    case 9:
        range(0.0, 40.0, -12.0);
        lowShelf(20.0, 110.0, -18.0);
        break;
    case 10:
        bell(2200.0, 7500.0, -3.5);
        highShelf(8500.0, 15000.0, -1.0);
        break;
    case 11:
        lowShelf(50.0, 140.0, -12.0);
        bell(150.0, 500.0, -2.0);
        bell(1200.0, 4500.0, 3.0);
        highShelf(7500.0, 14000.0, -1.5);
        break;
    default:
        break;
    }
    return bands;
}

inline double transformEffectFrequency(
    double sourceFrequency,
    double maximumFrequency,
    int mode,
    double pitchFactor,
    double frequencyShift) {
    if (sourceFrequency <= 0.0 ||
        maximumFrequency <= 0.0) {
        return 0.0;
    }
    const double transformedFrequency =
        mode == 2
            ? std::abs(
                  sourceFrequency +
                  frequencyShift)
            : sourceFrequency * pitchFactor;
    return clampValue(
        transformedFrequency,
        0.0,
        maximumFrequency);
}

inline QColor spectrogramColor(float decibels) {
    static const QColor stops[] = {
        QColor(14, 16, 28),
        QColor(34, 72, 144),
        QColor(114, 69, 173),
        QColor(221, 87, 77),
        QColor(250, 207, 83),
    };
    const double level = clampValue((static_cast<double>(decibels) + 100.0) / 100.0, 0.0, 1.0);
    const double scaled = level * 4.0;
    const int segment = std::min(3, static_cast<int>(scaled));
    const double amount = level >= 1.0 ? 1.0 : scaled - static_cast<double>(segment);
    return QColor(
        qRound(stops[segment].red() +
               (stops[segment + 1].red() - stops[segment].red()) * amount),
        qRound(stops[segment].green() +
               (stops[segment + 1].green() - stops[segment].green()) * amount),
        qRound(stops[segment].blue() +
               (stops[segment + 1].blue() - stops[segment].blue()) * amount));
}

inline void buildSpectrogramInBackground(
    BackgroundTaskControl *control,
    void *context) {
    auto *job =
        static_cast<StftReconstructionJob *>(context);
    if (job == nullptr) {
        return;
    }

    while (job->active &&
           !background_task_cancel_requested(control)) {
        stft_reconstruction_job_process(
            job,
            kSpectrogramFramesPerBatch);
        background_task_report_progress(
            control,
            stft_reconstruction_job_progress(job));
    }
    if (job->complete) {
        background_task_report_progress(control, 1.0f);
    }
}

inline bool reportEffectProgress(float progress, void *context) {
    auto *control =
        static_cast<BackgroundTaskControl *>(context);
    if (control == nullptr ||
        background_task_cancel_requested(control)) {
        return false;
    }
    background_task_report_progress(
        control,
        std::min(0.9f, progress * 0.9f));
    return true;
}

struct EffectSpectrumPreview {
    QList<qreal> values;
    double peakFrequency = 0.0;
};

inline EffectSpectrumPreview effectSpectrumPreviewFromSpectrum(
    const Spectrum &spectrum) {
    EffectSpectrumPreview preview;
    if (spectrum.count == 0U ||
        spectrum.magnitudes == nullptr ||
        spectrum.frequencies == nullptr) {
        return preview;
    }

    size_t peakIndex = 0U;
    float peakMagnitude = 0.0f;
    for (size_t index = 1U;
         index < spectrum.count;
         ++index) {
        if (spectrum.frequencies[index] >= 20.0f &&
            spectrum.magnitudes[index] >
                peakMagnitude) {
            peakMagnitude =
                spectrum.magnitudes[index];
            peakIndex = index;
        }
    }

    preview.values.reserve(
        static_cast<qsizetype>(spectrum.count));
    for (size_t index = 0U;
         index < spectrum.count;
         ++index) {
        const float decibels =
            20.0f *
            std::log10(
                std::max(
                    spectrum.magnitudes[index],
                    0.00001f));
        preview.values.append(
            std::max(
                kEffectSpectrumFloorDb,
                std::min(0.0f, decibels)));
    }
    if (peakMagnitude > 1.0e-8f) {
        preview.peakFrequency =
            static_cast<double>(
                spectrum.frequencies[peakIndex]);
    }
    return preview;
}

inline Spectrum computeEffectAveragedSpectrum(
    const InterleavedBuffer &buffer) {
    Spectrum spectrum = {};
    if (buffer.samples == nullptr ||
        buffer.frame_count == 0U ||
        buffer.sample_rate == 0U ||
        buffer.channel_count == 0U) {
        return spectrum;
    }

    std::vector<float> mono;
    try {
        mono.resize(buffer.frame_count);
    } catch (const std::bad_alloc &) {
        return spectrum;
    }
    if (!downmix_interleaved_to_mono(
            buffer.samples,
            buffer.frame_count,
            buffer.channel_count,
            mono.data())) {
        return spectrum;
    }
    return compute_averaged_magnitude_spectrum(
        mono.data(),
        mono.size(),
        buffer.sample_rate);
}

inline EffectSpectrumPreview transformEffectSpectrum(
    const QList<qreal> &source,
    double maximumFrequency,
    int mode,
    double pitchFactor,
    double frequencyShift) {
    EffectSpectrumPreview preview;
    if (source.isEmpty() ||
        maximumFrequency <= 0.0) {
        return preview;
    }

    const qsizetype count = source.size();
    std::vector<double> magnitudes(
        static_cast<size_t>(count),
        0.0);
    for (qsizetype index = 1;
         index < count;
         ++index) {
        const double decibels =
            source[index];
        const double magnitude =
            std::pow(10.0, decibels / 20.0);
        const double sourceFrequency =
            static_cast<double>(index) /
            static_cast<double>(count) *
            maximumFrequency;
        const double targetFrequency =
            mode == 2
                ? std::abs(
                      sourceFrequency +
                      frequencyShift)
                : sourceFrequency * pitchFactor;
        const double targetPosition =
            targetFrequency / maximumFrequency *
            static_cast<double>(count);
        if (targetPosition < 0.0 ||
            targetPosition >=
                static_cast<double>(count)) {
            continue;
        }

        const size_t first =
            static_cast<size_t>(targetPosition);
        const double fraction =
            targetPosition -
            static_cast<double>(first);
        magnitudes[first] +=
            magnitude * (1.0 - fraction);
        if (first + 1U <
            static_cast<size_t>(count)) {
            magnitudes[first + 1U] +=
                magnitude * fraction;
        }
    }

    double peakMagnitude = 0.0;
    size_t peakIndex = 0U;
    for (size_t index = 1U;
         index < magnitudes.size();
         ++index) {
        const double frequency =
            static_cast<double>(index) /
            static_cast<double>(count) *
            maximumFrequency;
        if (frequency >= 20.0 &&
            magnitudes[index] > peakMagnitude) {
            peakMagnitude = magnitudes[index];
            peakIndex = index;
        }
    }

    preview.values.reserve(count);
    for (double magnitude : magnitudes) {
        const double decibels =
            magnitude > 0.0
                ? 20.0 * std::log10(magnitude)
                : static_cast<double>(
                      kEffectSpectrumFloorDb);
        preview.values.append(
            std::max(
                static_cast<double>(
                    kEffectSpectrumFloorDb),
                std::min(0.0, decibels)));
    }
    if (peakMagnitude > 1.0e-12) {
        preview.peakFrequency =
            static_cast<double>(peakIndex) /
            static_cast<double>(count) *
            maximumFrequency;
    }
    return preview;
}
}  // namespace spectra_controller_detail

#endif
