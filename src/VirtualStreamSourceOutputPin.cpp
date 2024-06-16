#include "StdAfx.h"
#include "global.h"
#include "VirtualStreamSourceOutputPin.h"
#include <strstream>
#include "MediaSampleX.h"

const DWORD BITS_PER_BYTE = 8;

CVirtualStreamSourceOutputPin::CVirtualStreamSourceOutputPin(HRESULT *phr, CVirtualStreamSource* pFilter, LPCTSTR szName, ALLOCATOR_PROPERTIES allocProps) : CSourceStream(_T("StreamSource"), phr, (CSource*)pFilter, szName) 
{
	m_allocProperties = allocProps;
	m_pFilter = pFilter;
 	m_bConnected = false;
	m_hPipe = NULL;
	m_fFirstSampleDelivered = false;
 	Reset();
}

CVirtualStreamSourceOutputPin::~CVirtualStreamSourceOutputPin(void)
{
	FilterTrace("CVirtualStreamSourceOutputPin::~CVirtualStreamSourceOutputPin()\n");
	DisconnectPipe();
}
  
HRESULT CVirtualStreamSourceOutputPin::Reset(void)
{
	FilterTrace("CVirtualStreamSourceOutputPin::Reset()\n");
	m_fFirstSampleDelivered = false;
	return S_OK;
}

HRESULT CVirtualStreamSourceOutputPin::Stop(void)
{
	FilterTrace("CVirtualStreamSourceOutputPin::Reset()\n");
	m_fFirstSampleDelivered = false;
	m_bRunning = false;
	return S_OK;
}
 
STDMETHODIMP CVirtualStreamSourceOutputPin::SuggestAllocatorProperties(const ALLOCATOR_PROPERTIES *pprop)
{
	return S_OK;
}

STDMETHODIMP CVirtualStreamSourceOutputPin::GetAllocatorProperties(ALLOCATOR_PROPERTIES *pprop)
{
	this->m_pAllocator->GetProperties(pprop);
	return S_OK;
}

STDMETHODIMP CVirtualStreamSourceOutputPin::SetFormat(AM_MEDIA_TYPE *pMediaType)
{
	FilterTrace("CVirtualStreamSourceOutputPin::SetFormat()\n");
	// m_mt.Set(*pMediaType);
	return S_OK;
}

STDMETHODIMP CVirtualStreamSourceOutputPin::GetFormat(AM_MEDIA_TYPE **pMediaType)
{
	FilterTrace("CVirtualStreamSourceOutputPin::GetFormat()\n");
	*pMediaType = ::CreateMediaType((const AM_MEDIA_TYPE*)&m_mt);   
 	return S_OK;
}

STDMETHODIMP CVirtualStreamSourceOutputPin::GetNumberOfCapabilities(int *piCount,int *piSize)
{
	FilterTrace("CVirtualStreamSourceOutputPin::GetNumberOfCapabilities()\n");
	
	*piCount = 0;

	if (m_mt.majortype == MEDIATYPE_Audio)
	{
		*piCount = 1;
		*piSize = sizeof(AUDIO_STREAM_CONFIG_CAPS);
		return S_OK;
	}
	  
	return S_OK;
}

STDMETHODIMP CVirtualStreamSourceOutputPin::GetStreamCaps(int iIndex, AM_MEDIA_TYPE **ppMediaType, BYTE *pSCC)
{
	FilterTrace("CVirtualStreamSourceOutputPin::GetStreamCaps()\n");
	
	if(iIndex < 0)
		return E_INVALIDARG;
	if(iIndex > 0)
		return S_FALSE;
	if(pSCC == NULL)
		return E_POINTER;

    (*ppMediaType) = ::CreateMediaType((const AM_MEDIA_TYPE*)&m_mt);  

	if (*ppMediaType == NULL) 
		return E_OUTOFMEMORY;

	WAVEFORMATEX* pMainAudioFormat = (WAVEFORMATEX*)(m_mt.pbFormat);

	AUDIO_STREAM_CONFIG_CAPS* pASCC = (AUDIO_STREAM_CONFIG_CAPS*) pSCC;
	ZeroMemory(pASCC, sizeof(AUDIO_STREAM_CONFIG_CAPS)); 

	// Set the audio capabilities
	pASCC->guid = MEDIATYPE_Audio;
	pASCC->ChannelsGranularity = pMainAudioFormat->nChannels;
	pASCC->MaximumChannels = pMainAudioFormat->nChannels;
	pASCC->MinimumChannels = pMainAudioFormat->nChannels;
	pASCC->MaximumSampleFrequency = pMainAudioFormat->nSamplesPerSec;
	pASCC->BitsPerSampleGranularity = pMainAudioFormat->wBitsPerSample;
	pASCC->MaximumBitsPerSample = pMainAudioFormat->wBitsPerSample;
	pASCC->MinimumBitsPerSample = pMainAudioFormat->wBitsPerSample;
	pASCC->MinimumSampleFrequency = pMainAudioFormat->nSamplesPerSec;
	pASCC->SampleFrequencyGranularity = pMainAudioFormat->nSamplesPerSec;

	return S_OK;
}

