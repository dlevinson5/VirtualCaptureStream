#include "stdafx.h"
#include "global.h"
#include "VirtualStreamSinkInputPin.h"
#include "VirtualStreamSink.h"
#include <strstream>

CVirtualStreamSinkInputPin::CVirtualStreamSinkInputPin(TCHAR   *pName,
                           CVirtualStreamSink    *pTee,
                           HRESULT *phr,
                           LPCWSTR pPinName) :
    CBaseInputPin(pName, pTee, &m_pinLock, phr, pPinName),
    m_pFilter(pTee),
	m_trackId(-1),
	m_hPipe(NULL),
	m_hThread(NULL),
	m_bConnected(false),
	m_bSyncPointHit(false),
	m_rtRefPoint(-999999999),
	m_bWriteBuffering(false),
	m_bPipeServerRunning(false)
{
	// Create the sink pipe with the properly allocated buffer size
	memset(m_lpszPipename, 0, MAX_PATH);
	_stprintf(m_lpszPipename, L"\\\\.\\pipe\\%s", this->m_pName);


    ATLASSERT(pTee); 
	CreateMemoryAllocator(&m_pBufferAllocator);
}
  
CVirtualStreamSinkInputPin::~CVirtualStreamSinkInputPin()
{
	// Close pipe handle
	if (m_hPipe != NULL)
		StopServer();
  	  
	if (m_pBufferAllocator != NULL)
		m_pBufferAllocator->Release();
   
    // ATLASSERT(m_pFilter->m_pAllocator == NULL);
}

HRESULT CVirtualStreamSinkInputPin::Active()
{
	CAutoLock lock_it(m_pLock);   
   
    // Pass the call on to the base class    
    return CBaseInputPin::Active();   
}

HRESULT CVirtualStreamSinkInputPin::CheckMediaType(const CMediaType *pmt)
{
    CAutoLock lock_it(m_pLock);
     
	// Accept all video formats
	if (pmt->majortype == MEDIATYPE_Video)
		return NOERROR;

	// Accept all audio formats
	if (pmt->majortype == MEDIATYPE_Audio)
		return NOERROR;

	// Accept line 21 data
	if (pmt->majortype == MEDIATYPE_AUXLine21Data)
		return NOERROR;

    return E_INVALIDARG;  
}  

HRESULT CVirtualStreamSinkInputPin::SetMediaType(const CMediaType *pmt)
{
	FilterTrace("CVirtualStreamSinkInputPin::SetMediaType()\n");

    // CAutoLock lock_it(m_pLock);
    HRESULT hr = NOERROR;

    // Make sure that the base class likes it
    if (FAILED(hr = CBaseInputPin::SetMediaType(pmt)))
        return hr;
    
	ATLASSERT(m_Connected != NULL);
    return NOERROR;

} // SetMediaType

HRESULT CVirtualStreamSinkInputPin::BreakConnect()
{
	FilterTrace("CVirtualStreamSinkInputPin::BreakConnect()\n");

	HRESULT hr;

	// Make sure that the base class likes it
    if (FAILED(hr = CBaseInputPin::BreakConnect()))
        return hr;

    // Release any allocator that we are holding
    if (m_pFilter->m_pAllocator)
    {
		FilterTrace("CVirtualStreamSinkInputPin::BreakConnect() ==> Release Allocator\n");
        m_pFilter->m_pAllocator->Release();
        m_pFilter->m_pAllocator = NULL;
    }

	// terminate the media type pipe

	if (m_hThread2 != NULL)
	{
		FilterTrace("CVirtualStreamSinkInputPin::BreakConnect() ==> Stop Write Buffering\n");

		m_bWriteBuffering = false;
   	 
		if (WaitForSingleObject(m_hThread2, 10000) != WAIT_OBJECT_0)
			TerminateThread(m_hThread2, 0);
 	
		m_hThread2 = NULL;

		this->m_writeBuffer.empty();
	}

	FilterTrace("CVirtualStreamSinkInputPin::BreakConnect() (exit) \n");
    return NOERROR;

} // BreakConnect

STDMETHODIMP CVirtualStreamSinkInputPin::AddTrack()
{
	FilterTrace("CVirtualStreamSinkInputPin::AddTrack()\n");

	IMemAllocator* pAllocator;
	GetAllocator(&pAllocator);
	ALLOCATOR_PROPERTIES allocProps;
	pAllocator->GetProperties(&allocProps);
 
	return S_OK;
}

