#include "stdafx.h"

bool CRenderDevice::on_message(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& result)
{
    switch (uMsg)
    {
    case WM_SYSKEYDOWN: 
	{
        return true;
    }
    case WM_ACTIVATE: 
	{
        OnWM_Activate(wParam, lParam);
        return (false);
    }
    case WM_SETCURSOR: 
	{
        result = 1;
        return (true);
    }
    case WM_SYSCOMMAND: 
	{
        // Prevent moving/sizing and power loss in fullscreen mode
        switch (wParam)
        {
        case SC_MOVE:
        case SC_SIZE:
        case SC_MAXIMIZE:
        case SC_MONITORPOWER: result = 1; return (true);
        }
        return (false);
    }
    case WM_CLOSE: 
	{
        result = 0;
        return (true);
    }
    }

    return (false);
}
//-----------------------------------------------------------------------------
// Name: WndProc()
// Desc: Static msg handler which passes messages to the application class.
//-----------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LRESULT result;
    if (Device.on_message(hWnd, uMsg, wParam, lParam, result))
        return (result);

    return (DefWindowProc(hWnd, uMsg, wParam, lParam));
}