STDMETHODIMP CVirtualStreamSourceOutputPin::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
	// FilterTrace("CVirtualStreamSourceOutputPin::NonDelegatingQueryInterface()\n");

	if (riid == IID_IAMStreamConfig) 
		return GetInterface((IAMStreamConfig *) this, ppv);
	if (riid == IID_IKsPropertySet) 
		return GetInterface((IKsPropertySet *) this, ppv);

	if (m_mt.majortype == MEDIATYPE_Audio)
		if (riid == IID_IAMBufferNegotiation) 
			return GetInterface((IAMBufferNegotiation *) this, ppv);
	  
	// FilterTrace("CVirtualStreamSourceOutputPin::NonDelegatingQueryInterface() - UNSUP INTF\n");
	return CSourceStream::NonDelegatingQueryInterface(riid, ppv);
}
    
HRESULT CVirtualStreamSourceOutputPin::GetMediaType(CMediaType *pMediaType)
{
	FilterTrace("CVirtualStreamSourceOutputPin::GetMediaType()\n");
    CAutoLock lock(m_pFilter->pStateLock());
  
	ZeroMemory(pMediaType, sizeof(CMediaType));
  	 
	pMediaType->InitMediaType();
	pMediaType->Set(m_mt);
	return S_OK;
}

HRESULT CVirtualStreamSourceOutputPin::CheckMediaType(const CMediaType *pMediaType)
{
	FilterTrace("CVirtualStreamSourceOutputPin::CheckMediaType()\n");
	return CSourceStream::CheckMediaType(pMediaType);
}

HRESULT CVirtualStreamSourceOutputPin::GetMediaType(int iPosition, CMediaType *pMediaType)
{
	FilterTrace("CVirtualStreamSourceOutputPin::GetMediaType()\n");
	return CSourceStream::GetMediaType(iPosition, pMediaType);
}

HRESULT CVirtualStreamSourceOutputPin::DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pRequest)
{
	FilterTrace("CVirtualStreamSourceOutputPin::DecideBufferSize()\n");
 
	CAutoLock cAutoLock(m_pFilter->pStateLock());
	ATLASSERT(pAlloc);
	ATLASSERT(pRequest);
 
	HRESULT hr = S_OK;

	pRequest->cbAlign = m_allocProperties.cbAlign;
	pRequest->cbBuffer = m_allocProperties.cbBuffer;
	pRequest->cbPrefix = m_allocProperties.cbPrefix;
	pRequest->cBuffers = m_allocProperties.cBuffers;
	
	// Ask the allocator to reserve us some sample memory, NOTE the function
	// can succeed (that is return NOERROR) but still not have allocated the
	// memory that we requested, so we must check we got whatever we wanted

	ALLOCATOR_PROPERTIES Actual;

	if (FAILED(hr = pAlloc->SetProperties(pRequest, &Actual)))
		return hr;

	// Is this allocator unsuitable
	if (Actual.cbBuffer < pRequest->cbBuffer) 
		return E_FAIL;

	return S_OK;
}

int nFrameRate = 0;
 
