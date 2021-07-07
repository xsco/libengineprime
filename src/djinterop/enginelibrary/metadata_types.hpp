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

namespace djinterop::enginelibrary
{
enum class metadata_str_type
{
    title = 1,
    artist = 2,
    album = 3,
    genre = 4,
    comment = 5,
    publisher = 6,
    composer = 7,
    unknown_8 = 8,
    unknown_9 = 9,
    duration_mm_ss = 10,
    ever_played = 12,
    file_extension = 13,
    unknown_15 = 15,
    unknown_16 = 16,
    unknown_17 = 17,
};

enum class metadata_int_type
{
    last_played_ts = 1,
    last_modified_ts = 2,
    last_accessed_ts = 3,
    musical_key = 4,
    rating = 5,
    unknown_6 = 6,
    unknown_7 = 7,
    unknown_8 = 8,
    unknown_9 = 9,
    last_play_hash = 10,
    unknown_11 = 11,
    unknown_12 = 12,
};

}  // namespace djinterop::enginelibrary
