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
#ifndef DJINTEROP_CONFIG_HPP
#define DJINTEROP_CONFIG_HPP

#if __cplusplus < 201703L
#error This library needs at least a C++17 compliant compiler
#endif

#if defined _WIN32 || defined __CYGWIN__
#define DJINTEROP_SYMBOL_IMPORT __declspec(dllimport)
#define DJINTEROP_SYMBOL_EXPORT __declspec(dllexport)
#define DJINTEROP_SYMBOL_LOCAL
#else
#if __GNUC__ >= 4
#define DJINTEROP_SYMBOL_IMPORT __attribute__((visibility("default")))
#define DJINTEROP_SYMBOL_EXPORT __attribute__((visibility("default")))
#define DJINTEROP_SYMBOL_LOCAL __attribute__((visibility("hidden")))
#else
#define DJINTEROP_SYMBOL_IMPORT
#define DJINTEROP_SYMBOL_EXPORT
#define DJINTEROP_SYMBOL_LOCAL
#endif
#endif

#ifdef DJINTEROP_SOURCE  // Defined if building the library (cf. using it)
#define DJINTEROP_PUBLIC DJINTEROP_SYMBOL_EXPORT
#else
#define DJINTEROP_PUBLIC DJINTEROP_SYMBOL_IMPORT
#endif  // DJINTEROP_SOURCE
#define DJINTEROP_LOCAL DJINTEROP_SYMBOL_LOCAL

#endif  // DJINTEROP_CONFIG_HPP
