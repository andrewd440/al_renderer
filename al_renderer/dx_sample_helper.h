#pragma once

inline void GetAssetsPath( WCHAR* path, UINT pathSize )
{
	Assert( path );

	DWORD size = GetModuleFileName( nullptr, path, pathSize );
	if ( size == 0 || size == pathSize )
	{
		// Method failed or path was truncated.
		Assert_0();
	}

	WCHAR* lastSlash = wcsrchr( path, L'\\' );
	if ( lastSlash )
	{
		*(lastSlash + 1) = L'\0';
	}
}

inline HRESULT ReadDataFromFile( LPCWSTR filename, byte** data, UINT* size )
{
	using namespace Microsoft::WRL;

	CREATEFILE2_EXTENDED_PARAMETERS extendedParams = {};
	extendedParams.dwSize = sizeof( CREATEFILE2_EXTENDED_PARAMETERS );
	extendedParams.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
	extendedParams.dwFileFlags = FILE_FLAG_SEQUENTIAL_SCAN;
	extendedParams.dwSecurityQosFlags = SECURITY_ANONYMOUS;
	extendedParams.lpSecurityAttributes = nullptr;
	extendedParams.hTemplateFile = nullptr;

	Wrappers::FileHandle file( CreateFile2( filename, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, &extendedParams ) );
	if ( file.Get() == INVALID_HANDLE_VALUE )
		Assert_0();

	FILE_STANDARD_INFO fileInfo = {};
	if ( !GetFileInformationByHandleEx( file.Get(), FileStandardInfo, &fileInfo, sizeof( fileInfo ) ) )
		Assert_0();

	if ( fileInfo.EndOfFile.HighPart != 0 )
		Assert_0();

	*data = reinterpret_cast<byte*>(malloc( fileInfo.EndOfFile.LowPart ));
	*size = fileInfo.EndOfFile.LowPart;

	if ( !ReadFile( file.Get(), *data, fileInfo.EndOfFile.LowPart, nullptr, nullptr ) )
		Assert_0();

	return S_OK;
}

// Assign a name to the object to aid with debugging.
#if defined(_DEBUG)
inline void SetName( ID3D12Object* pObject, LPCWSTR name )
{
	pObject->SetName( name );
}
inline void SetNameIndexed( ID3D12Object* pObject, LPCWSTR name, UINT index )
{
	WCHAR fullName[50];
	if ( swprintf_s( fullName, L"%s[%u]", name, index ) > 0 )
		pObject->SetName( fullName );
}
#else
inline void SetName( ID3D12Object*, LPCWSTR )
{
}
inline void SetNameIndexed( ID3D12Object*, LPCWSTR, UINT )
{
}
#endif

// Naming helper for D3DPtr<T>.
// Assigns the name of the variable as the name of the object.
// The indexed variant will include the index in the name of the object.
#define NAME_D3D12_OBJECT(x) SetName(x.Get(), L#x)
#define NAME_D3D12_OBJECT_INDEXED(x, n) SetNameIndexed(x[n].Get(), L#x, n)