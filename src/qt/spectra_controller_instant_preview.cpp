// Instant region previews shared by the effect and EQ workspaces.

#include "qt/controller_support.h"

using namespace spectra_controller_detail;

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