HRESULT CVirtualStreamSourceOutputPin::FillBuffer(IMediaSample *pSample)
{
	HRESULT hr = S_OK;
	 
	if (!m_bRunning)
	{
		FilterTrace("CVirtualStreamSourceOutputPin::FillBuffer(STREAM-NOT-RUNNING)\n");
		DisconnectPipe();
		return S_FALSE;
	}

	if (!m_bConnected)
		ConnectToSinkPin();

	if (m_bConnected)
	{
		DWORD cbRead = 0;
		MediaSampleInfo mediaSampleInfo;
		ZeroMemory(&mediaSampleInfo,  sizeof(MediaSampleInfo));

		// Read from the pipe. 
		BOOL fSuccess = ReadFile( 
			m_hPipe,    // pipe handle 
			(BYTE*)&mediaSampleInfo,    // buffer to receive reply 
			sizeof(MediaSampleInfo),  // size of buffer 
			&cbRead,  // number of bytes read 
			NULL);    // not overlapped 
		
		if (!fSuccess)
			return S_OK;
 
		pSample->SetActualDataLength(mediaSampleInfo.actualDataLength);
   
		if (!GetFirstSampleDelivered())
			this->m_rtStart = mediaSampleInfo.timeStart;

		mediaSampleInfo.timeStart = (mediaSampleInfo.timeStart - m_rtStart);
		mediaSampleInfo.timeEnd = (mediaSampleInfo.timeEnd - m_rtStart);
		pSample->SetTime(&mediaSampleInfo.timeStart, &mediaSampleInfo.timeEnd);

		pSample->SetMediaTime(&mediaSampleInfo.mediaTimeStart, &mediaSampleInfo.mediaTimeEnd);
		pSample->SetTime(&mediaSampleInfo.timeStart, &mediaSampleInfo.timeEnd);

		SetFirstSampleDelivered();

		if (FAILED(pSample->SetSyncPoint(mediaSampleInfo.isSyncPoint)))
			return hr;
		 
		if (FAILED(hr = pSample->SetPreroll(FALSE)))
			return hr;
  	 
		if (FAILED(hr = pSample->SetMediaType(&m_mt)))
			return hr;
 
		if (FAILED(hr = pSample->SetDiscontinuity(!GetFirstSampleDelivered())))
			return hr;

		if (mediaSampleInfo.actualDataLength > 0)
		{
			BYTE* pBuffer = NULL;
			pSample->GetPointer(&pBuffer);

			BOOL fSuccess = ReadFile( 
				m_hPipe,    // pipe handle 
				(BYTE*)pBuffer,    // buffer to receive reply 
				mediaSampleInfo.actualDataLength,  // size of buffer 
				&cbRead,  // number of bytes read 
				NULL);    // not overlapped 
		}
	}

	return hr;
}

STDMETHODIMP CVirtualStreamSourceOutputPin::Notify(IBaseFilter *pSelf, Quality q) 
{
	// FilterTrace("CVirtualStreamSourceOutputPin::Notify()\n");
	return E_NOTIMPL;
}

HRESULT CVirtualStreamSourceOutputPin::OnThreadDestroy(void)
{
	FilterTrace("CVirtualStreamSourceOutputPin::OnThreadDestroy()\n");
	m_bRunning = false;
	return CSourceStream::OnThreadDestroy();
}

HRESULT CVirtualStreamSourceOutputPin::OnThreadStartPlay(void)
{
	FilterTrace("CVirtualStreamSourceOutputPin::OnThreadCreate()\n");
	m_fFirstSampleDelivered = false;
	m_bRunning = true;
 	return CSourceStream::OnThreadStartPlay();
}
 
HRESULT CVirtualStreamSourceOutputPin::Set(REFGUID guidPropSet, DWORD dwPropID, LPVOID pInstanceData, DWORD cbInstanceData, LPVOID pPropData, DWORD cbPropData)
{
	FilterTrace("CVirtualStreamSourceOutputPin::Set()\n");
    return E_NOTIMPL;
}

HRESULT CVirtualStreamSourceOutputPin::Get(REFGUID guidPropSet, DWORD dwPropID, LPVOID pInstanceData, DWORD cbInstanceData, LPVOID pPropData, DWORD cbPropData, DWORD *pcbReturned)
{
	FilterTrace("CVirtualStreamSourceOutputPin::Get()\n");

	if (guidPropSet != AMPROPSETID_Pin) 
		return E_PROP_SET_UNSUPPORTED;
	if (dwPropID != AMPROPERTY_PIN_CATEGORY)
		return E_PROP_ID_UNSUPPORTED;
	if (pPropData == NULL && pcbReturned == NULL)
		return E_POINTER;
	if (pcbReturned)
		*pcbReturned = sizeof(GUID);
	if (pPropData == NULL)  // Caller just wants to know the size.
		return S_OK;
	if (cbPropData < sizeof(GUID)) // The buffer is too small.
		return E_UNEXPECTED;
	
	*(GUID *)pPropData = PIN_CATEGORY_CAPTURE;
    return S_OK;
}

