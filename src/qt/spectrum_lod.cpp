#include "qt/spectrum_lod.h"

#include <algorithm>

void SpectrumLodCache::build(const std::vector<float> &values) {
    clear();
    values_ = values;
    if (values_.size() < 2U) {
        return;
    }

    SpectrumLodLevel level;
    level.blockSize = 2U;
    level.envelopes.reserve((values_.size() + 1U) / 2U);
    for (std::size_t start = 0U; start < values_.size(); start += 2U) {
        const std::size_t end =
            std::min(start + 2U, values_.size());
        float minimum = values_[start];
        float maximum = values_[start];
        for (std::size_t index = start + 1U; index < end; ++index) {
            minimum = std::min(minimum, values_[index]);
            maximum = std::max(maximum, values_[index]);
        }
        level.envelopes.push_back({minimum, maximum});
    }
    levels_.push_back(std::move(level));

    while (levels_.back().envelopes.size() > 1U) {
        const SpectrumLodLevel &previous = levels_.back();
        SpectrumLodLevel next;
        next.blockSize = previous.blockSize * 2U;
        next.envelopes.reserve(
            (previous.envelopes.size() + 1U) / 2U);
        for (std::size_t start = 0U;
             start < previous.envelopes.size();
             start += 2U) {
            SpectrumEnvelope combined = previous.envelopes[start];
            if (start + 1U < previous.envelopes.size()) {
                combined.minimum = std::min(
                    combined.minimum,
                    previous.envelopes[start + 1U].minimum);
                combined.maximum = std::max(
                    combined.maximum,
                    previous.envelopes[start + 1U].maximum);
            }
            next.envelopes.push_back(combined);
        }
        levels_.push_back(std::move(next));
    }
}

void SpectrumLodCache::clear() {
    values_.clear();
    levels_.clear();
}

const std::vector<float> &SpectrumLodCache::values() const {
    return values_;
}

const std::vector<SpectrumLodLevel> &SpectrumLodCache::levels() const {
    return levels_;
}

const SpectrumLodLevel *SpectrumLodCache::levelForView(
    std::size_t visibleSampleCount,
    std::size_t pixelWidth) const {
    if (visibleSampleCount < 2U || pixelWidth == 0U ||
        visibleSampleCount <= pixelWidth * 2U) {
        return nullptr;
    }

    for (const SpectrumLodLevel &level : levels_) {
        const std::size_t visibleEnvelopeCount =
            (visibleSampleCount + level.blockSize - 1U) /
            level.blockSize;
        if (visibleEnvelopeCount <= pixelWidth) {
            return &level;
        }
    }
    return levels_.empty() ? nullptr : &levels_.back();
}

std::size_t SpectrumLodCache::estimatedBytes() const {
    std::size_t bytes =
        values_.capacity() * sizeof(float);
    for (const SpectrumLodLevel &level : levels_) {
        bytes += level.envelopes.capacity() *
            sizeof(SpectrumEnvelope);
    }
    return bytes;
}
