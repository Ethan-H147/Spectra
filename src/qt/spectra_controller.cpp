#include "qt/spectra_controller.h"

#include <QBuffer>
#include <QColor>
#include <QFileInfo>
#include <QImage>
#include <QPointF>
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

namespace {
constexpr unsigned int kSampleRate = 44100U;
constexpr int kHarmonicCount = 16;
constexpr int kMaximumPeaks = 12;
constexpr int kWaveformPoints = 512;
constexpr int kSpectrumPoints = 512;
constexpr int kSourceWaveformPoints = 1024;
constexpr unsigned int kFullFileWindowSize = 2048U;
constexpr unsigned int kFullFileHopSize = 512U;
constexpr unsigned int kSpectrogramTimeBins = 256U;
constexpr unsigned int kSpectrogramFrequencyBins = 128U;
constexpr float kSpectrogramMaximumFrequency = 12000.0f;
constexpr double kFullFileMaximumDuration = 600.0;
constexpr size_t kGlobalOperationsPerTick = 65536U;
constexpr size_t kStftFramesPerTick = 16U;
constexpr size_t kReconstructionCacheLimit =
    256U * 1024U * 1024U;
constexpr ADSREnvelope kDefaultEnvelope = {0.015f, 0.08f, 0.75f, 0.12f};

const char *const kPresetNames[] = {
    "Sine",
    "Square-like",
    "Saw-like",
    "Clarinet-like",
    "Bright string",
};

double clampValue(double value, double minimum, double maximum) {
    return std::max(minimum, std::min(value, maximum));
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
}  // namespace

SpectraController::SpectraController(QObject *parent)
    : QObject(parent) {
    audio_clip_init(&synthClip_);
    audio_clip_init(&sourceClip_);
    audio_clip_init(&regionClip_);
    audio_clip_init(&harmonicClip_);
    audio_clip_init(&originalFrameClip_);
    audio_clip_init(&fourierClip_);
    audio_clip_init(&fullFileClip_);
    fourier_frame_analysis_init(&fourierAnalysis_);
    global_fourier_job_init(&globalFourierJob_);
    global_fourier_job_init(&globalFourierJobRight_);
    stft_reconstruction_job_init(&spectrogramJob_);
    stft_reconstruction_job_init(&fullFileStftJob_);
    stft_reconstruction_job_init(&fullFileStftJobRight_);
    reconstruction_cache_init(
        &reconstructionCache_,
        kReconstructionCacheLimit);
    imported_audio_init(&importedAudio_);

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

    fullFileTimer_.setInterval(16);
    connect(
        &fullFileTimer_,
        &QTimer::timeout,
        this,
        &SpectraController::processFullFileWork);

    applySynthPreset(0);
    rebuildSynth();
}

SpectraController::~SpectraController() {
    fullFileTimer_.stop();
    audio_clip_unload(&fullFileClip_);
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
            return QStringLiteral("Top-%1 Fourier frame")
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
    const int maximum =
        global_fourier_available_component_count(
            importedAudio_.mono.count);
    return maximum > 0
        ? static_cast<qulonglong>(
              global_fourier_estimated_multichannel_analysis_bytes(
                  importedAudio_.mono.count,
                  maximum,
                  1U))
        : 0;
}

qulonglong SpectraController::fullFileEstimatedSourceBytes() const {
    if (!sourceLoaded()) {
        return 0;
    }
    const int maximum =
        global_fourier_available_component_count(
            importedAudio_.mono.count);
    const unsigned int channels =
        importedAudio_.source_channels == 2U &&
                importedAudio_.interleaved.channel_count == 2U
            ? 2U
            : 1U;
    return maximum > 0
        ? static_cast<qulonglong>(
              global_fourier_estimated_multichannel_analysis_bytes(
                  importedAudio_.mono.count,
                  maximum,
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
    if (globalFourierJob_.maximum_component_count > 0) {
        return globalFourierJob_.maximum_component_count;
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

void SpectraController::setCurrentPage(int page) {
    const int bounded = std::max(0, std::min(page, 5));
    if (bounded == currentPage_) {
        return;
    }
    currentPage_ = bounded;
    emit currentPageChanged();
}

void SpectraController::setTextScale(double scale) {
    const double bounded = clampValue(scale, 0.90, 1.40);
    if (qFuzzyCompare(bounded, textScale_)) {
        return;
    }
    textScale_ = bounded;
    emit textScaleChanged();
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

    SampleBuffer fftFrame = region;
    if (fftFrame.count > 16384U) {
        fftFrame.samples += (fftFrame.count - 16384U) / 2U;
        fftFrame.count = 16384U;
    }
    analysisSpectrumBuffer_ = compute_magnitude_spectrum(
        fftFrame.samples,
        fftFrame.count,
        fftFrame.sample_rate);
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
    if (!analysisReady_) {
        analyzeRegion();
    }
    if (!audioReady_ || !analysisReady_) {
        return;
    }
    haltAllAudio();
    if (audio_clip_play(&regionClip_)) {
        playbackTarget_ = RegionPlayback;
        lastLabPlaybackTarget_ = RegionPlayback;
        setStatusText(QStringLiteral("Playing analyzed region"));
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
        setStatusText(QStringLiteral("Playing Top-%1 Fourier frame")
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

void SpectraController::rebuildFourierFrame(int componentCount) {
    if (rebuildFourierFrameBuffer(componentCount)) {
        setStatusText(QStringLiteral("Rebuilt frame from %1 Fourier components")
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
                      ? QStringLiteral("Exported Fourier frame")
                      : QStringLiteral("Could not export Fourier frame"));
    return exported;
}

void SpectraController::setFullFileMode(int mode) {
    const int bounded = mode == 1 ? 1 : 0;
    if (bounded == fullFileMode_) {
        return;
    }

    cacheFullFileOutput();
    fullFileTimer_.stop();
    if (fullFileWork_ == SpectrogramBuild) {
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
            globalFourierJob_.analysis_ready) {
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
    } else if (globalFourierJob_.analysis_ready) {
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
    if (globalFourierJob_.analysis_ready) {
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
    constexpr qulonglong minimum =
        128ULL * 1024ULL * 1024ULL;
    constexpr qulonglong maximum =
        4ULL * 1024ULL * 1024ULL * 1024ULL;
    const qulonglong bounded =
        std::max(minimum, std::min(bytes, maximum));
    if (bounded == fullFileMemoryLimitBytes_) {
        return;
    }
    fullFileMemoryLimitBytes_ = bounded;
    setStatusText(
        QStringLiteral("Whole-file FFT memory limit set to %1 MB")
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
    if (sourceDuration() > kFullFileMaximumDuration) {
        setStatusText(QStringLiteral("Whole-file reconstruction is limited to 600 seconds"));
        return;
    }
    if (fullFileProcessing()) {
        return;
    }

    clearFullFileOutput();
    fullFileProgress_ = 0.0;
    if (restoreFullFileOutput()) {
        emit fullFileChanged();
        emit playbackChanged();
        return;
    }
    const bool started =
        fullFileMode_ == 0
            ? (globalFourierJob_.analysis_ready
                   ? startGlobalReconstruction()
                   : startGlobalAnalysis())
            : startStftReconstruction();
    if (!started) {
        fullFileWork_ = FullFileIdle;
        emit fullFileChanged();
    }
}

void SpectraController::playFullFileOriginal() {
    if (!audioReady_ || !sourceLoaded()) {
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
    if (!audioReady_ || !fullFileReady()) {
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
        if (fullFileReady()) {
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
    playbackTarget_ = NoPlayback;
}

void SpectraController::importAudioFile(const QUrl &url) {
    const QString localPath = url.isLocalFile() ? url.toLocalFile() : url.toString();
    if (localPath.isEmpty()) {
        return;
    }

    stopPlayback();
    resetAnalysis();
    resetFullFile();
    sourceWaveformMinimums_.clear();
    sourceWaveformMaximums_.clear();
    audio_clip_unload(&sourceClip_);
    imported_audio_unload(&importedAudio_);
    imported_audio_init(&importedAudio_);
    audio_clip_init(&sourceClip_);

    QByteArray encodedPath = localPath.toUtf8();
    char error[256] = {};
    if (!imported_audio_load(encodedPath.constData(), &importedAudio_, error, sizeof(error))) {
        setStatusText(QString::fromUtf8(error));
        emit sourceChanged();
        return;
    }
    if (audioReady_) {
        audio_clip_set_interleaved(&sourceClip_, &importedAudio_.interleaved);
    }
    sourceFileName_ = QFileInfo(localPath).fileName();
    regionDuration_ = std::min(1.0, sourceDuration());
    regionStart_ = std::max(0.0, (sourceDuration() - regionDuration_) * 0.5);
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
    setStatusText(QStringLiteral("Loaded %1").arg(sourceFileName_));
    emit sourceChanged();
    emit playbackChanged();
    analyzeRegion();
    startSpectrogramBuild();
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
        const size_t visibleCount = std::min(
            synthSpectrumBuffer_.count,
            static_cast<size_t>(8000.0 * synthSpectrumBuffer_.count * 2.0 / kSampleRate));
        synthSpectrum_.reserve(kSpectrumPoints);
        for (int point = 0; point < kSpectrumPoints; ++point) {
            const size_t index = static_cast<size_t>(point) * std::max<size_t>(1U, visibleCount - 1U)
                / std::max(1, kSpectrumPoints - 1);
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
        const size_t visibleCount = analysisSpectrumBuffer_.count;
        analysisSpectrum_.reserve(kSpectrumPoints);
        for (int point = 0; point < kSpectrumPoints; ++point) {
            const size_t index = static_cast<size_t>(point) *
                std::max<size_t>(1U, visibleCount - 1U) /
                std::max(1, kSpectrumPoints - 1);
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
            audio_clip_set_samples(&originalFrameClip_, &fourierAnalysis_.windowed_frame);
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
    fourierBuffer_ = reconstruct_fourier_frame(
        &fourierAnalysis_,
        static_cast<size_t>(bounded));
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
    emit fullFileChanged();
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
    if (sourceDuration() > kFullFileMaximumDuration) {
        setStatusText(QStringLiteral("Spectrogram analysis is limited to 600 seconds"));
        return;
    }

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

bool SpectraController::startGlobalAnalysis() {
    const unsigned int channelCount =
        fullFileReconstructionChannels();
    const int available =
        global_fourier_available_component_count(importedAudio_.mono.count);
    if (available <= 0) {
        setStatusText(QStringLiteral("Could not determine the whole-file FFT grid"));
        return false;
    }
    if (!global_fourier_analysis_fits_memory(
            importedAudio_.mono.count,
            available,
            channelCount,
            static_cast<size_t>(fullFileMemoryLimitBytes_))) {
        const double estimatedMegabytes =
            static_cast<double>(
                global_fourier_estimated_multichannel_analysis_bytes(
                    importedAudio_.mono.count,
                    available,
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
            available);
        if (started) {
            started = global_fourier_job_begin_analysis_strided(
                &globalFourierJobRight_,
                importedAudio_.interleaved.samples + 1U,
                importedAudio_.interleaved.frame_count,
                2U,
                importedAudio_.interleaved.sample_rate,
                available);
        }
    } else {
        started = global_fourier_job_begin_analysis(
            &globalFourierJob_,
            &importedAudio_.mono,
            available);
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
    fullFileWork_ = GlobalAnalysis;
    setStatusText(QStringLiteral("Ranking the whole-file FFT components"));
    fullFileTimer_.start();
    emit fullFileChanged();
    return true;
}

bool SpectraController::startGlobalReconstruction() {
    const unsigned int channelCount =
        fullFileReconstructionChannels();
    if (!globalFourierJob_.analysis_ready ||
        (channelCount == 2U &&
         !globalFourierJobRight_.analysis_ready)) {
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
    fullFileWork_ = GlobalReconstruction;
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
    fullFileWork_ = StftReconstruction;
    setStatusText(
        QStringLiteral("Reconstructing each STFT frame from its top %1 components")
            .arg(fullFileSelectedComponents_));
    fullFileTimer_.start();
    emit fullFileChanged();
    return true;
}

void SpectraController::processFullFileWork() {
    if (fullFileWork_ == SpectrogramBuild) {
        stft_reconstruction_job_process(
            &spectrogramJob_,
            kStftFramesPerTick);
        fullFileProgress_ =
            stft_reconstruction_job_progress(&spectrogramJob_);
        if (spectrogramJob_.complete) {
            finishSpectrogramBuild();
            return;
        }
        if (spectrogramJob_.failed) {
            fullFileTimer_.stop();
            fullFileWork_ = FullFileIdle;
            setStatusText(QStringLiteral("Spectrogram analysis failed"));
        }
        emit fullFileChanged();
        return;
    }

    if (fullFileWork_ == GlobalAnalysis ||
        fullFileWork_ == GlobalReconstruction) {
        const unsigned int channelCount =
            fullFileReconstructionChannels();
        global_fourier_job_process(
            &globalFourierJob_,
            kGlobalOperationsPerTick);
        if (channelCount == 2U) {
            global_fourier_job_process(
                &globalFourierJobRight_,
                kGlobalOperationsPerTick);
        }
        const double leftProgress =
            global_fourier_job_progress(&globalFourierJob_);
        const double rightProgress =
            channelCount == 2U
                ? global_fourier_job_progress(
                      &globalFourierJobRight_)
                : leftProgress;
        fullFileProgress_ =
            (leftProgress + rightProgress) * 0.5;
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
        if (fullFileWork_ == GlobalAnalysis &&
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
        if (fullFileWork_ == GlobalReconstruction &&
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
        emit fullFileChanged();
        return;
    }

    if (fullFileWork_ == StftReconstruction) {
        const unsigned int channelCount =
            fullFileReconstructionChannels();
        stft_reconstruction_job_process(
            &fullFileStftJob_,
            kStftFramesPerTick);
        if (channelCount == 2U) {
            stft_reconstruction_job_process(
                &fullFileStftJobRight_,
                kStftFramesPerTick);
        }
        const double leftProgress =
            stft_reconstruction_job_progress(
                &fullFileStftJob_);
        const double rightProgress =
            channelCount == 2U
                ? stft_reconstruction_job_progress(
                      &fullFileStftJobRight_)
                : leftProgress;
        fullFileProgress_ =
            (leftProgress + rightProgress) * 0.5;
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

void SpectraController::refreshPlaybackState() {
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
    return nullptr;
}
