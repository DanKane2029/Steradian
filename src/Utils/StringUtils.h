#pragma once

#include <algorithm>
#include <string>
#include <vector>

/**
 * \brief Splits a string on a delimiter.
 *
 * \param inputString The string to split.
 * \param delimiter The separator.
 * \param keepEmpty Whether to keep empty fields.
 *
 *        Both answers are needed, and getting it wrong is not obvious. Splitting a line
 *        into words wants runs of spaces treated as one separator, so empty fields are
 *        dropped by default. Splitting an .obj face index does not: "2//2" means vertex
 *        2 with normal 2 and *no* texture coordinate, and the empty field in the middle
 *        is what says so. Dropping it silently turns the normal index into a texture
 *        index, and the model loads with no vertex normals at all.
 *
 *        That is not hypothetical. It is what this function did to the Stanford bunny,
 *        the dragon and two other models -- every one of which ships per-vertex normals
 *        and every one of which was rendered flat shaded because of this line.
 */
inline auto splitString(const std::string &inputString, const std::string &delimiter,
                        bool keepEmpty = false) -> std::vector<std::string>
{
    std::vector<std::string> splitString;

    size_t last = 0;
    size_t next = 0;

    std::string token;

    while ((next = inputString.find(delimiter, last)) != std::string::npos)
    {
        splitString.push_back(inputString.substr(last, next - last));
        last = next + delimiter.length();
    }

    splitString.push_back(inputString.substr(last, inputString.length()));

    if (!keepEmpty)
    {
        splitString.erase(
            std::remove_if(splitString.begin(), splitString.end(), [](const std::string &s) { return s.empty(); }),
            splitString.end());
    }

    return splitString;
}