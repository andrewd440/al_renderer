#include "stdafx.h"
#include "win32app.h"

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow )
{
	return Win32_Run( 1280, 720, hInstance, nCmdShow );
}