// Spectrogram workspace: STFT display, PDF export, and whole-file reconstruction.

#include "qt/controller_support.h"
#include "qt/spectrogram_pdf_export.h"

using namespace spectra_controller_detail;

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
