#ifndef SPECTRA_QT_SPECTRUM_LOD_H
#define SPECTRA_QT_SPECTRUM_LOD_H

#include <cstddef>
#include <vector>

struct SpectrumEnvelope {
    float minimum = 0.0f;
    float maximum = 0.0f;
};

struct SpectrumLodLevel {
    std::size_t blockSize = 0U;
    std::vector<SpectrumEnvelope> envelopes;
};

class SpectrumLodCache {
public:
    void build(const std::vector<float> &values);
    void clear();

    const std::vector<float> &values() const;
    const std::vector<SpectrumLodLevel> &levels() const;
    const SpectrumLodLevel *levelForView(
        std::size_t visibleSampleCount,
        std::size_t pixelWidth) const;
    std::size_t estimatedBytes() const;

private:
    std::vector<float> values_;
    std::vector<SpectrumLodLevel> levels_;
};

#endif
