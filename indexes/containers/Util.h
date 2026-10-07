#pragma once

#include <cstdlib>
#include <cstdint>
#include <cstddef>
#include <cassert>
#include <limits>
#include <type_traits>
#include <utility>
#include <initializer_list>

using std::size_t;

#define LENGTH(array) (sizeof(array) / sizeof(array[0]))


template<typename T>
T* array_calloc(size_t size) noexcept {
	return static_cast<T*>(std::calloc(size, sizeof(T)));
}

#include <iostream>
#include <iomanip>

template<typename T>
T* array_malloc(size_t size) noexcept{
	return static_cast<T*>(std::malloc(size * sizeof(T)));
}

template <typename T>
T next_power_of_two(T x)
{
	x--;
	                   x |= x >>  1;
	                   x |= x >>  2;
	                   x |= x >>  4;
	if (sizeof(T) > 1) x |= x >>  8;
	if (sizeof(T) > 2) x |= x >> 16;
	if (sizeof(T) > 4) x |= x >> 32;
	x++;
	return x;
}
