#include "StdAfx.h"

#include "VirtualStreamSource.h"
#include "VirtualStreamSourceOutputPin.h"
#include "Global.h"
#include <strstream>

using namespace std;

#define BUFSIZE 100000 // 100K

CVirtualStreamSource::CVirtualStreamSource(IUnknown *pUnk, HRESULT *phr) : CSource(L"Stream Buffer Source", pUnk, CLSID_VirtualStreamSource)
{
	m_dwSeekingCaps = 0;
	TCHAR* pStreamName = NULL;
	TCHAR szStreamID[MAX_PATH];
	
	memset(szStreamID, 0, MAX_PATH);

	// Look for the stream name in the environment variable
	GetEnvironmentVariable(_T("VirtualStreamID"), szStreamID, MAX_PATH);
	
	pStreamName = szStreamID;
	  
	if (pStreamName == NULL || _tcslen(pStreamName) == 0)
	{
		// Get the command-line 
		LPWSTR lpStr = GetCommandLine();

		// Look for a stream name in the command line
		pStreamName = _tcsstr(lpStr, _T("/stream"));

		// Point to the stream name value 
		if (pStreamName != NULL)
			pStreamName = pStreamName += 8;
	}

	// If a stream name is found then parse and use it
	if (pStreamName != NULL && _tcslen(pStreamName) > 0)
		CreateOutputPins(pStreamName);
	else
		CreateOutputPins(L"VirtualSink1");
	 
}
  
CVirtualStreamSource::~CVirtualStreamSource(void)
{
	FilterTrace("CVirtualStreamSource::~CVirtualStreamSource()\n");
	  
	for(int i = 0; i < (int)m_pinList.size(); i++)
		delete m_pinList[i];

	m_pinList.clear();
}

CUnknown * WINAPI CVirtualStreamSource::CreateInstance(IUnknown *pUnk, HRESULT *phr)
{
	FilterTrace("CVirtualStreamSource::CreateInstance()\n");

	CVirtualStreamSource *pNewFilter = new CVirtualStreamSource(pUnk, phr );

	if (phr)
	{
		if (pNewFilter == NULL) 
			*phr = E_OUTOFMEMORY;
		else
			*phr = S_OK;
	}

	return pNewFilter;
}

STDMETHODIMP CVirtualStreamSource::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
	if (riid == IID_IVirtualStreamSource) 
		return GetInterface((IVirtualStreamSource *) this, ppv);

	return CBaseFilter::NonDelegatingQueryInterface(riid, ppv);
}

STDMETHODIMP CVirtualStreamSource::Stop() 
{
	FilterTrace("CVirtualStreamSource::Stop()\n");

	for(int i = 0; i < (int)m_pinList.size(); i++)
		((CVirtualStreamSourceOutputPin*)m_pinList[i])->Stop();
 
 	HRESULT hr = CSource::Stop();

	return hr;
}

STDMETHODIMP CVirtualStreamSource::Pause()
{
	FilterTrace("CVirtualStreamSource::Pause()\n");
	return CSource::Pause();
}


STDMETHODIMP CVirtualStreamSource::Run(REFERENCE_TIME tStart)
{
	m_rtVideoRefPoint = -1;
	for(int i = 0; i < (int)m_pinList.size(); i++)
		((CVirtualStreamSourceOutputPin*)m_pinList[i])->Reset();

 	return CSource::Run(tStart);
}

STDMETHODIMP CVirtualStreamSource::GetState(DWORD dw, FILTER_STATE *pState)
{
	// FilterTrace("CVirtualStreamSource::GetState()\n");
 
	*pState = m_State;

	if (m_State == State_Paused || m_State == State_Stopped)
		return VFW_S_CANT_CUE;
	else
		return S_OK;
 
	return S_OK;

}

