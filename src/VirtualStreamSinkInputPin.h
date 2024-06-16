#pragma once

#include "stdafx.h"
#include "MediaSampleX.h"
#include <vector>
#include <queue>

class CVirtualStreamSink;

class CVirtualStreamSinkInputPin : public CBaseInputPin
{
    friend class CVirtualStreamSinkOutputPin;
 	
	public:
		long m_trackId;
		GUID m_majorType;
		CVirtualStreamSink *m_pFilter;   
		IMemAllocator* m_pBufferAllocator; 
		CCritSec m_pinLock;
		HANDLE m_hPipe;
		HANDLE m_hThread;
		HANDLE m_hThread2;
		DWORD m_dwThreadId;
		DWORD m_dwThreadId2;
		BOOL m_bConnected;
		REFERENCE_TIME m_rtRefPoint;
		REFERENCE_TIME m_rtRefPoint2;
		BOOL m_bSyncPointHit;
		std::queue<CMediaSampleX*> m_writeBuffer;
		bool m_bWriteBuffering;
		bool m_bPipeServerRunning;
		TCHAR m_lpszPipename[1024];

	public:

		AM_MEDIA_TYPE GetConnectedMediaType()
		{
			return this->m_mt;
		}
		 
		// Constructor and destructor
		CVirtualStreamSinkInputPin(TCHAR *pObjName, CVirtualStreamSink *pTee, HRESULT *phr, LPCWSTR pPinName);
		~CVirtualStreamSinkInputPin();
 
		// Used to check the input pin connection
		HRESULT CheckMediaType(const CMediaType *pmt);
		HRESULT SetMediaType(const CMediaType *pmt);
		HRESULT BreakConnect();
		GUID    GetPinMajorType();
 
		// Reconnect outputs if necessary at end of completion
		virtual HRESULT CompleteConnect(IPin *pReceivePin);
		virtual HRESULT Active();
  
		STDMETHODIMP NotifyAllocator(IMemAllocator *pAllocator, BOOL bReadOnly);
 
		// Pass through calls downstream
		virtual STDMETHODIMP EndOfStream();
		virtual STDMETHODIMP BeginFlush();
		virtual STDMETHODIMP EndFlush();
		virtual STDMETHODIMP NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate);

		// Handles the next block of data from the stream
		virtual STDMETHODIMP Receive(IMediaSample *pSample);
 
		// Add track to the stream buffer
		STDMETHODIMP AddTrack();

		HRESULT Reset();

		static DWORD WINAPI InstanceThread(LPVOID lpvParam);
		static DWORD WINAPI InstanceWritePipeThread(LPVOID lpvParam);
		CMediaSampleX* GetNextWriteBufferSample();
		void CreateWriteBufferThread();
		void InitializePipeStream();
		void StopServer();

		void SetStreamName(LPCTSTR szStreamName)
		{
			// Update the pin name 
			wcscpy(m_pName, szStreamName);

			// Update the pin pipe name
 			memset(m_lpszPipename, 0, MAX_PATH);
			_stprintf(m_lpszPipename, L"\\\\.\\pipe\\%s", szStreamName);

			// Initialize the pipe stream
			InitializePipeStream();
		}
};