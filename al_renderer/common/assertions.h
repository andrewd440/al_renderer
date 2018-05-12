#pragma once

#ifdef _DEBUG
#define Assert( expr ) \
	if ( !(expr) ) \
	{ \
		ReportAssertionFailure( (#expr), __FILE__, __LINE__); \
		__debugbreak(); \
	}

inline void ReportAssertionFailure( const char* expr, const char* file, const unsigned int line )
{
	char msg[512];
	sprintf_s( msg, "Assertion Failed: %s File: %s Line: %d", expr, file, line );
	OutputDebugStringA( msg );
}

#define Assert_0() Assert( false )
#else
#define Assert(expr)
#define Assert_0()
#endif

#ifdef _DEBUG
#define Verify( exp, op, expected ) Assert( (exp) op (expected) )
#else
#define Verify( expr, op, expected ) (expr)
#endif

#define D3D_Verify( exp ) Verify( exp, ==, S_OK )