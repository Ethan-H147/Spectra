// Synthesizer workspace: additive tone generation and visualization.

#include "qt/controller_support.h"

using namespace spectra_controller_detail;

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

QList<qreal> SpectraController::synthSpectrum() const {
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
