#include "stdafx.h"
#include "input.h"
#include "type_funcs.h"

enum class KeyState_e
{
	Up,
	Down,
} s_keyState[256];

void Input_KeyUp( WPARAM wParam, LPARAM lParam )
{
	s_keyState[wParam] = KeyState_e::Up;
}

void Input_KeyDown( WPARAM wParam, LPARAM lParam )
{
	s_keyState[wParam] = KeyState_e::Down;
}
