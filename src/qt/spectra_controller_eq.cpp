// Range EQ workspace: band editing, presets, preview, and rendering.

#include "qt/controller_support.h"

using namespace spectra_controller_detail;

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

QList<qreal> SpectraController::eqOriginalSpectrum() const {
    return effectOriginalSpectrum_;
}

QList<qreal> SpectraController::eqTransformedSpectrum() const {
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

void SpectraController::clearEqOutput() {
    if (playbackTarget_ == EqPlayback) {
        playbackTarget_ = NoPlayback;
    }
    audio_clip_unload(&eqClip_);
    audio_clip_init(&eqClip_);
    interleaved_buffer_free(&eqBuffer_);
    spectrum_free(&eqOutputSpectrumBuffer_);
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
                effectOriginalSpectrum_[index] +
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
