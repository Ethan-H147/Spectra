#include "qt/spectra_controller.h"
#include "qt/spectrogram_pdf_export.h"

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

namespace {
constexpr unsigned int kSampleRate = 44100U;
constexpr int kHarmonicCount = 16;
constexpr int kMaximumPeaks = 12;
constexpr int kWaveformPoints = 512;
constexpr int kSourceWaveformPoints = 1024;
constexpr unsigned int kFullFileWindowSize = 2048U;
constexpr unsigned int kFullFileHopSize = 512U;
constexpr unsigned int kEffectWindowSize = 2048U;
constexpr unsigned int kEffectHopSize = 512U;
constexpr double kInstantPreviewSeconds = 5.0;
constexpr double kInstantPreviewFadeSeconds = 0.012;
constexpr int kInstantPreviewDebounceMilliseconds = 160;
constexpr float kEffectSpectrumFloorDb = -100.0f;
constexpr size_t kMaximumEqBands = 16U;
constexpr int kEqPresetCount = 12;
constexpr unsigned int kSpectrogramTimeBins = 256U;
constexpr unsigned int kSpectrogramFrequencyBins = 128U;
constexpr float kSpectrogramMaximumFrequency = 12000.0f;
constexpr size_t kGlobalOperationsPerBatch = 65536U;
constexpr size_t kStftFramesPerBatch = 2U;
constexpr size_t kSpectrogramFramesPerBatch = 32U;
constexpr size_t kReconstructionCacheLimit =
    256U * 1024U * 1024U;
constexpr ADSREnvelope kDefaultEnvelope = {0.015f, 0.08f, 0.75f, 0.12f};
constexpr double kMinimumTextScale = 0.90;
constexpr double kMaximumTextScale = 1.40;
constexpr qulonglong kMinimumModelMemoryBytes =
    128ULL * 1024ULL * 1024ULL;
constexpr qulonglong kMaximumModelMemoryBytes =
    4ULL * 1024ULL * 1024ULL * 1024ULL;

unsigned int threadLimitForPerformanceMode(int mode) {
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

const char *const kPresetNames[] = {
    "Sine",
    "Square-like",
    "Saw-like",
    "Clarinet-like",
    "Bright string",
};

const char *const kEqPresetNames[kEqPresetCount] = {
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

const char *const kEqBandShapeNames[
    SPECTRAL_EQ_SHAPE_COUNT] = {
    "Range",
    "Bell",
    "Low shelf",
    "High shelf",
};

double clampValue(double value, double minimum, double maximum) {
    return std::max(minimum, std::min(value, maximum));
}

void applyInstantPreviewFade(InterleavedBuffer *buffer) {
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

void appendEqPresetBand(
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

std::vector<SpectralEqBand> buildEqPresetBands(
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

double transformEffectFrequency(
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

QColor spectrogramColor(float decibels) {
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

void buildSpectrogramInBackground(
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

bool reportEffectProgress(float progress, void *context) {
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
    QVariantList values;
    double peakFrequency = 0.0;
};

EffectSpectrumPreview effectSpectrumPreviewFromSpectrum(
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

Spectrum computeEffectAveragedSpectrum(
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

EffectSpectrumPreview transformEffectSpectrum(
    const QVariantList &source,
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
            source[index].toDouble();
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
}  // namespace

void SpectraController::processGlobalFullFileInBackground(
    BackgroundTaskControl *control,
    void *context) {
    auto *controller =
        static_cast<SpectraController *>(context);
    if (controller == nullptr) {
        return;
    }

    for (;;) {
        if (background_task_cancel_requested(control)) {
            break;
        }
        bool anyActive = false;
        double progress = 0.0;
        for (unsigned int channel = 0U;
             channel < controller->fullFileWorkerChannels_;
             ++channel) {
            GlobalFourierJob *job =
                channel == 0U
                    ? &controller->globalFourierJob_
                    : &controller->globalFourierJobRight_;
            if (job->active) {
                anyActive = true;
                global_fourier_job_process(
                    job,
                    kGlobalOperationsPerBatch);
            }
            progress +=
                global_fourier_job_progress(job);
        }
        background_task_report_progress(
            control,
            static_cast<float>(
                progress /
                static_cast<double>(
                    controller->fullFileWorkerChannels_)));
        if (!anyActive) {
            break;
        }
    }
}

void SpectraController::processStftFullFileInBackground(
    BackgroundTaskControl *control,
    void *context) {
    auto *controller =
        static_cast<SpectraController *>(context);
    if (controller == nullptr) {
        return;
    }

    for (;;) {
        if (background_task_cancel_requested(control)) {
            break;
        }
        bool anyActive = false;
        double progress = 0.0;
        for (unsigned int channel = 0U;
             channel < controller->fullFileWorkerChannels_;
             ++channel) {
            StftReconstructionJob *job =
                channel == 0U
                    ? &controller->fullFileStftJob_
                    : &controller->fullFileStftJobRight_;
            if (job->active) {
                anyActive = true;
                stft_reconstruction_job_process(
                    job,
                    kStftFramesPerBatch);
            }
            progress +=
                stft_reconstruction_job_progress(job);
        }
        background_task_report_progress(
            control,
            static_cast<float>(
                progress /
                static_cast<double>(
                    controller->fullFileWorkerChannels_)));
        if (!anyActive) {
            break;
        }
    }
}

void SpectraController::processEffectSourceSpectrumInBackground(
    BackgroundTaskControl *control,
    void *context) {
    auto *controller =
        static_cast<SpectraController *>(context);
    if (controller == nullptr || control == nullptr) {
        return;
    }

    spectrum_free(
        &controller->effectSourceSpectrumWorker_);
    controller->effectSourceSpectrumWorkerSucceeded_ =
        false;
    if (background_task_cancel_requested(control) ||
        controller->importedAudio_.mono.samples == nullptr ||
        controller->importedAudio_.mono.count == 0U) {
        return;
    }

    Spectrum spectrum =
        compute_averaged_magnitude_spectrum(
            controller->importedAudio_.mono.samples,
            controller->importedAudio_.mono.count,
            controller->importedAudio_.mono.sample_rate);
    if (background_task_cancel_requested(control)) {
        spectrum_free(&spectrum);
        return;
    }

    controller->effectSourceSpectrumWorker_ =
        spectrum;
    controller->effectSourceSpectrumWorkerSucceeded_ =
        spectrum.count > 0U &&
        spectrum.magnitudes != nullptr &&
        spectrum.frequencies != nullptr;
    background_task_report_progress(control, 1.0f);
}

void SpectraController::processEffectInBackground(
    BackgroundTaskControl *control,
    void *context) {
    auto *controller =
        static_cast<SpectraController *>(context);
    if (controller == nullptr || control == nullptr) {
        return;
    }

    bool succeeded = false;
    switch (controller->effectWorkerMode_) {
        case 0: {
            const float pitchFactor = static_cast<float>(
                std::pow(
                    2.0,
                    controller->effectWorkerAmount_ /
                        12.0));
            succeeded = playback_rate_pitch_shift(
                &controller->importedAudio_.interleaved,
                pitchFactor,
                &controller->effectWorkerBuffer_,
                reportEffectProgress,
                control);
            break;
        }
        case 1: {
            const float pitchFactor = static_cast<float>(
                std::pow(
                    2.0,
                    controller->effectWorkerAmount_ /
                        12.0));
            succeeded = phase_vocoder_pitch_shift(
                &controller->importedAudio_.interleaved,
                pitchFactor,
                kEffectWindowSize,
                kEffectHopSize,
                &controller->effectWorkerBuffer_,
                reportEffectProgress,
                control);
            break;
        }
        case 2:
            succeeded = analytic_frequency_shift(
                &controller->importedAudio_.interleaved,
                static_cast<float>(
                    controller->effectWorkerAmount_),
                kEffectWindowSize,
                kEffectHopSize,
                &controller->effectWorkerBuffer_,
                reportEffectProgress,
                control);
            break;
        default:
            break;
    }

    spectrum_free(
        &controller->effectWorkerSpectrum_);
    if (succeeded &&
        !background_task_cancel_requested(control)) {
        background_task_report_progress(
            control,
            0.92f);
        controller->effectWorkerSpectrum_ =
            computeEffectAveragedSpectrum(
                controller->effectWorkerBuffer_);
        if (background_task_cancel_requested(control)) {
            spectrum_free(
                &controller->effectWorkerSpectrum_);
        } else {
            background_task_report_progress(
                control,
                1.0f);
        }
    }

    controller->effectWorkerSucceeded_ =
        succeeded &&
        !background_task_cancel_requested(control);
    if (!controller->effectWorkerSucceeded_) {
        interleaved_buffer_free(
            &controller->effectWorkerBuffer_);
        spectrum_free(
            &controller->effectWorkerSpectrum_);
    }
}

void SpectraController::processEqInBackground(
    BackgroundTaskControl *control,
    void *context) {
    auto *controller =
        static_cast<SpectraController *>(context);
    if (controller == nullptr || control == nullptr) {
        return;
    }

    interleaved_buffer_free(
        &controller->eqWorkerBuffer_);
    spectrum_free(&controller->eqWorkerSpectrum_);
    const SpectralEqBand *bands =
        controller->eqWorkerBands_.empty()
            ? nullptr
            : controller->eqWorkerBands_.data();
    const bool succeeded = spectral_range_equalize(
        &controller->importedAudio_.interleaved,
        bands,
        controller->eqWorkerBands_.size(),
        kEffectWindowSize,
        kEffectHopSize,
        &controller->eqWorkerBuffer_,
        reportEffectProgress,
        control);

    if (succeeded &&
        !background_task_cancel_requested(control)) {
        background_task_report_progress(control, 0.92f);
        controller->eqWorkerSpectrum_ =
            computeEffectAveragedSpectrum(
                controller->eqWorkerBuffer_);
        if (background_task_cancel_requested(control)) {
            spectrum_free(
                &controller->eqWorkerSpectrum_);
        } else {
            background_task_report_progress(
                control, 1.0f);
        }
    }

    controller->eqWorkerSucceeded_ =
        succeeded &&
        !background_task_cancel_requested(control);
    if (!controller->eqWorkerSucceeded_) {
        interleaved_buffer_free(
            &controller->eqWorkerBuffer_);
        spectrum_free(
            &controller->eqWorkerSpectrum_);
    }
}

void SpectraController::processInstantPreviewInBackground(
    BackgroundTaskControl *control,
    void *context) {
    auto *controller =
        static_cast<SpectraController *>(context);
    if (controller == nullptr || control == nullptr) {
        return;
    }

    interleaved_buffer_free(
        &controller->instantPreviewWorkerBuffer_);
    bool succeeded = false;
    switch (controller->instantPreviewWorkerKind_) {
        case EffectInstantPreview: {
            const int mode =
                controller->instantPreviewWorkerEffectMode_;
            const double amount =
                controller->instantPreviewWorkerEffectAmount_;
            if (mode == 0) {
                const float factor = static_cast<float>(
                    std::pow(2.0, amount / 12.0));
                succeeded = playback_rate_pitch_shift(
                    &controller->instantPreviewSourceBuffer_,
                    factor,
                    &controller->instantPreviewWorkerBuffer_,
                    reportEffectProgress,
                    control);
            } else if (mode == 1) {
                const float factor = static_cast<float>(
                    std::pow(2.0, amount / 12.0));
                succeeded = phase_vocoder_pitch_shift(
                    &controller->instantPreviewSourceBuffer_,
                    factor,
                    kEffectWindowSize,
                    kEffectHopSize,
                    &controller->instantPreviewWorkerBuffer_,
                    reportEffectProgress,
                    control);
            } else if (mode == 2) {
                succeeded = analytic_frequency_shift(
                    &controller->instantPreviewSourceBuffer_,
                    static_cast<float>(amount),
                    kEffectWindowSize,
                    kEffectHopSize,
                    &controller->instantPreviewWorkerBuffer_,
                    reportEffectProgress,
                    control);
            }
            break;
        }
        case EqInstantPreview: {
            const SpectralEqBand *bands =
                controller->instantPreviewWorkerBands_.empty()
                    ? nullptr
                    : controller->instantPreviewWorkerBands_.data();
            succeeded = spectral_range_equalize(
                &controller->instantPreviewSourceBuffer_,
                bands,
                controller->instantPreviewWorkerBands_.size(),
                kEffectWindowSize,
                kEffectHopSize,
                &controller->instantPreviewWorkerBuffer_,
                reportEffectProgress,
                control);
            break;
        }
        default:
            break;
    }

    controller->instantPreviewWorkerSucceeded_ =
        succeeded &&
        !background_task_cancel_requested(control);
    if (controller->instantPreviewWorkerSucceeded_) {
        applyInstantPreviewFade(
            &controller->instantPreviewWorkerBuffer_);
        background_task_report_progress(control, 1.0f);
    } else {
        interleaved_buffer_free(
            &controller->instantPreviewWorkerBuffer_);
    }
}

SpectraController::SpectraController(QObject *parent)
    : QObject(parent) {
    QSettings settings;
    textScale_ = clampValue(
        settings.value(
            QStringLiteral("appearance/textScale"),
            textScale_)
            .toDouble(),
        kMinimumTextScale,
        kMaximumTextScale);
    performanceMode_ = std::max(
        0,
        std::min(
            settings.value(
                QStringLiteral("performance/mode"), 0)
                .toInt(),
            3));
    fullFileMemoryLimitBytes_ = std::max(
        kMinimumModelMemoryBytes,
        std::min(
            settings.value(
                QStringLiteral("models/memoryLimitBytes"),
                fullFileMemoryLimitBytes_)
                .toULongLong(),
            kMaximumModelMemoryBytes));
    spectra_set_parallel_thread_limit(
        threadLimitForPerformanceMode(performanceMode_));

    audio_clip_init(&synthClip_);
    audio_clip_init(&sourceClip_);
    audio_clip_init(&regionClip_);
    audio_clip_init(&harmonicClip_);
    audio_clip_init(&originalFrameClip_);
    audio_clip_init(&fourierClip_);
    audio_clip_init(&fullFileClip_);
    audio_clip_init(&effectClip_);
    audio_clip_init(&eqClip_);
    audio_clip_init(&instantPreviewOriginalClip_);
    audio_clip_init(&instantPreviewProcessedClip_);
    fourier_frame_analysis_init(&fourierAnalysis_);
    global_fourier_job_init(&globalFourierJob_);
    global_fourier_job_init(&globalFourierJobRight_);
    stft_reconstruction_job_init(&spectrogramJob_);
    background_task_init(&spectrogramTask_);
    background_task_init(&fullFileTask_);
    background_task_init(&effectTask_);
    background_task_init(&effectSpectrumTask_);
    background_task_init(&eqTask_);
    background_task_init(&instantPreviewTask_);
    background_task_init(&importTask_);
    stft_reconstruction_job_init(&fullFileStftJob_);
    stft_reconstruction_job_init(&fullFileStftJobRight_);
    reconstruction_cache_init(
        &reconstructionCache_,
        kReconstructionCacheLimit);
    imported_audio_init(&importedAudio_);
    imported_audio_init(&importWorkerAudio_);

    InitAudioDevice();
    audioReady_ = IsAudioDeviceReady();
    if (audioReady_) {
        SetMasterVolume(1.0f);
        statusText_ = QStringLiteral("Audio device ready");
    } else {
        statusText_ = QStringLiteral("Audio device unavailable; WAV export remains available");
    }

    synthRebuildTimer_.setSingleShot(true);
    synthRebuildTimer_.setInterval(32);
    connect(&synthRebuildTimer_, &QTimer::timeout, this, &SpectraController::rebuildSynth);

    playbackTimer_.setInterval(50);
    connect(&playbackTimer_, &QTimer::timeout, this, &SpectraController::refreshPlaybackState);
    playbackTimer_.start();

    importTimer_.setInterval(16);
    connect(
        &importTimer_,
        &QTimer::timeout,
        this,
        &SpectraController::processAudioImportWork);

    fullFileTimer_.setInterval(16);
    connect(
        &fullFileTimer_,
        &QTimer::timeout,
        this,
        &SpectraController::processFullFileWork);

    effectTimer_.setInterval(16);
    connect(
        &effectTimer_,
        &QTimer::timeout,
        this,
        &SpectraController::processEffectWork);

    effectSpectrumTimer_.setInterval(16);
    connect(
        &effectSpectrumTimer_,
        &QTimer::timeout,
        this,
        &SpectraController::processEffectSourceSpectrumWork);

    eqTimer_.setInterval(16);
    connect(
        &eqTimer_,
        &QTimer::timeout,
        this,
        &SpectraController::processEqWork);

    instantPreviewDebounceTimer_.setSingleShot(true);
    instantPreviewDebounceTimer_.setInterval(
        kInstantPreviewDebounceMilliseconds);
    connect(
        &instantPreviewDebounceTimer_,
        &QTimer::timeout,
        this,
        &SpectraController::startInstantPreview);

    instantPreviewTimer_.setInterval(16);
    connect(
        &instantPreviewTimer_,
        &QTimer::timeout,
        this,
        &SpectraController::processInstantPreviewWork);

    applySynthPreset(0);
    rebuildSynth();
}

SpectraController::~SpectraController() {
    instantPreviewDebounceTimer_.stop();
    instantPreviewTimer_.stop();
    importTimer_.stop();
    eqTimer_.stop();
    effectSpectrumTimer_.stop();
    effectTimer_.stop();
    fullFileTimer_.stop();
    background_task_cancel_and_join(
        &effectSpectrumTask_);
    background_task_cancel_and_join(&importTask_);
    background_task_cancel_and_join(&effectTask_);
    background_task_cancel_and_join(&eqTask_);
    background_task_cancel_and_join(
        &instantPreviewTask_);
    background_task_cancel_and_join(&spectrogramTask_);
    background_task_cancel_and_join(&fullFileTask_);
    audio_clip_unload(&fullFileClip_);
    audio_clip_unload(&effectClip_);
    audio_clip_unload(&eqClip_);
    audio_clip_unload(&instantPreviewOriginalClip_);
    audio_clip_unload(&instantPreviewProcessedClip_);
    interleaved_buffer_free(&effectWorkerBuffer_);
    interleaved_buffer_free(&effectBuffer_);
    spectrum_free(&effectSourceSpectrumWorker_);
    spectrum_free(&effectSourceSpectrumBuffer_);
    spectrum_free(&effectWorkerSpectrum_);
    spectrum_free(&effectOutputSpectrumBuffer_);
    interleaved_buffer_free(&eqWorkerBuffer_);
    interleaved_buffer_free(&eqBuffer_);
    spectrum_free(&eqWorkerSpectrum_);
    spectrum_free(&eqOutputSpectrumBuffer_);
    interleaved_buffer_free(
        &instantPreviewSourceBuffer_);
    interleaved_buffer_free(
        &instantPreviewWorkerBuffer_);
    interleaved_buffer_free(
        &instantPreviewBuffer_);
    interleaved_buffer_free(&fullFileBuffer_);
    reconstruction_cache_free(&reconstructionCache_);
    stft_reconstruction_job_free(&fullFileStftJobRight_);
    stft_reconstruction_job_free(&fullFileStftJob_);
    global_fourier_job_free(&globalFourierJobRight_);
    global_fourier_job_free(&globalFourierJob_);
    stft_reconstruction_job_free(&spectrogramJob_);
    audio_clip_unload(&fourierClip_);
    audio_clip_unload(&originalFrameClip_);
    audio_clip_unload(&harmonicClip_);
    sample_buffer_free(&fourierBuffer_);
    fourier_frame_analysis_free(&fourierAnalysis_);
    sample_buffer_free(&harmonicBuffer_);
    audio_clip_unload(&regionClip_);
    spectrum_free(&analysisSpectrumBuffer_);
    audio_clip_unload(&sourceClip_);
    imported_audio_unload(&importWorkerAudio_);
    imported_audio_unload(&importedAudio_);
    audio_clip_unload(&synthClip_);
    spectrum_free(&synthSpectrumBuffer_);
    sample_buffer_free(&synthBuffer_);
    if (audioReady_) {
        CloseAudioDevice();
    }
}

int SpectraController::currentPage() const {
    return currentPage_;
}

bool SpectraController::audioReady() const {
    return audioReady_;
}

QString SpectraController::statusText() const {
    return statusText_;
}

double SpectraController::textScale() const {
    return textScale_;
}

int SpectraController::performanceMode() const {
    return performanceMode_;
}

QString SpectraController::performanceModeName() const {
    switch (performanceMode_) {
        case 1:
            return QStringLiteral("Maximum");
        case 2:
            return QStringLiteral("Efficient");
        case 3:
            return QStringLiteral("Single core");
        default:
            return QStringLiteral("Automatic");
    }
}

int SpectraController::processingWorkerCount() const {
    return static_cast<int>(
        spectra_effective_worker_count());
}

int SpectraController::hardwareThreadCount() const {
    return static_cast<int>(
        spectra_hardware_thread_count());
}

QString SpectraController::processingBackendName() const {
    return QStringLiteral("Optimized multicore CPU");
}

QString SpectraController::applicationVersion() const {
    return QCoreApplication::applicationVersion();
}

QString SpectraController::buildCommit() const {
    return QStringLiteral(SPECTRA_BUILD_COMMIT);
}

QString SpectraController::systemDescription() const {
    return QStringLiteral("%1 · %2")
        .arg(QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture());
}

QString SpectraController::diagnosticsText() const {
    const double memoryMegabytes =
        static_cast<double>(fullFileMemoryLimitBytes_) /
        (1024.0 * 1024.0);
    QStringList lines = {
        QStringLiteral("Spectra version: %1").arg(applicationVersion()),
        QStringLiteral("Build commit: %1").arg(buildCommit()),
        QStringLiteral("Operating system: %1")
            .arg(QSysInfo::prettyProductName()),
        QStringLiteral("CPU architecture: %1")
            .arg(QSysInfo::currentCpuArchitecture()),
        QStringLiteral("Logical processors: %1").arg(hardwareThreadCount()),
        QStringLiteral("Processing mode: %1").arg(performanceModeName()),
        QStringLiteral("Processing workers: %1").arg(processingWorkerCount()),
        QStringLiteral("Model memory limit: %1 MB")
            .arg(memoryMegabytes, 0, 'f', 0),
        QStringLiteral("Audio runtime: %1")
            .arg(audioReady_ ? QStringLiteral("ready")
                             : QStringLiteral("unavailable"))
    };

    if (sourceLoaded()) {
        lines.append(
            QStringLiteral("Loaded source: %1 Hz, %2 channel(s), %3 s")
                .arg(sourceSampleRate())
                .arg(sourceChannels())
                .arg(sourceDuration(), 0, 'f', 2));
    } else {
        lines.append(QStringLiteral("Loaded source: none"));
    }

    return lines.join(QLatin1Char('\n'));
}

void SpectraController::copyDiagnostics() {
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (clipboard == nullptr) {
        setStatusText(QStringLiteral("Could not access the clipboard"));
        return;
    }
    clipboard->setText(diagnosticsText());
    setStatusText(QStringLiteral("Copied diagnostics to the clipboard"));
}

double SpectraController::synthFrequency() const {
    return synthFrequency_;
}

double SpectraController::synthDuration() const {
    return synthDuration_;
}

double SpectraController::synthGain() const {
    return synthGain_;
}

int SpectraController::synthPreset() const {
    return synthPreset_;
}

QString SpectraController::synthPresetName() const {
    if (synthPreset_ < 0 || synthPreset_ >= 5) {
        return QStringLiteral("Custom");
    }
    return QString::fromLatin1(kPresetNames[synthPreset_]);
}

QVariantList SpectraController::harmonicAmplitudes() const {
    QVariantList values;
    values.reserve(kHarmonicCount);
    for (const Harmonic &harmonic : harmonics_) {
        values.append(harmonic.amplitude);
    }
    return values;
}

QVariantList SpectraController::synthWaveform() const {
    return synthWaveform_;
}

QVariantList SpectraController::synthWaveformMinimums() const {
    return synthWaveformMinimums_;
}

QVariantList SpectraController::synthWaveformMaximums() const {
    return synthWaveformMaximums_;
}

QVariantList SpectraController::synthSpectrum() const {
    return synthSpectrum_;
}

QVariantList SpectraController::synthPeaks() const {
    return synthPeaks_;
}

QString SpectraController::synthPitch() const {
    if (!synthPitchEstimate_.valid) {
        return QStringLiteral("No stable pitch");
    }
    return QString::number(synthPitchEstimate_.frequency_hz, 'f', 2) + QStringLiteral(" Hz");
}

QString SpectraController::synthNote() const {
    if (!synthPitchEstimate_.valid) {
        return QStringLiteral("Raise a harmonic above silence");
    }
    return QStringLiteral("%1%2  |  %3%4 cents")
        .arg(QString::fromLatin1(pitch_note_name(synthPitchEstimate_.midi_note)))
        .arg(pitch_note_octave(synthPitchEstimate_.midi_note))
        .arg(synthPitchEstimate_.cents >= 0.0f ? QStringLiteral("+") : QString())
        .arg(synthPitchEstimate_.cents, 0, 'f', 1);
}

int SpectraController::synthPeakCount() const {
    return synthPeakCount_;
}

bool SpectraController::synthPlaying() const {
    return audio_clip_is_playing(&synthClip_);
}

bool SpectraController::sourceLoaded() const {
    return importedAudio_.mono.samples != nullptr;
}

QString SpectraController::sourceFileName() const {
    return sourceFileName_;
}

bool SpectraController::sourceImporting() const {
    return sourceImporting_;
}

QString SpectraController::sourceImportFileName() const {
    return sourceImportFileName_;
}

int SpectraController::sourceChannels() const {
    return static_cast<int>(importedAudio_.source_channels);
}

int SpectraController::sourceSampleRate() const {
    return static_cast<int>(importedAudio_.mono.sample_rate);
}

double SpectraController::sourceDuration() const {
    return importedAudio_.duration_seconds;
}

bool SpectraController::sourcePlaying() const {
    return audio_clip_is_playing(&sourceClip_);
}

double SpectraController::playbackPosition() const {
    const AudioClip *clip = activeClip();
    return clip != nullptr ? audio_clip_position_seconds(clip) : 0.0;
}

double SpectraController::playbackDuration() const {
    const AudioClip *clip = activeClip();
    return clip != nullptr ? audio_clip_duration_seconds(clip) : 0.0;
}

bool SpectraController::analysisReady() const {
    return analysisReady_;
}

double SpectraController::regionStart() const {
    return regionStart_;
}

double SpectraController::regionDuration() const {
    return regionDuration_;
}

QVariantList SpectraController::sourceWaveformMinimums() const {
    return sourceWaveformMinimums_;
}

QVariantList SpectraController::sourceWaveformMaximums() const {
    return sourceWaveformMaximums_;
}

QVariantList SpectraController::analysisSpectrum() const {
    return analysisSpectrum_;
}

QVariantList SpectraController::analysisPeaks() const {
    return analysisPeaks_;
}

int SpectraController::analysisPeakReadoutMode() const {
    return analysisPeakReadoutMode_;
}

QString SpectraController::analysisPitch() const {
    if (!analysisPitchEstimate_.valid) {
        return QStringLiteral("No stable pitch");
    }
    return QString::number(analysisPitchEstimate_.frequency_hz, 'f', 2) + QStringLiteral(" Hz");
}

QString SpectraController::analysisNote() const {
    if (!analysisPitchEstimate_.valid) {
        return QStringLiteral("The selected region is not stably pitched");
    }
    return QStringLiteral("%1%2  |  %3%4 cents")
        .arg(QString::fromLatin1(pitch_note_name(analysisPitchEstimate_.midi_note)))
        .arg(pitch_note_octave(analysisPitchEstimate_.midi_note))
        .arg(analysisPitchEstimate_.cents >= 0.0f ? QStringLiteral("+") : QString())
        .arg(analysisPitchEstimate_.cents, 0, 'f', 1);
}

double SpectraController::analysisConfidence() const {
    return analysisPitchEstimate_.confidence;
}

int SpectraController::analysisPeakCount() const {
    return analysisPeakCount_;
}

bool SpectraController::regionPlaying() const {
    return audio_clip_is_playing(&regionClip_);
}

bool SpectraController::regionActive() const {
    return playbackTarget_ == RegionPlayback &&
        audio_clip_is_active(&regionClip_);
}

bool SpectraController::harmonicReady() const {
    return harmonicBuffer_.samples != nullptr;
}

int SpectraController::harmonicCount() const {
    return harmonicCount_;
}

int SpectraController::detectedHarmonicCount() const {
    return detectedHarmonicCount_;
}

QVariantList SpectraController::extractedHarmonics() const {
    return extractedHarmonics_;
}

bool SpectraController::fourierFrameReady() const {
    return fourierBuffer_.samples != nullptr;
}

int SpectraController::fourierMaximumComponents() const {
    return static_cast<int>(std::min<size_t>(1000U, fourierAnalysis_.component_count));
}

int SpectraController::fourierSelectedComponents() const {
    return fourierSelectedComponents_;
}

int SpectraController::fourierFftSize() const {
    return static_cast<int>(fourierAnalysis_.fft_size);
}

double SpectraController::fourierFrameDuration() const {
    if (fourierAnalysis_.windowed_frame.sample_rate == 0U) {
        return 0.0;
    }
    return static_cast<double>(
               fourierAnalysis_.windowed_frame.count) /
        static_cast<double>(
               fourierAnalysis_.windowed_frame.sample_rate);
}

QVariantList SpectraController::fourierComponents() const {
    QVariantList components;
    if (fourierAnalysis_.ranked_components == nullptr) {
        return components;
    }
    const int count = std::min(
        {8,
         fourierSelectedComponents_,
         static_cast<int>(fourierAnalysis_.component_count)});
    components.reserve(count);
    for (int index = 0; index < count; ++index) {
        const FourierComponent &source =
            fourierAnalysis_.ranked_components[index];
        QVariantMap component;
        component.insert(QStringLiteral("rank"), index + 1);
        component.insert(
            QStringLiteral("frequency"),
            source.frequency);
        component.insert(QStringLiteral("db"), source.db);
        component.insert(QStringLiteral("phase"), source.phase);
        components.append(component);
    }
    return components;
}

bool SpectraController::harmonicPlaying() const {
    return audio_clip_is_playing(&harmonicClip_);
}

bool SpectraController::framePlaying() const {
    return audio_clip_is_playing(&originalFrameClip_);
}

bool SpectraController::fourierPlaying() const {
    return audio_clip_is_playing(&fourierClip_);
}

QString SpectraController::labPlaybackTitle() const {
    switch (lastLabPlaybackTarget_) {
        case HarmonicPlayback:
            return QStringLiteral("Integer-harmonic resynthesis");
        case OriginalFramePlayback:
            return QStringLiteral("Original windowed FFT frame");
        case FourierFramePlayback:
            return QStringLiteral("Top-%1 evolving Fourier reconstruction")
                .arg(fourierSelectedComponents_);
        case RegionPlayback:
        default:
            return QStringLiteral("Original selected region");
    }
}

bool SpectraController::fullFileProcessing() const {
    return fullFileWork_ != FullFileIdle;
}

bool SpectraController::fullFileReady() const {
    return fullFileBuffer_.samples != nullptr &&
        fullFileBuffer_.frame_count > 0U;
}

int SpectraController::fullFileMode() const {
    return fullFileMode_;
}

int SpectraController::fullFileChannelMode() const {
    return fullFileChannelMode_;
}

int SpectraController::fullFileSelectionMode() const {
    return fullFileSelectionMode_;
}

double SpectraController::fullFileEnergyTarget() const {
    return fullFileEnergyTarget_;
}

qulonglong SpectraController::fullFileEstimatedMonoBytes() const {
    if (!sourceLoaded()) {
        return 0;
    }
    if (fullFileMode_ == 1) {
        return static_cast<qulonglong>(
            stft_reconstruction_estimated_bytes(
                importedAudio_.mono.count,
                kFullFileWindowSize,
                fullFileSelectedComponents_,
                false,
                0U,
                0U,
                1U));
    }
    const int componentCapacity =
        requiredGlobalAnalysisComponents();
    return componentCapacity > 0
        ? static_cast<qulonglong>(
              global_fourier_estimated_multichannel_analysis_bytes(
                  importedAudio_.mono.count,
                  componentCapacity,
                  1U))
        : 0;
}

qulonglong SpectraController::fullFileEstimatedSourceBytes() const {
    if (!sourceLoaded()) {
        return 0;
    }
    const int componentCapacity =
        requiredGlobalAnalysisComponents();
    const unsigned int channels =
        importedAudio_.source_channels == 2U &&
                importedAudio_.interleaved.channel_count == 2U
            ? 2U
            : 1U;
    if (fullFileMode_ == 1) {
        return static_cast<qulonglong>(
            stft_reconstruction_estimated_bytes(
                importedAudio_.mono.count,
                kFullFileWindowSize,
                fullFileSelectedComponents_,
                false,
                0U,
                0U,
                channels));
    }
    return componentCapacity > 0
        ? static_cast<qulonglong>(
              global_fourier_estimated_multichannel_analysis_bytes(
                  importedAudio_.mono.count,
                  componentCapacity,
                  channels))
        : 0;
}

qulonglong SpectraController::fullFileMemoryLimitBytes() const {
    return fullFileMemoryLimitBytes_;
}

int SpectraController::fullFileCacheEntries() const {
    return static_cast<int>(
        reconstruction_cache_entry_count(
            &reconstructionCache_));
}

int SpectraController::fullFileOutputChannels() const {
    return static_cast<int>(fullFileBuffer_.channel_count);
}

double SpectraController::fullFileProgress() const {
    return fullFileProgress_;
}

int SpectraController::fullFileSelectedComponents() const {
    return fullFileSelectedComponents_;
}

int SpectraController::fullFileMaximumComponents() const {
    if (fullFileMode_ == 1) {
        return static_cast<int>(kFullFileWindowSize / 2U - 1U);
    }
    if (!sourceLoaded()) {
        return 1;
    }
    return std::max(
        1,
        global_fourier_available_component_count(importedAudio_.mono.count));
}

double SpectraController::fullFileRetainedEnergy() const {
    return fullFileRetainedEnergy_;
}

int SpectraController::fullFileWindowSize() const {
    return static_cast<int>(kFullFileWindowSize);
}

int SpectraController::fullFileHopSize() const {
    return static_cast<int>(kFullFileHopSize);
}

qsizetype SpectraController::fullFileFrameCount() const {
    return fullFileFrameCount_;
}

qsizetype SpectraController::fullFileTransformSize() const {
    if (fullFileMode_ == 0 && globalFourierJob_.fft_size > 0U) {
        return static_cast<qsizetype>(globalFourierJob_.fft_size);
    }
    return static_cast<qsizetype>(kFullFileWindowSize);
}

double SpectraController::fullFileFrequencyResolution() const {
    const qsizetype transformSize = fullFileTransformSize();
    return sourceSampleRate() > 0 && transformSize > 0
        ? static_cast<double>(sourceSampleRate()) / static_cast<double>(transformSize)
        : 0.0;
}

double SpectraController::spectrogramMaximumFrequency() const {
    return sourceSampleRate() > 0
        ? std::min(
              static_cast<double>(kSpectrogramMaximumFrequency),
              static_cast<double>(sourceSampleRate()) * 0.5)
        : 0.0;
}

QUrl SpectraController::spectrogramImageUrl() const {
    return spectrogramImageUrl_;
}

bool SpectraController::fullFilePlaying() const {
    return audio_clip_is_playing(&fullFileClip_) ||
        (playbackTarget_ == SourcePlayback && audio_clip_is_playing(&sourceClip_));
}

int SpectraController::fullFilePlaybackSelection() const {
    return lastFullFilePlaybackTarget_ == FullFilePlayback ? 1 : 0;
}

bool SpectraController::effectProcessing() const {
    return background_task_has_work(&effectTask_);
}

bool SpectraController::effectReady() const {
    return effectBuffer_.samples != nullptr &&
        effectBuffer_.frame_count > 0U;
}

int SpectraController::effectMode() const {
    return effectMode_;
}

QString SpectraController::effectModeName() const {
    switch (effectMode_) {
        case 0:
            return QStringLiteral("Tape speed");
        case 1:
            return QStringLiteral("Pitch shift");
        case 2:
            return QStringLiteral("Frequency shift");
        default:
            return QStringLiteral("Audio effect");
    }
}

double SpectraController::effectSemitones() const {
    return effectSemitones_;
}

double SpectraController::effectPitchFactor() const {
    return std::pow(2.0, effectSemitones_ / 12.0);
}

double SpectraController::effectFrequencyShift() const {
    return effectFrequencyShift_;
}

double SpectraController::effectProgress() const {
    return effectProgress_;
}

double SpectraController::effectOutputDuration() const {
    return effectBuffer_.sample_rate > 0U
        ? static_cast<double>(effectBuffer_.frame_count) /
              static_cast<double>(effectBuffer_.sample_rate)
        : 0.0;
}

int SpectraController::effectOutputChannels() const {
    return static_cast<int>(effectBuffer_.channel_count);
}

int SpectraController::effectWindowSize() const {
    return static_cast<int>(kEffectWindowSize);
}

int SpectraController::effectHopSize() const {
    return static_cast<int>(kEffectHopSize);
}

QVariantList SpectraController::effectOriginalSpectrum() const {
    return effectOriginalSpectrum_;
}

QVariantList SpectraController::effectTransformedSpectrum() const {
    return effectTransformedSpectrum_;
}

double SpectraController::effectSpectrumMaximumFrequency() const {
    return sourceSampleRate() > 0
        ? static_cast<double>(sourceSampleRate()) * 0.5
        : 1.0;
}

double SpectraController::effectOriginalPeakFrequency() const {
    return effectOriginalPeakFrequency_;
}

double SpectraController::effectTransformedPeakFrequency() const {
    return effectTransformedPeakFrequency_;
}

bool SpectraController::effectSpectrumPreview() const {
    return effectSpectrumPreview_;
}

bool SpectraController::effectSpectrumAnalyzing() const {
    return background_task_is_running(
        &effectSpectrumTask_);
}

bool SpectraController::effectPlaying() const {
    return audio_clip_is_playing(&effectClip_) ||
        (playbackTarget_ == SourcePlayback &&
         audio_clip_is_playing(&sourceClip_));
}

int SpectraController::effectPlaybackSelection() const {
    return lastEffectPlaybackTarget_ == EffectPlayback ? 1 : 0;
}

QStringList SpectraController::eqPresetNames() const {
    QStringList result;
    result.reserve(kEqPresetCount);
    for (int index = 0; index < kEqPresetCount; ++index) {
        result.append(
            QString::fromUtf8(kEqPresetNames[index]));
    }
    return result;
}

QStringList SpectraController::eqBandShapeNames() const {
    QStringList result;
    result.reserve(SPECTRAL_EQ_SHAPE_COUNT);
    for (int index = 0;
         index < SPECTRAL_EQ_SHAPE_COUNT;
         ++index) {
        result.append(
            QString::fromUtf8(
                kEqBandShapeNames[index]));
    }
    return result;
}

int SpectraController::eqPreset() const {
    return eqPreset_;
}

QVariantList SpectraController::eqBands() const {
    QVariantList result;
    result.reserve(
        static_cast<qsizetype>(eqBands_.size()));
    for (size_t index = 0U;
         index < eqBands_.size();
         ++index) {
        const SpectralEqBand &band = eqBands_[index];
        QVariantMap entry;
        entry.insert(
            QStringLiteral("index"),
            static_cast<int>(index));
        entry.insert(
            QStringLiteral("lowHz"),
            static_cast<double>(band.low_hz));
        entry.insert(
            QStringLiteral("highHz"),
            static_cast<double>(band.high_hz));
        entry.insert(
            QStringLiteral("gainDb"),
            static_cast<double>(band.gain_db));
        entry.insert(
            QStringLiteral("enabled"),
            band.enabled);
        entry.insert(
            QStringLiteral("shape"),
            static_cast<int>(band.shape));
        const int shapeIndex =
            static_cast<int>(band.shape) >= 0 &&
                    static_cast<int>(band.shape) <
                        SPECTRAL_EQ_SHAPE_COUNT
                ? static_cast<int>(band.shape)
                : SPECTRAL_EQ_SHAPE_RANGE;
        entry.insert(
            QStringLiteral("shapeName"),
            QString::fromUtf8(
                kEqBandShapeNames[shapeIndex]));
        result.append(entry);
    }
    return result;
}

int SpectraController::eqBandCount() const {
    return static_cast<int>(eqBands_.size());
}

bool SpectraController::eqProcessing() const {
    return background_task_has_work(&eqTask_);
}

bool SpectraController::eqReady() const {
    return eqBuffer_.samples != nullptr &&
        eqBuffer_.frame_count > 0U;
}

double SpectraController::eqProgress() const {
    return eqProgress_;
}

double SpectraController::eqOutputDuration() const {
    if (!eqReady() || eqBuffer_.sample_rate == 0U) {
        return sourceDuration();
    }
    return static_cast<double>(eqBuffer_.frame_count) /
        static_cast<double>(eqBuffer_.sample_rate);
}

int SpectraController::eqOutputChannels() const {
    return eqReady()
        ? static_cast<int>(eqBuffer_.channel_count)
        : sourceChannels();
}

QVariantList SpectraController::eqOriginalSpectrum() const {
    return effectOriginalSpectrum_;
}

QVariantList SpectraController::eqTransformedSpectrum() const {
    return eqTransformedSpectrum_;
}

double SpectraController::eqSpectrumMaximumFrequency() const {
    return effectSpectrumMaximumFrequency();
}

bool SpectraController::eqSpectrumPreview() const {
    return eqSpectrumPreview_;
}

bool SpectraController::eqSpectrumAnalyzing() const {
    return effectSpectrumAnalyzing();
}

bool SpectraController::eqPlaying() const {
    return audio_clip_is_playing(&eqClip_) ||
        (playbackTarget_ == SourcePlayback &&
         audio_clip_is_playing(&sourceClip_));
}

int SpectraController::eqPlaybackSelection() const {
    return lastEqPlaybackTarget_ == EqPlayback ? 1 : 0;
}

bool SpectraController::instantPreviewProcessing() const {
    return instantPreviewPending_ ||
        instantPreviewDebounceTimer_.isActive() ||
        background_task_has_work(&instantPreviewTask_);
}

bool SpectraController::instantPreviewReady() const {
    return instantPreviewReadyKind_ == instantPreviewKind_ &&
        instantPreviewBuffer_.samples != nullptr &&
        instantPreviewBuffer_.frame_count > 0U;
}

int SpectraController::instantPreviewKind() const {
    return static_cast<int>(instantPreviewKind_);
}

double SpectraController::instantPreviewProgress() const {
    return instantPreviewProgress_;
}

double SpectraController::instantPreviewStart() const {
    return instantPreviewStart_;
}

double SpectraController::instantPreviewDuration() const {
    return instantPreviewDuration_;
}

QString SpectraController::instantPreviewRangeLabel() const {
    if (instantPreviewDuration_ <= 0.0) {
        return QStringLiteral("No region");
    }
    const auto formatTime = [](double seconds) {
        const int totalSeconds = std::max(
            0,
            static_cast<int>(std::floor(seconds)));
        return QStringLiteral("%1:%2")
            .arg(totalSeconds / 60)
            .arg(totalSeconds % 60, 2, 10, QLatin1Char('0'));
    };
    return QStringLiteral("%1–%2")
        .arg(
            formatTime(instantPreviewStart_),
            formatTime(
                instantPreviewStart_ +
                instantPreviewDuration_));
}

bool SpectraController::instantPreviewPlaying() const {
    return audio_clip_is_playing(
               &instantPreviewOriginalClip_) ||
        audio_clip_is_playing(
               &instantPreviewProcessedClip_);
}

int SpectraController::instantPreviewPlaybackSelection() const {
    return lastInstantPreviewPlaybackTarget_ ==
            InstantPreviewProcessedPlayback
        ? 1
        : 0;
}

double SpectraController::instantPreviewPlaybackPosition() const {
    const AudioClip *clip =
        playbackTarget_ == InstantPreviewProcessedPlayback ||
                (playbackTarget_ != InstantPreviewOriginalPlayback &&
                 lastInstantPreviewPlaybackTarget_ ==
                     InstantPreviewProcessedPlayback)
            ? &instantPreviewProcessedClip_
            : &instantPreviewOriginalClip_;
    return audio_clip_position_seconds(clip);
}

double SpectraController::instantPreviewPlaybackDuration() const {
    const AudioClip *clip =
        playbackTarget_ == InstantPreviewProcessedPlayback ||
                (playbackTarget_ != InstantPreviewOriginalPlayback &&
                 lastInstantPreviewPlaybackTarget_ ==
                     InstantPreviewProcessedPlayback)
            ? &instantPreviewProcessedClip_
            : &instantPreviewOriginalClip_;
    const double duration =
        audio_clip_duration_seconds(clip);
    return duration > 0.0 ? duration : instantPreviewDuration_;
}

void SpectraController::setCurrentPage(int page) {
    const int bounded = std::max(0, std::min(page, 7));
    if (bounded == currentPage_) {
        return;
    }
    if (playbackTarget_ ==
            InstantPreviewOriginalPlayback ||
        playbackTarget_ ==
            InstantPreviewProcessedPlayback) {
        audio_clip_stop(&instantPreviewOriginalClip_);
        audio_clip_stop(&instantPreviewProcessedClip_);
        playbackTarget_ = NoPlayback;
        emit playbackChanged();
    }
    currentPage_ = bounded;
    emit currentPageChanged();
}

void SpectraController::setTextScale(double scale) {
    const double bounded = clampValue(
        scale, kMinimumTextScale, kMaximumTextScale);
    if (qFuzzyCompare(bounded, textScale_)) {
        return;
    }
    textScale_ = bounded;
    QSettings settings;
    settings.setValue(
        QStringLiteral("appearance/textScale"),
        textScale_);
    emit textScaleChanged();
}

void SpectraController::setPerformanceMode(int mode) {
    const int bounded = std::max(0, std::min(mode, 3));
    if (bounded == performanceMode_) {
        return;
    }
    performanceMode_ = bounded;
    spectra_set_parallel_thread_limit(
        threadLimitForPerformanceMode(performanceMode_));
    QSettings settings;
    settings.setValue(
        QStringLiteral("performance/mode"),
        performanceMode_);
    emit performanceChanged();
    setStatusText(
        QStringLiteral("%1 processing: up to %2 of %3 logical processors")
            .arg(performanceModeName())
            .arg(processingWorkerCount())
            .arg(hardwareThreadCount()));
}

void SpectraController::setSynthFrequency(double frequency) {
    const double bounded = clampValue(frequency, 40.0, 1200.0);
    if (qFuzzyCompare(bounded, synthFrequency_)) {
        return;
    }
    synthFrequency_ = bounded;
    emit synthChanged();
    scheduleSynthRebuild();
}

void SpectraController::setSynthDuration(double duration) {
    const double bounded = clampValue(duration, 0.2, 3.0);
    if (qFuzzyCompare(bounded, synthDuration_)) {
        return;
    }
    synthDuration_ = bounded;
    emit synthChanged();
    scheduleSynthRebuild();
}

void SpectraController::setSynthGain(double gain) {
    const double bounded = clampValue(gain, 0.05, 1.0);
    if (qFuzzyCompare(bounded, synthGain_)) {
        return;
    }
    synthGain_ = bounded;
    emit synthChanged();
    scheduleSynthRebuild();
}

void SpectraController::setHarmonicAmplitude(int index, double amplitude) {
    if (index < 0 || index >= kHarmonicCount) {
        return;
    }
    const float bounded = static_cast<float>(clampValue(amplitude, 0.0, 1.0));
    if (qFuzzyCompare(harmonics_[index].amplitude, bounded)) {
        return;
    }
    harmonics_[index].amplitude = bounded;
    synthPreset_ = -1;
    emit synthChanged();
    scheduleSynthRebuild();
}

void SpectraController::applySynthPreset(int preset) {
    const int boundedPreset = std::max(0, std::min(preset, 4));
    for (int index = 0; index < kHarmonicCount; ++index) {
        const int multiple = index + 1;
        float amplitude = 0.0f;
        switch (boundedPreset) {
            case 0:
                amplitude = multiple == 1 ? 0.8f : 0.0f;
                break;
            case 1:
                amplitude = multiple % 2 == 1 ? 0.8f / static_cast<float>(multiple) : 0.0f;
                break;
            case 2:
                amplitude = 0.65f / static_cast<float>(multiple);
                break;
            case 3:
                amplitude = multiple % 2 == 1
                    ? 0.85f / std::pow(static_cast<float>(multiple), 1.25f)
                    : 0.03f / static_cast<float>(multiple);
                break;
            case 4:
                amplitude = 0.72f * std::exp(-0.16f * static_cast<float>(multiple - 1));
                break;
        }
        harmonics_[index] = Harmonic{multiple, amplitude, 0.0f};
    }
    synthPreset_ = boundedPreset;
    emit synthChanged();
    scheduleSynthRebuild();
    setStatusText(QStringLiteral("Loaded %1 preset").arg(synthPresetName()));
}

void SpectraController::playSynth() {
    if (!audioReady_) {
        setStatusText(QStringLiteral("No audio device detected"));
        return;
    }
    haltAllAudio();
    if (audio_clip_play(&synthClip_)) {
        playbackTarget_ = SynthPlayback;
        setStatusText(QStringLiteral("Playing %1 Hz %2 tone")
                          .arg(synthFrequency_, 0, 'f', 1)
                          .arg(synthPresetName()));
        emit playbackChanged();
    }
}

void SpectraController::toggleSynthPlayback() {
    playbackTarget_ = SynthPlayback;
    if (audio_clip_is_playing(&synthClip_)) {
        audio_clip_pause(&synthClip_);
        setStatusText(QStringLiteral("Tone paused"));
    } else if (audio_clip_is_paused(&synthClip_)) {
        audio_clip_resume(&synthClip_);
        setStatusText(QStringLiteral("Tone playback resumed"));
    } else {
        playSynth();
    }
    emit playbackChanged();
}

void SpectraController::seekSynthPlayback(double position) {
    seekClip(&synthClip_, SynthPlayback, position);
}

void SpectraController::toggleSourcePlayback() {
    if (!sourceLoaded()) {
        return;
    }
    playbackTarget_ = SourcePlayback;
    if (audio_clip_is_playing(&sourceClip_)) {
        audio_clip_pause(&sourceClip_);
        setStatusText(QStringLiteral("Source playback paused"));
    } else if (audio_clip_is_paused(&sourceClip_)) {
        audio_clip_resume(&sourceClip_);
        setStatusText(QStringLiteral("Source playback resumed"));
    } else {
        haltAllAudio();
        if (audio_clip_play(&sourceClip_)) {
            playbackTarget_ = SourcePlayback;
            setStatusText(QStringLiteral("Playing imported source"));
        }
    }
    emit playbackChanged();
}

void SpectraController::seekSourcePlayback(double position) {
    seekClip(&sourceClip_, SourcePlayback, position);
}

void SpectraController::setRegionStart(double start) {
    if (!sourceLoaded()) {
        return;
    }
    const double maximumStart = std::max(0.0, sourceDuration() - 0.01);
    const double bounded = clampValue(start, 0.0, maximumStart);
    if (qFuzzyCompare(bounded, regionStart_)) {
        return;
    }
    regionStart_ = bounded;
    regionDuration_ = std::min(regionDuration_, sourceDuration() - regionStart_);
    resetAnalysis();
}

void SpectraController::setRegionDuration(double duration) {
    if (!sourceLoaded()) {
        return;
    }
    const double maximumDuration = std::max(0.01, sourceDuration() - regionStart_);
    const double bounded = clampValue(duration, 0.01, maximumDuration);
    if (qFuzzyCompare(bounded, regionDuration_)) {
        return;
    }
    regionDuration_ = bounded;
    resetAnalysis();
}

void SpectraController::setAnalysisPeakReadoutMode(int mode) {
    const int bounded = mode == 1 ? 1 : 0;
    if (bounded == analysisPeakReadoutMode_) {
        return;
    }
    analysisPeakReadoutMode_ = bounded;
    if (analysisReady_) {
        rebuildAnalysisPeaks();
        rebuildAnalysisVisualization();
    }
    setStatusText(
        analysisPeakReadoutMode_ == 0
            ? QStringLiteral("Showing interpolated spectral peaks")
            : QStringLiteral("Showing raw FFT-bin peaks"));
    emit analysisChanged();
}

void SpectraController::analyzeRegion() {
    SampleBuffer region = selectedRegion();
    if (region.samples == nullptr || region.count == 0U) {
        setStatusText(QStringLiteral("Import audio before analyzing a region"));
        return;
    }

    resetAnalysis();

    analysisSpectrumBuffer_ = compute_averaged_magnitude_spectrum(
        region.samples,
        region.count,
        region.sample_rate);
    if (analysisSpectrumBuffer_.count == 0U) {
        setStatusText(QStringLiteral("Could not compute the selected region spectrum"));
        emit analysisChanged();
        return;
    }

    const float maximumFrequency = std::min(
        20000.0f,
        static_cast<float>(region.sample_rate) * 0.5f);
    rebuildAnalysisPeaks();
    analysisPitchEstimate_ = estimate_pitch(
        &region,
        50.0f,
        std::min(2000.0f, maximumFrequency));
    if (audioReady_) {
        audio_clip_set_samples(&regionClip_, &region);
    }
    rebuildRegionModels(region);
    analysisReady_ = true;
    rebuildAnalysisVisualization();
    setStatusText(QStringLiteral("Analysis ready: %1 spectral peak%2")
                      .arg(analysisPeakCount_)
                      .arg(analysisPeakCount_ == 1 ? QString() : QStringLiteral("s")));
    emit analysisChanged();
    emit playbackChanged();
}

void SpectraController::playRegion() {
    if (!audioReady_) {
        return;
    }
    if (!regionClip_.loaded) {
        SampleBuffer region = selectedRegion();
        if (region.samples == nullptr || region.count == 0U ||
            !audio_clip_set_samples(&regionClip_, &region)) {
            setStatusText(QStringLiteral("Could not prepare the selected region for playback"));
            emit playbackChanged();
            return;
        }
    }

    haltAllAudio();
    if (audio_clip_play(&regionClip_)) {
        playbackTarget_ = RegionPlayback;
        lastLabPlaybackTarget_ = RegionPlayback;
        setStatusText(QStringLiteral("Playing selected region"));
        emit playbackChanged();
    }
}

void SpectraController::toggleRegionPlayback() {
    playbackTarget_ = RegionPlayback;
    if (audio_clip_is_playing(&regionClip_)) {
        audio_clip_pause(&regionClip_);
        setStatusText(QStringLiteral("Region playback paused"));
    } else if (audio_clip_is_paused(&regionClip_)) {
        audio_clip_resume(&regionClip_);
        setStatusText(QStringLiteral("Region playback resumed"));
    } else {
        playRegion();
    }
    emit playbackChanged();
}

void SpectraController::seekRegionPlayback(double position) {
    if (!regionClip_.loaded) {
        return;
    }
    lastLabPlaybackTarget_ = RegionPlayback;
    seekClip(&regionClip_, RegionPlayback, position);
}

void SpectraController::playHarmonicModel() {
    if (!audioReady_ || !harmonicReady()) {
        return;
    }
    haltAllAudio();
    if (audio_clip_play(&harmonicClip_)) {
        playbackTarget_ = HarmonicPlayback;
        lastLabPlaybackTarget_ = HarmonicPlayback;
        setStatusText(QStringLiteral("Playing integer-harmonic resynthesis"));
        emit playbackChanged();
    }
}

void SpectraController::playOriginalFrame() {
    if (!audioReady_ || fourierAnalysis_.windowed_frame.samples == nullptr) {
        return;
    }
    haltAllAudio();
    if (audio_clip_play(&originalFrameClip_)) {
        playbackTarget_ = OriginalFramePlayback;
        lastLabPlaybackTarget_ = OriginalFramePlayback;
        setStatusText(QStringLiteral("Playing original windowed FFT frame"));
        emit playbackChanged();
    }
}

void SpectraController::playFourierFrame() {
    if (!audioReady_ || !fourierFrameReady()) {
        return;
    }
    haltAllAudio();
    if (audio_clip_play(&fourierClip_)) {
        playbackTarget_ = FourierFramePlayback;
        lastLabPlaybackTarget_ = FourierFramePlayback;
        setStatusText(QStringLiteral("Playing Top-%1 evolving Fourier reconstruction")
                          .arg(fourierSelectedComponents_));
        emit playbackChanged();
    }
}

void SpectraController::toggleLabPlayback() {
    AudioClip *clip = nullptr;
    switch (lastLabPlaybackTarget_) {
        case HarmonicPlayback:
            clip = &harmonicClip_;
            break;
        case OriginalFramePlayback:
            clip = &originalFrameClip_;
            break;
        case FourierFramePlayback:
            clip = &fourierClip_;
            break;
        case RegionPlayback:
        default:
            clip = &regionClip_;
            break;
    }
    if (audio_clip_is_playing(clip)) {
        playbackTarget_ = lastLabPlaybackTarget_;
        audio_clip_pause(clip);
        setStatusText(QStringLiteral("Reconstruction playback paused"));
        emit playbackChanged();
        return;
    }
    if (audio_clip_is_paused(clip)) {
        playbackTarget_ = lastLabPlaybackTarget_;
        audio_clip_resume(clip);
        setStatusText(QStringLiteral("Reconstruction playback resumed"));
        emit playbackChanged();
        return;
    }
    switch (lastLabPlaybackTarget_) {
        case HarmonicPlayback:
            playHarmonicModel();
            break;
        case OriginalFramePlayback:
            playOriginalFrame();
            break;
        case FourierFramePlayback:
            playFourierFrame();
            break;
        case RegionPlayback:
        default:
            playRegion();
            break;
    }
}

void SpectraController::seekLabPlayback(double position) {
    AudioClip *clip = nullptr;
    switch (lastLabPlaybackTarget_) {
        case HarmonicPlayback:
            clip = &harmonicClip_;
            break;
        case OriginalFramePlayback:
            clip = &originalFrameClip_;
            break;
        case FourierFramePlayback:
            clip = &fourierClip_;
            break;
        case RegionPlayback:
        default:
            clip = &regionClip_;
            break;
    }
    seekClip(clip, lastLabPlaybackTarget_, position);
}

void SpectraController::rebuildFourierFrame(int componentCount) {
    if (rebuildFourierFrameBuffer(componentCount)) {
        setStatusText(QStringLiteral("Rebuilt passage with %1 Fourier bins per frame")
                          .arg(fourierSelectedComponents_));
        emit reconstructionChanged();
        emit playbackChanged();
    }
}

bool SpectraController::exportHarmonicFile(const QUrl &url) {
    const QString localPath = url.isLocalFile() ? url.toLocalFile() : url.toString();
    if (localPath.isEmpty() || !harmonicReady()) {
        return false;
    }
    const QString wavPath = localPath.endsWith(QStringLiteral(".wav"), Qt::CaseInsensitive)
        ? localPath
        : localPath + QStringLiteral(".wav");
    const QByteArray encodedPath = wavPath.toUtf8();
    const bool exported = export_samples_to_wav(encodedPath.constData(), &harmonicBuffer_);
    setStatusText(exported
                      ? QStringLiteral("Exported harmonic reconstruction")
                      : QStringLiteral("Could not export harmonic reconstruction"));
    return exported;
}

bool SpectraController::exportFourierFile(const QUrl &url) {
    const QString localPath = url.isLocalFile() ? url.toLocalFile() : url.toString();
    if (localPath.isEmpty() || !fourierFrameReady()) {
        return false;
    }
    const QString wavPath = localPath.endsWith(QStringLiteral(".wav"), Qt::CaseInsensitive)
        ? localPath
        : localPath + QStringLiteral(".wav");
    const QByteArray encodedPath = wavPath.toUtf8();
    const bool exported = export_samples_to_wav(encodedPath.constData(), &fourierBuffer_);
    setStatusText(exported
                      ? QStringLiteral("Exported evolving Fourier reconstruction")
                      : QStringLiteral("Could not export Fourier reconstruction"));
    return exported;
}

void SpectraController::setFullFileMode(int mode) {
    const int bounded = mode == 1 ? 1 : 0;
    if (bounded == fullFileMode_) {
        return;
    }

    cacheFullFileOutput();
    fullFileTimer_.stop();
    background_task_cancel_and_join(&fullFileTask_);
    if (fullFileWork_ == SpectrogramBuild) {
        background_task_cancel_and_join(&spectrogramTask_);
        stft_reconstruction_job_free(&spectrogramJob_);
        stft_reconstruction_job_init(&spectrogramJob_);
    } else if (fullFileWork_ == GlobalAnalysis ||
        fullFileWork_ == GlobalReconstruction) {
        global_fourier_job_free(&globalFourierJob_);
        global_fourier_job_init(&globalFourierJob_);
        global_fourier_job_free(&globalFourierJobRight_);
        global_fourier_job_init(&globalFourierJobRight_);
    } else if (fullFileWork_ == StftReconstruction) {
        stft_reconstruction_job_free(&fullFileStftJob_);
        stft_reconstruction_job_init(&fullFileStftJob_);
        stft_reconstruction_job_free(&fullFileStftJobRight_);
        stft_reconstruction_job_init(&fullFileStftJobRight_);
    }
    fullFileWork_ = FullFileIdle;
    clearFullFileOutput();
    fullFileMode_ = bounded;
    if (fullFileMode_ == 0) {
        fullFileSelectedComponents_ = globalSelectedComponents_;
        if (fullFileSelectionMode_ == 1 &&
            hasReusableGlobalAnalysis()) {
            int resolved =
                global_fourier_job_component_count_for_energy(
                    &globalFourierJob_,
                    static_cast<float>(fullFileEnergyTarget_));
            if (fullFileChannelMode_ == 1 &&
                globalFourierJobRight_.analysis_ready) {
                resolved = std::max(
                    resolved,
                    global_fourier_job_component_count_for_energy(
                        &globalFourierJobRight_,
                        static_cast<float>(
                            fullFileEnergyTarget_)));
            }
            if (resolved > 0) {
                fullFileSelectedComponents_ = resolved;
            }
        }
    } else {
        fullFileSelectedComponents_ = stftSelectedComponents_;
    }
    fullFileProgress_ = 0.0;
    setStatusText(
        fullFileMode_ == 0
            ? QStringLiteral("Whole-file model set to fixed global FFT")
            : QStringLiteral("Whole-file model set to time-varying STFT"));
    emit fullFileChanged();
    emit playbackChanged();
}

void SpectraController::setFullFileChannelMode(int mode) {
    int bounded = mode == 1 ? 1 : 0;
    if (sourceChannels() < 2) {
        bounded = 0;
    }
    if (fullFileMode_ != 0) {
        setFullFileMode(0);
    }
    if (bounded == fullFileChannelMode_) {
        return;
    }
    if (fullFileProcessing()) {
        return;
    }

    haltAllAudio();
    cacheFullFileOutput();
    global_fourier_job_free(&globalFourierJob_);
    global_fourier_job_init(&globalFourierJob_);
    global_fourier_job_free(&globalFourierJobRight_);
    global_fourier_job_init(&globalFourierJobRight_);
    clearFullFileOutput();
    fullFileChannelMode_ = bounded;
    fullFileProgress_ = 0.0;
    setStatusText(
        fullFileChannelMode_ == 1
            ? QStringLiteral("Whole-file FFT set to source channels")
            : QStringLiteral("Whole-file FFT set to mono"));
    emit fullFileChanged();
    emit playbackChanged();
}

void SpectraController::setFullFileSelectionMode(int mode) {
    const int bounded = mode == 1 ? 1 : 0;
    if (fullFileMode_ != 0) {
        setFullFileMode(0);
    }
    if (bounded == fullFileSelectionMode_) {
        return;
    }
    if (fullFileProcessing()) {
        return;
    }
    cacheFullFileOutput();
    clearFullFileOutput();
    fullFileSelectionMode_ = bounded;
    if (fullFileSelectionMode_ == 0) {
        fullFileSelectedComponents_ = globalSelectedComponents_;
    } else if (hasReusableGlobalAnalysis()) {
        int resolved =
            global_fourier_job_component_count_for_energy(
                &globalFourierJob_,
                static_cast<float>(fullFileEnergyTarget_));
        if (fullFileChannelMode_ == 1 &&
            globalFourierJobRight_.analysis_ready) {
            resolved = std::max(
                resolved,
                global_fourier_job_component_count_for_energy(
                    &globalFourierJobRight_,
                    static_cast<float>(fullFileEnergyTarget_)));
        }
        if (resolved > 0) {
            fullFileSelectedComponents_ = resolved;
        }
    }
    setStatusText(
        fullFileSelectionMode_ == 1
            ? QStringLiteral("Global FFT selection set to spectral energy")
            : QStringLiteral("Global FFT selection set to ranked bins"));
    emit fullFileChanged();
    emit playbackChanged();
}

void SpectraController::setFullFileEnergyTarget(double target) {
    const double bounded = clampValue(target, 0.0001, 1.0);
    if (qFuzzyCompare(bounded, fullFileEnergyTarget_) &&
        fullFileSelectionMode_ == 1) {
        return;
    }
    if (fullFileProcessing()) {
        return;
    }
    fullFileEnergyTarget_ = bounded;
    if (fullFileMode_ != 0) {
        setFullFileMode(0);
    }
    cacheFullFileOutput();
    clearFullFileOutput();
    fullFileSelectionMode_ = 1;
    if (hasReusableGlobalAnalysis()) {
        int resolved =
            global_fourier_job_component_count_for_energy(
                &globalFourierJob_,
                static_cast<float>(fullFileEnergyTarget_));
        if (fullFileChannelMode_ == 1 &&
            globalFourierJobRight_.analysis_ready) {
            resolved = std::max(
                resolved,
                global_fourier_job_component_count_for_energy(
                    &globalFourierJobRight_,
                    static_cast<float>(fullFileEnergyTarget_)));
        }
        if (resolved > 0) {
            fullFileSelectedComponents_ = resolved;
        }
    }
    setStatusText(
        QStringLiteral("Global FFT energy target set to %1%")
            .arg(fullFileEnergyTarget_ * 100.0, 0, 'f', 2));
    emit fullFileChanged();
    emit playbackChanged();
}

void SpectraController::setFullFileMemoryLimitBytes(
    qulonglong bytes) {
    const qulonglong bounded =
        std::max(
            kMinimumModelMemoryBytes,
            std::min(bytes, kMaximumModelMemoryBytes));
    if (bounded == fullFileMemoryLimitBytes_) {
        return;
    }
    fullFileMemoryLimitBytes_ = bounded;
    QSettings settings;
    settings.setValue(
        QStringLiteral("models/memoryLimitBytes"),
        fullFileMemoryLimitBytes_);
    setStatusText(
        QStringLiteral("Whole-file model memory limit set to %1 MB")
            .arg(static_cast<double>(bounded) /
                (1024.0 * 1024.0), 0, 'f', 0));
    emit fullFileChanged();
}

void SpectraController::setFullFileComponentCount(int componentCount) {
    if (fullFileProcessing()) {
        return;
    }
    const int maximum = fullFileMaximumComponents();
    const int bounded = std::max(1, std::min(componentCount, maximum));
    const bool switchesGlobalSelection =
        fullFileMode_ == 0 && fullFileSelectionMode_ != 0;
    if (bounded == fullFileSelectedComponents_ &&
        !switchesGlobalSelection) {
        return;
    }
    cacheFullFileOutput();
    clearFullFileOutput();
    fullFileSelectedComponents_ = bounded;
    if (fullFileMode_ == 0) {
        fullFileSelectionMode_ = 0;
        globalSelectedComponents_ = bounded;
    } else {
        stftSelectedComponents_ = bounded;
    }
    emit fullFileChanged();
    emit playbackChanged();
}

void SpectraController::buildFullFileModel() {
    if (!sourceLoaded()) {
        setStatusText(QStringLiteral("Import audio before building a whole-file model"));
        return;
    }
    if (fullFileProcessing()) {
        return;
    }

    lastFullFilePlaybackTarget_ = FullFilePlayback;
    emit playbackChanged();
    clearFullFileOutput();
    fullFileProgress_ = 0.0;
    if (restoreFullFileOutput()) {
        emit fullFileChanged();
        emit playbackChanged();
        return;
    }
    const bool started =
        fullFileMode_ == 0
            ? (hasReusableGlobalAnalysis()
                   ? startGlobalReconstruction()
                   : startGlobalAnalysis())
            : startStftReconstruction();
    if (!started) {
        fullFileWork_ = FullFileIdle;
        emit fullFileChanged();
    }
}

void SpectraController::playFullFileOriginal() {
    if (!sourceLoaded()) {
        return;
    }
    lastFullFilePlaybackTarget_ = SourcePlayback;
    emit playbackChanged();
    if (!audioReady_) {
        return;
    }
    haltAllAudio();
    if (audio_clip_play(&sourceClip_)) {
        playbackTarget_ = SourcePlayback;
        setStatusText(QStringLiteral("Playing original imported source"));
        emit playbackChanged();
    }
}

void SpectraController::playFullFileReconstruction() {
    if (!fullFileReady()) {
        return;
    }
    lastFullFilePlaybackTarget_ = FullFilePlayback;
    emit playbackChanged();
    if (!audioReady_) {
        return;
    }
    haltAllAudio();
    if (audio_clip_play(&fullFileClip_)) {
        playbackTarget_ = FullFilePlayback;
        setStatusText(
            fullFileMode_ == 0
                ? QStringLiteral("Playing fixed whole-file FFT reconstruction")
                : QStringLiteral("Playing time-varying STFT reconstruction"));
        emit playbackChanged();
    }
}

void SpectraController::toggleFullFilePlayback() {
    AudioClip *clip = nullptr;
    if (playbackTarget_ == FullFilePlayback) {
        clip = &fullFileClip_;
    } else if (playbackTarget_ == SourcePlayback) {
        clip = &sourceClip_;
    }
    if (clip == nullptr) {
        if (lastFullFilePlaybackTarget_ == FullFilePlayback &&
            fullFileReady()) {
            playFullFileReconstruction();
        } else {
            playFullFileOriginal();
        }
        return;
    }

    if (audio_clip_is_playing(clip)) {
        audio_clip_pause(clip);
        setStatusText(QStringLiteral("Whole-file playback paused"));
    } else if (audio_clip_is_paused(clip)) {
        audio_clip_resume(clip);
        setStatusText(QStringLiteral("Whole-file playback resumed"));
    } else if (clip == &fullFileClip_) {
        playFullFileReconstruction();
        return;
    } else {
        playFullFileOriginal();
        return;
    }
    emit playbackChanged();
}

void SpectraController::seekFullFilePlayback(double position) {
    PlaybackTarget target = playbackTarget_;
    AudioClip *clip = nullptr;
    if (target == FullFilePlayback) {
        clip = &fullFileClip_;
    } else if (target == SourcePlayback) {
        clip = &sourceClip_;
    } else if (lastFullFilePlaybackTarget_ == FullFilePlayback &&
        fullFileReady()) {
        target = FullFilePlayback;
        clip = &fullFileClip_;
    } else {
        target = SourcePlayback;
        clip = &sourceClip_;
    }
    seekClip(clip, target, position);
}

bool SpectraController::exportFullFileFile(const QUrl &url) {
    const QString localPath = url.isLocalFile() ? url.toLocalFile() : url.toString();
    if (localPath.isEmpty() || !fullFileReady()) {
        return false;
    }
    const QString wavPath = localPath.endsWith(QStringLiteral(".wav"), Qt::CaseInsensitive)
        ? localPath
        : localPath + QStringLiteral(".wav");
    const QByteArray encodedPath = wavPath.toUtf8();
    const bool exported =
        export_interleaved_to_wav(
            encodedPath.constData(),
            &fullFileBuffer_);
    setStatusText(
        exported
            ? QStringLiteral("Exported whole-file reconstruction")
            : QStringLiteral("Could not export whole-file reconstruction"));
    return exported;
}

bool SpectraController::exportSpectrogramPdf(const QUrl &url) {
    const QString localPath = url.isLocalFile() ? url.toLocalFile() : url.toString();
    if (localPath.isEmpty() || spectrogramImageUrl_.isEmpty()) {
        setStatusText(QStringLiteral("Build the spectrogram before exporting it"));
        return false;
    }

    const QString pdfPath = localPath.endsWith(
                                QStringLiteral(".pdf"),
                                Qt::CaseInsensitive)
        ? localPath
        : localPath + QStringLiteral(".pdf");
    const QString encodedImage = spectrogramImageUrl_.toString();
    const qsizetype payloadOffset = encodedImage.indexOf(QLatin1Char(','));
    QImage spectrogram;
    if (payloadOffset < 0 ||
        !spectrogram.loadFromData(
            QByteArray::fromBase64(
                encodedImage.mid(payloadOffset + 1).toLatin1()),
            "PNG")) {
        setStatusText(QStringLiteral("Could not read the spectrogram image"));
        return false;
    }

    SpectrogramPdfMetadata metadata;
    metadata.sourceName = sourceFileName();
    metadata.durationSeconds = sourceDuration();
    metadata.sampleRate = sourceSampleRate();
    metadata.channelCount = sourceChannels();
    metadata.maximumFrequency = spectrogramMaximumFrequency();
    metadata.windowSize = static_cast<int>(kFullFileWindowSize);
    metadata.hopSize = static_cast<int>(kFullFileHopSize);
    metadata.frameCount = importedAudio_.mono.count == 0U
        ? 0
        : static_cast<qsizetype>(
              1U +
              (importedAudio_.mono.count - 1U) /
                  static_cast<size_t>(kFullFileHopSize));

    const bool exported = exportSpectrogramLandscapePdf(
        pdfPath,
        spectrogram,
        metadata);
    setStatusText(
        exported
            ? QStringLiteral("Exported landscape spectrogram PDF")
            : QStringLiteral("Could not export the spectrogram PDF"));
    return exported;
}

void SpectraController::setEffectMode(int mode) {
    if (effectProcessing()) {
        return;
    }
    const int bounded = std::max(0, std::min(mode, 2));
    if (bounded == effectMode_) {
        return;
    }
    clearEffectOutput();
    effectMode_ = bounded;
    effectProgress_ = 0.0;
    rebuildEffectSpectrumPreview();
    if (currentPage_ == 5 ||
        instantPreviewKind_ == EffectInstantPreview) {
        scheduleInstantPreview(
            EffectInstantPreview, false);
    }
    emit effectChanged();
    emit playbackChanged();
}

void SpectraController::setEffectSemitones(double semitones) {
    if (effectProcessing()) {
        return;
    }
    const double bounded =
        clampValue(semitones, -12.0, 12.0);
    if (qFuzzyCompare(
            bounded + 13.0,
            effectSemitones_ + 13.0)) {
        return;
    }
    clearEffectOutput();
    effectSemitones_ = bounded;
    effectProgress_ = 0.0;
    rebuildEffectSpectrumPreview();
    if (currentPage_ == 5 ||
        instantPreviewKind_ == EffectInstantPreview) {
        scheduleInstantPreview(
            EffectInstantPreview, false);
    }
    emit effectChanged();
    emit playbackChanged();
}

void SpectraController::setEffectFrequencyShift(double shiftHz) {
    if (effectProcessing()) {
        return;
    }
    const double maximum =
        sourceSampleRate() > 0
            ? std::min(
                  5000.0,
                  static_cast<double>(
                      sourceSampleRate()) *
                      0.5)
            : 5000.0;
    const double bounded =
        clampValue(shiftHz, -maximum, maximum);
    if (qFuzzyCompare(
            bounded + maximum + 1.0,
            effectFrequencyShift_ + maximum + 1.0)) {
        return;
    }
    clearEffectOutput();
    effectFrequencyShift_ = bounded;
    effectProgress_ = 0.0;
    rebuildEffectSpectrumPreview();
    if (currentPage_ == 5 ||
        instantPreviewKind_ == EffectInstantPreview) {
        scheduleInstantPreview(
            EffectInstantPreview, false);
    }
    emit effectChanged();
    emit playbackChanged();
}

void SpectraController::buildEffect() {
    if (!sourceLoaded()) {
        setStatusText(
            QStringLiteral(
                "Import audio before building an effect"));
        return;
    }
    if (effectProcessing()) {
        return;
    }

    haltAllAudio();
    clearEffectOutput();
    rebuildEffectSpectrumPreview();
    interleaved_buffer_free(&effectWorkerBuffer_);
    spectrum_free(&effectWorkerSpectrum_);
    effectWorkerMode_ = effectMode_;
    effectWorkerAmount_ =
        effectMode_ == 2
            ? effectFrequencyShift_
            : effectSemitones_;
    effectWorkerSucceeded_ = false;
    effectProgress_ = 0.0;

    if (!background_task_start(
            &effectTask_,
            &SpectraController::processEffectInBackground,
            this)) {
        setStatusText(
            QStringLiteral(
                "Could not start effect processing"));
        emit effectChanged();
        emit playbackChanged();
        return;
    }

    effectTimer_.start();
    setStatusText(
        QStringLiteral("Building %1")
            .arg(effectModeName().toLower()));
    emit effectChanged();
    emit playbackChanged();
}

void SpectraController::playEffectOriginal() {
    if (!sourceLoaded()) {
        return;
    }
    lastEffectPlaybackTarget_ = SourcePlayback;
    emit playbackChanged();
    if (!audioReady_) {
        return;
    }
    haltAllAudio();
    if (audio_clip_play(&sourceClip_)) {
        playbackTarget_ = SourcePlayback;
        setStatusText(
            QStringLiteral(
                "Playing original imported source"));
        emit playbackChanged();
    }
}

void SpectraController::playEffectProcessed() {
    if (!effectReady()) {
        return;
    }
    lastEffectPlaybackTarget_ = EffectPlayback;
    emit playbackChanged();
    if (!audioReady_) {
        return;
    }
    haltAllAudio();
    if (audio_clip_play(&effectClip_)) {
        playbackTarget_ = EffectPlayback;
        setStatusText(
            QStringLiteral("Playing %1 output")
                .arg(effectModeName().toLower()));
        emit playbackChanged();
    }
}

void SpectraController::toggleEffectPlayback() {
    AudioClip *clip = nullptr;
    if (playbackTarget_ == EffectPlayback) {
        clip = &effectClip_;
    } else if (playbackTarget_ == SourcePlayback) {
        clip = &sourceClip_;
    }
    if (clip == nullptr) {
        if (lastEffectPlaybackTarget_ == EffectPlayback &&
            effectReady()) {
            playEffectProcessed();
        } else {
            playEffectOriginal();
        }
        return;
    }

    if (audio_clip_is_playing(clip)) {
        audio_clip_pause(clip);
        setStatusText(
            QStringLiteral("Effect playback paused"));
    } else if (audio_clip_is_paused(clip)) {
        audio_clip_resume(clip);
        setStatusText(
            QStringLiteral("Effect playback resumed"));
    } else if (clip == &effectClip_) {
        playEffectProcessed();
        return;
    } else {
        playEffectOriginal();
        return;
    }
    emit playbackChanged();
}

void SpectraController::seekEffectPlayback(double position) {
    PlaybackTarget target = playbackTarget_;
    AudioClip *clip = nullptr;
    if (target == EffectPlayback) {
        clip = &effectClip_;
    } else if (target == SourcePlayback) {
        clip = &sourceClip_;
    } else if (lastEffectPlaybackTarget_ == EffectPlayback &&
               effectReady()) {
        target = EffectPlayback;
        clip = &effectClip_;
    } else {
        target = SourcePlayback;
        clip = &sourceClip_;
    }
    seekClip(clip, target, position);
}

bool SpectraController::exportEffectFile(const QUrl &url) {
    const QString localPath =
        url.isLocalFile()
            ? url.toLocalFile()
            : url.toString();
    if (localPath.isEmpty() || !effectReady()) {
        return false;
    }
    const QString wavPath =
        localPath.endsWith(
            QStringLiteral(".wav"),
            Qt::CaseInsensitive)
            ? localPath
            : localPath + QStringLiteral(".wav");
    const QByteArray encodedPath = wavPath.toUtf8();
    const bool exported =
        export_interleaved_to_wav(
            encodedPath.constData(),
            &effectBuffer_);
    setStatusText(
        exported
            ? QStringLiteral("Exported %1 output")
                  .arg(effectModeName().toLower())
            : QStringLiteral(
                  "Could not export effect output"));
    return exported;
}

int SpectraController::addEqBand(
    double lowHz,
    double highHz,
    double gainDb) {
    if (!sourceLoaded() ||
        eqProcessing() ||
        eqBands_.size() >= kMaximumEqBands) {
        return -1;
    }
    const double maximum =
        static_cast<double>(sourceSampleRate()) * 0.5;
    double low = clampValue(
        std::min(lowHz, highHz), 0.0, maximum);
    double high = clampValue(
        std::max(lowHz, highHz), 0.0, maximum);
    const double minimumWidth = std::min(
        20.0, maximum);
    if (high - low < minimumWidth) {
        high = std::min(maximum, low + minimumWidth);
        low = std::max(0.0, high - minimumWidth);
    }
    eqBands_.push_back(SpectralEqBand{
        static_cast<float>(low),
        static_cast<float>(high),
        static_cast<float>(
            clampValue(gainDb, -24.0, 24.0)),
        true,
        SPECTRAL_EQ_SHAPE_RANGE,
    });
    eqPreset_ = -1;
    clearEqOutput();
    rebuildEqSpectrumPreview();
    if (currentPage_ == 6 ||
        instantPreviewKind_ == EqInstantPreview) {
        scheduleInstantPreview(
            EqInstantPreview, false);
    }
    emit eqChanged();
    emit playbackChanged();
    return static_cast<int>(eqBands_.size() - 1U);
}

void SpectraController::updateEqBand(
    int index,
    double lowHz,
    double highHz,
    double gainDb) {
    if (eqProcessing() || index < 0 ||
        static_cast<size_t>(index) >= eqBands_.size()) {
        return;
    }
    const double maximum =
        sourceLoaded()
            ? static_cast<double>(
                  sourceSampleRate()) * 0.5
            : 22050.0;
    double low = clampValue(
        std::min(lowHz, highHz), 0.0, maximum);
    double high = clampValue(
        std::max(lowHz, highHz), 0.0, maximum);
    const double minimumWidth = std::min(
        20.0, maximum);
    if (high - low < minimumWidth) {
        high = std::min(maximum, low + minimumWidth);
        low = std::max(0.0, high - minimumWidth);
    }
    const SpectralEqBand updated = {
        static_cast<float>(low),
        static_cast<float>(high),
        static_cast<float>(
            clampValue(gainDb, -24.0, 24.0)),
        eqBands_[static_cast<size_t>(index)].enabled,
        eqBands_[static_cast<size_t>(index)].shape,
    };
    const SpectralEqBand &current =
        eqBands_[static_cast<size_t>(index)];
    if (std::abs(current.low_hz - updated.low_hz) <
            0.001f &&
        std::abs(current.high_hz - updated.high_hz) <
            0.001f &&
        std::abs(current.gain_db - updated.gain_db) <
            0.001f) {
        return;
    }
    eqBands_[static_cast<size_t>(index)] = updated;
    eqPreset_ = -1;
    clearEqOutput();
    rebuildEqSpectrumPreview();
    if (currentPage_ == 6 ||
        instantPreviewKind_ == EqInstantPreview) {
        scheduleInstantPreview(
            EqInstantPreview, false);
    }
    emit eqChanged();
    emit playbackChanged();
}

void SpectraController::setEqBandEnabled(
    int index,
    bool enabled) {
    if (eqProcessing() || index < 0 ||
        static_cast<size_t>(index) >= eqBands_.size() ||
        eqBands_[static_cast<size_t>(index)].enabled ==
            enabled) {
        return;
    }
    eqBands_[static_cast<size_t>(index)].enabled =
        enabled;
    eqPreset_ = -1;
    clearEqOutput();
    rebuildEqSpectrumPreview();
    if (currentPage_ == 6 ||
        instantPreviewKind_ == EqInstantPreview) {
        scheduleInstantPreview(
            EqInstantPreview, false);
    }
    emit eqChanged();
    emit playbackChanged();
}

void SpectraController::setEqBandShape(
    int index,
    int shape) {
    if (eqProcessing() || index < 0 ||
        static_cast<size_t>(index) >= eqBands_.size()) {
        return;
    }
    const SpectralEqBandShape bounded =
        shape >= 0 && shape < SPECTRAL_EQ_SHAPE_COUNT
            ? static_cast<SpectralEqBandShape>(shape)
            : SPECTRAL_EQ_SHAPE_RANGE;
    SpectralEqBand &band =
        eqBands_[static_cast<size_t>(index)];
    if (band.shape == bounded) {
        return;
    }
    band.shape = bounded;
    eqPreset_ = -1;
    clearEqOutput();
    rebuildEqSpectrumPreview();
    if (currentPage_ == 6 ||
        instantPreviewKind_ == EqInstantPreview) {
        scheduleInstantPreview(
            EqInstantPreview, false);
    }
    emit eqChanged();
    emit playbackChanged();
}

void SpectraController::removeEqBand(int index) {
    if (eqProcessing() || index < 0 ||
        static_cast<size_t>(index) >= eqBands_.size()) {
        return;
    }
    eqBands_.erase(
        eqBands_.begin() +
        static_cast<std::ptrdiff_t>(index));
    eqPreset_ = -1;
    clearEqOutput();
    rebuildEqSpectrumPreview();
    if (currentPage_ == 6 ||
        instantPreviewKind_ == EqInstantPreview) {
        scheduleInstantPreview(
            EqInstantPreview, false);
    }
    emit eqChanged();
    emit playbackChanged();
}

void SpectraController::clearEqBands() {
    if (eqProcessing() || eqBands_.empty()) {
        return;
    }
    eqBands_.clear();
    eqPreset_ = -1;
    clearEqOutput();
    rebuildEqSpectrumPreview();
    if (currentPage_ == 6 ||
        instantPreviewKind_ == EqInstantPreview) {
        scheduleInstantPreview(
            EqInstantPreview, false);
    }
    emit eqChanged();
    emit playbackChanged();
}

void SpectraController::applyEqPreset(int preset) {
    if (!sourceLoaded() ||
        eqProcessing() ||
        preset < 0 ||
        preset >= kEqPresetCount) {
        return;
    }
    const double maximum =
        static_cast<double>(sourceSampleRate()) * 0.5;
    std::vector<SpectralEqBand> bands =
        buildEqPresetBands(preset, maximum);
    if (bands.empty()) {
        return;
    }
    eqBands_ = std::move(bands);
    eqPreset_ = preset;
    clearEqOutput();
    rebuildEqSpectrumPreview();
    if (currentPage_ == 6 ||
        instantPreviewKind_ == EqInstantPreview) {
        scheduleInstantPreview(
            EqInstantPreview, false);
    }
    emit eqChanged();
    emit playbackChanged();
    setStatusText(
        QStringLiteral("Loaded %1 EQ preset")
            .arg(QString::fromUtf8(
                kEqPresetNames[preset])));
}

void SpectraController::buildEq() {
    if (!sourceLoaded() || eqProcessing()) {
        return;
    }
    const bool hasEnabledBand =
        std::any_of(
            eqBands_.begin(),
            eqBands_.end(),
            [](const SpectralEqBand &band) {
                return band.enabled;
            });
    if (!hasEnabledBand) {
        setStatusText(
            QStringLiteral(
                "Add or enable an EQ band before building"));
        return;
    }

    haltAllAudio();
    clearEqOutput();
    interleaved_buffer_free(&eqWorkerBuffer_);
    spectrum_free(&eqWorkerSpectrum_);
    eqWorkerBands_ = eqBands_;
    eqWorkerSucceeded_ = false;
    eqProgress_ = 0.0;
    if (!background_task_start(
            &eqTask_,
            &SpectraController::processEqInBackground,
            this)) {
        setStatusText(
            QStringLiteral(
                "Could not start range EQ processing"));
        emit eqChanged();
        emit playbackChanged();
        return;
    }

    eqTimer_.start();
    setStatusText(
        QStringLiteral("Building range EQ output"));
    emit eqChanged();
    emit playbackChanged();
}

void SpectraController::playEqOriginal() {
    if (!sourceLoaded()) {
        return;
    }
    lastEqPlaybackTarget_ = SourcePlayback;
    emit playbackChanged();
    if (!audioReady_) {
        return;
    }
    haltAllAudio();
    if (audio_clip_play(&sourceClip_)) {
        playbackTarget_ = SourcePlayback;
        setStatusText(
            QStringLiteral(
                "Playing original imported source"));
        emit playbackChanged();
    }
}

void SpectraController::playEqProcessed() {
    if (!eqReady()) {
        return;
    }
    lastEqPlaybackTarget_ = EqPlayback;
    emit playbackChanged();
    if (!audioReady_) {
        return;
    }
    haltAllAudio();
    if (audio_clip_play(&eqClip_)) {
        playbackTarget_ = EqPlayback;
        setStatusText(
            QStringLiteral("Playing range EQ output"));
        emit playbackChanged();
    }
}

void SpectraController::toggleEqPlayback() {
    AudioClip *clip = nullptr;
    if (playbackTarget_ == EqPlayback) {
        clip = &eqClip_;
    } else if (playbackTarget_ == SourcePlayback) {
        clip = &sourceClip_;
    }
    if (clip == nullptr) {
        if (lastEqPlaybackTarget_ == EqPlayback &&
            eqReady()) {
            playEqProcessed();
        } else {
            playEqOriginal();
        }
        return;
    }

    if (audio_clip_is_playing(clip)) {
        audio_clip_pause(clip);
        setStatusText(
            QStringLiteral("EQ playback paused"));
    } else if (audio_clip_is_paused(clip)) {
        audio_clip_resume(clip);
        setStatusText(
            QStringLiteral("EQ playback resumed"));
    } else if (clip == &eqClip_) {
        playEqProcessed();
        return;
    } else {
        playEqOriginal();
        return;
    }
    emit playbackChanged();
}

void SpectraController::seekEqPlayback(
    double position) {
    PlaybackTarget target = playbackTarget_;
    AudioClip *clip = nullptr;
    if (target == EqPlayback) {
        clip = &eqClip_;
    } else if (target == SourcePlayback) {
        clip = &sourceClip_;
    } else if (lastEqPlaybackTarget_ == EqPlayback &&
               eqReady()) {
        target = EqPlayback;
        clip = &eqClip_;
    } else {
        target = SourcePlayback;
        clip = &sourceClip_;
    }
    seekClip(clip, target, position);
}

bool SpectraController::exportEqFile(
    const QUrl &url) {
    const QString localPath =
        url.isLocalFile()
            ? url.toLocalFile()
            : url.toString();
    if (localPath.isEmpty() || !eqReady()) {
        return false;
    }
    const QString wavPath =
        localPath.endsWith(
            QStringLiteral(".wav"),
            Qt::CaseInsensitive)
            ? localPath
            : localPath + QStringLiteral(".wav");
    const QByteArray encodedPath = wavPath.toUtf8();
    const bool exported =
        export_interleaved_to_wav(
            encodedPath.constData(),
            &eqBuffer_);
    setStatusText(
        exported
            ? QStringLiteral(
                  "Exported range EQ output")
            : QStringLiteral(
                  "Could not export range EQ output"));
    return exported;
}

void SpectraController::requestInstantPreview(
    int kind,
    bool recenter) {
    if (!sourceLoaded() ||
        kind < EffectInstantPreview ||
        kind > EqInstantPreview) {
        return;
    }
    const InstantPreviewKind requestedKind =
        static_cast<InstantPreviewKind>(kind);
    if (!recenter &&
        requestedKind == instantPreviewKind_ &&
        instantPreviewReady() &&
        !instantPreviewProcessing()) {
        return;
    }
    scheduleInstantPreview(requestedKind, recenter);
}

void SpectraController::playInstantPreviewOriginal() {
    if (instantPreviewSourceBuffer_.samples == nullptr ||
        instantPreviewSourceBuffer_.frame_count == 0U) {
        requestInstantPreview(
            static_cast<int>(instantPreviewKind_),
            true);
        return;
    }
    lastInstantPreviewPlaybackTarget_ =
        InstantPreviewOriginalPlayback;
    emit playbackChanged();
    if (!audioReady_) {
        return;
    }
    haltAllAudio();
    if (audio_clip_play(&instantPreviewOriginalClip_)) {
        playbackTarget_ =
            InstantPreviewOriginalPlayback;
        setStatusText(
            QStringLiteral("Playing original preview"));
        emit playbackChanged();
    }
}

void SpectraController::playInstantPreviewProcessed() {
    if (!instantPreviewReady()) {
        return;
    }
    lastInstantPreviewPlaybackTarget_ =
        InstantPreviewProcessedPlayback;
    emit playbackChanged();
    if (!audioReady_) {
        return;
    }
    haltAllAudio();
    if (audio_clip_play(&instantPreviewProcessedClip_)) {
        playbackTarget_ =
            InstantPreviewProcessedPlayback;
        setStatusText(
            QStringLiteral("Playing processed preview"));
        emit playbackChanged();
    }
}

void SpectraController::toggleInstantPreviewPlayback() {
    AudioClip *clip = nullptr;
    if (playbackTarget_ ==
        InstantPreviewProcessedPlayback) {
        clip = &instantPreviewProcessedClip_;
    } else if (playbackTarget_ ==
               InstantPreviewOriginalPlayback) {
        clip = &instantPreviewOriginalClip_;
    }
    if (clip == nullptr) {
        if (lastInstantPreviewPlaybackTarget_ ==
                InstantPreviewProcessedPlayback &&
            instantPreviewReady()) {
            playInstantPreviewProcessed();
        } else {
            playInstantPreviewOriginal();
        }
        return;
    }

    if (audio_clip_is_playing(clip)) {
        audio_clip_pause(clip);
    } else if (audio_clip_is_paused(clip)) {
        audio_clip_resume(clip);
    } else if (clip == &instantPreviewProcessedClip_) {
        playInstantPreviewProcessed();
        return;
    } else {
        playInstantPreviewOriginal();
        return;
    }
    emit playbackChanged();
}

void SpectraController::seekInstantPreviewPlayback(
    double position) {
    PlaybackTarget target =
        lastInstantPreviewPlaybackTarget_;
    AudioClip *clip =
        target == InstantPreviewProcessedPlayback
            ? &instantPreviewProcessedClip_
            : &instantPreviewOriginalClip_;
    if (target == InstantPreviewProcessedPlayback &&
        !instantPreviewReady()) {
        target = InstantPreviewOriginalPlayback;
        clip = &instantPreviewOriginalClip_;
    }
    seekClip(clip, target, position);
}

void SpectraController::scheduleInstantPreview(
    InstantPreviewKind kind,
    bool recenter) {
    if (!sourceLoaded() || kind == NoInstantPreview) {
        return;
    }
    instantPreviewKind_ = kind;
    instantPreviewPendingKind_ = kind;
    instantPreviewPending_ = true;
    instantPreviewPendingRecenter_ =
        instantPreviewPendingRecenter_ || recenter ||
        instantPreviewSourceBuffer_.samples == nullptr;
    instantPreviewProgress_ = 0.0;
    instantPreviewDebounceTimer_.start();
    emit instantPreviewChanged();
}

double SpectraController::instantPreviewAnchor() const {
    if (playbackTarget_ == SourcePlayback ||
        playbackTarget_ == FullFilePlayback ||
        playbackTarget_ == EffectPlayback ||
        playbackTarget_ == EqPlayback) {
        return playbackPosition();
    }
    if (playbackTarget_ ==
        InstantPreviewOriginalPlayback) {
        return instantPreviewStart_ +
            audio_clip_position_seconds(
                &instantPreviewOriginalClip_);
    }
    if (playbackTarget_ ==
        InstantPreviewProcessedPlayback) {
        const double processedDuration =
            audio_clip_duration_seconds(
                &instantPreviewProcessedClip_);
        const double ratio = processedDuration > 0.0
            ? audio_clip_position_seconds(
                  &instantPreviewProcessedClip_) /
                  processedDuration
            : 0.0;
        return instantPreviewStart_ +
            ratio * instantPreviewDuration_;
    }
    return regionStart_ + regionDuration_ * 0.5;
}

bool SpectraController::rebuildInstantPreviewSource() {
    const InterleavedBuffer &source =
        importedAudio_.interleaved;
    if (source.samples == nullptr ||
        source.frame_count == 0U ||
        source.sample_rate == 0U ||
        source.channel_count == 0U) {
        return false;
    }

    const size_t requestedFrames = static_cast<size_t>(
        std::llround(
            kInstantPreviewSeconds *
            static_cast<double>(source.sample_rate)));
    const size_t frameCount = std::min(
        source.frame_count,
        std::max<size_t>(1U, requestedFrames));
    if (frameCount >
        SIZE_MAX /
            static_cast<size_t>(source.channel_count)) {
        return false;
    }
    const size_t sampleCount =
        frameCount *
        static_cast<size_t>(source.channel_count);
    float *samples = static_cast<float *>(
        std::calloc(sampleCount, sizeof(float)));
    if (samples == nullptr) {
        return false;
    }

    const double previewDuration =
        static_cast<double>(frameCount) /
        static_cast<double>(source.sample_rate);
    const double maximumStart =
        static_cast<double>(
            source.frame_count - frameCount) /
        static_cast<double>(source.sample_rate);
    const double requestedStart = clampValue(
        instantPreviewAnchor() - previewDuration * 0.5,
        0.0,
        maximumStart);
    const size_t startFrame = std::min(
        source.frame_count - frameCount,
        static_cast<size_t>(std::llround(
            requestedStart *
            static_cast<double>(source.sample_rate))));
    std::memcpy(
        samples,
        source.samples +
            startFrame * source.channel_count,
        sampleCount * sizeof(float));

    InterleavedBuffer rebuilt = {
        samples,
        frameCount,
        source.sample_rate,
        source.channel_count,
    };
    applyInstantPreviewFade(&rebuilt);

    if (playbackTarget_ ==
            InstantPreviewOriginalPlayback ||
        playbackTarget_ ==
            InstantPreviewProcessedPlayback) {
        playbackTarget_ = NoPlayback;
    }
    audio_clip_unload(&instantPreviewOriginalClip_);
    audio_clip_init(&instantPreviewOriginalClip_);
    audio_clip_unload(&instantPreviewProcessedClip_);
    audio_clip_init(&instantPreviewProcessedClip_);
    interleaved_buffer_free(
        &instantPreviewSourceBuffer_);
    interleaved_buffer_free(&instantPreviewBuffer_);
    instantPreviewSourceBuffer_ = rebuilt;
    instantPreviewReadyKind_ = NoInstantPreview;
    instantPreviewStart_ =
        static_cast<double>(startFrame) /
        static_cast<double>(source.sample_rate);
    instantPreviewDuration_ = previewDuration;

    if (audioReady_ &&
        audio_clip_set_interleaved(
            &instantPreviewOriginalClip_,
            &instantPreviewSourceBuffer_)) {
        audio_clip_set_looping(
            &instantPreviewOriginalClip_, true);
    }
    emit playbackChanged();
    return true;
}

void SpectraController::startInstantPreview() {
    if (!instantPreviewPending_ || !sourceLoaded()) {
        return;
    }
    if (background_task_has_work(&instantPreviewTask_)) {
        background_task_request_cancel(
            &instantPreviewTask_);
        return;
    }

    const InstantPreviewKind kind =
        instantPreviewPendingKind_;
    if (instantPreviewPendingRecenter_ ||
        instantPreviewSourceBuffer_.samples == nullptr) {
        if (!rebuildInstantPreviewSource()) {
            instantPreviewPending_ = false;
            instantPreviewPendingRecenter_ = false;
            setStatusText(
                QStringLiteral(
                    "Could not prepare the preview region"));
            emit instantPreviewChanged();
            return;
        }
    } else if (instantPreviewReadyKind_ != kind) {
        if (playbackTarget_ ==
            InstantPreviewProcessedPlayback) {
            audio_clip_stop(
                &instantPreviewProcessedClip_);
            playbackTarget_ = NoPlayback;
        }
        audio_clip_unload(
            &instantPreviewProcessedClip_);
        audio_clip_init(
            &instantPreviewProcessedClip_);
        interleaved_buffer_free(
            &instantPreviewBuffer_);
        instantPreviewReadyKind_ = NoInstantPreview;
    }

    interleaved_buffer_free(
        &instantPreviewWorkerBuffer_);
    instantPreviewWorkerKind_ = kind;
    instantPreviewWorkerEffectMode_ = effectMode_;
    instantPreviewWorkerEffectAmount_ =
        effectMode_ == 2
            ? effectFrequencyShift_
            : effectSemitones_;
    instantPreviewWorkerBands_ = eqBands_;
    instantPreviewWorkerSucceeded_ = false;
    instantPreviewProgress_ = 0.0;
    instantPreviewPending_ = false;
    instantPreviewPendingRecenter_ = false;

    if (!background_task_start(
            &instantPreviewTask_,
            &SpectraController::
                processInstantPreviewInBackground,
            this)) {
        setStatusText(
            QStringLiteral(
                "Could not start the preview renderer"));
        emit instantPreviewChanged();
        return;
    }
    instantPreviewTimer_.start();
    emit instantPreviewChanged();
}

void SpectraController::processInstantPreviewWork() {
    if (!background_task_has_work(
            &instantPreviewTask_)) {
        instantPreviewTimer_.stop();
        return;
    }

    const double progress = static_cast<double>(
        background_task_progress(
            &instantPreviewTask_));
    if (std::abs(
            progress - instantPreviewProgress_) >
        0.002) {
        instantPreviewProgress_ = progress;
        emit instantPreviewChanged();
    }
    if (!background_task_is_complete(
            &instantPreviewTask_)) {
        return;
    }

    const bool processedWasPlaying =
        playbackTarget_ ==
            InstantPreviewProcessedPlayback &&
        audio_clip_is_playing(
            &instantPreviewProcessedClip_);
    const bool processedWasPaused =
        playbackTarget_ ==
            InstantPreviewProcessedPlayback &&
        audio_clip_is_paused(
            &instantPreviewProcessedClip_);
    const double previousDuration =
        audio_clip_duration_seconds(
            &instantPreviewProcessedClip_);
    const double previousRatio =
        previousDuration > 0.0
            ? audio_clip_position_seconds(
                  &instantPreviewProcessedClip_) /
                  previousDuration
            : 0.0;
    const InstantPreviewKind completedKind =
        instantPreviewWorkerKind_;

    background_task_join(&instantPreviewTask_);
    instantPreviewTimer_.stop();
    const bool acceptOutput =
        instantPreviewWorkerSucceeded_ &&
        !instantPreviewPending_ &&
        completedKind == instantPreviewKind_ &&
        instantPreviewWorkerBuffer_.samples != nullptr;
    if (acceptOutput) {
        interleaved_buffer_free(
            &instantPreviewBuffer_);
        instantPreviewBuffer_ =
            instantPreviewWorkerBuffer_;
        instantPreviewWorkerBuffer_ =
            InterleavedBuffer{};
        instantPreviewReadyKind_ = completedKind;
        instantPreviewProgress_ = 1.0;

        bool playbackReady = !audioReady_;
        if (audioReady_) {
            playbackReady = audio_clip_set_interleaved(
                &instantPreviewProcessedClip_,
                &instantPreviewBuffer_);
            if (playbackReady) {
                audio_clip_set_looping(
                    &instantPreviewProcessedClip_,
                    true);
            }
        }
        if (playbackReady &&
            (processedWasPlaying || processedWasPaused)) {
            audio_clip_play(
                &instantPreviewProcessedClip_);
            audio_clip_seek(
                &instantPreviewProcessedClip_,
                static_cast<float>(
                    previousRatio *
                    audio_clip_duration_seconds(
                        &instantPreviewProcessedClip_)));
            if (processedWasPaused) {
                audio_clip_pause(
                    &instantPreviewProcessedClip_);
            }
            playbackTarget_ =
                InstantPreviewProcessedPlayback;
        }
    } else {
        interleaved_buffer_free(
            &instantPreviewWorkerBuffer_);
        if (!instantPreviewPending_ &&
            completedKind == instantPreviewKind_) {
            instantPreviewProgress_ = 0.0;
            setStatusText(
                QStringLiteral(
                    "Could not render the preview region"));
        }
    }

    instantPreviewWorkerBands_.clear();
    instantPreviewWorkerSucceeded_ = false;
    instantPreviewWorkerKind_ = NoInstantPreview;
    emit instantPreviewChanged();
    emit playbackChanged();
    if (instantPreviewPending_ &&
        !instantPreviewDebounceTimer_.isActive()) {
        instantPreviewDebounceTimer_.start(0);
    }
}

void SpectraController::clearInstantPreview() {
    instantPreviewDebounceTimer_.stop();
    instantPreviewTimer_.stop();
    background_task_cancel_and_join(
        &instantPreviewTask_);
    if (playbackTarget_ ==
            InstantPreviewOriginalPlayback ||
        playbackTarget_ ==
            InstantPreviewProcessedPlayback) {
        playbackTarget_ = NoPlayback;
    }
    audio_clip_unload(&instantPreviewOriginalClip_);
    audio_clip_init(&instantPreviewOriginalClip_);
    audio_clip_unload(&instantPreviewProcessedClip_);
    audio_clip_init(&instantPreviewProcessedClip_);
    interleaved_buffer_free(
        &instantPreviewSourceBuffer_);
    interleaved_buffer_free(
        &instantPreviewWorkerBuffer_);
    interleaved_buffer_free(&instantPreviewBuffer_);
    instantPreviewWorkerBands_.clear();
    instantPreviewKind_ = NoInstantPreview;
    instantPreviewPendingKind_ = NoInstantPreview;
    instantPreviewWorkerKind_ = NoInstantPreview;
    instantPreviewReadyKind_ = NoInstantPreview;
    instantPreviewStart_ = 0.0;
    instantPreviewDuration_ = 0.0;
    instantPreviewProgress_ = 0.0;
    instantPreviewPending_ = false;
    instantPreviewPendingRecenter_ = false;
    instantPreviewWorkerSucceeded_ = false;
    lastInstantPreviewPlaybackTarget_ =
        InstantPreviewOriginalPlayback;
    emit instantPreviewChanged();
    emit playbackChanged();
}

void SpectraController::stopPlayback() {
    haltAllAudio();
    playbackTarget_ = NoPlayback;
    setStatusText(QStringLiteral("Playback stopped"));
    emit playbackChanged();
}

void SpectraController::haltAllAudio() {
    audio_clip_stop(&synthClip_);
    audio_clip_stop(&sourceClip_);
    audio_clip_stop(&regionClip_);
    audio_clip_stop(&harmonicClip_);
    audio_clip_stop(&originalFrameClip_);
    audio_clip_stop(&fourierClip_);
    audio_clip_stop(&fullFileClip_);
    audio_clip_stop(&effectClip_);
    audio_clip_stop(&eqClip_);
    audio_clip_stop(&instantPreviewOriginalClip_);
    audio_clip_stop(&instantPreviewProcessedClip_);
    playbackTarget_ = NoPlayback;
}

void SpectraController::processAudioImportInBackground(
    BackgroundTaskControl *control,
    void *context) {
    auto *controller =
        static_cast<SpectraController *>(context);
    if (controller == nullptr || control == nullptr) {
        return;
    }

    imported_audio_unload(
        &controller->importWorkerAudio_);
    imported_audio_init(
        &controller->importWorkerAudio_);
    controller->importWorkerSucceeded_ = false;
    controller->importWorkerError_[0] = '\0';
    if (background_task_cancel_requested(control)) {
        return;
    }

    controller->importWorkerSucceeded_ =
        imported_audio_load(
            controller->importWorkerPath_.constData(),
            &controller->importWorkerAudio_,
            controller->importWorkerError_,
            sizeof(controller->importWorkerError_));
    background_task_report_progress(control, 1.0f);
}

void SpectraController::processAudioImportWork() {
    if (!background_task_has_work(&importTask_)) {
        importTimer_.stop();
        return;
    }
    if (!background_task_is_complete(&importTask_)) {
        return;
    }

    background_task_join(&importTask_);
    importTimer_.stop();
    sourceImporting_ = false;

    if (!importWorkerSucceeded_) {
        imported_audio_unload(&importWorkerAudio_);
        imported_audio_init(&importWorkerAudio_);
        sourceImportFileName_.clear();
        setStatusText(
            QString::fromUtf8(importWorkerError_));
        emit sourceImportChanged();
        return;
    }

    clearInstantPreview();
    resetAnalysis();
    resetFullFile();
    resetEq();
    resetEffect();
    sourceWaveformMinimums_.clear();
    sourceWaveformMaximums_.clear();
    audio_clip_unload(&sourceClip_);
    audio_clip_init(&sourceClip_);
    imported_audio_unload(&importedAudio_);
    importedAudio_ = importWorkerAudio_;
    imported_audio_init(&importWorkerAudio_);

    if (audioReady_) {
        audio_clip_set_interleaved(
            &sourceClip_,
            &importedAudio_.interleaved);
    }
    sourceFileName_ = sourceImportFileName_;
    sourceImportFileName_.clear();
    effectFrequencyShift_ = clampValue(
        effectFrequencyShift_,
        -std::min(
            5000.0,
            static_cast<double>(sourceSampleRate()) * 0.5),
        std::min(
            5000.0,
            static_cast<double>(sourceSampleRate()) * 0.5));
    rebuildEffectSourceSpectrum();
    rebuildEffectSpectrumPreview();
    regionDuration_ = std::min(1.0, sourceDuration());
    regionStart_ = std::max(
        0.0,
        (sourceDuration() - regionDuration_) * 0.5);
    const int availableGlobalComponents =
        global_fourier_available_component_count(
            importedAudio_.mono.count);
    if (availableGlobalComponents > 0) {
        const unsigned int recommendedChannels =
            global_fourier_recommended_channel_count(
                importedAudio_.mono.count,
                availableGlobalComponents,
                importedAudio_.source_channels,
                static_cast<size_t>(
                    fullFileMemoryLimitBytes_));
        fullFileChannelMode_ =
            recommendedChannels > 1U ? 1 : 0;
    }
    rebuildSourceWaveform();
    setStatusText(
        QStringLiteral("Loaded %1").arg(sourceFileName_));
    emit sourceChanged();
    emit sourceImportChanged();
    emit effectChanged();
    emit playbackChanged();
    analyzeRegion();
    startSpectrogramBuild();
    if (currentPage_ == 5) {
        scheduleInstantPreview(
            EffectInstantPreview, true);
    } else if (currentPage_ == 6) {
        scheduleInstantPreview(
            EqInstantPreview, true);
    }
}

void SpectraController::importAudioFile(const QUrl &url) {
    const QString localPath =
        url.isLocalFile() ? url.toLocalFile() : url.toString();
    if (localPath.isEmpty() || sourceImporting_) {
        return;
    }

    stopPlayback();
    sourceImportFileName_ =
        QFileInfo(localPath).fileName();
    importWorkerPath_ = localPath.toUtf8();
    importWorkerSucceeded_ = false;
    importWorkerError_[0] = '\0';
    sourceImporting_ = true;

    if (!background_task_start(
            &importTask_,
            &SpectraController::processAudioImportInBackground,
            this)) {
        sourceImporting_ = false;
        sourceImportFileName_.clear();
        setStatusText(QStringLiteral(
            "Could not start the audio import worker"));
        emit sourceImportChanged();
        return;
    }

    setStatusText(
        QStringLiteral("Loading %1…")
            .arg(sourceImportFileName_));
    emit sourceImportChanged();
    importTimer_.start();
}

bool SpectraController::exportSynthFile(const QUrl &url) {
    const QString localPath = url.isLocalFile() ? url.toLocalFile() : url.toString();
    if (localPath.isEmpty() || synthBuffer_.samples == nullptr) {
        return false;
    }
    const QString wavPath = localPath.endsWith(QStringLiteral(".wav"), Qt::CaseInsensitive)
        ? localPath
        : localPath + QStringLiteral(".wav");
    const QByteArray encodedPath = wavPath.toUtf8();
    const bool exported = export_samples_to_wav(encodedPath.constData(), &synthBuffer_);
    setStatusText(exported
                      ? QStringLiteral("Exported %1").arg(QFileInfo(wavPath).fileName())
                      : QStringLiteral("Could not export WAV"));
    return exported;
}

void SpectraController::setStatusText(const QString &status) {
    if (status == statusText_) {
        return;
    }
    statusText_ = status;
    emit statusTextChanged();
}

void SpectraController::scheduleSynthRebuild() {
    synthRebuildTimer_.start();
}

void SpectraController::rebuildSynth() {
    const bool resumePlayback = audio_clip_is_playing(&synthClip_);
    audio_clip_unload(&synthClip_);
    audio_clip_init(&synthClip_);
    spectrum_free(&synthSpectrumBuffer_);
    sample_buffer_free(&synthBuffer_);

    Harmonic active[kHarmonicCount];
    for (int index = 0; index < kHarmonicCount; ++index) {
        active[index] = harmonics_[index];
        active[index].amplitude *= static_cast<float>(synthGain_);
    }
    synthBuffer_ = generate_additive_tone(
        static_cast<float>(synthFrequency_),
        active,
        kHarmonicCount,
        static_cast<float>(synthDuration_),
        kSampleRate,
        kDefaultEnvelope);
    synthSpectrumBuffer_ = compute_magnitude_spectrum(
        synthBuffer_.samples,
        synthBuffer_.count,
        synthBuffer_.sample_rate);
    synthPeakCount_ = find_peaks(
        &synthSpectrumBuffer_,
        20.0f,
        8000.0f,
        -55.0f,
        synthPeaksBuffer_,
        kMaximumPeaks);
    synthPitchEstimate_ = estimate_pitch(&synthBuffer_, 40.0f, 1200.0f);
    if (audioReady_) {
        audio_clip_set_samples(&synthClip_, &synthBuffer_);
        if (resumePlayback) {
            audio_clip_play(&synthClip_);
        }
    }
    rebuildSynthVisualization();
    emit synthVisualizationChanged();
    emit playbackChanged();
}

void SpectraController::rebuildSynthVisualization() {
    synthWaveform_.clear();
    synthWaveformMinimums_.clear();
    synthWaveformMaximums_.clear();
    if (synthBuffer_.samples != nullptr && synthBuffer_.count > 0U) {
        synthWaveform_.reserve(kWaveformPoints);
        synthWaveformMinimums_.reserve(kWaveformPoints);
        synthWaveformMaximums_.reserve(kWaveformPoints);
        for (int point = 0; point < kWaveformPoints; ++point) {
            const size_t begin = static_cast<size_t>(point) * synthBuffer_.count / kWaveformPoints;
            const size_t end = static_cast<size_t>(point + 1) * synthBuffer_.count / kWaveformPoints;
            float minimum = 1.0f;
            float maximum = -1.0f;
            for (size_t sample = begin; sample < std::max(begin + 1U, end); ++sample) {
                minimum = std::min(minimum, synthBuffer_.samples[sample]);
                maximum = std::max(maximum, synthBuffer_.samples[sample]);
            }
            synthWaveform_.append(
                std::max(std::fabs(minimum), std::fabs(maximum)));
            synthWaveformMinimums_.append(minimum);
            synthWaveformMaximums_.append(maximum);
        }
    }

    synthSpectrum_.clear();
    if (synthSpectrumBuffer_.magnitudes != nullptr && synthSpectrumBuffer_.count > 0U) {
        synthSpectrum_.reserve(
            static_cast<qsizetype>(synthSpectrumBuffer_.count));
        for (size_t index = 0; index < synthSpectrumBuffer_.count; ++index) {
            const float magnitude = synthSpectrumBuffer_.magnitudes[index];
            const float decibels = 20.0f * std::log10(std::max(magnitude, 0.00001f));
            synthSpectrum_.append(std::max(-80.0f, std::min(0.0f, decibels)));
        }
    }

    synthPeaks_.clear();
    synthPeaks_.reserve(synthPeakCount_);
    for (int index = 0; index < synthPeakCount_; ++index) {
        QVariantMap peak;
        peak.insert(
            QStringLiteral("frequency"),
            synthPeaksBuffer_[index].frequency);
        peak.insert(
            QStringLiteral("magnitude"),
            synthPeaksBuffer_[index].magnitude);
        peak.insert(
            QStringLiteral("db"),
            synthPeaksBuffer_[index].db);
        synthPeaks_.append(peak);
    }
}

void SpectraController::rebuildSourceWaveform() {
    sourceWaveformMinimums_.clear();
    sourceWaveformMaximums_.clear();
    if (!sourceLoaded()) {
        return;
    }

    float minimums[kSourceWaveformPoints] = {};
    float maximums[kSourceWaveformPoints] = {};
    const size_t count = summarize_waveform(
        importedAudio_.mono.samples,
        importedAudio_.mono.count,
        minimums,
        maximums,
        kSourceWaveformPoints);
    sourceWaveformMinimums_.reserve(static_cast<qsizetype>(count));
    sourceWaveformMaximums_.reserve(static_cast<qsizetype>(count));
    for (size_t index = 0; index < count; ++index) {
        sourceWaveformMinimums_.append(minimums[index]);
        sourceWaveformMaximums_.append(maximums[index]);
    }
}

void SpectraController::rebuildAnalysisPeaks() {
    analysisPeakCount_ = 0;
    if (analysisSpectrumBuffer_.count == 0U) {
        return;
    }
    const float maximumFrequency = std::min(
        20000.0f,
        static_cast<float>(sourceSampleRate()) * 0.5f);
    analysisPeakCount_ =
        analysisPeakReadoutMode_ == 0
            ? find_interpolated_peaks(
                  &analysisSpectrumBuffer_,
                  20.0f,
                  maximumFrequency,
                  -55.0f,
                  analysisPeaksBuffer_,
                  64)
            : find_peaks(
                  &analysisSpectrumBuffer_,
                  20.0f,
                  maximumFrequency,
                  -55.0f,
                  analysisPeaksBuffer_,
                  64);
}

void SpectraController::rebuildAnalysisVisualization() {
    analysisSpectrum_.clear();
    if (analysisSpectrumBuffer_.magnitudes != nullptr &&
        analysisSpectrumBuffer_.count > 0U) {
        analysisSpectrum_.reserve(
            static_cast<qsizetype>(analysisSpectrumBuffer_.count));
        for (size_t index = 0;
             index < analysisSpectrumBuffer_.count;
             ++index) {
            const float magnitude = analysisSpectrumBuffer_.magnitudes[index];
            const float decibels = 20.0f * std::log10(std::max(magnitude, 0.00001f));
            analysisSpectrum_.append(std::max(-100.0f, std::min(0.0f, decibels)));
        }
    }

    analysisPeaks_.clear();
    analysisPeaks_.reserve(analysisPeakCount_);
    for (int index = 0; index < analysisPeakCount_; ++index) {
        QVariantMap peak;
        peak.insert(QStringLiteral("frequency"), analysisPeaksBuffer_[index].frequency);
        peak.insert(QStringLiteral("magnitude"), analysisPeaksBuffer_[index].magnitude);
        peak.insert(QStringLiteral("db"), analysisPeaksBuffer_[index].db);
        analysisPeaks_.append(peak);
    }
}

void SpectraController::rebuildRegionModels(const SampleBuffer &region) {
    audio_clip_unload(&harmonicClip_);
    audio_clip_init(&harmonicClip_);
    sample_buffer_free(&harmonicBuffer_);
    harmonicCount_ = 0;
    detectedHarmonicCount_ = 0;
    extractedHarmonics_.clear();

    audio_clip_unload(&originalFrameClip_);
    audio_clip_init(&originalFrameClip_);
    audio_clip_unload(&fourierClip_);
    audio_clip_init(&fourierClip_);
    sample_buffer_free(&fourierBuffer_);
    fourier_frame_analysis_free(&fourierAnalysis_);
    fourier_frame_analysis_init(&fourierAnalysis_);
    fourierSelectedComponents_ = 0;

    if (analysisPitchEstimate_.valid && analysisPitchEstimate_.confidence >= 0.60f) {
        float strongestMagnitude = 0.0f;
        for (size_t bin = 0; bin < analysisSpectrumBuffer_.count; ++bin) {
            strongestMagnitude = std::max(
                strongestMagnitude,
                analysisSpectrumBuffer_.magnitudes[bin]);
        }
        const float thresholdDb = std::max(
            -100.0f,
            linear_to_db(strongestMagnitude) - 50.0f);
        harmonicCount_ = extract_harmonics(
            &analysisSpectrumBuffer_,
            analysisPitchEstimate_.frequency_hz,
            24,
            30.0f,
            thresholdDb,
            extractedHarmonicsBuffer_,
            24);
        for (int index = 0; index < harmonicCount_; ++index) {
            if (extractedHarmonicsBuffer_[index].detected) {
                ++detectedHarmonicCount_;
            }
            QVariantMap harmonic;
            harmonic.insert(QStringLiteral("number"), extractedHarmonicsBuffer_[index].harmonic_number);
            harmonic.insert(QStringLiteral("expected"), extractedHarmonicsBuffer_[index].expected_frequency);
            harmonic.insert(QStringLiteral("detectedFrequency"), extractedHarmonicsBuffer_[index].detected_frequency);
            harmonic.insert(QStringLiteral("db"), extractedHarmonicsBuffer_[index].db);
            harmonic.insert(QStringLiteral("detected"), extractedHarmonicsBuffer_[index].detected);
            extractedHarmonics_.append(harmonic);
        }
        harmonicBuffer_ = resynthesize_from_harmonics(
            analysisPitchEstimate_.frequency_hz,
            extractedHarmonicsBuffer_,
            harmonicCount_,
            static_cast<float>(regionDuration_),
            region.sample_rate);
        if (audioReady_ && harmonicReady()) {
            audio_clip_set_samples(&harmonicClip_, &harmonicBuffer_);
        }
    }

    if (analyze_fourier_frame(&region, &fourierAnalysis_)) {
        if (audioReady_) {
            audio_clip_set_samples(
                &originalFrameClip_,
                &fourierAnalysis_.windowed_frame);
        }
        rebuildFourierFrameBuffer(
            static_cast<int>(std::min<size_t>(25U, fourierAnalysis_.component_count)));
    }
    emit reconstructionChanged();
}

bool SpectraController::rebuildFourierFrameBuffer(int componentCount) {
    if (fourierAnalysis_.component_count == 0U) {
        return false;
    }
    const int bounded = std::max(
        1,
        std::min(componentCount, fourierMaximumComponents()));
    audio_clip_unload(&fourierClip_);
    audio_clip_init(&fourierClip_);
    sample_buffer_free(&fourierBuffer_);
    const SampleBuffer region = selectedRegion();
    StftReconstructionJob job = {};
    stft_reconstruction_job_init(&job);
    const bool started = stft_reconstruction_job_begin(
        &job,
        &region,
        kFullFileWindowSize,
        kFullFileHopSize,
        bounded,
        false,
        0U,
        0U,
        0.0f);
    if (!started) {
        fourierSelectedComponents_ = 0;
        return false;
    }
    while (job.active && !job.complete && !job.failed) {
        stft_reconstruction_job_process(&job, 64U);
    }
    if (job.complete && !job.failed) {
        fourierBuffer_ =
            stft_reconstruction_job_take_output(&job);
    }
    stft_reconstruction_job_free(&job);
    if (fourierBuffer_.samples == nullptr) {
        fourierSelectedComponents_ = 0;
        return false;
    }
    fourierSelectedComponents_ = bounded;
    if (audioReady_) {
        audio_clip_set_samples(&fourierClip_, &fourierBuffer_);
    }
    return true;
}

SampleBuffer SpectraController::selectedRegion() const {
    SampleBuffer region = {};
    if (!sourceLoaded() || importedAudio_.mono.sample_rate == 0U) {
        return region;
    }
    const double startSeconds = clampValue(regionStart_, 0.0, sourceDuration());
    const double maximumDuration = std::max(0.0, sourceDuration() - startSeconds);
    const double durationSeconds = clampValue(regionDuration_, 0.0, maximumDuration);
    size_t start = static_cast<size_t>(
        startSeconds * static_cast<double>(importedAudio_.mono.sample_rate));
    size_t count = static_cast<size_t>(
        durationSeconds * static_cast<double>(importedAudio_.mono.sample_rate));
    if (start >= importedAudio_.mono.count) {
        start = importedAudio_.mono.count - 1U;
    }
    count = std::max<size_t>(1U, std::min(count, importedAudio_.mono.count - start));
    region.samples = importedAudio_.mono.samples + start;
    region.count = count;
    region.sample_rate = importedAudio_.mono.sample_rate;
    return region;
}

void SpectraController::resetAnalysis() {
    if (playbackTarget_ == RegionPlayback ||
        playbackTarget_ == HarmonicPlayback ||
        playbackTarget_ == OriginalFramePlayback ||
        playbackTarget_ == FourierFramePlayback) {
        playbackTarget_ = NoPlayback;
    }
    audio_clip_unload(&fourierClip_);
    audio_clip_init(&fourierClip_);
    audio_clip_unload(&originalFrameClip_);
    audio_clip_init(&originalFrameClip_);
    audio_clip_unload(&harmonicClip_);
    audio_clip_init(&harmonicClip_);
    sample_buffer_free(&fourierBuffer_);
    fourier_frame_analysis_free(&fourierAnalysis_);
    fourier_frame_analysis_init(&fourierAnalysis_);
    sample_buffer_free(&harmonicBuffer_);
    audio_clip_unload(&regionClip_);
    audio_clip_init(&regionClip_);
    spectrum_free(&analysisSpectrumBuffer_);
    analysisReady_ = false;
    analysisPeakCount_ = 0;
    analysisPitchEstimate_ = PitchEstimate{};
    analysisSpectrum_.clear();
    analysisPeaks_.clear();
    harmonicCount_ = 0;
    detectedHarmonicCount_ = 0;
    extractedHarmonics_.clear();
    fourierSelectedComponents_ = 0;
    emit analysisChanged();
    emit reconstructionChanged();
    emit playbackChanged();
}

void SpectraController::resetFullFile() {
    fullFileTimer_.stop();
    background_task_cancel_and_join(&spectrogramTask_);
    background_task_cancel_and_join(&fullFileTask_);
    fullFileWork_ = FullFileIdle;
    stft_reconstruction_job_free(&fullFileStftJobRight_);
    stft_reconstruction_job_init(&fullFileStftJobRight_);
    stft_reconstruction_job_free(&fullFileStftJob_);
    stft_reconstruction_job_init(&fullFileStftJob_);
    global_fourier_job_free(&globalFourierJobRight_);
    global_fourier_job_init(&globalFourierJobRight_);
    global_fourier_job_free(&globalFourierJob_);
    global_fourier_job_init(&globalFourierJob_);
    stft_reconstruction_job_free(&spectrogramJob_);
    stft_reconstruction_job_init(&spectrogramJob_);
    clearFullFileOutput();
    reconstruction_cache_free(&reconstructionCache_);
    reconstruction_cache_init(
        &reconstructionCache_,
        kReconstructionCacheLimit);
    fullFileMode_ = 0;
    fullFileChannelMode_ = 0;
    fullFileSelectionMode_ = 0;
    fullFileEnergyTarget_ = 0.90;
    globalSelectedComponents_ = 5;
    stftSelectedComponents_ = 100;
    fullFileSelectedComponents_ = globalSelectedComponents_;
    fullFileProgress_ = 0.0;
    fullFileRetainedEnergy_ = 0.0;
    fullFileFrameCount_ = 0;
    spectrogramImageUrl_ = QUrl();
    lastFullFilePlaybackTarget_ = SourcePlayback;
    emit fullFileChanged();
    emit playbackChanged();
}

void SpectraController::resetEffect() {
    effectSpectrumTimer_.stop();
    effectTimer_.stop();
    background_task_cancel_and_join(
        &effectSpectrumTask_);
    background_task_cancel_and_join(&effectTask_);
    interleaved_buffer_free(&effectWorkerBuffer_);
    spectrum_free(&effectSourceSpectrumWorker_);
    spectrum_free(&effectSourceSpectrumBuffer_);
    spectrum_free(&effectWorkerSpectrum_);
    spectrum_free(&effectOutputSpectrumBuffer_);
    effectWorkerSucceeded_ = false;
    effectSourceSpectrumWorkerSucceeded_ = false;
    clearEffectOutput();
    effectOriginalSpectrum_.clear();
    effectTransformedSpectrum_.clear();
    effectOriginalPeakFrequency_ = 0.0;
    effectTransformedPeakFrequency_ = 0.0;
    effectSpectrumPreview_ = false;
    emit effectSpectrumChanged();
    effectProgress_ = 0.0;
    lastEffectPlaybackTarget_ = SourcePlayback;
    emit effectChanged();
    emit playbackChanged();
}

void SpectraController::resetEq() {
    eqTimer_.stop();
    background_task_cancel_and_join(&eqTask_);
    interleaved_buffer_free(&eqWorkerBuffer_);
    spectrum_free(&eqWorkerSpectrum_);
    eqWorkerBands_.clear();
    eqWorkerSucceeded_ = false;
    clearEqOutput();
    eqBands_.clear();
    eqPreset_ = -1;
    eqTransformedSpectrum_.clear();
    eqSpectrumPreview_ = false;
    eqProgress_ = 0.0;
    lastEqPlaybackTarget_ = SourcePlayback;
    emit eqChanged();
    emit eqSpectrumChanged();
    emit playbackChanged();
}

void SpectraController::cacheFullFileOutput() {
    if (!fullFileReady() ||
        fullFileSelectedComponents_ <= 0) {
        return;
    }
    if (playbackTarget_ == FullFilePlayback) {
        playbackTarget_ = NoPlayback;
    }
    audio_clip_unload(&fullFileClip_);
    audio_clip_init(&fullFileClip_);
    const ReconstructionCacheKey key = {
        fullFileMode_ == 0
            ? RECONSTRUCTION_CACHE_GLOBAL
            : RECONSTRUCTION_CACHE_STFT,
        fullFileSelectedComponents_,
        fullFileBuffer_.channel_count,
    };
    if (!reconstruction_cache_store_move(
            &reconstructionCache_,
            key,
            &fullFileBuffer_,
            static_cast<float>(
                fullFileRetainedEnergy_))) {
        interleaved_buffer_free(&fullFileBuffer_);
    }
    fullFileRetainedEnergy_ = 0.0;
    fullFileProgress_ = 0.0;
}

bool SpectraController::restoreFullFileOutput() {
    const ReconstructionCacheKey key = {
        fullFileMode_ == 0
            ? RECONSTRUCTION_CACHE_GLOBAL
            : RECONSTRUCTION_CACHE_STFT,
        fullFileSelectedComponents_,
        fullFileReconstructionChannels(),
    };
    float retainedEnergy = 0.0f;
    if (!reconstruction_cache_take(
            &reconstructionCache_,
            key,
            &fullFileBuffer_,
            &retainedEnergy)) {
        return false;
    }
    fullFileRetainedEnergy_ =
        clampValue(
            static_cast<double>(retainedEnergy),
            0.0,
            1.0);
    fullFileProgress_ = 1.0;
    const bool playbackReady =
        !audioReady_ ||
        audio_clip_set_interleaved(
            &fullFileClip_,
            &fullFileBuffer_);
    setStatusText(
        playbackReady
            ? QStringLiteral(
                  "Restored %1 from the reconstruction cache")
                  .arg(
                      fullFileMode_ == 0
                          ? QStringLiteral("%1 FFT bins")
                                .arg(
                                    fullFileSelectedComponents_)
                          : QStringLiteral(
                                "STFT Top-%1")
                                .arg(
                                    fullFileSelectedComponents_))
            : QStringLiteral(
                  "Cached reconstruction restored for export; playback is unavailable"));
    return true;
}

void SpectraController::clearFullFileOutput() {
    if (playbackTarget_ == FullFilePlayback) {
        playbackTarget_ = NoPlayback;
    }
    audio_clip_unload(&fullFileClip_);
    audio_clip_init(&fullFileClip_);
    interleaved_buffer_free(&fullFileBuffer_);
    fullFileRetainedEnergy_ = 0.0;
}

void SpectraController::clearEffectOutput() {
    if (playbackTarget_ == EffectPlayback) {
        playbackTarget_ = NoPlayback;
    }
    audio_clip_unload(&effectClip_);
    audio_clip_init(&effectClip_);
    interleaved_buffer_free(&effectBuffer_);
    spectrum_free(&effectOutputSpectrumBuffer_);
}

void SpectraController::clearEqOutput() {
    if (playbackTarget_ == EqPlayback) {
        playbackTarget_ = NoPlayback;
    }
    audio_clip_unload(&eqClip_);
    audio_clip_init(&eqClip_);
    interleaved_buffer_free(&eqBuffer_);
    spectrum_free(&eqOutputSpectrumBuffer_);
}

void SpectraController::rebuildEffectSourceSpectrum() {
    effectSpectrumTimer_.stop();
    background_task_cancel_and_join(
        &effectSpectrumTask_);
    spectrum_free(&effectSourceSpectrumWorker_);
    spectrum_free(&effectSourceSpectrumBuffer_);
    effectSourceSpectrumWorkerSucceeded_ = false;
    effectOriginalSpectrum_.clear();
    effectOriginalPeakFrequency_ = 0.0;
    eqTransformedSpectrum_.clear();
    eqSpectrumPreview_ = false;

    if (!sourceLoaded() ||
        !background_task_start(
            &effectSpectrumTask_,
            &SpectraController::
                processEffectSourceSpectrumInBackground,
            this)) {
        emit effectSpectrumChanged();
        emit eqSpectrumChanged();
        return;
    }

    effectSpectrumTimer_.start();
    emit effectSpectrumChanged();
    emit eqSpectrumChanged();
}

void SpectraController::processEffectSourceSpectrumWork() {
    if (!background_task_has_work(
            &effectSpectrumTask_)) {
        effectSpectrumTimer_.stop();
        return;
    }
    if (!background_task_is_complete(
            &effectSpectrumTask_)) {
        return;
    }

    background_task_join(&effectSpectrumTask_);
    effectSpectrumTimer_.stop();
    if (effectSourceSpectrumWorkerSucceeded_) {
        spectrum_free(
            &effectSourceSpectrumBuffer_);
        effectSourceSpectrumBuffer_ =
            effectSourceSpectrumWorker_;
        effectSourceSpectrumWorker_ = Spectrum{};
        const EffectSpectrumPreview preview =
            effectSpectrumPreviewFromSpectrum(
                effectSourceSpectrumBuffer_);
        effectOriginalSpectrum_ = preview.values;
        effectOriginalPeakFrequency_ =
            preview.peakFrequency;
    } else {
        spectrum_free(
            &effectSourceSpectrumWorker_);
        effectOriginalSpectrum_.clear();
        effectOriginalPeakFrequency_ = 0.0;
    }
    effectSourceSpectrumWorkerSucceeded_ = false;

    if (effectOutputSpectrumBuffer_.count > 0U) {
        rebuildEffectOutputSpectrum();
    } else {
        rebuildEffectSpectrumPreview();
    }
    if (eqOutputSpectrumBuffer_.count > 0U) {
        rebuildEqOutputSpectrum();
    } else {
        rebuildEqSpectrumPreview();
    }
}

void SpectraController::rebuildEffectSpectrumPreview() {
    if (!sourceLoaded() ||
        effectOriginalSpectrum_.isEmpty()) {
        effectTransformedSpectrum_.clear();
        effectTransformedPeakFrequency_ = 0.0;
        effectSpectrumPreview_ = false;
        emit effectSpectrumChanged();
        return;
    }

    const EffectSpectrumPreview preview =
        transformEffectSpectrum(
            effectOriginalSpectrum_,
            effectSpectrumMaximumFrequency(),
            effectMode_,
            effectPitchFactor(),
            effectFrequencyShift_);
    effectTransformedSpectrum_ = preview.values;
    effectTransformedPeakFrequency_ =
        transformEffectFrequency(
            effectOriginalPeakFrequency_,
            effectSpectrumMaximumFrequency(),
            effectMode_,
            effectPitchFactor(),
            effectFrequencyShift_);
    effectSpectrumPreview_ =
        !effectTransformedSpectrum_.isEmpty();
    emit effectSpectrumChanged();
}

void SpectraController::rebuildEffectOutputSpectrum() {
    const EffectSpectrumPreview preview =
        effectSpectrumPreviewFromSpectrum(
            effectOutputSpectrumBuffer_);
    if (preview.values.isEmpty()) {
        rebuildEffectSpectrumPreview();
        return;
    }
    effectTransformedSpectrum_ = preview.values;
    effectTransformedPeakFrequency_ =
        transformEffectFrequency(
            effectOriginalPeakFrequency_,
            effectSpectrumMaximumFrequency(),
            effectMode_,
            effectPitchFactor(),
            effectFrequencyShift_);
    effectSpectrumPreview_ = false;
    emit effectSpectrumChanged();
}

void SpectraController::rebuildEqSpectrumPreview() {
    if (!sourceLoaded() ||
        effectOriginalSpectrum_.isEmpty()) {
        eqTransformedSpectrum_.clear();
        eqSpectrumPreview_ = false;
        emit eqSpectrumChanged();
        return;
    }

    const qsizetype count =
        effectOriginalSpectrum_.size();
    const double maximum =
        eqSpectrumMaximumFrequency();
    const double denominator =
        std::max<qsizetype>(1, count - 1);
    const SpectralEqBand *bands =
        eqBands_.empty() ? nullptr : eqBands_.data();
    eqTransformedSpectrum_.clear();
    eqTransformedSpectrum_.reserve(count);
    for (qsizetype index = 0;
         index < count;
         ++index) {
        const float frequency = static_cast<float>(
            static_cast<double>(index) /
            static_cast<double>(denominator) *
            maximum);
        const double response =
            spectral_eq_response_db(
                frequency,
                bands,
                eqBands_.size());
        eqTransformedSpectrum_.append(
            clampValue(
                effectOriginalSpectrum_[index]
                    .toDouble() +
                    response,
                static_cast<double>(
                    kEffectSpectrumFloorDb),
                0.0));
    }
    eqSpectrumPreview_ = true;
    emit eqSpectrumChanged();
}

void SpectraController::rebuildEqOutputSpectrum() {
    const EffectSpectrumPreview preview =
        effectSpectrumPreviewFromSpectrum(
            eqOutputSpectrumBuffer_);
    if (preview.values.isEmpty()) {
        rebuildEqSpectrumPreview();
        return;
    }
    eqTransformedSpectrum_ = preview.values;
    eqSpectrumPreview_ = false;
    emit eqSpectrumChanged();
}

void SpectraController::processEffectWork() {
    if (!background_task_has_work(&effectTask_)) {
        effectTimer_.stop();
        return;
    }

    const double progress =
        static_cast<double>(
            background_task_progress(&effectTask_));
    if (std::abs(progress - effectProgress_) >
        0.0005) {
        effectProgress_ = progress;
        emit effectChanged();
    }
    if (!background_task_is_complete(&effectTask_)) {
        return;
    }

    background_task_join(&effectTask_);
    effectTimer_.stop();
    if (effectWorkerSucceeded_ &&
        effectWorkerBuffer_.samples != nullptr) {
        interleaved_buffer_free(&effectBuffer_);
        effectBuffer_ = effectWorkerBuffer_;
        effectWorkerBuffer_ = InterleavedBuffer{};
        spectrum_free(
            &effectOutputSpectrumBuffer_);
        effectOutputSpectrumBuffer_ =
            effectWorkerSpectrum_;
        effectWorkerSpectrum_ = Spectrum{};
        effectProgress_ = 1.0;
        rebuildEffectOutputSpectrum();
        lastEffectPlaybackTarget_ = EffectPlayback;
        const bool playbackReady =
            !audioReady_ ||
            audio_clip_set_interleaved(
                &effectClip_, &effectBuffer_);
        setStatusText(
            playbackReady
                ? QStringLiteral("Built %1")
                      .arg(effectModeName().toLower())
                : QStringLiteral(
                      "Effect built for export; playback is unavailable"));
    } else {
        interleaved_buffer_free(&effectWorkerBuffer_);
        spectrum_free(&effectWorkerSpectrum_);
        effectProgress_ = 0.0;
        setStatusText(
            QStringLiteral(
                "Could not build the effect; try a shorter file"));
    }
    effectWorkerSucceeded_ = false;
    emit effectChanged();
    emit playbackChanged();
}

void SpectraController::processEqWork() {
    if (!background_task_has_work(&eqTask_)) {
        eqTimer_.stop();
        return;
    }

    const double progress = static_cast<double>(
        background_task_progress(&eqTask_));
    if (std::abs(progress - eqProgress_) > 0.0005) {
        eqProgress_ = progress;
        emit eqChanged();
    }
    if (!background_task_is_complete(&eqTask_)) {
        return;
    }

    background_task_join(&eqTask_);
    eqTimer_.stop();
    if (eqWorkerSucceeded_ &&
        eqWorkerBuffer_.samples != nullptr) {
        interleaved_buffer_free(&eqBuffer_);
        eqBuffer_ = eqWorkerBuffer_;
        eqWorkerBuffer_ = InterleavedBuffer{};
        spectrum_free(&eqOutputSpectrumBuffer_);
        eqOutputSpectrumBuffer_ = eqWorkerSpectrum_;
        eqWorkerSpectrum_ = Spectrum{};
        eqProgress_ = 1.0;
        rebuildEqOutputSpectrum();
        lastEqPlaybackTarget_ = EqPlayback;
        const bool playbackReady =
            !audioReady_ ||
            audio_clip_set_interleaved(
                &eqClip_, &eqBuffer_);
        setStatusText(
            playbackReady
                ? QStringLiteral(
                      "Built range EQ output")
                : QStringLiteral(
                      "EQ built for export; playback is unavailable"));
    } else {
        interleaved_buffer_free(&eqWorkerBuffer_);
        spectrum_free(&eqWorkerSpectrum_);
        eqProgress_ = 0.0;
        setStatusText(
            QStringLiteral(
                "Range EQ processing did not complete"));
    }
    eqWorkerBands_.clear();
    eqWorkerSucceeded_ = false;
    emit eqChanged();
    emit playbackChanged();
}

void SpectraController::startSpectrogramBuild() {
    if (!sourceLoaded()) {
        return;
    }
    if (!stft_reconstruction_fits_memory(
            importedAudio_.mono.count,
            kFullFileWindowSize,
            0,
            true,
            kSpectrogramTimeBins,
            kSpectrogramFrequencyBins,
            1U,
            static_cast<size_t>(fullFileMemoryLimitBytes_))) {
        setStatusText(QStringLiteral(
            "Spectrogram working memory exceeds the configured limit"));
        return;
    }

    background_task_cancel_and_join(&spectrogramTask_);
    stft_reconstruction_job_free(&spectrogramJob_);
    stft_reconstruction_job_init(&spectrogramJob_);
    const float maximumFrequency = std::min(
        kSpectrogramMaximumFrequency,
        static_cast<float>(sourceSampleRate()) * 0.5f);
    if (!stft_reconstruction_job_begin(
            &spectrogramJob_,
            &importedAudio_.mono,
            kFullFileWindowSize,
            kFullFileHopSize,
            0,
            true,
            kSpectrogramTimeBins,
            kSpectrogramFrequencyBins,
            maximumFrequency)) {
        setStatusText(QStringLiteral("Could not allocate the spectrogram analysis"));
        emit fullFileChanged();
        return;
    }

    fullFileFrameCount_ = static_cast<qsizetype>(spectrogramJob_.frame_count);
    fullFileProgress_ = 0.0;
    fullFileWork_ = SpectrogramBuild;
    if (!background_task_start(
            &spectrogramTask_,
            buildSpectrogramInBackground,
            &spectrogramJob_)) {
        stft_reconstruction_job_free(&spectrogramJob_);
        stft_reconstruction_job_init(&spectrogramJob_);
        fullFileWork_ = FullFileIdle;
        setStatusText(
            QStringLiteral(
                "Could not start the spectrogram worker"));
        emit fullFileChanged();
        return;
    }
    setStatusText(QStringLiteral("Building whole-file spectrogram"));
    fullFileTimer_.start();
    emit fullFileChanged();
}

unsigned int SpectraController::fullFileReconstructionChannels() const {
    if (fullFileMode_ == 0 && fullFileChannelMode_ == 0) {
        return 1U;
    }
    return importedAudio_.source_channels == 2U &&
            importedAudio_.interleaved.channel_count == 2U
        ? 2U
        : 1U;
}

int SpectraController::requiredGlobalAnalysisComponents() const {
    if (!sourceLoaded()) {
        return 1;
    }
    const int available =
        std::max(
            1,
            global_fourier_available_component_count(
                importedAudio_.mono.count));
    return fullFileSelectionMode_ == 1
        ? available
        : std::max(
              1,
              std::min(
                  fullFileSelectedComponents_,
                  available));
}

bool SpectraController::hasReusableGlobalAnalysis() const {
    const unsigned int channelCount =
        fullFileReconstructionChannels();
    const int required =
        requiredGlobalAnalysisComponents();
    return globalFourierJob_.analysis_ready &&
        globalFourierJob_.maximum_component_count >= required &&
        (channelCount == 1U ||
         (globalFourierJobRight_.analysis_ready &&
          globalFourierJobRight_.maximum_component_count >=
              required));
}

bool SpectraController::startGlobalAnalysis() {
    const unsigned int channelCount =
        fullFileReconstructionChannels();
    const int available =
        global_fourier_available_component_count(importedAudio_.mono.count);
    if (available <= 0) {
        setStatusText(QStringLiteral("Could not determine the whole-file FFT grid"));
        return false;
    }
    const int componentCapacity =
        requiredGlobalAnalysisComponents();
    if (!global_fourier_analysis_fits_memory(
            importedAudio_.mono.count,
            componentCapacity,
            channelCount,
            static_cast<size_t>(fullFileMemoryLimitBytes_))) {
        const double estimatedMegabytes =
            static_cast<double>(
                global_fourier_estimated_multichannel_analysis_bytes(
                    importedAudio_.mono.count,
                    componentCapacity,
                    channelCount)) /
            (1024.0 * 1024.0);
        const double limitMegabytes =
            static_cast<double>(fullFileMemoryLimitBytes_) /
            (1024.0 * 1024.0);
        setStatusText(
            QStringLiteral("%1 FFT needs %2 MB, above the %3 MB limit")
                .arg(channelCount > 1U
                         ? QStringLiteral("Stereo")
                         : QStringLiteral("Mono"))
                .arg(estimatedMegabytes, 0, 'f', 0)
                .arg(limitMegabytes, 0, 'f', 0));
        return false;
    }

    background_task_cancel_and_join(&fullFileTask_);
    global_fourier_job_free(&globalFourierJob_);
    global_fourier_job_init(&globalFourierJob_);
    global_fourier_job_free(&globalFourierJobRight_);
    global_fourier_job_init(&globalFourierJobRight_);

    bool started = false;
    if (channelCount == 2U) {
        started = global_fourier_job_begin_analysis_strided(
            &globalFourierJob_,
            importedAudio_.interleaved.samples,
            importedAudio_.interleaved.frame_count,
            2U,
            importedAudio_.interleaved.sample_rate,
            componentCapacity);
        if (started) {
            started = global_fourier_job_begin_analysis_strided(
                &globalFourierJobRight_,
                importedAudio_.interleaved.samples + 1U,
                importedAudio_.interleaved.frame_count,
                2U,
                importedAudio_.interleaved.sample_rate,
                componentCapacity);
        }
    } else {
        started = global_fourier_job_begin_analysis(
            &globalFourierJob_,
            &importedAudio_.mono,
            componentCapacity);
    }
    if (!started) {
        global_fourier_job_free(&globalFourierJob_);
        global_fourier_job_init(&globalFourierJob_);
        global_fourier_job_free(&globalFourierJobRight_);
        global_fourier_job_init(&globalFourierJobRight_);
        setStatusText(QStringLiteral("Could not allocate the whole-file FFT model"));
        return false;
    }
    fullFileSelectedComponents_ =
        std::max(1, std::min(fullFileSelectedComponents_, available));
    globalSelectedComponents_ = fullFileSelectedComponents_;
    fullFileProgress_ = 0.0;
    fullFileWorkerChannels_ = channelCount;
    fullFileWork_ = GlobalAnalysis;
    if (!background_task_start(
            &fullFileTask_,
            processGlobalFullFileInBackground,
            this)) {
        global_fourier_job_free(&globalFourierJob_);
        global_fourier_job_init(&globalFourierJob_);
        global_fourier_job_free(&globalFourierJobRight_);
        global_fourier_job_init(&globalFourierJobRight_);
        fullFileWork_ = FullFileIdle;
        setStatusText(
            QStringLiteral(
                "Could not start the background Fourier worker"));
        return false;
    }
    setStatusText(
        fullFileSelectionMode_ == 1
            ? QStringLiteral(
                  "Ranking all %1 whole-file FFT bins")
                  .arg(available)
            : QStringLiteral(
                  "Finding the strongest %1 of %2 whole-file FFT bins")
                  .arg(componentCapacity)
                  .arg(available));
    fullFileTimer_.start();
    emit fullFileChanged();
    return true;
}

bool SpectraController::startGlobalReconstruction() {
    const unsigned int channelCount =
        fullFileReconstructionChannels();
    if (!hasReusableGlobalAnalysis()) {
        return false;
    }
    const int maximum = std::max(1, globalFourierJob_.maximum_component_count);
    if (fullFileSelectionMode_ == 1) {
        int resolved =
            global_fourier_job_component_count_for_energy(
                &globalFourierJob_,
                static_cast<float>(fullFileEnergyTarget_));
        if (channelCount == 2U) {
            resolved = std::max(
                resolved,
                global_fourier_job_component_count_for_energy(
                    &globalFourierJobRight_,
                    static_cast<float>(fullFileEnergyTarget_)));
        }
        if (resolved <= 0) {
            setStatusText(
                QStringLiteral("Could not resolve the spectral-energy target"));
            return false;
        }
        fullFileSelectedComponents_ = resolved;
    }
    fullFileSelectedComponents_ =
        std::max(1, std::min(fullFileSelectedComponents_, maximum));
    if (fullFileSelectionMode_ == 0) {
        globalSelectedComponents_ = fullFileSelectedComponents_;
    }
    background_task_cancel_and_join(&fullFileTask_);
    bool started = global_fourier_job_begin_reconstruction(
            &globalFourierJob_,
            fullFileSelectedComponents_);
    if (started && channelCount == 2U) {
        started = global_fourier_job_begin_reconstruction(
            &globalFourierJobRight_,
            fullFileSelectedComponents_);
    }
    if (!started) {
        global_fourier_job_free(&globalFourierJob_);
        global_fourier_job_init(&globalFourierJob_);
        global_fourier_job_free(&globalFourierJobRight_);
        global_fourier_job_init(&globalFourierJobRight_);
        setStatusText(QStringLiteral("Could not allocate the global FFT reconstruction"));
        return false;
    }
    fullFileProgress_ = 0.0;
    fullFileWorkerChannels_ = channelCount;
    fullFileWork_ = GlobalReconstruction;
    if (!background_task_start(
            &fullFileTask_,
            processGlobalFullFileInBackground,
            this)) {
        global_fourier_job_free(&globalFourierJob_);
        global_fourier_job_init(&globalFourierJob_);
        global_fourier_job_free(&globalFourierJobRight_);
        global_fourier_job_init(&globalFourierJobRight_);
        fullFileWork_ = FullFileIdle;
        setStatusText(
            QStringLiteral(
                "Could not start the background reconstruction worker"));
        return false;
    }
    setStatusText(
        fullFileSelectionMode_ == 1
            ? QStringLiteral(
                  "Targeting %1% energy with %2 ranked FFT bins")
                  .arg(fullFileEnergyTarget_ * 100.0, 0, 'f', 2)
                  .arg(fullFileSelectedComponents_)
            : QStringLiteral(
                  "Reconstructing the file from %1 ranked FFT bins")
                  .arg(fullFileSelectedComponents_));
    fullFileTimer_.start();
    emit fullFileChanged();
    return true;
}

bool SpectraController::startStftReconstruction() {
    const unsigned int channelCount =
        fullFileReconstructionChannels();
    const size_t estimatedBytes =
        stft_reconstruction_estimated_bytes(
            importedAudio_.mono.count,
            kFullFileWindowSize,
            fullFileSelectedComponents_,
            false,
            0U,
            0U,
            channelCount);
    if (!stft_reconstruction_fits_memory(
            importedAudio_.mono.count,
            kFullFileWindowSize,
            fullFileSelectedComponents_,
            false,
            0U,
            0U,
            channelCount,
            static_cast<size_t>(fullFileMemoryLimitBytes_))) {
        if (estimatedBytes == 0U) {
            setStatusText(QStringLiteral(
                "Could not estimate STFT reconstruction memory"));
        } else {
            const double estimatedMegabytes =
                static_cast<double>(estimatedBytes) /
                (1024.0 * 1024.0);
            const double limitMegabytes =
                static_cast<double>(fullFileMemoryLimitBytes_) /
                (1024.0 * 1024.0);
            setStatusText(
                QStringLiteral(
                    "%1-channel STFT needs %2 MB, above the %3 MB limit")
                    .arg(channelCount)
                    .arg(estimatedMegabytes, 0, 'f', 0)
                    .arg(limitMegabytes, 0, 'f', 0));
        }
        return false;
    }
    background_task_cancel_and_join(&fullFileTask_);
    stft_reconstruction_job_free(&fullFileStftJob_);
    stft_reconstruction_job_init(&fullFileStftJob_);
    stft_reconstruction_job_free(&fullFileStftJobRight_);
    stft_reconstruction_job_init(&fullFileStftJobRight_);
    const int maximum = static_cast<int>(kFullFileWindowSize / 2U - 1U);
    fullFileSelectedComponents_ =
        std::max(1, std::min(fullFileSelectedComponents_, maximum));
    stftSelectedComponents_ = fullFileSelectedComponents_;
    bool started = false;
    if (channelCount == 2U) {
        started = stft_reconstruction_job_begin_strided(
            &fullFileStftJob_,
            importedAudio_.interleaved.samples,
            importedAudio_.interleaved.frame_count,
            2U,
            importedAudio_.interleaved.sample_rate,
            kFullFileWindowSize,
            kFullFileHopSize,
            fullFileSelectedComponents_,
            false,
            0U,
            0U,
            0.0f);
        if (started) {
            started = stft_reconstruction_job_begin_strided(
                &fullFileStftJobRight_,
                importedAudio_.interleaved.samples + 1U,
                importedAudio_.interleaved.frame_count,
                2U,
                importedAudio_.interleaved.sample_rate,
                kFullFileWindowSize,
                kFullFileHopSize,
                fullFileSelectedComponents_,
                false,
                0U,
                0U,
                0.0f);
        }
    } else {
        started = stft_reconstruction_job_begin(
            &fullFileStftJob_,
            &importedAudio_.mono,
            kFullFileWindowSize,
            kFullFileHopSize,
            fullFileSelectedComponents_,
            false,
            0U,
            0U,
            0.0f);
    }
    if (!started) {
        stft_reconstruction_job_free(&fullFileStftJob_);
        stft_reconstruction_job_init(&fullFileStftJob_);
        stft_reconstruction_job_free(&fullFileStftJobRight_);
        stft_reconstruction_job_init(&fullFileStftJobRight_);
        setStatusText(QStringLiteral("Could not allocate the STFT reconstruction"));
        return false;
    }
    fullFileFrameCount_ =
        static_cast<qsizetype>(fullFileStftJob_.frame_count);
    fullFileProgress_ = 0.0;
    fullFileWorkerChannels_ = channelCount;
    fullFileWork_ = StftReconstruction;
    if (!background_task_start(
            &fullFileTask_,
            processStftFullFileInBackground,
            this)) {
        stft_reconstruction_job_free(&fullFileStftJob_);
        stft_reconstruction_job_init(&fullFileStftJob_);
        stft_reconstruction_job_free(&fullFileStftJobRight_);
        stft_reconstruction_job_init(&fullFileStftJobRight_);
        fullFileWork_ = FullFileIdle;
        setStatusText(
            QStringLiteral(
                "Could not start the background STFT worker"));
        return false;
    }
    setStatusText(
        QStringLiteral("Reconstructing each STFT frame from its top %1 components")
            .arg(fullFileSelectedComponents_));
    fullFileTimer_.start();
    emit fullFileChanged();
    return true;
}

void SpectraController::processFullFileWork() {
    if (fullFileWork_ == SpectrogramBuild) {
        fullFileProgress_ =
            background_task_progress(&spectrogramTask_);
        if (background_task_is_complete(
                &spectrogramTask_)) {
            background_task_join(&spectrogramTask_);
            if (spectrogramJob_.complete) {
                finishSpectrogramBuild();
                return;
            }
            fullFileTimer_.stop();
            fullFileWork_ = FullFileIdle;
            setStatusText(
                QStringLiteral(
                    "Spectrogram analysis failed"));
        }
        emit fullFileChanged();
        return;
    }

    if (fullFileWork_ == GlobalAnalysis ||
        fullFileWork_ == GlobalReconstruction) {
        fullFileProgress_ =
            background_task_progress(&fullFileTask_);
        if (!background_task_is_complete(&fullFileTask_)) {
            emit fullFileChanged();
            return;
        }

        const FullFileWork completedWork =
            fullFileWork_;
        const unsigned int channelCount =
            fullFileWorkerChannels_;
        background_task_join(&fullFileTask_);
        if (globalFourierJob_.phase == GLOBAL_FOURIER_FAILED ||
            (channelCount == 2U &&
             globalFourierJobRight_.phase ==
                 GLOBAL_FOURIER_FAILED)) {
            fullFileTimer_.stop();
            fullFileWork_ = FullFileIdle;
            setStatusText(QStringLiteral("Whole-file FFT processing failed"));
            emit fullFileChanged();
            return;
        }
        if (completedWork == GlobalAnalysis &&
            globalFourierJob_.analysis_ready &&
            (channelCount == 1U ||
             globalFourierJobRight_.analysis_ready)) {
            if (!startGlobalReconstruction()) {
                fullFileTimer_.stop();
                fullFileWork_ = FullFileIdle;
                emit fullFileChanged();
            }
            return;
        }
        if (completedWork == GlobalReconstruction &&
            globalFourierJob_.reconstruction_ready &&
            (channelCount == 1U ||
             globalFourierJobRight_.reconstruction_ready)) {
            const double totalEnergy =
                globalFourierJob_.total_spectral_energy +
                (channelCount == 2U
                     ? globalFourierJobRight_
                           .total_spectral_energy
                     : 0.0);
            const double retainedEnergy =
                totalEnergy > 0.0
                    ? (globalFourierJob_
                               .retained_spectral_energy +
                           (channelCount == 2U
                                ? globalFourierJobRight_
                                      .retained_spectral_energy
                                : 0.0)) /
                          totalEnergy
                    : 0.0;
            SampleBuffer left =
                global_fourier_job_take_output(&globalFourierJob_);
            SampleBuffer right = {};
            if (channelCount == 2U) {
                right = global_fourier_job_take_output(
                    &globalFourierJobRight_);
            }
            finishFullFileReconstruction(
                left,
                right,
                channelCount,
                retainedEnergy);
            return;
        }
        fullFileTimer_.stop();
        fullFileWork_ = FullFileIdle;
        setStatusText(
            completedWork == GlobalAnalysis
                ? QStringLiteral(
                      "Background whole-file Fourier analysis did not complete")
                : QStringLiteral(
                      "Background whole-file reconstruction did not complete"));
        emit fullFileChanged();
        return;
    }

    if (fullFileWork_ == StftReconstruction) {
        fullFileProgress_ =
            background_task_progress(&fullFileTask_);
        if (!background_task_is_complete(&fullFileTask_)) {
            emit fullFileChanged();
            return;
        }

        const unsigned int channelCount =
            fullFileWorkerChannels_;
        background_task_join(&fullFileTask_);
        if (fullFileStftJob_.failed ||
            (channelCount == 2U &&
             fullFileStftJobRight_.failed)) {
            fullFileTimer_.stop();
            fullFileWork_ = FullFileIdle;
            setStatusText(QStringLiteral("STFT reconstruction failed"));
            emit fullFileChanged();
            return;
        }
        if (fullFileStftJob_.complete &&
            (channelCount == 1U ||
             fullFileStftJobRight_.complete)) {
            const double totalEnergy =
                fullFileStftJob_.total_spectral_energy +
                (channelCount == 2U
                     ? fullFileStftJobRight_
                           .total_spectral_energy
                     : 0.0);
            const double retainedEnergy =
                totalEnergy > 0.0
                    ? (fullFileStftJob_
                               .retained_spectral_energy +
                           (channelCount == 2U
                                ? fullFileStftJobRight_
                                      .retained_spectral_energy
                                : 0.0)) /
                          totalEnergy
                    : 0.0;
            SampleBuffer left =
                stft_reconstruction_job_take_output(
                    &fullFileStftJob_);
            SampleBuffer right = {};
            if (channelCount == 2U) {
                right =
                    stft_reconstruction_job_take_output(
                        &fullFileStftJobRight_);
            }
            stft_reconstruction_job_free(&fullFileStftJob_);
            stft_reconstruction_job_init(&fullFileStftJob_);
            stft_reconstruction_job_free(
                &fullFileStftJobRight_);
            stft_reconstruction_job_init(
                &fullFileStftJobRight_);
            finishFullFileReconstruction(
                left,
                right,
                channelCount,
                retainedEnergy);
            return;
        }
        fullFileTimer_.stop();
        fullFileWork_ = FullFileIdle;
        setStatusText(
            QStringLiteral(
                "Background STFT reconstruction did not complete"));
        emit fullFileChanged();
    }
}

void SpectraController::finishSpectrogramBuild() {
    SpectrogramData spectrogram =
        stft_reconstruction_job_take_spectrogram(&spectrogramJob_);
    if (spectrogram.db_values != nullptr &&
        spectrogram.time_bins > 0U &&
        spectrogram.frequency_bins > 0U) {
        QImage image(
            static_cast<int>(spectrogram.time_bins),
            static_cast<int>(spectrogram.frequency_bins),
            QImage::Format_RGB32);
        for (unsigned int y = 0U;
             y < spectrogram.frequency_bins;
             ++y) {
            const unsigned int sourceY =
                spectrogram.frequency_bins - 1U - y;
            for (unsigned int x = 0U;
                 x < spectrogram.time_bins;
                 ++x) {
                const size_t index =
                    static_cast<size_t>(sourceY) *
                        spectrogram.time_bins +
                    x;
                image.setPixelColor(
                    static_cast<int>(x),
                    static_cast<int>(y),
                    spectrogramColor(
                        spectrogram.db_values[index]));
            }
        }

        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        if (image.save(&buffer, "PNG")) {
            spectrogramImageUrl_ = QUrl(
                QStringLiteral("data:image/png;base64,") +
                QString::fromLatin1(png.toBase64()));
        }
    }
    spectrogram_data_free(&spectrogram);
    stft_reconstruction_job_free(&spectrogramJob_);
    stft_reconstruction_job_init(&spectrogramJob_);
    fullFileTimer_.stop();
    fullFileWork_ = FullFileIdle;
    fullFileProgress_ = 1.0;
    setStatusText(
        spectrogramImageUrl_.isEmpty()
            ? QStringLiteral("Spectrogram image encoding failed")
            : QStringLiteral("Whole-file spectrogram ready"));
    emit fullFileChanged();
}

void SpectraController::finishFullFileReconstruction(
    SampleBuffer left,
    SampleBuffer right,
    unsigned int channelCount,
    double retainedEnergy) {
    fullFileTimer_.stop();
    fullFileWork_ = FullFileIdle;
    clearFullFileOutput();
    SampleBuffer channels[2] = {left, right};
    const bool interleaved = interleave_sample_buffers(
        channels,
        channelCount,
        &fullFileBuffer_);
    sample_buffer_free(&left);
    sample_buffer_free(&right);
    fullFileRetainedEnergy_ =
        interleaved
            ? clampValue(retainedEnergy, 0.0, 1.0)
            : 0.0;
    fullFileProgress_ = fullFileReady() ? 1.0 : 0.0;
    bool playbackReady = true;
    if (audioReady_ && fullFileReady()) {
        playbackReady = audio_clip_set_interleaved(
            &fullFileClip_,
            &fullFileBuffer_);
    }
    setStatusText(
        fullFileReady()
            ? (playbackReady
                   ? (fullFileMode_ == 0 &&
                              fullFileSelectionMode_ == 1
                          ? QStringLiteral(
                                "%1% target resolved to %2 bins; %3% retained")
                                .arg(
                                    fullFileEnergyTarget_ * 100.0,
                                    0,
                                    'f',
                                    2)
                                .arg(fullFileSelectedComponents_)
                                .arg(
                                    fullFileRetainedEnergy_ * 100.0,
                                    0,
                                    'f',
                                    3)
                          : QStringLiteral(
                                "%1 reconstruction ready; %2% retained energy")
                                .arg(
                                    fullFileMode_ == 0
                                        ? (channelCount > 1U
                                               ? QStringLiteral("Stereo FFT")
                                               : QStringLiteral("Mono FFT"))
                                        : QStringLiteral("STFT"))
                                .arg(
                                    fullFileRetainedEnergy_ * 100.0,
                                    0,
                                    'f',
                                    2))
                   : QStringLiteral("Reconstruction ready for export; playback is unavailable"))
            : QStringLiteral("Whole-file reconstruction produced no samples"));
    emit fullFileChanged();
    emit playbackChanged();
}

void SpectraController::seekClip(
    AudioClip *clip,
    PlaybackTarget target,
    double position) {
    if (clip == nullptr ||
        audio_clip_duration_seconds(clip) <= 0.0f) {
        return;
    }
    if (playbackTarget_ != target) {
        haltAllAudio();
        playbackTarget_ = target;
    }
    if (audio_clip_seek(
            clip,
            static_cast<float>(position))) {
        emit playbackChanged();
    }
}

void SpectraController::refreshPlaybackState() {
    audio_clip_update(activeClip());
    if (playbackTarget_ != NoPlayback && activeClip() != nullptr &&
        !audio_clip_is_active(activeClip())) {
        playbackTarget_ = NoPlayback;
    }
    emit playbackChanged();
}

AudioClip *SpectraController::activeClip() {
    if (playbackTarget_ == SynthPlayback) {
        return &synthClip_;
    }
    if (playbackTarget_ == SourcePlayback) {
        return &sourceClip_;
    }
    if (playbackTarget_ == RegionPlayback) {
        return &regionClip_;
    }
    if (playbackTarget_ == HarmonicPlayback) {
        return &harmonicClip_;
    }
    if (playbackTarget_ == OriginalFramePlayback) {
        return &originalFrameClip_;
    }
    if (playbackTarget_ == FourierFramePlayback) {
        return &fourierClip_;
    }
    if (playbackTarget_ == FullFilePlayback) {
        return &fullFileClip_;
    }
    if (playbackTarget_ == EffectPlayback) {
        return &effectClip_;
    }
    if (playbackTarget_ == EqPlayback) {
        return &eqClip_;
    }
    if (playbackTarget_ ==
        InstantPreviewOriginalPlayback) {
        return &instantPreviewOriginalClip_;
    }
    if (playbackTarget_ ==
        InstantPreviewProcessedPlayback) {
        return &instantPreviewProcessedClip_;
    }
    return nullptr;
}

const AudioClip *SpectraController::activeClip() const {
    if (playbackTarget_ == SynthPlayback) {
        return &synthClip_;
    }
    if (playbackTarget_ == SourcePlayback) {
        return &sourceClip_;
    }
    if (playbackTarget_ == RegionPlayback) {
        return &regionClip_;
    }
    if (playbackTarget_ == HarmonicPlayback) {
        return &harmonicClip_;
    }
    if (playbackTarget_ == OriginalFramePlayback) {
        return &originalFrameClip_;
    }
    if (playbackTarget_ == FourierFramePlayback) {
        return &fourierClip_;
    }
    if (playbackTarget_ == FullFilePlayback) {
        return &fullFileClip_;
    }
    if (playbackTarget_ == EffectPlayback) {
        return &effectClip_;
    }
    if (playbackTarget_ == EqPlayback) {
        return &eqClip_;
    }
    if (playbackTarget_ ==
        InstantPreviewOriginalPlayback) {
        return &instantPreviewOriginalClip_;
    }
    if (playbackTarget_ ==
        InstantPreviewProcessedPlayback) {
        return &instantPreviewProcessedClip_;
    }
    return nullptr;
}
