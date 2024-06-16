#pragma once

#include <vector>
#include "stdafx.h" 
#include "VirtualStreamSinkInputPin.h"
#include "global.h"
#include <time.h>

class CVirtualStreamSink;
class CVirtualStreamSinkOutputPin;
class CVirtualStreamSinkInputPin;

MIDL_INTERFACE("3C0CA9D9-C468-42DE-A7D5-FBF61CD31F15")
IVirtualStreamSink : public IUnknown
{
	public:
		virtual HRESULT STDMETHODCALLTYPE SetLogFile(__in_opt LPCOLESTR szLogFilePath) = 0;
		virtual HRESULT STDMETHODCALLTYPE SetStreamName(__in_opt LPCOLESTR szStreamName) = 0;
};

class CVirtualStreamSink: public CCritSec, public CBaseFilter, ISpecifyPropertyPages, IVirtualStreamSink
{
    friend class CVirtualStreamSinkInputPin;
    	 
	private:
		std::vector<CBasePin*> m_pinList;			// list of pins
		IMemAllocator *m_pAllocator;				// sample allocator

		HANDLE m_hPipe;
		BOOL m_fConnected;
		HANDLE m_hThread;
		DWORD m_dwThreadId;
		BOOL m_bAlive;
		TCHAR m_szStreamName[1024];
		bool m_bServerRunning;
		bool m_bConnected;
		TCHAR m_lpszPipename[1024];
 
 	public:

		DECLARE_IUNKNOWN;
		 
		CVirtualStreamSink(TCHAR *pName,LPUNKNOWN pUnk,HRESULT *hr);
		~CVirtualStreamSink();
 
		// Override virtual pin calls
		virtual CBasePin *GetPin(int n);
		virtual int GetPinCount();
		 
		// internal pin management calls 
		void RemovePin(CBasePin* pPin);
		HRESULT AddInputPin(void);
		void InitializePipe();

		// Function needed for the class factory
		static CUnknown * WINAPI CreateInstance(LPUNKNOWN pUnk, HRESULT *phr);
 
		// Override base pin functions 
		STDMETHODIMP Run(REFERENCE_TIME tStart);
		STDMETHODIMP Pause();
		STDMETHODIMP Stop();
		STDMETHODIMP GetPages(CAUUID *pPages);
		STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);
    	
		// IStreamBuffer implementation

		static DWORD WINAPI InstanceThread(LPVOID lpvParam);
		STDMETHODIMP PipeOutMediaTypes();
		void StopServer();

		virtual HRESULT STDMETHODCALLTYPE SetLogFile(LPCOLESTR szLogFilePath)
		{
			SetTraceFile(szLogFilePath);
			return S_OK;
		}

		virtual HRESULT STDMETHODCALLTYPE SetStreamName(LPCOLESTR szStreamName)
		{
			FilterTrace("CVirtualStreamSink::SetStreamName() [%S]\n", szStreamName);
			USES_CONVERSION;
			_tcscpy(m_szStreamName, ::OLE2CT(szStreamName));
			 
			for(long i = 0; i < (long)m_pinList.size(); i++)
				RemovePin(m_pinList[i]);
 
			m_pinList.clear();
			AddInputPin();
			 
			if (m_hPipe == NULL)
				InitializePipe();

			return S_OK;
		}
};

 
