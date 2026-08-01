#pragma once

#include <array>
#include <string>

/**
 * \brief Image file output for the renderer.
 *
 * The renderer accumulates linear radiance as floats. Display and file output are the
 * only places that should apply an sRGB transfer function, so the encode lives here
 * rather than in the render loop.
 */
namespace ImageIO
{

/**
 * \brief Encodes a single linear channel value to sRGB.
 *
 * Clamps to [0, 1] and applies the standard sRGB transfer function. Values that are
 * not finite (NaN from a degenerate shading result, for example) are treated as 0 so a
 * single bad sample cannot produce an undefined byte in the output file.
 *
 * \param linear The linear channel value.
 * \returns The sRGB-encoded value in [0, 1].
 */
auto linearToSrgb(float linear) -> float;

/**
 * \brief Compresses unbounded linear radiance into displayable range.
 *
 * \param linearRgb Pointer to three linear channel values.
 * \returns Tone mapped values in [0, 1].
 */
auto toneMap(const float *linearRgb) -> std::array<float, 3>;

/**
 * \brief Writes a linear float RGB buffer to an 8-bit sRGB PNG.
 *
 * The buffer is expected to be tightly packed RGB triples with row 0 at the *bottom*,
 * matching the layout the renderer and the OpenGL viewer use. The rows are flipped on
 * write so the resulting PNG is the right way up.
 *
 * \param path The output file path.
 * \param width The image width in pixels.
 * \param height The image height in pixels.
 * \param linearRgb The linear RGB float buffer, width * height * 3 elements.
 * \returns True if the file was written successfully.
 */
auto writePNG(const std::string &path, int width, int height, const float *linearRgb) -> bool;

} // namespace ImageIO