STDMETHODIMP CVirtualStreamSinkInputPin::NotifyAllocator(IMemAllocator *pAllocator, BOOL bReadOnly)
{
	FilterTrace("CVirtualStreamSinkInputPin::NotifyAllocator()\n");

    CheckPointer(pAllocator,E_FAIL);
    CAutoLock lock_it(m_pLock);

    // Free the old allocator if any
    if (m_pFilter->m_pAllocator)
		m_pFilter->m_pAllocator->Release();

    // Store away the new allocator
    pAllocator->AddRef();
    m_pFilter->m_pAllocator = pAllocator;

 	ALLOCATOR_PROPERTIES allocProps;
	this->m_pFilter->m_pAllocator->GetProperties(&allocProps);
	  
	if (m_hPipe == NULL)
	{
		FilterTrace("CVirtualStreamSinkInputPin::NotifyAllocator() ==> Creating Pipe [%S]\n", m_lpszPipename);

		m_hPipe = CreateNamedPipe( 
			  m_lpszPipename,           // pipe name 
			  PIPE_ACCESS_DUPLEX,       // read/write access 
			  PIPE_TYPE_MESSAGE |       // message type pipe 
			  PIPE_READMODE_MESSAGE |   // message-read mode 
			  PIPE_WAIT,                // blocking mode 
			  PIPE_UNLIMITED_INSTANCES, // max. instances  
			  allocProps.cbBuffer * (allocProps.cBuffers * 10), // output buffer size 
			  allocProps.cbBuffer * (allocProps.cBuffers * 10), // input buffer size 
			  2000,                      // client time-out 
			  NULL);                    // default security attribute 

		FilterTrace("CVirtualStreamSinkInputPin::NotifyAllocator() ==> Creating Pipe Server [%S]\n", m_lpszPipename);

		// Create a thread for the pipe client connections. 
		m_hThread = CreateThread( 
			NULL,              // no security attribute 
			0,                 // default stack size 
			CVirtualStreamSinkInputPin::InstanceThread,    // thread proc
			(LPVOID) this,    // thread parameter 
			0,                 // not suspended 
			&m_dwThreadId);      // returns thread ID 
	}

	// Notify the base class about the allocator
    return CBaseInputPin::NotifyAllocator(pAllocator,bReadOnly);
} 

HRESULT CVirtualStreamSinkInputPin::EndOfStream()
{
	FilterTrace("CVirtualStreamSinkInputPin::EndOfStream()\n");

    HRESULT hr = NOERROR;

	if (m_hPipe != NULL)
		StopServer();
 
	return(NOERROR);
} 

HRESULT CVirtualStreamSinkInputPin::BeginFlush()
{
	FilterTrace("CVirtualStreamSinkInputPin::BeginFlush()\n");

    CAutoLock lock_it(m_pLock);
    HRESULT hr = NOERROR;
   
    return CBaseInputPin::BeginFlush();
} 

HRESULT CVirtualStreamSinkInputPin::EndFlush()
{
	FilterTrace("CVirtualStreamSinkInputPin::EndFlush()\n");

    CAutoLock lock_it(m_pLock);
    HRESULT hr = NOERROR;
    
    return CBaseInputPin::EndFlush();

}
              
HRESULT CVirtualStreamSinkInputPin::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
	FilterTrace("CVirtualStreamSinkInputPin::NewSegment()\n");
	  
    CAutoLock lock_it(m_pLock);
    HRESULT hr = NOERROR;
    
    return CBaseInputPin::NewSegment(tStart, tStop, dRate);

} 

void CVirtualStreamSinkInputPin::CreateWriteBufferThread()
{
	FilterTrace("CVirtualStreamSinkInputPin::CreateWriteBufferThread() ==> Pipe [%S]\n", m_lpszPipename);

	this->m_bWriteBuffering = true;

	// Create a thread for the pipe writes
	m_hThread2 = CreateThread( 
		NULL,              // no security attribute 
		0,                 // default stack size 
		CVirtualStreamSinkInputPin::InstanceWritePipeThread,    // thread proc
		(LPVOID) this,    // thread parameter 
		0,                 // not suspended 
		&m_dwThreadId2);   
}

