
#include "stdafx.h"
#include "VirtualStreamSink.h"
#include "global.h"
#include "VirtualStreamSinkPropertyPage.h"
#include <vector>
#include <queue>
#include <strstream>

using namespace std;

#define BUFSIZE 10000

//
// CreateInstance
//
// Creator function for the class ID
//
CUnknown * WINAPI CVirtualStreamSink::CreateInstance(LPUNKNOWN pUnk, HRESULT *phr)
{
	FilterTrace("CVirtualStreamSink::CreateInstance()\n");

	CVirtualStreamSink *pNewFilter = new CVirtualStreamSink(NAME("Stream Buffer Sink"), pUnk, phr);

	if (phr)
	{
		if (pNewFilter == NULL) 
			*phr = E_OUTOFMEMORY;
		else
			*phr = S_OK;
	}

	return pNewFilter;
}


//
// Constructor
//
CVirtualStreamSink::CVirtualStreamSink(TCHAR *pName, LPUNKNOWN pUnk, HRESULT *phr) :
    CBaseFilter(NAME("Virtual Stream Sink"), pUnk, this, CLSID_VirtualStreamSink),
	m_pAllocator(NULL), 
	m_hPipe(NULL),
	m_hThread(NULL),
	m_bServerRunning(false),
	m_bConnected(false)
{
	ASSERT(phr);
 
	FilterTrace("CVirtualStreamSink::CVirtualStreamSink()\n");		
	_tcscpy(m_szStreamName, L"VirtualSink1");
	*phr = AddInputPin();
}


void CVirtualStreamSink::InitializePipe()
{
	FilterTrace("CVirtualStreamSink::InitializePipe()\n");		

	if (m_hPipe != NULL)
		StopServer();
	 
	m_hPipe = NULL;

	// Create the named pipe
	memset(m_lpszPipename, 0, MAX_PATH);
	_stprintf(m_lpszPipename, L"\\\\.\\pipe\\%s", m_szStreamName);
	 
	FilterTrace("CVirtualStreamSink::InitializePipe() ==> Creating Sink Pin [%S]\n", m_lpszPipename);

	m_hPipe = CreateNamedPipe( 
          m_lpszPipename,             // pipe name 
          PIPE_ACCESS_DUPLEX,       // read/write access 
          PIPE_TYPE_MESSAGE |       // message type pipe 
          PIPE_READMODE_MESSAGE |   // message-read mode 
          PIPE_WAIT,                // blocking mode 
          PIPE_UNLIMITED_INSTANCES, // max. instances  
          BUFSIZE,                  // output buffer size 
          BUFSIZE,                  // input buffer size 
          0,                        // client time-out 
          NULL);                    // default security attribute 

	// Create a thread for this client. 
    m_hThread = CreateThread( 
		NULL,              // no security attribute 
		0,                 // default stack size 
		CVirtualStreamSink::InstanceThread,    // thread proc
		(LPVOID) this,    // thread parameter 
		0,                 // not suspended 
		&m_dwThreadId);      // returns thread ID 
}

//
// Destructor
//
CVirtualStreamSink::~CVirtualStreamSink()
{
	FilterTrace("CVirtualStreamSink::~CVirtualStreamSink()\n");

	for(int i = 0; i < (int)m_pinList.size(); i++)
		if (m_pinList[i] != NULL)
			delete m_pinList[i];

	if (m_hPipe != NULL)
		StopServer();
    
	m_pinList.clear();
}

//
// GetPinCount
//
int CVirtualStreamSink::GetPinCount()
{
	return (int)m_pinList.size();
}

//
// RemovePin
//
void CVirtualStreamSink::RemovePin(CBasePin* pPin)
{
	FilterTrace("CVirtualStreamSink::RemovePin()\n");

	if (m_pinList.size() == 0)
		return;
 	 
	for(long i = 0; i < (long)m_pinList.size(); i++)
	{
		if (m_pinList[i] == pPin)
		{
			delete m_pinList[i];
			m_pinList.erase(m_pinList.begin() + i);
			return;
		}
	}
}

//
// GetPin
//
CBasePin *CVirtualStreamSink::GetPin(int n)
{
    if ((int)m_pinList.size() < 1)
        return NULL ;

    // Pin zero is the one and only input pin
	return m_pinList[n];
  
    // return the output pin at position(n - 1) (zero based)
    return NULL;
}
 
