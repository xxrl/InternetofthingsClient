#ifndef __WAV_OUT_H__
#define __WAV_OUT_H__

#pragma once
#include <afxmt.h>
#include <mmsystem.h>
#include <list>
using namespace std;

#define BUFFER_SIZE		(32)
#pragma comment(lib,"Winmm")

//#define MAX_AU_SIZE (2000)//这里要改，，！！！！一定是对方传过来的数据的大小



//#define BLOCK_SIZE		(MAX_AU_SIZE + sizeof(WAVEHDR))


class CWaveOut
{
public:
	static void __stdcall mywaveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
	{
		if (uMsg == WOM_DONE)
		{
			WAVEHDR *pWavhdr = (WAVEHDR *)dwParam1;
			CWaveOut *pWvo = (CWaveOut *)dwInstance;
			pWvo->m_cs.Lock();
			pWvo->m_buf_free.push_back((char *)pWavhdr);
			pWvo->m_cs.Unlock();
		}
	}
	//char szTempBuff[2048];
	//int m_nTemp;
public:
	WAVEFORMATEX	m_waveFormat;
	HWAVEOUT		m_hWavOut;

public:
	char*			m_buf[BUFFER_SIZE];// [BLOCK_SIZE];
	list<char*>		m_buf_free;
	CCriticalSection m_cs;
	

	CWaveOut()
	{
		m_hWavOut = NULL;
	};
	virtual ~CWaveOut()
	{
		Close();
	};

	BOOL Open(int bufsize, int nChannels, int nSamplesPerSec, int wBitsPerSample, int cbSize=0)
	{
		if (m_hWavOut != NULL)
		{
			return TRUE;
		}

		/*

		TWaveFormatEx = packed record
		wFormatTag: Word;       {指定格式类型; 默认 WAVE_FORMAT_PCM = 1;}
		nChannels: Word;        {指出波形数据的通道数; 单声道为 1, 立体声为 2}
		nSamplesPerSec: DWORD;  {指定样本速率(每秒的样本数)}
		nAvgBytesPerSec: DWORD; {指定数据传输的平均速率(每秒的字节数)}
		nBlockAlign: Word;      {指定块对齐(单位字节), 块对齐是数据的最小单位}
		wBitsPerSample: Word;   {采样大小(字节)}
		cbSize: Word;           {附加信息大小; PCM 格式没这个字段}
		end;
		{16 位立体声 PCM 的块对齐是 4 字节(每个样本2字节, 2个通道)}
		*/
		m_waveFormat.wFormatTag = WAVE_FORMAT_PCM;
		m_waveFormat.nChannels = nChannels;// 1;
		m_waveFormat.nSamplesPerSec = nSamplesPerSec;// 8000;
		m_waveFormat.wBitsPerSample = wBitsPerSample;// 16;
		m_waveFormat.nAvgBytesPerSec = wBitsPerSample*nChannels*nSamplesPerSec / 8;// 16000;
		m_waveFormat.nBlockAlign = wBitsPerSample*nChannels / 8;// 2;
		m_waveFormat.cbSize = cbSize;// 0;

		/*
		waveOutOpen(
		lphWaveOut: PHWaveOut;   {用于返回设备句柄的指针; 如果 dwFlags=WAVE_FORMAT_QUERY, 这里应是 nil}
		uDeviceID: UINT;         {设备ID; 可以指定为: WAVE_MAPPER, 这样函数会根据给定的波形格式选择合适的设备}
		lpFormat: PWaveFormatEx; {TWaveFormat 结构的指针; TWaveFormat 包含要申请的波形格式}
		dwCallback: DWORD        {回调函数地址或窗口句柄; 若不使用回调机制, 设为 nil}
		dwInstance: DWORD        {给回调函数的实例数据; 不用于窗口}
		dwFlags: DWORD           {打开选项}
		)
		*/
		MMRESULT nRet = waveOutOpen(&m_hWavOut, WAVE_MAPPER, &m_waveFormat, (DWORD)mywaveOutProc, (DWORD) this, CALLBACK_FUNCTION);
		if (nRet != MMSYSERR_NOERROR)
		{
			return FALSE;
		}


		for (int i = 0; i < BUFFER_SIZE; ++i)
		{
			m_buf[i] = new char[bufsize + sizeof(WAVEHDR)];


			WAVEHDR *pWavhdr = (WAVEHDR *)m_buf[i];
			pWavhdr->dwBufferLength = bufsize;
			pWavhdr->lpData = m_buf[i] + sizeof(WAVEHDR);
			pWavhdr->dwFlags = 0;

			waveOutPrepareHeader(m_hWavOut, pWavhdr, sizeof(WAVEHDR));
			m_buf_free.push_back((char *)pWavhdr);
		}


		//	m_nTemp = 0;
		return TRUE;
	};
	BOOL Close()
	{
		if (m_hWavOut != NULL)
		{
			while (m_buf_free.size() != BUFFER_SIZE)
				Sleep(80);
			m_buf_free.clear();
			waveOutPause(m_hWavOut);
			waveOutReset(m_hWavOut);

			for (int i = 0; i < BUFFER_SIZE; ++i)
			{
				WAVEHDR *pWavhdr = (WAVEHDR *)m_buf[i];
				waveOutUnprepareHeader(m_hWavOut, pWavhdr, sizeof(WAVEHDR));
				delete[]m_buf[i];
			}

			waveOutClose(m_hWavOut);
			m_hWavOut = NULL;

		}

		return TRUE;
	}
	;

	int input(unsigned char *buf , int videoLen)
	{
		size_t nSize = m_buf_free.size();
		if (nSize == 0)
		{
			TRACE("!!!~~~~~### %d - %d\n", videoLen, m_buf_free.size());
			return 0;
		}

		// 	ASSERT(videoLen <= MAX_AU_SIZE);
		// 	if ( videoLen > MAX_AU_SIZE )
		// 	{
		// 		TRACE("!!!videoLen error %d - %d\n",videoLen,m_buf_free.size());
		// 		return 0;
		// 	}

		m_cs.Lock();
		if (m_buf_free.size() <= 0)
		{
			m_cs.Unlock();
			return 0;
		}
		WAVEHDR *pWavehdr = (WAVEHDR *)m_buf_free.front();
		m_buf_free.pop_front();
		m_cs.Unlock();


		//memset(pWavehdr->lpData, sizeof(pWavehdr->lpData), 0);
		memcpy(pWavehdr->lpData, buf, videoLen);
		waveOutWrite(m_hWavOut, pWavehdr, sizeof(WAVEHDR));



		return 0;
	};
	BOOL inlineis_start() { return (m_hWavOut != NULL); }
private:


};

#endif 


