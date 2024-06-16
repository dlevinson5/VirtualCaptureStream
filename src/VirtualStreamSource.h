#pragma once

#include <source.h>
#include "global.h"

class CVirtualStreamSourceOutputPin;

MIDL_INTERFACE("E35D2EF8-FAE3-4C18-8985-950889AF920D")
IVirtualStreamSource : public IUnknown
{
	public:
		virtual HRESULT STDMETHODCALLTYPE SetStreamName(__in_opt LPCOLESTR szStreamName) = 0;
		virtual HRESULT STDMETHODCALLTYPE SetLogFile(__in_opt LPCOLESTR szLogFilePath) = 0;
};
  
class CVirtualStreamSource : public CSource, IVirtualStreamSource
{
	private:
		TCHAR m_szBufferFile[MAX_PATH];
		std::vector<CBasePin*> m_pinList;
		CVirtualStreamSourceOutputPin* m_pVideoPin;
		DWORD m_dwSeekingCaps;

	public:
		REFERENCE_TIME m_rtVideoRefPoint;
 
	public:
		CVirtualStreamSource(IUnknown *pUnk, HRESULT *phr);
		~CVirtualStreamSource(void);

	public:
		static CUnknown * WINAPI CreateInstance(IUnknown *pUnk, HRESULT *phr);  

		DECLARE_IUNKNOWN;

		// CBaseFilter 
		STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

		// IMediaFilter
		STDMETHODIMP GetState(DWORD dw, FILTER_STATE *pState);
		STDMETHODIMP Stop();
		STDMETHODIMP Pause();
		STDMETHODIMP Run(REFERENCE_TIME tStart);
 
	private:
		HRESULT CreateOutputPins(LPCTSTR szPipeName);

		virtual HRESULT STDMETHODCALLTYPE SetStreamName(LPCOLESTR szStreamName)
		{
			CreateOutputPins(szStreamName);
			return S_OK;
		}

		virtual HRESULT STDMETHODCALLTYPE SetLogFile(LPCOLESTR szLogFilePath)
		{
			SetTraceFile(szLogFilePath);
			return S_OK;
		}
};
