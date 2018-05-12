// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

#pragma warning( disable : 4996 )

#include <windows.h>

#include <d3d12.h>
#include <dxgi1_4.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include "d3dx12.h"

#include <wrl.h>
#include <shellapi.h>

#pragma warning( push )
#pragma warning( disable : 4577 )
#include <cstdio>
#include <thread>
#include <atomic>
#pragma warning( pop )

#include "assertions.h"
#include "basictypes.h"