HRESULT CVirtualStreamSource::CreateOutputPins(LPCTSTR szPipeName)
{
	FilterTrace("CVirtualStreamSource::CreateOutputPins()\n");
	
	for(int i = 0; i < (int)m_pinList.size(); i++)
	{
		((CVirtualStreamSourceOutputPin*)m_pinList[i])->Disconnect();
		RemovePin((CSourceStream*)m_pinList[i]);
		delete m_pinList[i];
	}
	  
	m_pinList.clear();
 
	BOOL   fSuccess = FALSE; 
	TCHAR  chBuf[BUFSIZE]; 
	DWORD  cbRead, dwMode; 
 
	TCHAR lpszPipename[MAX_PATH];
	_stprintf(lpszPipename, L"\\\\%s\\pipe\\%s", PIPEHOST, szPipeName);

	HANDLE hPipe;

	while (1) 
	{ 
		hPipe = CreateFile( 
				lpszPipename,   // pipe name 
				GENERIC_READ |  // read and write access 
				GENERIC_WRITE, 
				0,              // no sharing 
				NULL,           // default security attributes
				OPEN_EXISTING,  // opens existing pipe 
				0,              // default attributes 
				NULL);          // no template file 

		// Break if the pipe handle is valid. 
		if (hPipe != INVALID_HANDLE_VALUE) 
			break; 
 
		// Exit if an error other than ERROR_PIPE_BUSY occurs. 
 
		if (GetLastError() != ERROR_PIPE_BUSY) 
		{
			_tprintf( TEXT("Could not open pipe %s. GLE=%d\n"), lpszPipename, GetLastError() ); 
			return -1;
		}
 
		// All pipe instances are busy, so wait for 20 seconds. 
 
		if ( ! WaitNamedPipe(lpszPipename, 20000)) 
		{ 
			printf("Could not open pipe: 20 second wait timed out."); 
			return -1;
		} 
	}

	// The pipe connected; change to message-read mode. 
	dwMode = PIPE_READMODE_MESSAGE; 
	fSuccess = SetNamedPipeHandleState( 
		hPipe,    // pipe handle 
		&dwMode,  // new pipe mode 
		NULL,     // don't set maximum bytes 
		NULL);    // don't set maximum time 
   
	if (!fSuccess) 
	{
		_tprintf( TEXT("SetNamedPipeHandleState failed. GLE=%d\n"), GetLastError() ); 
		return -1;
	}

	do 
	{ 
		// Read from the pipe. 
		fSuccess = ReadFile( 
			hPipe,    // pipe handle 
			chBuf,    // buffer to receive reply 
			BUFSIZE,  // size of buffer 
			&cbRead,  // number of bytes read 
			NULL);    // not overlapped 
 
		if ( cbRead == 0 || (! fSuccess && GetLastError() != ERROR_MORE_DATA) )
			break; 

		TCHAR pinName[100];
		ALLOCATOR_PROPERTIES allocProps;
		AM_MEDIA_TYPE amt;
		HRESULT hr;

		CHAR szPinName[100];
		memset(szPinName, 0, 100);

		std::istrstream myFile((char*)chBuf, cbRead);
		myFile.read((char*)szPinName, 100);
		myFile.read((char*)&amt, sizeof(AM_MEDIA_TYPE));
		myFile.read((char*)&allocProps, sizeof(ALLOCATOR_PROPERTIES));
		amt.pbFormat = (byte*)malloc(amt.cbFormat);
		memset(amt.pbFormat, 0, amt.cbFormat);
		myFile.read((char*)amt.pbFormat, amt.cbFormat);
  
		USES_CONVERSION;
		_stprintf(pinName, A2W(szPinName));
		CVirtualStreamSourceOutputPin* pPin = (CVirtualStreamSourceOutputPin*)new CVirtualStreamSourceOutputPin(&hr, this, pinName, allocProps);
 		pPin->SetMediaType(&CMediaType(amt));
 		m_pinList.push_back(pPin);

	} while (fSuccess);   
	
	CloseHandle(hPipe);

	return S_OK;
}
 