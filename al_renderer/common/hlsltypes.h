#pragma once

#ifdef __cplusplus

#include "basictypes.h"
#include <DirectXMath.h>

#define HLSL_CAPITALIZE_float DirectX::XMFLOAT
#define HLSL_CAPITALIZE_int DirectX::XMINT
#define HLSL_CAPITALIZE_uint DirectX::XMUINT
#define HLSL_TO_XM( type ) HLSL_CAPITALIZE_##type

#define DECLARE_HLSL_VECTOR( basetype ) \
	using basetype##2 = ##HLSL_TO_XM( basetype ) ## 2; \
	using basetype##3 = ##HLSL_TO_XM( basetype ) ## 3; \
	using basetype##4 = ##HLSL_TO_XM( basetype ) ## 4; \

#define DECLARE_HLSL_MATRIX( basetype ) \
	using basetype##3x3 = ##HLSL_TO_XM( basetype ) ## 3X3; \
	using basetype##4x4 = ##HLSL_TO_XM( basetype ) ## 4X4; \
	
using uint = u32;

DECLARE_HLSL_VECTOR( float )
DECLARE_HLSL_VECTOR( int )
DECLARE_HLSL_VECTOR( uint )

DECLARE_HLSL_MATRIX( float )

#define DEFINE_CBUFFER( name, reg ) struct name

#else

#define DEFINE_CBUFFER( name, reg ) cbuffer name : register( c##reg )

#endif