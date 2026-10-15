// Construction, settings, diagnostics, source import, and shared playback.

#include "qt/controller_support.h"

using namespace spectra_controller_detail;

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

void SpectraController::setStatusText(const QString &status) {
    if (status == statusText_) {
        return;
    }
    statusText_ = status;
    emit statusTextChanged();
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
