#include "stdafx.h"
#include "win32app.h"
#include "graphics.h"
#include "input.h"
#include "imgui\imgui.h"

HWND g_hwnd;
static LRESULT CALLBACK WindowProc( HWND hWnd, u32 message, WPARAM wParam, LPARAM lParam );

int Win32_Run( u32 windowWidth, u32 windowHeight, HINSTANCE hInstance, i32 nCmdShow )
{
	// Initialize the window class.
	WNDCLASSEX windowClass = { 0 };
	windowClass.cbSize = sizeof( WNDCLASSEX );
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WindowProc;
	windowClass.hInstance = hInstance;
	windowClass.hCursor = LoadCursor( nullptr, IDC_ARROW );
	windowClass.lpszClassName = L"al_renderer";
	RegisterClassEx( &windowClass );

	RECT windowRect = { 0, 0, static_cast< LONG >( windowWidth ), static_cast< LONG >( windowHeight ) };
	AdjustWindowRect( &windowRect, WS_OVERLAPPEDWINDOW, FALSE );

	// Create the window and store a handle to it.
	g_hwnd = CreateWindow(
		windowClass.lpszClassName,
		L"al_renderer",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		nullptr,		// We have no parent window.
		nullptr,		// We aren't using menus.
		hInstance,
		nullptr );

	ShowWindow( g_hwnd, nCmdShow );
	Graphics_Init( windowWidth, windowHeight );

	// Main sample loop.
	MSG msg = {};
	while ( msg.message != WM_QUIT )
	{
		// Process any messages in the queue.
		if ( PeekMessage( &msg, nullptr, 0, 0, PM_REMOVE ) )
		{
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}
	}
	
	Graphics_Destroy();

	// Return this part of the WM_QUIT message to Windows.
	return static_cast< char >( msg.wParam );
}

// Main message handler for the sample.
static LRESULT CALLBACK WindowProc( HWND hWnd, u32 message, WPARAM wParam, LPARAM lParam )
{
	switch ( message )
	{
		case WM_CREATE:
		{
			// Save the DXSample* passed in to CreateWindow.
			LPCREATESTRUCT pCreateStruct = reinterpret_cast< LPCREATESTRUCT >( lParam );
			SetWindowLongPtr( hWnd, GWLP_USERDATA, reinterpret_cast< LONG_PTR >( pCreateStruct->lpCreateParams ) );
		}
		return 0;

		case WM_SYSKEYDOWN:
		case WM_KEYDOWN:
			Input_KeyDown( wParam, lParam );

			if ( ImGui::GetCurrentContext() && wParam < 256 )
				ImGui::GetIO().KeysDown[wParam] = 0;

			return 0;

		case WM_SYSKEYUP:
		case WM_KEYUP:
			Input_KeyUp( wParam, lParam );

			if ( ImGui::GetCurrentContext() && wParam < 256 )
				ImGui::GetIO().KeysDown[wParam] = 1;
			return 0;

		case WM_PAINT:
			Graphics_Update();
			Graphics_Render();
			return 0;

		case WM_DESTROY:
			PostQuitMessage( 0 );
			return 0;

		
	}

	if ( ImGui::GetCurrentContext() )
	{
		ImGuiIO* const io = &ImGui::GetIO();
		switch ( message )
		{
			case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
			case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:
			case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK:
			{
				int button = 0;
				if ( message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK ) button = 0;
				if ( message == WM_RBUTTONDOWN || message == WM_RBUTTONDBLCLK ) button = 1;
				if ( message == WM_MBUTTONDOWN || message == WM_MBUTTONDBLCLK ) button = 2;
				if ( !ImGui::IsAnyMouseDown() && ::GetCapture() == NULL )
					::SetCapture( g_hwnd );
				io->MouseDown[button] = true;
				return 0;
			}
			case WM_LBUTTONUP:
			case WM_RBUTTONUP:
			case WM_MBUTTONUP:
			{
				int button = 0;
				if ( message == WM_LBUTTONUP ) button = 0;
				if ( message == WM_RBUTTONUP ) button = 1;
				if ( message == WM_MBUTTONUP ) button = 2;
				io->MouseDown[button] = false;
				if ( !ImGui::IsAnyMouseDown() && ::GetCapture() == g_hwnd )
					::ReleaseCapture();
				return 0;
			}
			case WM_MOUSEWHEEL:
				io->MouseWheel += GET_WHEEL_DELTA_WPARAM( wParam ) > 0 ? +1.0f : -1.0f;
				return 0;

			case WM_MOUSEHWHEEL:
				io->MouseWheelH += GET_WHEEL_DELTA_WPARAM( wParam ) > 0 ? +1.0f : -1.0f;
				return 0;

			case WM_MOUSEMOVE:
				io->MousePos.x = (signed short)(lParam);
				io->MousePos.y = (signed short)(lParam >> 16);
				return 0;

			case WM_CHAR:
				// You can also use ToAscii()+GetKeyboardState() to retrieve characters.
				if ( wParam > 0 && wParam < 0x10000 )
					io->AddInputCharacter( (unsigned short)wParam );
				return 0;
		}
	}


	// Handle any messages the switch statement didn't.
	return DefWindowProc( hWnd, message, wParam, lParam );
}