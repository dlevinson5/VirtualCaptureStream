#pragma once

#include "VirtualStreamSource.h"
#include <queue>

class CVirtualStreamSourceOutputPin : public CSourceStream, IAMStreamConfig, IKsPropertySet, IAMBufferNegotiation
{
	private:
		HANDLE			m_hPipe;
		BOOL			m_bConnected;
		BOOL			m_fFirstSampleDelivered;
		OVERLAPPED		m_ol;
		REFERENCE_TIME  m_rtStart;
		REFERENCE_TIME  m_mtStart;
		REFERENCE_TIME  m_rtOffset;
		BOOL			m_bRunning;

		ALLOCATOR_PROPERTIES m_allocProperties;
 
	public:
		CVirtualStreamSourceOutputPin(HRESULT *phr, CVirtualStreamSource* pFilter, LPCTSTR szName, ALLOCATOR_PROPERTIES allocProps);
		~CVirtualStreamSourceOutputPin(void);
  
	public:
		DECLARE_IUNKNOWN;
 
		STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);
 
		// IAMStreamConfig
        STDMETHODIMP SetFormat(AM_MEDIA_TYPE *pmt);
        STDMETHODIMP GetFormat(AM_MEDIA_TYPE **ppmt);
        STDMETHODIMP GetNumberOfCapabilities(int *piCount,int *piSize);
        STDMETHODIMP GetStreamCaps(int iIndex, AM_MEDIA_TYPE **ppmt, BYTE *pSCC);
		STDMETHODIMP Notify(IBaseFilter *pSelf, Quality q);

		// IKsPropertySet stuff - to tell the world we are a "preview" type pin
		STDMETHODIMP Set(REFGUID guidPropSet, DWORD dwPropID, LPVOID pInstanceData, DWORD cbInstanceData, LPVOID pPropData, DWORD cbPropData);
		STDMETHODIMP Get(REFGUID guidPropSet, DWORD dwPropID, LPVOID pInstanceData, DWORD cbInstanceData, LPVOID pPropData, DWORD cbPropData, DWORD *pcbReturned);
		STDMETHODIMP QuerySupported(REFGUID guidPropSet, DWORD dwPropID, DWORD *pTypeSupport);

		// IAMBufferNegotiation  
        STDMETHODIMP SuggestAllocatorProperties(const ALLOCATOR_PROPERTIES *pprop);
        STDMETHODIMP GetAllocatorProperties(ALLOCATOR_PROPERTIES *pprop);

		bool CreateMediaType(DWORD biCompression, LONG biBitCount, LONG dwHeight, LONG dwWidth, CMediaType& mediaType);

		// Virtual overrides
		virtual HRESULT CheckMediaType(const CMediaType *pMediaType);
		virtual HRESULT GetMediaType(int iPosition, CMediaType *pMediaType);   
		virtual HRESULT GetMediaType(CMediaType *pMediaType);
		virtual HRESULT DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pRequest);
		virtual HRESULT FillBuffer(IMediaSample *pSample);
		virtual HRESULT OnThreadStartPlay(void);
		virtual HRESULT OnThreadDestroy(void);
 
		// Returns a pointer to the parent filter
		CVirtualStreamSource* GetFilter(void) { return ((CVirtualStreamSource*)m_pFilter); }

		// Reset the pin state
		HRESULT Reset(void);
		HRESULT Stop(void);

		// Sets a flag indicating that the first sample was delivered to the pin
		void SetFirstSampleDelivered() { m_fFirstSampleDelivered = true; }
		// Sets a flag indicating that the first sample was delivered to the pin
		void UnSetFirstSampleDelivered() { m_fFirstSampleDelivered = false;  }
		// Gets flag indicating the first sample was delivered to the pin
		bool GetFirstSampleDelivered() { return m_fFirstSampleDelivered==TRUE; }

		HRESULT ConnectToSinkPin(void);
		HRESULT DisconnectPipe(void);
};