//
// Stop
// 
// Overridden to handle no input connections
//
STDMETHODIMP CVirtualStreamSink::Stop()
{
	FilterTrace("CVirtualStreamSink::Stop()\n");

	m_State = State_Stopped;
    CBaseFilter::Stop();
	
	for(int i = 0; i < (int)m_pinList.size(); i++)
    {
		CBasePin* pPin = m_pinList[i];
	    // pPin->EndOfStream();
    }
 
    return NOERROR;
}

//
// Pause
//
// Overridden to handle no input connections
//
STDMETHODIMP CVirtualStreamSink::Pause()
{
	FilterTrace("CVirtualStreamSink::Pause()\n");

    CAutoLock cObjectLock(m_pLock);
    HRESULT hr = CBaseFilter::Pause();

	// if (m_hPipe == NULL)
	InitializePipe();

    return hr;
}
 
//
// Run
//
// Overridden to handle no input connections
//
STDMETHODIMP CVirtualStreamSink::Run(REFERENCE_TIME tStart)
{ 
	FilterTrace("CVirtualStreamSink::Run()\n");

    CAutoLock cObjectLock(m_pLock);

	// Add the tracks back into the new buffer
	for(int i = 0; i < (int)m_pinList.size(); i++)
	{
		if (((CVirtualStreamSinkInputPin*)m_pinList[i])->IsConnected())
		{
			// Reset the pin
			((CVirtualStreamSinkInputPin*)m_pinList[i])->Reset();
			
			// Add a track store for the pin
			((CVirtualStreamSinkInputPin*)m_pinList[i])->AddTrack();
		}
	}
 
	if (m_hPipe == NULL)
		InitializePipe();

    HRESULT hr = CBaseFilter::Run(tStart);
    return hr;
}
  
// 
// AddInputPin
//
HRESULT CVirtualStreamSink::AddInputPin(void)
{
	FilterTrace("CVirtualStreamSink::AddInputPin()\n");

	// Check that there is currently not an available
	// disconnected pin. We only want to add a pin if no
	// unconnected pins are available.
	for(int i = 0; i < (int)m_pinList.size(); i++)
    {
		CBasePin* pPin = m_pinList[i];
 
		PIN_DIRECTION pinDir;
		pPin->QueryDirection(&pinDir);

		if (pPin->IsConnected() == FALSE) 
	        return S_OK;
    }
 
	TCHAR szPinName[100];
	_stprintf_s(szPinName, 100, L"%sStream-%i-%08x", m_szStreamName, (int)m_pinList.size() + 1, GetTickCount());
 
	// Create the new pin
	HRESULT hr = S_OK;
	CVirtualStreamSinkInputPin* pInputPin = new CVirtualStreamSinkInputPin(NAME("Input Pin"), this, &hr, szPinName);

	if (FAILED(hr))
		return hr;
	 
	// Add the pin to the list of filter pins
	m_pinList.push_back(pInputPin);
	
	return S_OK;
}

//
// GetPages
//
STDMETHODIMP CVirtualStreamSink::GetPages(CAUUID *pPages)
{
	CheckPointer(pPages,E_POINTER);

	pPages->cElems = 1;
	pPages->pElems = (GUID *) CoTaskMemAlloc(sizeof(GUID));
	
	if (pPages->pElems == NULL)
		return E_OUTOFMEMORY;

	pPages->pElems[0] = CLSID_VirtualStreamSinkPropertyPage;

	return NOERROR;
}

// 
// NonDelegatingQueryInterface
//
STDMETHODIMP CVirtualStreamSink::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
	if(riid == CLSID_VirtualStreamSinkPropertyPage)
		return GetInterface(NULL, ppv);
	else if (riid == IID_ISpecifyPropertyPages) 
		return GetInterface((ISpecifyPropertyPages *) this, ppv);
	else if (riid == IID_IVirtualStreamSink) 
		return GetInterface((IVirtualStreamSink *) this, ppv);

	 
	return CBaseFilter::NonDelegatingQueryInterface(riid, ppv);
}
  