HRESULT CVirtualStreamSourceOutputPin::QuerySupported(REFGUID guidPropSet, DWORD dwPropID, DWORD *pTypeSupport)
{
	FilterTrace("CVirtualStreamSourceOutputPin::QuerySupported()\n");

    if (guidPropSet != AMPROPSETID_Pin)
		return E_PROP_SET_UNSUPPORTED;

    if (dwPropID != AMPROPERTY_PIN_CATEGORY)
		return E_PROP_ID_UNSUPPORTED;

    if (pTypeSupport)
		*pTypeSupport = KSPROPERTY_SUPPORT_GET;

    return S_OK;
}

HRESULT CVirtualStreamSourceOutputPin::ConnectToSinkPin(void)
{
	FilterTrace("CVirtualStreamSource::CreateOutputPins()\n");

	if (m_hPipe != NULL)
		DisconnectPipe();

	m_hPipe = NULL;

	// m_allocProperties
	BOOL   fSuccess = FALSE; 
	m_bConnected = false;

	// Create the sink pipe with the properly allocated buffer size
	TCHAR lpszPipename[MAX_PATH];
	memset(lpszPipename, 0, MAX_PATH);
	_stprintf(lpszPipename, L"\\\\%s\\pipe\\%s", PIPEHOST, m_pName);
	FilterTrace("CVirtualStreamSourceOutputPin::CreateOutputPins() ==> Pipe [%S]\n", lpszPipename);

	memset(&m_ol, 0, sizeof(m_ol));
	m_ol.hEvent = CreateEvent(NULL, TRUE, FALSE, lpszPipename);  

	while (1) 
	{ 
		FilterTrace("CVirtualStreamSourceOutputPin::CreateOutputPins() ==> Connecting to Pipe [%S]\n", lpszPipename);

		m_hPipe = CreateFile( 
				lpszPipename,   // pipe name 
				GENERIC_READ |  // read and write access 
				GENERIC_WRITE, 
				0,              // no sharing 
				NULL,           // default security attributes
				OPEN_EXISTING,  // opens existing pipe 
				0,              // default attributes 
				NULL);          // no template file 

		// Break if the pipe handle is valid. 
		if (m_hPipe != INVALID_HANDLE_VALUE) 
			break; 
 
		// Exit if an error other than ERROR_PIPE_BUSY occurs. 
 
		if (GetLastError() != ERROR_PIPE_BUSY) 
		{
			FilterTrace( "CVirtualStreamSourceOutputPin::CreateOutputPins() ==> Could not open pipe. ErrorCode=%d\n", GetLastError() ); 
			return E_FAIL;
		}
 
		// All pipe instances are busy, so wait for 20 seconds. 
 
		if ( ! WaitNamedPipe(lpszPipename, 2000)) 
		{ 
			FilterTrace("CVirtualStreamSourceOutputPin::CreateOutputPins() ==> Could not open pipe: 2 second wait timed out.\n"); 
			return E_FAIL;
		} 
	}

	// The pipe connected; change to message-read mode. 
	DWORD dwMode = PIPE_READMODE_MESSAGE; 
	fSuccess = SetNamedPipeHandleState( 
		m_hPipe,    // pipe handle 
		&dwMode,  // new pipe mode 
		NULL,     // don't set maximum bytes 
		NULL);    // don't set maximum time 
   
	if (!fSuccess) 
	{
		FilterTrace( "CVirtualStreamSourceOutputPin::CreateOutputPins() ==> SetNamedPipeHandleState failed. ErrorCode=%d\n", GetLastError() ); 
		return E_FAIL;
	}

	m_bConnected = true;

	return S_OK;
}

HRESULT CVirtualStreamSourceOutputPin::DisconnectPipe(void)
{
	// Close pipe handle
	if (m_hPipe != NULL)
	{
		TCHAR lpszPipename[MAX_PATH];
		memset(lpszPipename, 0, MAX_PATH);
		_stprintf(lpszPipename, L"\\\\%s\\pipe\\%s", PIPEHOST, m_pName);
		FilterTrace( "CVirtualStreamSourceOutputPin::DisconnectPipe() ==> Closing Pipe [%S]\n", lpszPipename); 
		CloseHandle(m_hPipe); 
		m_hPipe = NULL;
	}

	return S_OK;
}