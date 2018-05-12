#include "stdafx.h"
#include "text_buf.h"

// TODO: Make thread local storage
constexpr u32 TEXBUFFER_SIZE = 4096;
char s_textBuffer[TEXBUFFER_SIZE];
u32 s_textBufferEnd;

const char* TxtAppend( const char* const fmt, ... )
{
	const char* const ret = &s_textBuffer[s_textBufferEnd];
	va_list args;
	va_start( args, fmt );
	s_textBufferEnd += snprintf( s_textBuffer + s_textBufferEnd, _countof( s_textBuffer ) - s_textBufferEnd, fmt, args );
	va_end( args );
	Assert( s_textBufferEnd < _countof( s_textBuffer ) );
	return ret;
}

const char* Txt( const char* const fmt, ... )
{
	va_list args;
	va_start( args, fmt );
	s_textBufferEnd = snprintf( s_textBuffer, _countof( s_textBuffer ), fmt, args );
	va_end( args );
	Assert( s_textBufferEnd < _countof( s_textBuffer ) );
	return s_textBuffer;
}