
#include <tchar.h>
#include "CPipeListener.h"

#define BUFSIZE 512

CPipeListener::CPipeListener(void)
{
	_fUnlocked = FALSE;
	_pProvider = NULL;
}

CPipeListener::~CPipeListener(void)
{
	if (_pProvider != NULL)
	{
		_pProvider->Release();
		_pProvider = NULL;
	}
}

HRESULT CPipeListener::Initialize(CSampleProvider *pProvider)
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

	// Create and launch the pipe thread.
	HANDLE hThread = ::CreateThread(NULL, 0, CPipeListener::_ThreadProc, (LPVOID) this, 0, NULL);
	if (hThread == NULL)
	{
		hr = HRESULT_FROM_WIN32(::GetLastError());
	}

	return hr;
}

BOOL CPipeListener::GetUnlockingStatus()
{
	return _fUnlocked;
}

DWORD WINAPI CPipeListener::_ThreadProc(LPVOID lpParameter)
{
	CPipeListener *pPipeListener = static_cast<CPipeListener *>(lpParameter);

	LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\NoninteractiveUnlockCredentialProvider");

	HANDLE hPipe = INVALID_HANDLE_VALUE;
	BOOL fConnected = FALSE;

	HANDLE hHeap = GetProcessHeap();
	TCHAR* pUsername = (TCHAR*)HeapAlloc(hHeap, 0, BUFSIZE*sizeof(TCHAR));
	TCHAR* pPassword = (TCHAR*)HeapAlloc(hHeap, 0, BUFSIZE*sizeof(TCHAR));
	BOOL fSuccess = FALSE;
	DWORD cbUsername = 0, cbPassword = 0;
	
	for (;;)
	{
		hPipe = CreateNamedPipe(
			lpszPipename,
			PIPE_ACCESS_INBOUND,
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
			PIPE_UNLIMITED_INSTANCES,
			BUFSIZE, BUFSIZE,
			0,
			NULL);
		fConnected = ConnectNamedPipe(hPipe, NULL);

		fSuccess = FALSE;
		cbUsername = 0;
		cbPassword = 0;
		fSuccess = ReadFile(hPipe, pUsername, BUFSIZE*sizeof(TCHAR), &cbUsername, NULL);
		fSuccess &= ReadFile(hPipe, pPassword, BUFSIZE*sizeof(TCHAR), &cbPassword, NULL);

		FlushFileBuffers(hPipe);
		DisconnectNamedPipe(hPipe);
		CloseHandle(hPipe);

		if (fSuccess && cbUsername && cbUsername)
		{
			pPipeListener->_fUnlocked = TRUE;
			pPipeListener->_pProvider->OnUnlockingStatusChanged();
			break;
		}
	}

	HeapFree(hHeap, 0, pUsername);
	HeapFree(hHeap, 0, pPassword);
	return 0;
}
