#pragma once

#include <algorithm>
#include <string>
#include <vector>

/**
 * splits a string into a vector of substrings delimited by the delimiter
 * parameter
 *
 * \param inputString - the string to be split
 * \param delimiter - the string that splits the input string
 */
inline auto splitString(const std::string &inputString, const std::string &delimiter) -> std::vector<std::string>
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

    splitString.erase(
        std::remove_if(splitString.begin(), splitString.end(), [](const std::string &s) { return s.empty(); }),
        splitString.end());

    return splitString;
}