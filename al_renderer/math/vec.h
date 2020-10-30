#pragma once

#include "common/basictypes.h"

template < typename T >
T Max(T a, T b)
{
	return a > b ? a : b;
}

template < typename T >
T Min(T a, T b)
{
	return a > b ? b : a;
}