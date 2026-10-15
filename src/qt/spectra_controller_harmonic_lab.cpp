// Harmonic Lab workspace: harmonic resynthesis and evolving Fourier frames.

#include "qt/controller_support.h"

using namespace spectra_controller_detail;

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
