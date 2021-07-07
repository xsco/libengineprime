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

#define BOOST_TEST_MODULE semantic_version_test
#include <boost/test/data/test_case.hpp>
#include <boost/test/included/unit_test.hpp>

#include <ostream>
#include <vector>

#include <djinterop/semantic_version.hpp>

namespace utf = boost::unit_test;

namespace
{
struct version_pair
{
    djinterop::semantic_version x;
    djinterop::semantic_version y;
};

std::ostream& operator<<(std::ostream& os, const version_pair& vp)
{
    os << vp.x << "/" << vp.y;
    return os;
}

const std::vector<version_pair> equal_versions{
    version_pair{
        djinterop::semantic_version{1, 6, 0},
        djinterop::semantic_version{1, 6, 0}},
    version_pair{
        djinterop::semantic_version{1, 7, 1},
        djinterop::semantic_version{1, 7, 1}},
    version_pair{
        djinterop::semantic_version{1, 18, 0, "abc"},
        djinterop::semantic_version{1, 18, 0, "abc"}},
};

const std::vector<version_pair> unequal_versions{
    version_pair{
        djinterop::semantic_version{1, 6, 0},
        djinterop::semantic_version{1, 7, 1}},
    version_pair{
        djinterop::semantic_version{1, 18, 0, "abc"},
        djinterop::semantic_version{1, 18, 0, "xyz"}},
};

const std::vector<version_pair> ascending_versions{
    version_pair{
        djinterop::semantic_version{1, 6, 0},
        djinterop::semantic_version{1, 7, 1}},
};

const std::vector<version_pair> descending_versions{
    version_pair{
        djinterop::semantic_version{1, 7, 1},
        djinterop::semantic_version{1, 6, 0}},
};

}  // namespace

BOOST_TEST_DECORATOR(
    * utf::description("operator== with various equal versions"))
BOOST_DATA_TEST_CASE(operator_eq__equal, equal_versions, operands)
{
    // Arrange/Act/Assert
    BOOST_CHECK(operands.x == operands.y);
    BOOST_CHECK(operands.y == operands.x);
}

BOOST_TEST_DECORATOR(
    * utf::description("operator== with various unequal versions"))
BOOST_DATA_TEST_CASE(operator_eq__unequal, unequal_versions, operands)
{
    // Arrange/Act/Assert
    BOOST_CHECK(!(operands.x == operands.y));
    BOOST_CHECK(!(operands.y == operands.x));
}

BOOST_TEST_DECORATOR(
    * utf::description("operator!= with various unequal versions"))
BOOST_DATA_TEST_CASE(operator_neq__unequal, unequal_versions, operands)
{
    // Arrange/Act/Assert
    BOOST_CHECK(operands.x != operands.y);
    BOOST_CHECK(operands.y != operands.x);
}

BOOST_TEST_DECORATOR(
    * utf::description("operator!= with various equal versions"))
BOOST_DATA_TEST_CASE(operator_neq__equal, equal_versions, operands)
{
    // Arrange/Act/Assert
    BOOST_CHECK(!(operands.x != operands.y));
    BOOST_CHECK(!(operands.y != operands.x));
}

BOOST_TEST_DECORATOR(* utf::description("operator< with various equal versions"))
BOOST_DATA_TEST_CASE(operator_lt__equal, equal_versions, operands)
{
    // Arrange/Act/Assert
    BOOST_CHECK(!(operands.x < operands.y));
    BOOST_CHECK(!(operands.y < operands.x));
}

BOOST_TEST_DECORATOR(
    * utf::description("operator< with various ascending versions"))
BOOST_DATA_TEST_CASE(operator_lt__ascending, ascending_versions, operands)
{
    // Arrange/Act/Assert
    BOOST_CHECK(operands.x < operands.y);
}

BOOST_TEST_DECORATOR(
    * utf::description("operator< with various descending versions"))
BOOST_DATA_TEST_CASE(operator_lt__descending, descending_versions, operands)
{
    // Arrange/Act/Assert
    BOOST_CHECK(!(operands.x < operands.y));
}

BOOST_TEST_DECORATOR(
    * utf::description("operator<= with various equal versions"))
BOOST_DATA_TEST_CASE(operator_le__equal, equal_versions, operands)
{
    // Arrange/Act/Assert
    BOOST_CHECK(operands.x <= operands.y);
    BOOST_CHECK(operands.y <= operands.x);
}

BOOST_TEST_DECORATOR(
    * utf::description("operator<= with various ascending versions"))
BOOST_DATA_TEST_CASE(operator_le__ascending, ascending_versions, operands)
{
    // Arrange/Act/Assert
    BOOST_CHECK(operands.x <= operands.y);
}

BOOST_TEST_DECORATOR(
    * utf::description("operator<= with various descending versions"))
BOOST_DATA_TEST_CASE(operator_le__descending, descending_versions, operands)
{
    // Arrange/Act/Assert
    BOOST_CHECK(!(operands.x <= operands.y));
}

BOOST_TEST_DECORATOR(
    * utf::description("operator> with various equal versions"))
BOOST_DATA_TEST_CASE(operator_gt__equal, equal_versions, operands)
{
    // Arrange/Act/Assert
    BOOST_CHECK(!(operands.x > operands.y));
    BOOST_CHECK(!(operands.y > operands.x));
}

BOOST_TEST_DECORATOR(
    * utf::description("operator> with various ascending versions"))
BOOST_DATA_TEST_CASE(operator_gt__ascending, ascending_versions, operands)
{
    // Arrange/Act/Assert
    BOOST_CHECK(!(operands.x > operands.y));
}

BOOST_TEST_DECORATOR(
    * utf::description("operator> with various descending versions"))
BOOST_DATA_TEST_CASE(operator_gt__descending, descending_versions, operands)
{
    // Arrange/Act/Assert
    BOOST_CHECK(operands.x > operands.y);
}

BOOST_TEST_DECORATOR(
    * utf::description("operator>= with various equal versions"))
BOOST_DATA_TEST_CASE(operator_ge__equal, equal_versions, operands)
{
    // Arrange/Act/Assert
    BOOST_CHECK(operands.x >= operands.y);
    BOOST_CHECK(operands.y >= operands.x);
}

BOOST_TEST_DECORATOR(
    * utf::description("operator>= with various ascending versions"))
BOOST_DATA_TEST_CASE(operator_ge__ascending, ascending_versions, operands)
{
    // Arrange/Act/Assert
    BOOST_CHECK(!(operands.x >= operands.y));
}

BOOST_TEST_DECORATOR(
    * utf::description("operator>= with various descending versions"))
BOOST_DATA_TEST_CASE(operator_ge__descending, descending_versions, operands)
{
    // Arrange/Act/Assert
    BOOST_CHECK(operands.x >= operands.y);
}
