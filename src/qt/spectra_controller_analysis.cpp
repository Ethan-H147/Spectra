// Audio Analysis workspace: region selection, spectrum, peaks, and pitch.

#include "qt/controller_support.h"

using namespace spectra_controller_detail;

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

QList<qreal> SpectraController::analysisSpectrum() const {
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
