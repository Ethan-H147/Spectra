// Pitch & Shift workspace: tape speed, phase-vocoder, and frequency-shift effects.

#include "qt/controller_support.h"

using namespace spectra_controller_detail;

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

QList<qreal> SpectraController::effectOriginalSpectrum() const {
    return effectOriginalSpectrum_;
}

QList<qreal> SpectraController::effectTransformedSpectrum() const {
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

void SpectraController::clearEffectOutput() {
    if (playbackTarget_ == EffectPlayback) {
        playbackTarget_ = NoPlayback;
    }
    audio_clip_unload(&effectClip_);
    audio_clip_init(&effectClip_);
    interleaved_buffer_free(&effectBuffer_);
    spectrum_free(&effectOutputSpectrumBuffer_);
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