HRESULT CVirtualStreamSinkInputPin::Receive(IMediaSample *pSample)
{
	CAutoLock lock_it(m_pLock);

	int hr = 0;

	// If the write buffer is not running then start it
	if (!this->m_bWriteBuffering)
		CreateWriteBufferThread();
	 
	CMediaSampleX* pMediaSample = new CMediaSampleX(0, pSample);
	m_writeBuffer.push(pMediaSample);
  
 	// Check that all is well with the base class
	if (FAILED(hr = CBaseInputPin::Receive(pSample)))
		return hr;

    return S_OK;
}  

HRESULT CVirtualStreamSinkInputPin::CompleteConnect(IPin *pReceivePin)
{
	FilterTrace("CVirtualStreamSinkInputPin::CompleteConnect()\n");

    ATLASSERT(pReceivePin);
    
    HRESULT hr;
	
	if (FAILED(hr = CBaseInputPin::CompleteConnect(pReceivePin)))
        return hr;
 
	m_pFilter->AddInputPin();
	  
    return S_OK;
}
 
HRESULT CVirtualStreamSinkInputPin::Reset(void)
{
	FilterTrace("CVirtualStreamSinkInputPin::Reset()\n");

	CAutoLock lock_it(m_pLock);
	m_writeBuffer.empty();
	m_rtRefPoint = -999999999;
	return S_OK;
}

GUID CVirtualStreamSinkInputPin::GetPinMajorType()
{
	return m_mt.majortype;
}


DWORD WINAPI CVirtualStreamSinkInputPin::InstanceThread(LPVOID lpvParam)
// This routine is a thread processing function to read from and reply to a client
// via the open pipe connection passed from the main loop. Note this allows
// the main loop to continue executing, potentially creating more threads of
// of this procedure to run concurrently, depending on the number of incoming
// client connections.
{ 
	CVirtualStreamSinkInputPin* pPin = (CVirtualStreamSinkInputPin*)lpvParam;

	FilterTrace("CVirtualStreamSinkInputPin::InstanceThread() [%S]\n", pPin->m_pName);

	HANDLE hPipe = NULL;
	BOOL bConnected = true;
	pPin->m_bPipeServerRunning = true;

	while (pPin->m_bPipeServerRunning && pPin->m_hPipe != NULL)
	{
		// Wait for connection
		if (pPin->m_hPipe != NULL)
			pPin->m_bConnected = ConnectNamedPipe(pPin->m_hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED); 

		if (pPin->m_bPipeServerRunning && !pPin->m_bConnected && pPin->m_hPipe != NULL && pPin->m_pName != NULL)
		{
			FilterTrace("CVirtualStreamSinkInputPin::InstanceThread() ==> Pipe Connection failed for Pin [%S]\n", pPin->m_pName);
			DisconnectNamedPipe(pPin->m_hPipe);
		}

		if (pPin->m_bPipeServerRunning)
			::Sleep(250);
	}
	 
	FilterTrace("CVirtualStreamSinkInputPin::InstanceThread() (exit) [%S]\n", pPin->m_pName);
	pPin->m_bPipeServerRunning = false;
	pPin->m_hThread = NULL;
	return 0;
}

void CVirtualStreamSinkInputPin::StopServer() 
{ 
	FilterTrace("CVirtualStreamSinkInputPin::StopServer()\n");

	// CAutoLock lock_it(m_pLock);

	FilterTrace("CVirtualStreamSinkInputPin::StopServer() ==> Pin [%S]\n", m_pName);

	if (m_hPipe != NULL)
	{
		m_bPipeServerRunning = false;

		// Check if the pipe connection can be established (releases the blocking pipe connection listener)
  	    HANDLE hPipe = CreateFile(m_lpszPipename, GENERIC_READ | GENERIC_WRITE, 0, NULL,OPEN_EXISTING, 0, NULL); 

		// Close the file handle if one was opened
	    if (hPipe != INVALID_HANDLE_VALUE) { 
	       CloseHandle(hPipe); 
		}  
		  
		// Wait for the pipe lister thread to terminate
		if (WaitForSingleObject(m_hThread, 10000) == WAIT_TIMEOUT)
			TerminateThread(m_hThread, 0);

		FilterTrace("CVirtualStreamSinkInputPin::StopServer() ==> DisconnectNamedPipe()\n");
		if (m_hPipe != NULL)
			DisconnectNamedPipe(m_hPipe);
		  
		FilterTrace("CVirtualStreamSinkInputPin::StopServer() ==> CloseHandle()\n");
		if (m_hPipe != NULL)
			CloseHandle(m_hPipe);
	}
  
	m_hPipe = NULL;
  	m_hThread = NULL;
	FilterTrace("CVirtualStreamSinkInputPin::StopServer() (exit) \n");
} 

