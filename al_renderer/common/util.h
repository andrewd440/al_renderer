#pragma once

#include "basictypes.h"

template < uXX ALIGNMENT >
inline uXX Align( uXX value )
{
	static_assert( (ALIGNMENT & ALIGNMENT - 1) == 0, "Alignment must be power of 2." );
	return (value + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

template < typename Enum >
inline std::underlying_type_t< Enum > ToIdx( const Enum value )
{
	return static_cast< std::underlying_type_t< Enum > >( value );
}

inline D3DPtr<ID3DBlob> CompileShader(
	const std::wstring& filename,
	const D3D_SHADER_MACRO* defines,
	const std::string& entrypoint,
	const std::string& target )
{
	UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)  
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	HRESULT hr = S_OK;

	D3DPtr<ID3DBlob> byteCode = nullptr;
	D3DPtr<ID3DBlob> errors;
	hr = D3DCompileFromFile( filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entrypoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors );

	if ( errors.IsValid() )
		OutputDebugStringA( (char*)errors->GetBufferPointer() );

	D3D_Verify( hr );

	return byteCode;
}