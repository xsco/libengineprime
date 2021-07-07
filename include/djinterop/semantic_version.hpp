/*
    This file is part of libdjinterop.

    libdjinterop is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    libdjinterop is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with libdjinterop.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#ifndef DJINTEROP_SEMANTIC_VERSION_HPP
#define DJINTEROP_SEMANTIC_VERSION_HPP

#if __cplusplus < 201703L
#error This library needs at least a C++17 compliant compiler
#endif

#include <cstring>
#include <ostream>

namespace djinterop
{
struct semantic_version
{
    int maj;
    int min;
    int pat;
    const char* metadata = nullptr;
};

inline std::ostream& operator<<(
    std::ostream& os, const semantic_version& version)
{
    if (version.metadata == nullptr)
        return os << version.maj << "." << version.min << "." << version.pat;
    else
        return os << version.maj << "." << version.min << "." << version.pat
                  << "+" << version.metadata;
}

inline bool operator==(const semantic_version& a, const semantic_version& b)
{
    bool metadata_equal;
    if (a.metadata == nullptr && a.metadata == nullptr)
        metadata_equal = true;
    else if (a.metadata == nullptr || b.metadata == nullptr)
        metadata_equal = false;
    else
        metadata_equal = !strcmp(a.metadata, b.metadata);

    return a.maj == b.maj && a.min == b.min && a.pat == b.pat && metadata_equal;
}

inline bool operator!=(const semantic_version& a, const semantic_version& b)
{
    return !(a == b);
}

inline bool operator>=(const semantic_version& a, const semantic_version& b)
{
    if (a.maj != b.maj)
        return a.maj > b.maj;
    if (a.min != b.min)
        return a.min > b.min;
    return a.pat >= b.pat;
}

inline bool operator>(const semantic_version& a, const semantic_version& b)
{
    if (a.maj != b.maj)
        return a.maj > b.maj;
    if (a.min != b.min)
        return a.min > b.min;
    return a.pat > b.pat;
}

inline bool operator<=(const semantic_version& a, const semantic_version& b)
{
    if (a.maj != b.maj)
        return a.maj < b.maj;
    if (a.min != b.min)
        return a.min < b.min;
    return a.pat <= b.pat;
}

inline bool operator<(const semantic_version& a, const semantic_version& b)
{
    if (a.maj != b.maj)
        return a.maj < b.maj;
    if (a.min != b.min)
        return a.min < b.min;
    return a.pat < b.pat;
}

}  // namespace djinterop

#endif  // DJINTEROP_SEMANTIC_VERSION_HPP
