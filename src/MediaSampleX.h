#pragma once
#include "global.h"

struct MediaSampleInfo
{
	long actualDataLength;
	LONGLONG mediaTimeStart;
	LONGLONG mediaTimeEnd;
	LONGLONG timeStart;
	LONGLONG timeEnd;
	LONGLONG timeOffset;
	LONGLONG mediaTimeOffset;
	LONG size;
	BOOL isDiscontinuity;
	BOOL isPreroll;
	BOOL isSyncPoint;
	AM_SAMPLE2_PROPERTIES sampleProperties;
};

class CMediaSampleX
{
	public:
		int m_trackId;
		long m_actualDataLength;
		LONGLONG m_mediaTimeStart;
		LONGLONG m_mediaTimeEnd;
		LONGLONG m_timeStart;
		LONGLONG m_timeEnd;
		LONG m_size;
		BOOL m_isDiscontinuity;
		BOOL m_isPreroll;
		bool m_isSyncPoint;
		AM_SAMPLE2_PROPERTIES m_sampleProperties;
		BYTE* m_pBuffer;
  
	public:
		CMediaSampleX(int trackId, IMediaSample* pMediaSample)
		{
			m_trackId = trackId;
			m_actualDataLength = pMediaSample->GetActualDataLength();
			m_size = pMediaSample->GetSize();
			m_isDiscontinuity = (pMediaSample->IsDiscontinuity() == S_OK);
			m_isPreroll = (pMediaSample->IsPreroll() == S_OK);
			m_isSyncPoint = (pMediaSample->IsSyncPoint() == S_OK);
			pMediaSample->GetMediaTime(&m_mediaTimeStart, &m_mediaTimeEnd);
			pMediaSample->GetTime(&m_timeStart, &m_timeEnd);
			m_pBuffer = NULL;
			if (pMediaSample->GetActualDataLength() > 0)
			{ 
				m_pBuffer = (BYTE*)malloc(m_actualDataLength);

				if (m_pBuffer != NULL)
				{
					BYTE* pBuffer = NULL;

					if (pMediaSample->GetPointer(&pBuffer) == S_OK)
						memcpy(m_pBuffer, pBuffer, m_actualDataLength);
				}
				else
				{
					// Drop the sample if memory cannot be allocated
					m_actualDataLength = 0;
				}
			}
		}
		~CMediaSampleX()
		{
			free(m_pBuffer);
		}

		void GetMediaSampleInfo(MediaSampleInfo* pMediaSampleInfo)
		{
			ZeroMemory(pMediaSampleInfo,  sizeof(MediaSampleInfo));
			pMediaSampleInfo->actualDataLength = m_actualDataLength;
			pMediaSampleInfo->size = m_size;
			pMediaSampleInfo->isDiscontinuity = m_isDiscontinuity;
			pMediaSampleInfo->isPreroll = m_isPreroll;
			pMediaSampleInfo->isSyncPoint = m_isSyncPoint;
			pMediaSampleInfo->mediaTimeStart = m_mediaTimeStart;
			pMediaSampleInfo->mediaTimeEnd = m_mediaTimeEnd;
			pMediaSampleInfo->timeStart = m_timeStart;
			pMediaSampleInfo->timeEnd = m_timeEnd;
		}

		BYTE* GetBuffer()
		{
			return m_pBuffer;
		}

		long GetBufferLength()
		{
			return m_actualDataLength;
		}

		LONGLONG GetStartTime()
		{
			return m_timeStart;
		}

		LONGLONG GetEndTime()
		{
			return m_timeEnd;
		}

		int GetTrackId()
		{
			return m_trackId;
		}

		bool IsSyncPoint()
		{
			return m_isSyncPoint;
		}

};
