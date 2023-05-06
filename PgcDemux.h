// PgcDemux.h : main header file for the PGCDEMUX application
//

#if !defined(AFX_PGCDEMUX_H__968EF9BC_A37B_4FE7_AABB_A6296A6D41E4__INCLUDED_)
#define AFX_PGCDEMUX_H__968EF9BC_A37B_4FE7_AABB_A6296A6D41E4__INCLUDED_

#include <string>
#include <vector>
#include <stdint.h>

#ifndef _WIN32
typedef uint32_t DWORD;
typedef bool BOOL;
typedef int64_t __int64;
#define TRUE true
#define FALSE false
#define _stati64 stat64
#else /* _WIN32 */
#include <windows.h>
#endif /* _WIN32 */


#define PGCDEMUX_VERSION "1.2.0.5"
#define MAXLENGTH  20*1024*1024
#define MAX_PGC  32768
#define MAX_LU   100
#define MAX_MPGC  32768
#define MODUPDATE 100
#define TITLES 1
#define MENUS 0
#define MAXLOOKFORAUDIO 10000  // Max number of explored packs in audio delay check

#define UNK 0
#define WAV 1
#define AC3 2
#define DTS 3
#define MP1 4
#define MP2 5
#define DDS 6

#define PGCMODE 0
#define VIDMODE 1
#define CIDMODE 2

typedef unsigned char uchar;

enum class Vid : int {};
enum class Cid : int {};

typedef struct ADT_CELL_LIST
{
    Vid VID;
	Cid CID;
	int iSize;
	int iIniSec,iEndSec;
	DWORD dwDuration;
} ADT_CELL_LIST;

typedef struct ADT_VID_LIST
{
    Vid VID;
	int iSize;
	int nCells;
	DWORD dwDuration;
} ADT_VID_LIST;

/////////////////////////////////////////////////////////////////////////////
// CPgcDemuxApp:
// See PgcDemux.cpp for the implementation of this class
//

class CPgcDemuxApp
{
public:
	CPgcDemuxApp();

	uchar	* m_pIFO;
	uchar	m_buffer[2050];

	bool	m_bInProcess, m_bAbort, m_bCLI;
	int		m_iRet, nResponse;

	BOOL	m_bCheckIFrame;
	BOOL	m_bCheckVideoPack;
	BOOL	m_bCheckAudioPack;
	BOOL	m_bCheckNavPack;
	BOOL	m_bCheckSubPack;
	BOOL	m_bCheckLBA;

	BOOL	m_bCheckAud;
	BOOL	m_bCheckSub;
	BOOL	m_bCheckVid;
	BOOL	m_bCheckVob;
	BOOL	m_bCheckVob2;
	BOOL	m_bCheckLog;
	BOOL	m_bCheckCellt;
	BOOL	m_bCheckEndTime;

        std::string	m_csInputIFO;
	std::string	m_csInputPath;
	std::string	m_csOutputPath;
	int		m_nPGCs;
	int		m_iIFOlen;
	int		m_nSelPGC;
	int		m_nSelAng;
	Vid		m_nVid;
	Cid		m_nCid;
	int		m_iMode;
	__int64	m_i64OutputLBA;
	int		m_nVobout,m_nVidout,m_nCidout;
	int		m_nCurrVid;		// Used to write in different VOB files, one per VID
	int		m_iDomain;
	int		m_nVidPacks,m_nAudPacks,m_nSubPacks,m_nNavPacks,m_nPadPacks,m_nUnkPacks;
	bool	m_bVMGM;
	int		m_nTotalFrames;
	bool	bNewCell;
	int		m_nLastVid,m_nLastCid;

	using CellList = std::vector<ADT_CELL_LIST>;
	CellList m_AADT_Cell_list;
	CellList m_MADT_Cell_list;
        std::vector<ADT_VID_LIST>	m_AADT_Vid_list;
        std::vector<ADT_VID_LIST>	m_MADT_Vid_list;

	int		m_iVTS_PTT_SRPT,m_iVTS_PGCI,m_iVTS_C_ADT;
    int		m_iVTS_VOBU_ADMAP,m_iVTS_TMAPTI;
	int		m_iVTSM_PGCI,m_iVTSM_C_ADT,m_iVTSM_VOBU_ADMAP;
	int		m_iVTS_PGC[MAX_PGC];
	int		m_C_PBKT[MAX_PGC];
	int		m_C_POST[MAX_PGC];
	int		m_nCells[MAX_PGC],m_nAngles[MAX_PGC];

	int		m_iVTSM_LU[MAX_LU],	m_nIniPGCinLU[MAX_LU],m_nPGCinLU[MAX_LU];
	int		m_iMENU_PGC[MAX_MPGC],m_M_C_PBKT[MAX_MPGC],m_M_C_POST[MAX_MPGC];
	int		m_nMCells[MAX_MPGC],m_nLU_MPGC[MAX_MPGC];

	DWORD	m_dwDuration[MAX_PGC],m_dwMDuration[MAX_MPGC];

	__int64	m_i64VOBSize[10];
	int		m_nVobFiles;

	FILE	*fsub[32],*faud[8],*fvid, *fvob;
	int		m_audfmt[8];
	std::string	m_csAudname[8];
	int		m_iFirstSubPTS[32],m_iFirstAudPTS[8],m_iFirstVidPTS,m_iFirstNavPTS0;
	int		m_iAudIndex[8];
	int		m_iSubPTS,m_iAudPTS,m_iVidPTS,m_iNavPTS0_old, m_iNavPTS0, m_iNavPTS1_old, m_iNavPTS1;
	int		m_iOffsetPTS;

	int		nLU,nAbsPGC;
	int		m_nLUs,m_nMPGCs;

// Only in PCM
	int		nbitspersample[8],nchannels[8],fsample[8];

public:
	virtual BOOL InitInstance(int argc, char *argv[]);


	virtual void UpdateProgress(int nPerc);
	virtual int ExitInstance();
	virtual BOOL ParseCommandLine(int argc, char *argv[]);
	virtual int ReadIFO();
	virtual int PgcDemux (int nPGC, int nAng);
	virtual int PgcMDemux(int nPGC);
	virtual int VIDDemux (int nVid);
	virtual int VIDMDemux(int nVid);
	virtual int CIDDemux (int nCell);
	virtual int CIDMDemux(int nCell);
	virtual void demuxvideo(uchar* buffer);
	virtual void demuxaudio(uchar* buffer, int nBytesOffset);
	virtual void demuxsubs(uchar* buffer);
	virtual void WritePack(uchar* buffer);
	virtual void CloseAndNull();
	virtual int  check_sub_open (uchar i);
	virtual int  check_aud_open (uchar i);
	virtual int  ProcessPack(bool bWrite);
	virtual void OutputLog(int nItem, int nAng, int iDomain);
	virtual int InsertCell (ADT_CELL_LIST myADT_Cell, int iDomain);
	virtual void FillDurations();
	virtual void IniDemuxGlobalVars();
	virtual int OpenVideoFile();
	virtual int GetAudHeader(uchar* buffer);



};


/////////////////////////////////////////////////////////////////////////////

#endif // !defined(AFX_PGCDEMUX_H__968EF9BC_A37B_4FE7_AABB_A6296A6D41E4__INCLUDED_)
