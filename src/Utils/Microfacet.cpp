#include "Microfacet.h"

#include "Utils/Random.h"

#include <vector>

namespace Microfacet
{

namespace
{

constexpr int albedoSamples = 2048;

/**
 * \brief Measures how much light the single-scattering lobe actually reflects.
 *
 * The microfacet model here follows light striking the surface, bouncing once, and
 * leaving. Light that would have bounced between facets before escaping is instead
 * dropped, and the rougher the surface the more of it there is: a fully reflective
 * conductor returns almost everything at low roughness and around a third of it at the
 * top of the range. That missing energy is what makes rough metal render too dark.
 *
 * Recovering it requires knowing how much was lost, which is what this table holds. It is
 * measured rather than taken from a published fit, and measured by sampling this file's
 * own scattering routine, so it describes the lobe this renderer actually has rather than
 * an idealised one.
 */
auto buildAlbedoTable() -> std::vector<float>
{
    std::vector<float> table(albedoResolution * albedoResolution);

    Rng rng(0x5eed'1234'5678'9abcULL, 1);

    for (int r = 0; r < albedoResolution; r++)
    {
        // Sampled in roughness rather than in alpha, so the resolution is spread the way
        // the parameter is actually used.
        const float roughness = (static_cast<float>(r) + 0.5f) / albedoResolution;
        const float alpha = roughnessToAlpha(roughness);

        for (int c = 0; c < albedoResolution; c++)
        {
            const float cosThetaO = std::max((static_cast<float>(c) + 0.5f) / albedoResolution, 1e-3f);
            const float sinThetaO = std::sqrt(std::max(0.0f, 1.0f - (cosThetaO * cosThetaO)));

            const Vec3 wo(sinThetaO, 0.0f, cosThetaO);

            float sum = 0.0f;
            for (int s = 0; s < albedoSamples; s++)
            {
                const Vec3 h = sampleVisibleNormal(wo, alpha, rng.nextFloat(), rng.nextFloat());
                const Vec3 wi = Sampling::reflect(-wo, h);

                if (wi.z > 0.0f)
                {
                    // With Fresnel taken as one, the weight carried by a sampled direction
                    // is the whole of what this lobe reflects.
                    sum += visibleNormalWeight(wo, wi, alpha);
                }
            }

            table[(r * albedoResolution) + c] = sum / static_cast<float>(albedoSamples);
        }
    }

    return table;
}

} // namespace

auto hostAlbedoTable() -> const float *
{
    // Built once, on first use. Costs a few million samples of the routines above, which
    // is milliseconds, and saves repeating that work for every shading point.
    static const std::vector<float> table = buildAlbedoTable();

    return table.data();
}

} // namespace Microfacet
