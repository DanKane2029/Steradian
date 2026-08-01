#pragma once

#include <cstddef>
#include <vector>

/**
 * \brief Edge-avoiding a-trous wavelet denoising.
 *
 * A path traced image is noisy because each pixel is an average of a limited number of
 * random light paths. The noise is high frequency and, crucially, it lives only in the
 * lighting: which surface is visible at a pixel, and which way it faces, are known almost
 * exactly from a single sample.
 *
 * That asymmetry is what makes this work. The lighting is blurred heavily, but the blur
 * is prevented from crossing places where the guide channels say a real edge exists. Two
 * neighbouring pixels on the same flat wall are averaged together freely; two either side
 * of a silhouette, or spanning the boundary between a red wall and a white floor, are
 * not. Noise disappears while edges survive.
 *
 * The filter is applied several times with an increasing gap between the taps it reads,
 * which reaches a wide radius for the cost of a small kernel. That "a-trous", or
 * with-holes, construction is the technique's namesake.
 *
 * Follows Dammertz et al. 2010, "Edge-Avoiding A-Trous Wavelet Transform for Fast Global
 * Illumination Filtering".
 *
 * This is a post-process and it approximates. Unlike everything else in the renderer, it
 * cannot be checked against a ground truth by the furnace test, because changing the
 * image is the entire point. It trades away fine lighting detail such as small caustics
 * and tight contact shadows, and works best applied to an image that is already
 * reasonably converged rather than as a substitute for sampling.
 */
namespace Denoiser
{

struct Settings
{
    /** number of filter passes; each doubles the reach of the previous one */
    int iterations = 5;

    /**
     * how much difference in linear radiance is tolerated before the blur is cut off
     *
     * This is the parameter that decides between a noisy image and a flat one. Too small
     * and noise survives; too large and real shading is smoothed away.
     */
    float colorSigma = 0.10f;

    /** how sharply differing normals cut off the blur; larger keeps geometry crisper */
    float normalSigma = 32.0f;

    /** how much difference in surface colour is tolerated before the blur is cut off */
    float albedoSigma = 0.12f;

    /**
     * how far above its neighbours a pixel may sit before it is treated as an outlier
     *
     * Isolated samples that are far brighter than everything around them survive the
     * filter untouched, because a bright dot is indistinguishable from a genuine edge:
     * the colour term protects both equally. Pulling those pixels back towards their
     * surroundings first is what lets the filter reach them. Zero disables it.
     */
    float outlierThreshold = 2.0f;
};

/**
 * \brief Denoises a linear colour buffer using albedo and normal as guides.
 *
 * All buffers are tightly packed RGB triples of width * height pixels.
 *
 * \param width Image width in pixels.
 * \param height Image height in pixels.
 * \param color Noisy linear radiance.
 * \param albedo Base colour of the surface visible at each pixel.
 * \param normal Surface normal visible at each pixel.
 * \param settings Filter tuning.
 * \returns The filtered image, in the same layout.
 */
auto denoise(int width, int height, const float *color, const float *albedo, const float *normal,
             const Settings &settings = {}) -> std::vector<float>;

} // namespace Denoiser
