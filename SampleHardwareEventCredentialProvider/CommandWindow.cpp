//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) 2006 Microsoft Corporation. All rights reserved.
//
//

#include "CommandWindow.h"
#include <strsafe.h>

// Custom messages for managing the behavior of the window thread.
#define WM_EXIT_THREAD              WM_USER + 1
#define WM_TOGGLE_CONNECTED_STATUS  WM_USER + 2

const TCHAR *g_wszClassName = "EventWindow";
const TCHAR *g_wszConnected = "Connected";
const TCHAR *g_wszDisconnected = "Disconnected";

CCommandWindow::CCommandWindow(void)
{
    _hWnd = NULL;
    _hInst = NULL;
    _fConnected = FALSE;
    _pProvider = NULL;
}

CCommandWindow::~CCommandWindow(void)
{
    // If we have an active window, we want to post it an exit message.
    if (_hWnd != NULL)
    {
        ::PostMessage(_hWnd, WM_EXIT_THREAD, 0, 0);
        _hWnd = NULL;
    }

    // We'll also make sure to release any reference we have to the provider.
    if (_pProvider != NULL)
    {
        _pProvider->Release();
        _pProvider = NULL;
    }
}

// Performs the work required to spin off our message so we can listen for events.
HRESULT CCommandWindow::Initialize(CSampleProvider *pProvider)
{
    HRESULT hr = S_OK;

    // Be sure to add a release any existing provider we might have, then add a reference
    // to the provider we're working with now.
    if (_pProvider != NULL)
    {
        _pProvider->Release();
    }
    _pProvider = pProvider;
    _pProvider->AddRef();
    
    // Create and launch the window thread.
    HANDLE hThread = ::CreateThread(NULL, 0, CCommandWindow::_ThreadProc, (LPVOID) this, 0, NULL);
    if (hThread == NULL)
    {
        hr = HRESULT_FROM_WIN32(::GetLastError());
    }

    return hr;
}

// Wraps our internal connected status so callers can easily access it.
BOOL CCommandWindow::GetConnectedStatus()
{
    return _fConnected;
}

//
//  FUNCTION: _MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage are only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
HRESULT CCommandWindow::_MyRegisterClass(void)
{
    HRESULT hr = S_OK;

    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style            = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = CCommandWindow::_WndProc;
    wcex.cbClsExtra        = 0;
    wcex.cbWndExtra        = 0;
    wcex.hInstance        = _hInst;
    wcex.hIcon            = NULL;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground    = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName    = NULL;
    wcex.lpszClassName    = ::g_wszClassName;
    wcex.hIconSm        = NULL;

    if (!RegisterClassEx(&wcex))
    {
        hr = HRESULT_FROM_WIN32(::GetLastError());
    }

    return hr;
}

//
//   FUNCTION: _InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
HRESULT CCommandWindow::_InitInstance()
{
    HRESULT hr = S_OK;

    // Create our window to receive events.
    _hWnd = ::CreateWindowEx(
        WS_EX_TOPMOST, 
        ::g_wszClassName, 
        ::g_wszDisconnected, 
        WS_DLGFRAME,
        200, 200, 200, 80, 
        NULL,
        NULL, _hInst, NULL);
    if (_hWnd == NULL)
    {
        hr = HRESULT_FROM_WIN32(::GetLastError());
    }

    if (SUCCEEDED(hr))
    {
        // Add a button to the window.
        _hWndButton = ::CreateWindow("Button", "Press to connect", 
                             WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 
                             10, 10, 180, 30, 
                             _hWnd, 
                             NULL,
                             _hInst,
                             NULL);
        if (_hWndButton == NULL)
        {
            hr = HRESULT_FROM_WIN32(::GetLastError());
        }

        if (SUCCEEDED(hr))
        {
            // Show and update the window.
            if (!::ShowWindow(_hWnd, SW_NORMAL))
            {
                hr = HRESULT_FROM_WIN32(::GetLastError());
            }

            if (SUCCEEDED(hr))
            {
                if (!::UpdateWindow(_hWnd))
                {
                   hr = HRESULT_FROM_WIN32(::GetLastError());
                }
            }
        }
    }

    return hr;
}

// Called from the separate thread to process the next message in the message queue. If
// there are no messages, it'll wait for one.
BOOL CCommandWindow::_ProcessNextMessage()
{
    // Grab, translate, and process the message.
    MSG msg;
    (void) ::GetMessage(&(msg), _hWnd, 0, 0);
    (void) ::TranslateMessage(&(msg));
    (void) ::DispatchMessage(&(msg));

    // This section performs some "post-processing" of the message. It's easier to do these
    // things here because we have the handles to the window, its button, and the provider
    // handy.
    switch (msg.message)
    {
    // Return to the thread loop and let it know to exit.
    case WM_EXIT_THREAD: return FALSE;

    // Toggle the connection status, which also involves updating the UI.
    case WM_TOGGLE_CONNECTED_STATUS:
        _fConnected = !_fConnected;
        if (_fConnected)
        {
            ::SetWindowText(_hWnd, ::g_wszConnected);
            ::SetWindowText(_hWndButton, "Press to disconnect");
        }
        else
        {
            ::SetWindowText(_hWnd, ::g_wszDisconnected);
            ::SetWindowText(_hWndButton, "Press to connect");
        }
        _pProvider->OnConnectStatusChanged();
        break;
    }
    return TRUE;
}

// Manages window messages on the window thread.
LRESULT CALLBACK CCommandWindow::_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    // Originally we were going to work with USB keys being inserted and removed, but it
    // seems as though these events don't get to us on the secure desktop. However, you
    // might see some messageboxi in CredUI.
    // TODO: Remove if we can't use from LogonUI.
    case WM_DEVICECHANGE:
        ::MessageBox(NULL, TEXT("Device change"), TEXT("Device change"), 0);
        break;

    // We assume this was the button being clicked.
    case WM_COMMAND:
        ::PostMessage(hWnd, WM_TOGGLE_CONNECTED_STATUS, 0, 0);
        break;

    // To play it safe, we hide the window when "closed" and post a message telling the 
    // thread to exit.
    case WM_CLOSE:
        ::ShowWindow(hWnd, SW_HIDE);
        ::PostMessage(hWnd, WM_EXIT_THREAD, 0, 0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Our thread procedure. We actually do a lot of work here that could be put back on the 
// main thread, such as setting up the window, etc.
DWORD WINAPI CCommandWindow::_ThreadProc(LPVOID lpParameter)
{
    CCommandWindow *pCommandWindow = static_cast<CCommandWindow *>(lpParameter);
    if (pCommandWindow == NULL)
    {
        // TODO: What's the best way to raise this error?
        return 0;
    }

    HRESULT hr = S_OK;

    // Create the window.
    pCommandWindow->_hInst = ::GetModuleHandle(NULL);
    if (pCommandWindow->_hInst != NULL)
    {            
        hr = pCommandWindow->_MyRegisterClass();
        if (SUCCEEDED(hr))
        {
            hr = pCommandWindow->_InitInstance();
        }
    }
    else
    {
        hr = HRESULT_FROM_WIN32(::GetLastError());
    }

    // ProcessNextMessage will pump our message pump and return false if it comes across
    // a message telling us to exit the thread.
    if (SUCCEEDED(hr))
    {        
        while (pCommandWindow->_ProcessNextMessage()) 
        {
        }
    }
    else
    {
        if (pCommandWindow->_hWnd != NULL)
        {
            pCommandWindow->_hWnd = NULL;
        }
    }

    return 0;
}

