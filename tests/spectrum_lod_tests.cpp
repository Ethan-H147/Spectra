#include "qt/spectrum_lod.h"

#include <cmath>
#include <cstdio>
#include <vector>

#define ASSERT_TRUE(condition, message) \
    do { \
        if (!(condition)) { \
            std::fprintf(stderr, "FAIL: %s\n", message); \
            return 1; \
        } \
    } while (0)

static int test_cache_preserves_extrema(void) {
    std::vector<float> values(8192U, -100.0f);
    values[17U] = -12.0f;
    values[4095U] = -117.0f;
    values[7001U] = -3.0f;

    SpectrumLodCache cache;
    cache.build(values);
    ASSERT_TRUE(!cache.levels().empty(),
                "a full spectrum should produce LOD levels");

    const SpectrumLodLevel &coarsest = cache.levels().back();
    ASSERT_TRUE(coarsest.envelopes.size() == 1U,
                "the coarsest level should summarize the complete spectrum");
    ASSERT_TRUE(std::fabs(coarsest.envelopes[0].minimum + 117.0f) < 0.001f,
                "the coarsest envelope must preserve the lowest bin");
    ASSERT_TRUE(std::fabs(coarsest.envelopes[0].maximum + 3.0f) < 0.001f,
                "the coarsest envelope must preserve the highest narrow peak");
    ASSERT_TRUE(cache.estimatedBytes() <= 110U * 1024U,
                "the 8192-bin multiresolution cache should remain near 100 KB");
    return 0;
}

static int test_lod_returns_to_raw_bins_when_zoomed(void) {
    std::vector<float> values(8192U, -80.0f);
    SpectrumLodCache cache;
    cache.build(values);

    ASSERT_TRUE(cache.levelForView(8192U, 1000U) != nullptr,
                "a zoomed-out view should select an envelope level");
    ASSERT_TRUE(cache.levelForView(1200U, 1000U) == nullptr,
                "a zoomed-in view should return to exact raw bins");
    return 0;
}

int main() {
    if (test_cache_preserves_extrema() != 0) return 1;
    if (test_lod_returns_to_raw_bins_when_zoomed() != 0) return 1;
    std::puts("All spectrum LOD tests passed.");
    return 0;
}
