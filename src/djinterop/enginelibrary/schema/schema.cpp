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

#include <djinterop/djinterop.hpp>
#include <djinterop/enginelibrary/schema/schema.hpp>
#include <djinterop/enginelibrary/schema/schema_1_11_1.hpp>
#include <djinterop/enginelibrary/schema/schema_1_13_0.hpp>
#include <djinterop/enginelibrary/schema/schema_1_13_1.hpp>
#include <djinterop/enginelibrary/schema/schema_1_13_2.hpp>
#include <djinterop/enginelibrary/schema/schema_1_15_0.hpp>
#include <djinterop/enginelibrary/schema/schema_1_17_0.hpp>
#include <djinterop/enginelibrary/schema/schema_1_18_0_ep.hpp>
#include <djinterop/enginelibrary/schema/schema_1_18_0_fw.hpp>
#include <djinterop/enginelibrary/schema/schema_1_6_0.hpp>
#include <djinterop/enginelibrary/schema/schema_1_7_1.hpp>
#include <djinterop/enginelibrary/schema/schema_1_9_1.hpp>

namespace djinterop::enginelibrary::schema
{
std::unique_ptr<schema_creator_validator> make_schema_creator_validator(
    const semantic_version& version)
{
    if (version == version_1_6_0)
        return std::make_unique<schema_1_6_0>();
    else if (version == version_1_7_1)
        return std::make_unique<schema_1_7_1>();
    else if (version == version_1_9_1)
        return std::make_unique<schema_1_9_1>();
    else if (version == version_1_11_1)
        return std::make_unique<schema_1_11_1>();
    else if (version == version_1_13_0)
        return std::make_unique<schema_1_13_0>();
    else if (version == version_1_13_1)
        return std::make_unique<schema_1_13_1>();
    else if (version == version_1_13_2)
        return std::make_unique<schema_1_13_2>();
    else if (version == version_1_15_0)
        return std::make_unique<schema_1_15_0>();
    else if (version == version_1_17_0)
        return std::make_unique<schema_1_17_0>();
    else if (version == version_1_18_0_ep)
        return std::make_unique<schema_1_18_0_ep>();
    else if (version == version_1_18_0_fw)
        return std::make_unique<schema_1_18_0_fw>();
    else
        throw unsupported_database_version{version};
}

bool is_supported(const semantic_version& version)
{
    return version == version_1_6_0 || version == version_1_7_1 ||
           version == version_1_9_1 || version == version_1_11_1 ||
           version == version_1_13_0 || version == version_1_13_1 ||
           version == version_1_13_2 || version == version_1_15_0 ||
           version == version_1_17_0 || version == version_1_18_0_ep ||
           version == version_1_18_0_fw;
}

}  // namespace djinterop::enginelibrary::schema