STDMETHODIMP CVirtualStreamSink::PipeOutMediaTypes()
{
	FilterTrace("CVirtualStreamSink::PipeOutMediaTypes()\n");

	DWORD cbBytesRead = 0, cbReplyBytes = 0, cbWritten = 0; 

	for(int i = 0; i < (int)m_pinList.size(); i++)
    {
		CVirtualStreamSinkInputPin* pPin = (CVirtualStreamSinkInputPin*)m_pinList[i];
		 
		PIN_DIRECTION pinDir;
		pPin->QueryDirection(&pinDir);

		if (pPin->IsConnected() == FALSE) 
	        continue;

		AM_MEDIA_TYPE amt = pPin->GetConnectedMediaType();

		// Get the size of the pin format info
 		UINT cbPinInfoBytes = 100 + sizeof(AM_MEDIA_TYPE) + sizeof(ALLOCATOR_PROPERTIES) + amt.cbFormat;

		// Allocate space for the pin information
		byte* pPinInfo = (byte*)malloc(cbPinInfoBytes);
		memset(pPinInfo, 0, cbPinInfoBytes);
	  
		// Get the pin allocator information
		ALLOCATOR_PROPERTIES allocProps;
		IMemAllocator* pMemAllocator;
		pPin->GetAllocator(&pMemAllocator); 
		pMemAllocator->GetProperties(&allocProps);
  		 
		CHAR szPinName[100];
		memset(szPinName, 0, 100);
		USES_CONVERSION;
		strcpy(szPinName, W2A(pPin->Name()));

		// Populate the pin info structure
		std::ostrstream myFile((char*)pPinInfo, cbPinInfoBytes, ios::out | ios::binary);
		myFile.write((char*)szPinName, 100);
		myFile.write((char*)&amt, sizeof(AM_MEDIA_TYPE));
		myFile.write((char*)&allocProps, sizeof(ALLOCATOR_PROPERTIES));
		myFile.write((char*)amt.pbFormat, amt.cbFormat);
		 
		// Write the pin information to the pipe
		BOOL fSuccess = WriteFile( 
			 m_hPipe,        // handle to pipe 
			 pPinInfo,     // buffer to write from 
			 cbPinInfoBytes, // number of bytes to write 
			 &cbWritten,   // number of bytes written 
			 NULL);        // not overlapped I/O 

		free(pPinInfo);
    }

	// Write the pin information to the pipe
	BOOL fSuccess = WriteFile( 
			m_hPipe,        // handle to pipe 
			NULL,     // buffer to write from 
			0, // number of bytes to write 
			&cbWritten,   // number of bytes written 
			NULL);        // not overlapped I/O 

	// Flush the pipe to allow the client to read the pipe's contents 
	// before disconnecting. Then disconnect the pipe, and close the 
	// handle to this pipe instance. 
	FlushFileBuffers(m_hPipe); 
	DisconnectNamedPipe(m_hPipe); 

	return S_OK;
}

DWORD WINAPI CVirtualStreamSink::InstanceThread(LPVOID lpvParam)
{ 
	FilterTrace("CVirtualStreamSink::InstanceThread()\n");

	CVirtualStreamSink* pSink = (CVirtualStreamSink*)lpvParam;

	HANDLE hPipe = NULL;
	pSink->m_bServerRunning = true;
	pSink->m_bConnected = false;

	while (pSink->m_bServerRunning)
	{
		// Wait for connection
		pSink->m_bConnected = ConnectNamedPipe(pSink->m_hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED); 
		 
		FilterTrace("CVirtualStreamSink::InstanceThread ==> Connected [%i]\n", pSink->m_bConnected);
		 
		if (pSink->m_bConnected  && pSink->m_bServerRunning)
			pSink->PipeOutMediaTypes();

		if (pSink->m_bServerRunning)
			::Sleep(250);
	}

	FilterTrace("CVirtualStreamSink::InstanceThread() (exit)\n");
	pSink->m_bServerRunning = false;
	pSink->m_bConnected = false;
	pSink->m_hThread = NULL;
	return 0;
}

void CVirtualStreamSink::StopServer() 
{ 
	FilterTrace("CVirtualStreamSink::StopServer()\n");

	if (m_hPipe != NULL)
	{
		this->m_bServerRunning = false;
	  
		HANDLE hPipe = CreateFile(m_lpszPipename, GENERIC_READ | GENERIC_WRITE, 0, NULL,OPEN_EXISTING, 0, NULL); 

		if (hPipe != INVALID_HANDLE_VALUE) { 
		   CloseHandle(hPipe); 
		}  
		 
		if (WaitForSingleObject(m_hThread, 10000) == WAIT_TIMEOUT)
			TerminateThread(m_hThread, 0);

		FilterTrace("CVirtualStreamSink::StopServer() ==> DisconnectNamedPipe()\n");
		if (m_hPipe != NULL)
			DisconnectNamedPipe(m_hPipe);
		   
		FilterTrace("CVirtualStreamSink::StopServer() ==> CloseHandle()\n");
		if (m_hPipe != NULL)
			CloseHandle(m_hPipe);
		 
		m_hPipe = NULL;
	}
	  
	FilterTrace("CVirtualStreamSink::StopServer() (exit) \n");
} 
