#ifndef SPECTRA_QT_SPECTRA_CONTROLLER_H
#define SPECTRA_QT_SPECTRA_CONTROLLER_H

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

#include <vector>

extern "C" {
#include "audio/audio_engine.h"
#include "audio/audio_import.h"
#include "audio/reconstruction_cache.h"
#include "dsp/dsp_types.h"
#include "dsp/fourier_reconstruction.h"
#include "dsp/global_fourier_reconstruction.h"
#include "dsp/harmonic_analysis.h"
#include "dsp/pitch_detection.h"
#include "dsp/spectral_effects.h"
#include "dsp/stft_reconstruction.h"
#include "platform/background_task.h"
#include "platform/parallel_for.h"
}

class SpectraController final : public QObject {
    Q_OBJECT

    Q_PROPERTY(int currentPage READ currentPage WRITE setCurrentPage NOTIFY currentPageChanged)
    Q_PROPERTY(bool audioReady READ audioReady NOTIFY audioReadyChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(double textScale READ textScale WRITE setTextScale NOTIFY textScaleChanged)
    Q_PROPERTY(int performanceMode READ performanceMode NOTIFY performanceChanged)
    Q_PROPERTY(QString performanceModeName READ performanceModeName NOTIFY performanceChanged)
    Q_PROPERTY(int processingWorkerCount READ processingWorkerCount NOTIFY performanceChanged)
    Q_PROPERTY(int hardwareThreadCount READ hardwareThreadCount CONSTANT)
    Q_PROPERTY(QString processingBackendName READ processingBackendName CONSTANT)
    Q_PROPERTY(QString applicationVersion READ applicationVersion CONSTANT)
    Q_PROPERTY(QString buildCommit READ buildCommit CONSTANT)
    Q_PROPERTY(QString systemDescription READ systemDescription CONSTANT)

    Q_PROPERTY(double synthFrequency READ synthFrequency WRITE setSynthFrequency NOTIFY synthChanged)
    Q_PROPERTY(double synthDuration READ synthDuration WRITE setSynthDuration NOTIFY synthChanged)
    Q_PROPERTY(double synthGain READ synthGain WRITE setSynthGain NOTIFY synthChanged)
    Q_PROPERTY(int synthPreset READ synthPreset NOTIFY synthChanged)
    Q_PROPERTY(QString synthPresetName READ synthPresetName NOTIFY synthChanged)
    Q_PROPERTY(QVariantList harmonicAmplitudes READ harmonicAmplitudes NOTIFY synthChanged)
    Q_PROPERTY(QVariantList synthWaveform READ synthWaveform NOTIFY synthVisualizationChanged)
    Q_PROPERTY(QVariantList synthWaveformMinimums READ synthWaveformMinimums NOTIFY synthVisualizationChanged)
    Q_PROPERTY(QVariantList synthWaveformMaximums READ synthWaveformMaximums NOTIFY synthVisualizationChanged)
    Q_PROPERTY(QVariantList synthSpectrum READ synthSpectrum NOTIFY synthVisualizationChanged)
    Q_PROPERTY(QVariantList synthPeaks READ synthPeaks NOTIFY synthVisualizationChanged)
    Q_PROPERTY(QString synthPitch READ synthPitch NOTIFY synthVisualizationChanged)
    Q_PROPERTY(QString synthNote READ synthNote NOTIFY synthVisualizationChanged)
    Q_PROPERTY(int synthPeakCount READ synthPeakCount NOTIFY synthVisualizationChanged)
    Q_PROPERTY(bool synthPlaying READ synthPlaying NOTIFY playbackChanged)

    Q_PROPERTY(bool sourceLoaded READ sourceLoaded NOTIFY sourceChanged)
    Q_PROPERTY(QString sourceFileName READ sourceFileName NOTIFY sourceChanged)
    Q_PROPERTY(int sourceChannels READ sourceChannels NOTIFY sourceChanged)
    Q_PROPERTY(int sourceSampleRate READ sourceSampleRate NOTIFY sourceChanged)
    Q_PROPERTY(double sourceDuration READ sourceDuration NOTIFY sourceChanged)
    Q_PROPERTY(bool sourcePlaying READ sourcePlaying NOTIFY playbackChanged)
    Q_PROPERTY(double playbackPosition READ playbackPosition NOTIFY playbackChanged)
    Q_PROPERTY(double playbackDuration READ playbackDuration NOTIFY playbackChanged)

    Q_PROPERTY(bool analysisReady READ analysisReady NOTIFY analysisChanged)
    Q_PROPERTY(double regionStart READ regionStart WRITE setRegionStart NOTIFY analysisChanged)
    Q_PROPERTY(double regionDuration READ regionDuration WRITE setRegionDuration NOTIFY analysisChanged)
    Q_PROPERTY(QVariantList sourceWaveformMinimums READ sourceWaveformMinimums NOTIFY sourceChanged)
    Q_PROPERTY(QVariantList sourceWaveformMaximums READ sourceWaveformMaximums NOTIFY sourceChanged)
    Q_PROPERTY(QVariantList analysisSpectrum READ analysisSpectrum NOTIFY analysisChanged)
    Q_PROPERTY(QVariantList analysisPeaks READ analysisPeaks NOTIFY analysisChanged)
    Q_PROPERTY(int analysisPeakReadoutMode READ analysisPeakReadoutMode NOTIFY analysisChanged)
    Q_PROPERTY(QString analysisPitch READ analysisPitch NOTIFY analysisChanged)
    Q_PROPERTY(QString analysisNote READ analysisNote NOTIFY analysisChanged)
    Q_PROPERTY(double analysisConfidence READ analysisConfidence NOTIFY analysisChanged)
    Q_PROPERTY(int analysisPeakCount READ analysisPeakCount NOTIFY analysisChanged)
    Q_PROPERTY(bool regionPlaying READ regionPlaying NOTIFY playbackChanged)
    Q_PROPERTY(bool regionActive READ regionActive NOTIFY playbackChanged)

    Q_PROPERTY(bool harmonicReady READ harmonicReady NOTIFY reconstructionChanged)
    Q_PROPERTY(int harmonicCount READ harmonicCount NOTIFY reconstructionChanged)
    Q_PROPERTY(int detectedHarmonicCount READ detectedHarmonicCount NOTIFY reconstructionChanged)
    Q_PROPERTY(QVariantList extractedHarmonics READ extractedHarmonics NOTIFY reconstructionChanged)
    Q_PROPERTY(bool fourierFrameReady READ fourierFrameReady NOTIFY reconstructionChanged)
    Q_PROPERTY(int fourierMaximumComponents READ fourierMaximumComponents NOTIFY reconstructionChanged)
    Q_PROPERTY(int fourierSelectedComponents READ fourierSelectedComponents NOTIFY reconstructionChanged)
    Q_PROPERTY(int fourierFftSize READ fourierFftSize NOTIFY reconstructionChanged)
    Q_PROPERTY(double fourierFrameDuration READ fourierFrameDuration NOTIFY reconstructionChanged)
    Q_PROPERTY(QVariantList fourierComponents READ fourierComponents NOTIFY reconstructionChanged)
    Q_PROPERTY(bool harmonicPlaying READ harmonicPlaying NOTIFY playbackChanged)
    Q_PROPERTY(bool framePlaying READ framePlaying NOTIFY playbackChanged)
    Q_PROPERTY(bool fourierPlaying READ fourierPlaying NOTIFY playbackChanged)
    Q_PROPERTY(QString labPlaybackTitle READ labPlaybackTitle NOTIFY playbackChanged)

    Q_PROPERTY(bool fullFileProcessing READ fullFileProcessing NOTIFY fullFileChanged)
    Q_PROPERTY(bool fullFileReady READ fullFileReady NOTIFY fullFileChanged)
    Q_PROPERTY(int fullFileMode READ fullFileMode NOTIFY fullFileChanged)
    Q_PROPERTY(int fullFileChannelMode READ fullFileChannelMode NOTIFY fullFileChanged)
    Q_PROPERTY(int fullFileSelectionMode READ fullFileSelectionMode NOTIFY fullFileChanged)
    Q_PROPERTY(double fullFileEnergyTarget READ fullFileEnergyTarget NOTIFY fullFileChanged)
    Q_PROPERTY(qulonglong fullFileEstimatedMonoBytes READ fullFileEstimatedMonoBytes NOTIFY fullFileChanged)
    Q_PROPERTY(qulonglong fullFileEstimatedSourceBytes READ fullFileEstimatedSourceBytes NOTIFY fullFileChanged)
    Q_PROPERTY(qulonglong fullFileMemoryLimitBytes READ fullFileMemoryLimitBytes NOTIFY fullFileChanged)
    Q_PROPERTY(int fullFileCacheEntries READ fullFileCacheEntries NOTIFY fullFileChanged)
    Q_PROPERTY(int fullFileOutputChannels READ fullFileOutputChannels NOTIFY fullFileChanged)
    Q_PROPERTY(double fullFileProgress READ fullFileProgress NOTIFY fullFileChanged)
    Q_PROPERTY(int fullFileSelectedComponents READ fullFileSelectedComponents NOTIFY fullFileChanged)
    Q_PROPERTY(int fullFileMaximumComponents READ fullFileMaximumComponents NOTIFY fullFileChanged)
    Q_PROPERTY(double fullFileRetainedEnergy READ fullFileRetainedEnergy NOTIFY fullFileChanged)
    Q_PROPERTY(int fullFileWindowSize READ fullFileWindowSize CONSTANT)
    Q_PROPERTY(int fullFileHopSize READ fullFileHopSize CONSTANT)
    Q_PROPERTY(qsizetype fullFileFrameCount READ fullFileFrameCount NOTIFY fullFileChanged)
    Q_PROPERTY(qsizetype fullFileTransformSize READ fullFileTransformSize NOTIFY fullFileChanged)
    Q_PROPERTY(double fullFileFrequencyResolution READ fullFileFrequencyResolution NOTIFY fullFileChanged)
    Q_PROPERTY(double spectrogramMaximumFrequency READ spectrogramMaximumFrequency NOTIFY fullFileChanged)
    Q_PROPERTY(QUrl spectrogramImageUrl READ spectrogramImageUrl NOTIFY fullFileChanged)
    Q_PROPERTY(bool fullFilePlaying READ fullFilePlaying NOTIFY playbackChanged)
    Q_PROPERTY(int fullFilePlaybackSelection READ fullFilePlaybackSelection NOTIFY playbackChanged)

    Q_PROPERTY(bool effectProcessing READ effectProcessing NOTIFY effectChanged)
    Q_PROPERTY(bool effectReady READ effectReady NOTIFY effectChanged)
    Q_PROPERTY(int effectMode READ effectMode NOTIFY effectChanged)
    Q_PROPERTY(QString effectModeName READ effectModeName NOTIFY effectChanged)
    Q_PROPERTY(double effectSemitones READ effectSemitones NOTIFY effectChanged)
    Q_PROPERTY(double effectPitchFactor READ effectPitchFactor NOTIFY effectChanged)
    Q_PROPERTY(double effectFrequencyShift READ effectFrequencyShift NOTIFY effectChanged)
    Q_PROPERTY(double effectProgress READ effectProgress NOTIFY effectChanged)
    Q_PROPERTY(double effectOutputDuration READ effectOutputDuration NOTIFY effectChanged)
    Q_PROPERTY(int effectOutputChannels READ effectOutputChannels NOTIFY effectChanged)
    Q_PROPERTY(int effectWindowSize READ effectWindowSize CONSTANT)
    Q_PROPERTY(int effectHopSize READ effectHopSize CONSTANT)
    Q_PROPERTY(QVariantList effectOriginalSpectrum READ effectOriginalSpectrum NOTIFY effectSpectrumChanged)
    Q_PROPERTY(QVariantList effectTransformedSpectrum READ effectTransformedSpectrum NOTIFY effectSpectrumChanged)
    Q_PROPERTY(double effectSpectrumMaximumFrequency READ effectSpectrumMaximumFrequency NOTIFY effectSpectrumChanged)
    Q_PROPERTY(double effectOriginalPeakFrequency READ effectOriginalPeakFrequency NOTIFY effectSpectrumChanged)
    Q_PROPERTY(double effectTransformedPeakFrequency READ effectTransformedPeakFrequency NOTIFY effectSpectrumChanged)
    Q_PROPERTY(bool effectSpectrumPreview READ effectSpectrumPreview NOTIFY effectSpectrumChanged)
    Q_PROPERTY(bool effectSpectrumAnalyzing READ effectSpectrumAnalyzing NOTIFY effectSpectrumChanged)
    Q_PROPERTY(bool effectPlaying READ effectPlaying NOTIFY playbackChanged)
    Q_PROPERTY(int effectPlaybackSelection READ effectPlaybackSelection NOTIFY playbackChanged)

    Q_PROPERTY(QStringList eqPresetNames READ eqPresetNames CONSTANT)
    Q_PROPERTY(int eqPreset READ eqPreset NOTIFY eqChanged)
    Q_PROPERTY(QVariantList eqBands READ eqBands NOTIFY eqChanged)
    Q_PROPERTY(int eqBandCount READ eqBandCount NOTIFY eqChanged)
    Q_PROPERTY(bool eqProcessing READ eqProcessing NOTIFY eqChanged)
    Q_PROPERTY(bool eqReady READ eqReady NOTIFY eqChanged)
    Q_PROPERTY(double eqProgress READ eqProgress NOTIFY eqChanged)
    Q_PROPERTY(double eqOutputDuration READ eqOutputDuration NOTIFY eqChanged)
    Q_PROPERTY(int eqOutputChannels READ eqOutputChannels NOTIFY eqChanged)
    Q_PROPERTY(QVariantList eqOriginalSpectrum READ eqOriginalSpectrum NOTIFY eqSpectrumChanged)
    Q_PROPERTY(QVariantList eqTransformedSpectrum READ eqTransformedSpectrum NOTIFY eqSpectrumChanged)
    Q_PROPERTY(double eqSpectrumMaximumFrequency READ eqSpectrumMaximumFrequency NOTIFY eqSpectrumChanged)
    Q_PROPERTY(bool eqSpectrumPreview READ eqSpectrumPreview NOTIFY eqSpectrumChanged)
    Q_PROPERTY(bool eqSpectrumAnalyzing READ eqSpectrumAnalyzing NOTIFY eqSpectrumChanged)
    Q_PROPERTY(bool eqPlaying READ eqPlaying NOTIFY playbackChanged)
    Q_PROPERTY(int eqPlaybackSelection READ eqPlaybackSelection NOTIFY playbackChanged)

public:
    explicit SpectraController(QObject *parent = nullptr);
    ~SpectraController() override;

    int currentPage() const;
    bool audioReady() const;
    QString statusText() const;
    double textScale() const;
    int performanceMode() const;
    QString performanceModeName() const;
    int processingWorkerCount() const;
    int hardwareThreadCount() const;
    QString processingBackendName() const;
    QString applicationVersion() const;
    QString buildCommit() const;
    QString systemDescription() const;

    double synthFrequency() const;
    double synthDuration() const;
    double synthGain() const;
    int synthPreset() const;
    QString synthPresetName() const;
    QVariantList harmonicAmplitudes() const;
    QVariantList synthWaveform() const;
    QVariantList synthWaveformMinimums() const;
    QVariantList synthWaveformMaximums() const;
    QVariantList synthSpectrum() const;
    QVariantList synthPeaks() const;
    QString synthPitch() const;
    QString synthNote() const;
    int synthPeakCount() const;
    bool synthPlaying() const;

    bool sourceLoaded() const;
    QString sourceFileName() const;
    int sourceChannels() const;
    int sourceSampleRate() const;
    double sourceDuration() const;
    bool sourcePlaying() const;
    double playbackPosition() const;
    double playbackDuration() const;

    bool analysisReady() const;
    double regionStart() const;
    double regionDuration() const;
    QVariantList sourceWaveformMinimums() const;
    QVariantList sourceWaveformMaximums() const;
    QVariantList analysisSpectrum() const;
    QVariantList analysisPeaks() const;
    int analysisPeakReadoutMode() const;
    QString analysisPitch() const;
    QString analysisNote() const;
    double analysisConfidence() const;
    int analysisPeakCount() const;
    bool regionPlaying() const;
    bool regionActive() const;

    bool harmonicReady() const;
    int harmonicCount() const;
    int detectedHarmonicCount() const;
    QVariantList extractedHarmonics() const;
    bool fourierFrameReady() const;
    int fourierMaximumComponents() const;
    int fourierSelectedComponents() const;
    int fourierFftSize() const;
    double fourierFrameDuration() const;
    QVariantList fourierComponents() const;
    bool harmonicPlaying() const;
    bool framePlaying() const;
    bool fourierPlaying() const;
    QString labPlaybackTitle() const;

    bool fullFileProcessing() const;
    bool fullFileReady() const;
    int fullFileMode() const;
    int fullFileChannelMode() const;
    int fullFileSelectionMode() const;
    double fullFileEnergyTarget() const;
    qulonglong fullFileEstimatedMonoBytes() const;
    qulonglong fullFileEstimatedSourceBytes() const;
    qulonglong fullFileMemoryLimitBytes() const;
    int fullFileCacheEntries() const;
    int fullFileOutputChannels() const;
    double fullFileProgress() const;
    int fullFileSelectedComponents() const;
    int fullFileMaximumComponents() const;
    double fullFileRetainedEnergy() const;
    int fullFileWindowSize() const;
    int fullFileHopSize() const;
    qsizetype fullFileFrameCount() const;
    qsizetype fullFileTransformSize() const;
    double fullFileFrequencyResolution() const;
    double spectrogramMaximumFrequency() const;
    QUrl spectrogramImageUrl() const;
    bool fullFilePlaying() const;
    int fullFilePlaybackSelection() const;

    bool effectProcessing() const;
    bool effectReady() const;
    int effectMode() const;
    QString effectModeName() const;
    double effectSemitones() const;
    double effectPitchFactor() const;
    double effectFrequencyShift() const;
    double effectProgress() const;
    double effectOutputDuration() const;
    int effectOutputChannels() const;
    int effectWindowSize() const;
    int effectHopSize() const;
    QVariantList effectOriginalSpectrum() const;
    QVariantList effectTransformedSpectrum() const;
    double effectSpectrumMaximumFrequency() const;
    double effectOriginalPeakFrequency() const;
    double effectTransformedPeakFrequency() const;
    bool effectSpectrumPreview() const;
    bool effectSpectrumAnalyzing() const;
    bool effectPlaying() const;
    int effectPlaybackSelection() const;

    QStringList eqPresetNames() const;
    int eqPreset() const;
    QVariantList eqBands() const;
    int eqBandCount() const;
    bool eqProcessing() const;
    bool eqReady() const;
    double eqProgress() const;
    double eqOutputDuration() const;
    int eqOutputChannels() const;
    QVariantList eqOriginalSpectrum() const;
    QVariantList eqTransformedSpectrum() const;
    double eqSpectrumMaximumFrequency() const;
    bool eqSpectrumPreview() const;
    bool eqSpectrumAnalyzing() const;
    bool eqPlaying() const;
    int eqPlaybackSelection() const;

    Q_INVOKABLE void setCurrentPage(int page);
    Q_INVOKABLE void setTextScale(double scale);
    Q_INVOKABLE void setPerformanceMode(int mode);
    Q_INVOKABLE QString diagnosticsText() const;
    Q_INVOKABLE void copyDiagnostics();
    Q_INVOKABLE void setSynthFrequency(double frequency);
    Q_INVOKABLE void setSynthDuration(double duration);
    Q_INVOKABLE void setSynthGain(double gain);
    Q_INVOKABLE void setHarmonicAmplitude(int index, double amplitude);
    Q_INVOKABLE void applySynthPreset(int preset);
    Q_INVOKABLE void playSynth();
    Q_INVOKABLE void toggleSynthPlayback();
    Q_INVOKABLE void seekSynthPlayback(double position);
    Q_INVOKABLE void toggleSourcePlayback();
    Q_INVOKABLE void seekSourcePlayback(double position);
    Q_INVOKABLE void setRegionStart(double start);
    Q_INVOKABLE void setRegionDuration(double duration);
    Q_INVOKABLE void setAnalysisPeakReadoutMode(int mode);
    Q_INVOKABLE void analyzeRegion();
    Q_INVOKABLE void playRegion();
    Q_INVOKABLE void toggleRegionPlayback();
    Q_INVOKABLE void seekRegionPlayback(double position);
    Q_INVOKABLE void playHarmonicModel();
    Q_INVOKABLE void playOriginalFrame();
    Q_INVOKABLE void playFourierFrame();
    Q_INVOKABLE void toggleLabPlayback();
    Q_INVOKABLE void seekLabPlayback(double position);
    Q_INVOKABLE void rebuildFourierFrame(int componentCount);
    Q_INVOKABLE bool exportHarmonicFile(const QUrl &url);
    Q_INVOKABLE bool exportFourierFile(const QUrl &url);
    Q_INVOKABLE void setFullFileMode(int mode);
    Q_INVOKABLE void setFullFileChannelMode(int mode);
    Q_INVOKABLE void setFullFileSelectionMode(int mode);
    Q_INVOKABLE void setFullFileEnergyTarget(double target);
    Q_INVOKABLE void setFullFileMemoryLimitBytes(qulonglong bytes);
    Q_INVOKABLE void setFullFileComponentCount(int componentCount);
    Q_INVOKABLE void buildFullFileModel();
    Q_INVOKABLE void playFullFileOriginal();
    Q_INVOKABLE void playFullFileReconstruction();
    Q_INVOKABLE void toggleFullFilePlayback();
    Q_INVOKABLE void seekFullFilePlayback(double position);
    Q_INVOKABLE bool exportFullFileFile(const QUrl &url);
    Q_INVOKABLE bool exportSpectrogramPdf(const QUrl &url);
    Q_INVOKABLE void setEffectMode(int mode);
    Q_INVOKABLE void setEffectSemitones(double semitones);
    Q_INVOKABLE void setEffectFrequencyShift(double shiftHz);
    Q_INVOKABLE void buildEffect();
    Q_INVOKABLE void playEffectOriginal();
    Q_INVOKABLE void playEffectProcessed();
    Q_INVOKABLE void toggleEffectPlayback();
    Q_INVOKABLE void seekEffectPlayback(double position);
    Q_INVOKABLE bool exportEffectFile(const QUrl &url);
    Q_INVOKABLE int addEqBand(
        double lowHz,
        double highHz,
        double gainDb = 0.0);
    Q_INVOKABLE void updateEqBand(
        int index,
        double lowHz,
        double highHz,
        double gainDb);
    Q_INVOKABLE void setEqBandEnabled(
        int index,
        bool enabled);
    Q_INVOKABLE void removeEqBand(int index);
    Q_INVOKABLE void clearEqBands();
    Q_INVOKABLE void applyEqPreset(int preset);
    Q_INVOKABLE void buildEq();
    Q_INVOKABLE void playEqOriginal();
    Q_INVOKABLE void playEqProcessed();
    Q_INVOKABLE void toggleEqPlayback();
    Q_INVOKABLE void seekEqPlayback(double position);
    Q_INVOKABLE bool exportEqFile(const QUrl &url);
    Q_INVOKABLE void stopPlayback();
    Q_INVOKABLE void importAudioFile(const QUrl &url);
    Q_INVOKABLE bool exportSynthFile(const QUrl &url);

signals:
    void currentPageChanged();
    void audioReadyChanged();
    void statusTextChanged();
    void textScaleChanged();
    void performanceChanged();
    void synthChanged();
    void synthVisualizationChanged();
    void sourceChanged();
    void analysisChanged();
    void reconstructionChanged();
    void fullFileChanged();
    void effectChanged();
    void effectSpectrumChanged();
    void eqChanged();
    void eqSpectrumChanged();
    void playbackChanged();

private:
    enum PlaybackTarget {
        NoPlayback = 0,
        SynthPlayback,
        SourcePlayback,
        RegionPlayback,
        HarmonicPlayback,
        OriginalFramePlayback,
        FourierFramePlayback,
        FullFilePlayback,
        EffectPlayback,
        EqPlayback,
    };

    enum FullFileWork {
        FullFileIdle = 0,
        SpectrogramBuild,
        GlobalAnalysis,
        GlobalReconstruction,
        StftReconstruction,
    };

    void setStatusText(const QString &status);
    void scheduleSynthRebuild();
    void rebuildSynth();
    void rebuildSynthVisualization();
    void rebuildSourceWaveform();
    void rebuildAnalysisPeaks();
    void rebuildAnalysisVisualization();
    void rebuildRegionModels(const SampleBuffer &region);
    bool rebuildFourierFrameBuffer(int componentCount);
    SampleBuffer selectedRegion() const;
    void resetAnalysis();
    void resetFullFile();
    void resetEffect();
    void cacheFullFileOutput();
    bool restoreFullFileOutput();
    void clearFullFileOutput();
    void startSpectrogramBuild();
    bool startGlobalAnalysis();
    bool startGlobalReconstruction();
    bool startStftReconstruction();
    static void processGlobalFullFileInBackground(
        BackgroundTaskControl *control,
        void *context);
    static void processStftFullFileInBackground(
        BackgroundTaskControl *control,
        void *context);
    void processFullFileWork();
    void finishSpectrogramBuild();
    void finishFullFileReconstruction(
        SampleBuffer left,
        SampleBuffer right,
        unsigned int channelCount,
        double retainedEnergy);
    void clearEffectOutput();
    static void processEffectInBackground(
        BackgroundTaskControl *control,
        void *context);
    static void processEffectSourceSpectrumInBackground(
        BackgroundTaskControl *control,
        void *context);
    void processEffectWork();
    void processEffectSourceSpectrumWork();
    void rebuildEffectSourceSpectrum();
    void rebuildEffectSpectrumPreview();
    void rebuildEffectOutputSpectrum();
    void resetEq();
    void clearEqOutput();
    void rebuildEqSpectrumPreview();
    void rebuildEqOutputSpectrum();
    static void processEqInBackground(
        BackgroundTaskControl *control,
        void *context);
    void processEqWork();
    int requiredGlobalAnalysisComponents() const;
    bool hasReusableGlobalAnalysis() const;
    unsigned int fullFileReconstructionChannels() const;
    void seekClip(
        AudioClip *clip,
        PlaybackTarget target,
        double position);
    void refreshPlaybackState();
    void haltAllAudio();
    AudioClip *activeClip();
    const AudioClip *activeClip() const;

    int currentPage_ = 0;
    bool audioReady_ = false;
    QString statusText_;
    double textScale_ = 1.00;
    int performanceMode_ = 0;

    double synthFrequency_ = 440.0;
    double synthDuration_ = 1.2;
    double synthGain_ = 0.85;
    int synthPreset_ = 0;
    Harmonic harmonics_[16] = {};
    SampleBuffer synthBuffer_ = {};
    Spectrum synthSpectrumBuffer_ = {};
    Peak synthPeaksBuffer_[12] = {};
    PitchEstimate synthPitchEstimate_ = {};
    int synthPeakCount_ = 0;
    QVariantList synthWaveform_;
    QVariantList synthWaveformMinimums_;
    QVariantList synthWaveformMaximums_;
    QVariantList synthSpectrum_;
    QVariantList synthPeaks_;
    AudioClip synthClip_ = {};

    ImportedAudio importedAudio_ = {};
    AudioClip sourceClip_ = {};
    AudioClip regionClip_ = {};
    QString sourceFileName_;
    QVariantList sourceWaveformMinimums_;
    QVariantList sourceWaveformMaximums_;

    bool analysisReady_ = false;
    double regionStart_ = 0.0;
    double regionDuration_ = 1.0;
    Spectrum analysisSpectrumBuffer_ = {};
    Peak analysisPeaksBuffer_[64] = {};
    int analysisPeakCount_ = 0;
    int analysisPeakReadoutMode_ = 0;
    PitchEstimate analysisPitchEstimate_ = {};
    QVariantList analysisSpectrum_;
    QVariantList analysisPeaks_;

    ExtractedHarmonic extractedHarmonicsBuffer_[24] = {};
    int harmonicCount_ = 0;
    int detectedHarmonicCount_ = 0;
    QVariantList extractedHarmonics_;
    SampleBuffer harmonicBuffer_ = {};
    AudioClip harmonicClip_ = {};
    FourierFrameAnalysis fourierAnalysis_ = {};
    SampleBuffer fourierBuffer_ = {};
    AudioClip originalFrameClip_ = {};
    AudioClip fourierClip_ = {};
    int fourierSelectedComponents_ = 0;
    PlaybackTarget lastLabPlaybackTarget_ = RegionPlayback;

    StftReconstructionJob spectrogramJob_ = {};
    BackgroundTask spectrogramTask_ = {};
    BackgroundTask fullFileTask_ = {};
    GlobalFourierJob globalFourierJob_ = {};
    GlobalFourierJob globalFourierJobRight_ = {};
    StftReconstructionJob fullFileStftJob_ = {};
    StftReconstructionJob fullFileStftJobRight_ = {};
    ReconstructionCache reconstructionCache_ = {};
    InterleavedBuffer fullFileBuffer_ = {};
    AudioClip fullFileClip_ = {};
    FullFileWork fullFileWork_ = FullFileIdle;
    unsigned int fullFileWorkerChannels_ = 1U;
    int fullFileMode_ = 0;
    int fullFileChannelMode_ = 0;
    int fullFileSelectionMode_ = 0;
    double fullFileEnergyTarget_ = 0.90;
    qulonglong fullFileMemoryLimitBytes_ =
        768ULL * 1024ULL * 1024ULL;
    int fullFileSelectedComponents_ = 5;
    int globalSelectedComponents_ = 5;
    int stftSelectedComponents_ = 100;
    double fullFileProgress_ = 0.0;
    double fullFileRetainedEnergy_ = 0.0;
    qsizetype fullFileFrameCount_ = 0;
    QUrl spectrogramImageUrl_;

    BackgroundTask effectTask_ = {};
    BackgroundTask effectSpectrumTask_ = {};
    InterleavedBuffer effectWorkerBuffer_ = {};
    InterleavedBuffer effectBuffer_ = {};
    Spectrum effectSourceSpectrumWorker_ = {};
    Spectrum effectSourceSpectrumBuffer_ = {};
    Spectrum effectWorkerSpectrum_ = {};
    Spectrum effectOutputSpectrumBuffer_ = {};
    AudioClip effectClip_ = {};
    int effectMode_ = 0;
    double effectSemitones_ = 7.0;
    double effectFrequencyShift_ = 250.0;
    double effectProgress_ = 0.0;
    int effectWorkerMode_ = 0;
    double effectWorkerAmount_ = 0.0;
    bool effectWorkerSucceeded_ = false;
    bool effectSourceSpectrumWorkerSucceeded_ = false;
    QVariantList effectOriginalSpectrum_;
    QVariantList effectTransformedSpectrum_;
    double effectOriginalPeakFrequency_ = 0.0;
    double effectTransformedPeakFrequency_ = 0.0;
    bool effectSpectrumPreview_ = false;

    std::vector<SpectralEqBand> eqBands_;
    std::vector<SpectralEqBand> eqWorkerBands_;
    int eqPreset_ = -1;
    BackgroundTask eqTask_ = {};
    InterleavedBuffer eqWorkerBuffer_ = {};
    InterleavedBuffer eqBuffer_ = {};
    Spectrum eqWorkerSpectrum_ = {};
    Spectrum eqOutputSpectrumBuffer_ = {};
    AudioClip eqClip_ = {};
    QVariantList eqTransformedSpectrum_;
    double eqProgress_ = 0.0;
    bool eqWorkerSucceeded_ = false;
    bool eqSpectrumPreview_ = false;

    PlaybackTarget playbackTarget_ = NoPlayback;
    PlaybackTarget lastFullFilePlaybackTarget_ = SourcePlayback;
    PlaybackTarget lastEffectPlaybackTarget_ = SourcePlayback;
    PlaybackTarget lastEqPlaybackTarget_ = SourcePlayback;
    QTimer synthRebuildTimer_;
    QTimer playbackTimer_;
    QTimer fullFileTimer_;
    QTimer effectTimer_;
    QTimer effectSpectrumTimer_;
    QTimer eqTimer_;
};

#endif
