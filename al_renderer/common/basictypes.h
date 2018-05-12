#pragma once

#include <inttypes.h>
#include <DirectXMath.h>
#include <d3d12.h>

#include "assertions.h"

using namespace DirectX;

using b8 = uint8_t;
using b16 = uint16_t;
using b32 = uint32_t;
using b64 = uint64_t;
using bXX = b64;

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using iXX = i64;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using uXX = u64;

using f32 = float;
using f64 = double;

struct int2
{
	union
	{
		XMINT2 xm;
		i32 v[2];

		struct
		{
			i32 x;
			i32 y;
		};
	};
};

struct uint2
{
	union
	{
		XMUINT2 xm;
		u32 v[2];

		struct
		{
			u32 x;
			u32 y;
		};
	};
};

struct uint4
{
	union
	{
		struct
		{
			u32 x;
			u32 y;
			u32 z;
			u32 w;
		};

		XMVECTOR xm;
		u32 v[4];
	};
};

struct float2
{
	union
	{
		struct
		{
			float x;
			float y;
		};

		XMFLOAT2 xm;
		float v[2];
	};
};

struct float3
{
	union
	{
		struct
		{
			float x;
			float y;
			float z;
		};	
		
		XMFLOAT3 xm;
		float v[3];
	};
};

struct float4
{
	union
	{
		struct
		{
			float x;
			float y;
			float z;
			float w;
		};	
  
		XMVECTOR xm;
		float v[4];
	};
};

struct float4u
{
	union
	{
		struct
		{
			float x;
			float y;
			float z;
			float w;
		};	
		
		XMFLOAT4 xm;
		float v[4];
	};
};

struct float4x4
{
	union
	{
		float4 row[4];
		float v[16];
		XMMATRIX xm;
	};
};

extern uint4 uint4_Zero;
extern uint4 uint4_One;
extern uint4 uint4_Max;

template < typename T >
void ZeroMemoryT( T* mem )
{
	memset( mem, 0, sizeof( T ) );
}

template < typename T >
class D3DPtr
{
public:
	D3DPtr( T* ptr = nullptr )
		: m_ptr( nullptr )
	{
		Reset( ptr );
	}

	D3DPtr( const D3DPtr<T>& o ) { m_ptr = nullptr; Reset( o.Get() ); }
	D3DPtr( D3DPtr<T>&& o ) { m_ptr = nullptr; Reset( o.Get() ); o.Release(); };
	D3DPtr<T>& operator=( const D3DPtr<T>& o ) { Reset( o.Get() ); return *this; };
	D3DPtr<T>& operator=( D3DPtr<T>&& o ) { if ( this != &o ) { Reset( o.Get() ); o.Release(); } return *this; };

	~D3DPtr()
	{
		Reset( nullptr );
	}

	T* operator->()
	{
		return m_ptr;
	}

	T* operator->() const
	{
		return m_ptr;
	}

	T* Get() const
	{
		return m_ptr;
	}

	bool IsValid() const
	{
		return m_ptr != nullptr;
	}

	void Reset( T* newPtr )
	{
		if ( newPtr )
			newPtr->AddRef();

		if ( m_ptr )
			m_ptr->Release();

		m_ptr = newPtr;
	}

	void Release()
	{
		if ( m_ptr )
			m_ptr->Release();

		m_ptr = nullptr;
	}
	
	T* Detach()
	{
		T* ptr = m_ptr;
		m_ptr = nullptr;
		return ptr;
	}

	T** operator&()
	{
		return &m_ptr;
	}

private:
	T* m_ptr;
};

template < typename To, typename From >
To size_cast( From from )
{
	Assert( from == static_cast< To >( from ) );
	return static_cast< To >( from );
}

template< typename Src, typename Dest >
void size_cast_assign( Src src, Dest* dest )
{
	*dest = size_cast< Dest >( src );
}

template < typename T > 
struct ArrayRange
{
	ArrayRange( T* const _begin, T* const _end )
		: begin( _begin )
		, end( _end )
	{ }

	T* begin;
	T* end;
};

template < typename T >
T* begin( ArrayRange<T> range )
{
	return range.begin;
}

template < typename T >
T* end( ArrayRange<T> range )
{
	return range.end;
}

struct DescriptorHeapDesc_s
{
	D3D12_CPU_DESCRIPTOR_HANDLE heapStart_cpu;
	D3D12_GPU_DESCRIPTOR_HANDLE heapStart_gpu;
	u32 incrementSize;
};