CMediaSampleX* CVirtualStreamSinkInputPin::GetNextWriteBufferSample()
{
	CAutoLock lock_it(m_pLock);
	
	if (m_writeBuffer.size() > 0)
	{
		CMediaSampleX* pSample = m_writeBuffer.front();
		m_writeBuffer.pop();
		return pSample;
	}

	return NULL;
}

DWORD WINAPI CVirtualStreamSinkInputPin::InstanceWritePipeThread(LPVOID lpvParam)
{ 
	FilterTrace("CVirtualStreamSinkInputPin::InstanceWritePipeThread()\n");

	CVirtualStreamSinkInputPin* pPin = (CVirtualStreamSinkInputPin*)lpvParam;
	 
	while (pPin->m_bWriteBuffering)
	{
		CMediaSampleX* pSample = NULL;

		while (pPin->m_bWriteBuffering && (pSample = pPin->GetNextWriteBufferSample()) != NULL)
		{
			REFERENCE_TIME rtStart, rtStop, rtmStart, rtmStop;
			rtStart = pSample->m_timeStart;
			rtStop = pSample->m_timeEnd;
			rtmStart = pSample->m_mediaTimeStart;
			rtmStop = pSample->m_mediaTimeEnd;
  	 
			if (pPin->m_rtRefPoint == -999999999)
			{
				pPin->m_rtRefPoint = rtStart;
				pPin->m_rtRefPoint2  = rtmStart;
			}
	   
			if (pPin->m_bConnected)
			{
				DWORD cbWritten = 0;

				MediaSampleInfo mediaSampleInfo;
				ZeroMemory(&mediaSampleInfo,  sizeof(MediaSampleInfo));
				mediaSampleInfo.actualDataLength = pSample->m_actualDataLength;
				mediaSampleInfo.size = pSample->m_size;
				mediaSampleInfo.isDiscontinuity = pSample->m_isDiscontinuity;
				mediaSampleInfo.isPreroll = pSample->m_isPreroll;
				mediaSampleInfo.isSyncPoint = pSample->IsSyncPoint();
				mediaSampleInfo.mediaTimeStart = pSample->m_mediaTimeStart;
				mediaSampleInfo.mediaTimeEnd = pSample->m_mediaTimeEnd;
				mediaSampleInfo.timeStart = pSample->m_timeStart;
				mediaSampleInfo.timeEnd = pSample->m_timeEnd;
				mediaSampleInfo.timeOffset = pPin->m_rtRefPoint;
				mediaSampleInfo.mediaTimeOffset = pPin->m_rtRefPoint2;

				// Write the media sample info
				BOOL fSuccess = WriteFile( 
					 pPin->m_hPipe,        // handle to pipe 
					 (BYTE*)&mediaSampleInfo,     // buffer to write from 
					 sizeof(MediaSampleInfo), // number of bytes to write 
					 &cbWritten,   // number of bytes written 
					 NULL);        // not overlapped I/O 

				 
				if (mediaSampleInfo.actualDataLength > 0)
				{
					BYTE* pBuffer = pSample->GetBuffer();

					BOOL fSuccess = WriteFile( 
						 pPin->m_hPipe,        // handle to pipe 
						 (BYTE*)pBuffer,     // buffer to write from 
						 mediaSampleInfo.actualDataLength, // number of bytes to write 
						 &cbWritten,   // number of bytes written 
						 NULL);        // not overlapped I/O 
				}
 			}

			delete pSample;
		}

		::Sleep(250);
	}

	FilterTrace("CVirtualStreamSinkInputPin::InstanceWritePipeThread() => (exit)\n");

	return 0;
}
 
