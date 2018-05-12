#pragma once

#include <type_traits>

template< typename Enum >
constexpr std::underlying_type_t< Enum > ToIdx( const Enum value )
{
	return static_cast< std::underlying_type_t< Enum > >( value );
}
