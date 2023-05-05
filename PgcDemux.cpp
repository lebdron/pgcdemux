// PgcDemux.cpp : Defines the class behaviors for the application.
//

#include <sys/types.h>
#include <sys/stat.h>
#include <iostream>
#include <algorithm>
#include <cstdio>
#include <sstream>
#include <fstream>
#include <iomanip>

#include "spdlog/spdlog.h"

#include "PgcDemux.h"

using namespace std;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern int readbuffer(uchar* buffer, FILE *in);
extern void writebuffer(uchar* buffer, FILE *out, int nbytes);
extern bool IsNav(uchar* buffer);
extern bool IsPad(uchar* buffer);
extern bool IsSynch (uchar* buffer);
extern bool IsVideo (uchar* buffer);
extern bool IsAudio (uchar* buffer);
extern bool IsAudMpeg (uchar* buffer);
extern bool IsSubs (uchar* buffer);
extern void ModifyCID (uchar* buffer, int VobId, int CellId);
extern int GetNbytes(int nNumber,uchar* address);
extern void Put4bytes(__int64 i64Number,uchar* address);
extern void MyErrorBox(char const *text);
extern void ModifyLBA (uchar* buffer, __int64 m_i64OutputLBA);
extern int readpts(uchar *buf);
extern int DurationInFrames(DWORD dwDuration);
extern DWORD AddDuration(DWORD dwDuration1, DWORD dwDuration2);
extern int getAudId(uchar * buffer);



uchar pcmheader[44] = {
	0x52, 0x49, 0x46, 0x46, 0x00, 0x00, 0x00, 0x00, 0x57, 0x41, 0x56, 0x45,	0x66, 0x6D, 0x74, 0x20,
	0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00,	0x80, 0xBB, 0x00, 0x00, 0x70, 0x17, 0x00, 0x00,
	0x04, 0x00, 0x10, 0x00,	0x64, 0x61, 0x74, 0x61, 0x00, 0x00, 0x00, 0x00 };
/*
uchar pcmheader2[58] = {
	0x52, 0x49, 0x46, 0x46, 0x58, 0x08, 0x2C, 0x00, 0x57, 0x41, 0x56, 0x45,	0x66, 0x6D, 0x74, 0x20,
	0x12, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00,	0x80, 0xBB, 0x00, 0x00, 0x00, 0x65, 0x04, 0x00,
	0x06, 0x00, 0x18, 0x00,	0x00, 0x00, 0x66, 0x61, 0x63, 0x74, 0x04, 0x00, 0x00, 0x00, 0x05, 0x56,
	0x07, 0x00, 0x64, 0x61, 0x74, 0x61, 0x1E, 0x04, 0x2C, 0x00 };
*/

////////////// AUX nibble functions used for code cleaning /////
#define hi_nib(a)	((a>>4) & 0x0f)
#define lo_nib(a)	(a & 0x0f)

/*
inline uchar hi_nib(uchar ch)
{
  return ((ch>>4) & 0x0f);
}

inline uchar lo_nib(uchar ch)
{
  return (ch & 0x0f);
}
*/
/////////////////////////////////////////////////////////////////////////////
// CPgcDemuxApp

/////////////////////////////////////////////////////////////////////////////
// CPgcDemuxApp construction

CPgcDemuxApp::CPgcDemuxApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CPgcDemuxApp object

CPgcDemuxApp theApp;

/////////////////////////////////////////////////////////////////////////////
// CPgcDemuxApp initialization

BOOL CPgcDemuxApp::InitInstance(int argc, char *argv[])
{
	unsigned i,k;
	int nSelVid,nSelCid;

	m_pIFO = NULL;

//	SetRegistryKey( "jsoto's tools" );
//	WriteProfileInt("MySection", "My Key",123);

	for (i=0;i<32;i++) fsub[i]=NULL;
	for (i=0;i<8;i++)
		faud[i]=NULL;
	fvob=fvid=NULL;

	m_bInProcess=false;
	m_bCLI=false;
	m_bAbort=false;
	m_bVMGM=false;

	m_bCheckAud=m_bCheckSub=m_bCheckLog=m_bCheckCellt=TRUE;
	m_bCheckVid=m_bCheckVob=m_bCheckVob2=m_bCheckEndTime=FALSE;

	m_bCheckIFrame=FALSE;
	m_bCheckLBA=m_bCheckVideoPack=m_bCheckAudioPack=m_bCheckNavPack=m_bCheckSubPack=TRUE;


	if ( argc > 2 )
		// CLI mode
	{
		m_bCLI=true;
		if (ParseCommandLine(argc, argv) ==TRUE)
		{
			m_bInProcess=true;
			m_iRet=ReadIFO();
			if (m_iRet==0)
			{
				if (m_iMode==PGCMODE)
				{
// Check if PGC exists in done in PgcDemux
					if (m_iDomain==TITLES)
						m_iRet=PgcDemux (m_nSelPGC,m_nSelAng);
					else
						m_iRet=PgcMDemux(m_nSelPGC);
				}
				if (m_iMode==VIDMODE)
				{
// Look for nSelVid
					nSelVid=-1;
					if (m_iDomain==TITLES)
					{
						for (k=0;k<m_AADT_Vid_list.size() && nSelVid==-1; k++)
						{
							auto const& vid = m_AADT_Vid_list[k];
							auto const duration = vid.dwDuration;
							SPDLOG_INFO("VID {:02d} ({:02d})--> {:02X}:{:02X}:{:02X}.{:02X}", k+1, vid.VID,
								duration/(256*256*256),(duration/(256*256))%256,(duration/256)%256,(duration%256) & 0x3f);
							if (m_AADT_Vid_list[k].VID==m_nVid)
								nSelVid=k;
						}
					}
					else
					{
						for (k=0;k<m_MADT_Vid_list.size() && nSelVid==-1; k++)
							if (m_MADT_Vid_list[k].VID==m_nVid)
								nSelVid=k;
					}
					if ( nSelVid==-1)
					{
						m_iRet=-1;
						SPDLOG_ERROR("Selected Vid not found!");
					}
					if (m_iRet==0)
					{
						if (m_iDomain==TITLES)
							m_iRet=VIDDemux(nSelVid);
						else
							m_iRet=VIDMDemux(nSelVid);
					}
				}
				if (m_iMode==CIDMODE)
				{
// Look for nSelVid
					nSelCid=-1;
					if (m_iDomain==TITLES)
					{
						for (k=0;k<m_AADT_Cell_list.size() && nSelCid==-1; k++)
						{
							auto const& cell = m_AADT_Cell_list[k];
							auto const duration = cell.dwDuration;
							SPDLOG_INFO("{:02d} ({:02d}/{:02d})--> {:02X}:{:02X}:{:02X}.{:02X}", k+1, cell.VID, cell.CID,
								duration/(256*256*256),(duration/(256*256))%256,(duration/256)%256,(duration%256) & 0x3f);
							if (m_AADT_Cell_list[k].VID==m_nVid && m_AADT_Cell_list[k].CID==m_nCid)
								nSelCid=k;
						}
					}
					else
					{
						for (k=0;k<m_MADT_Cell_list.size() && nSelCid==-1; k++)
							if (m_MADT_Cell_list[k].VID==m_nVid && m_MADT_Cell_list[k].CID==m_nCid)
								nSelCid=k;
					}
					if ( nSelCid==-1)
					{
						m_iRet=-1;
						SPDLOG_ERROR("Selected Vid/Cid not found!");
					}
					if (m_iRet==0)
					{
						if (m_iDomain==TITLES)
							m_iRet=CIDDemux(nSelCid);
						else
							m_iRet=CIDMDemux(nSelCid);
					}
				}


			}
			m_bInProcess=false;
		}
		else
			m_iRet=-1;

		//  return FALSE so that we exit the
		//  application, rather than start the application's message pump.
		return FALSE;
	}
        else
        {
                MyErrorBox("Missing arguments!");
                cout
                        << "pgcdemux [option1] [option2] ... [option12] <ifo_input_file> <destination_folder>" << endl
                        << "option1: [-pgc <pgcnumber>].      Selects the PGC number (from 1 to nPGCs). Default 1" << endl
                        << "option2: [-ang <angnumber>].      Selects the Angle number (from 1 to n). Default 1" << endl
                        << "option3: [-vid <vobid>].          Selects the vobid number (from 1 to n). Default 1" << endl
                        << "option4: [-cid <vobid> <cellid>]. Selects a vobid and cell (from 1 to n). Default 1" << endl
                        << "option5: {-m2v, -nom2v}. Extracts/No extracts video file. Default NO" << endl
                        << "option6: {-aud, -noaud}. Extracts/No extracts audio streams. Default YES" << endl
                        << "option7: {-sub, -nosub}. Extracts/No extracts subs streams. Default YES" << endl
                        << "option8: {-vob, -novob}. Generates a single PGC VOB. Default NO" << endl
                        << "option9: {-customvob <flags>}. Generates a custom VOB file. Flags:" << endl
                        << "          b: split VOB: one file per vob_id" << endl
                        << "          n: write nav packs" << endl
                        << "          v: write video packs" << endl
                        << "          a: write audio packs" << endl
                        << "          s: write subs packs" << endl
                        << "          i: only first Iframe" << endl
                        << "          l: patch LBA number" << endl
                        << "option10:{-cellt, -nocellt}. Generates a Celltimes.txt file. Default YES" << endl
                        << "option10:{-endt, -noendt}. Includes Last end time in Celltimes.txt. Default NO" << endl
                        << "option11:{-log, -nolog}. Generates a log file. Default YES" << endl
                        << "option12:{-menu, -title}. Domain. Default Title (except if filename is VIDEO_TS.IFO)" << endl;
                m_iRet=-1;
                return FALSE;
        }
}

void CPgcDemuxApp::UpdateProgress(int nPerc)
{
    int w = (nPerc * 40 + 50) / 100;
    string meter(w, '#');
    string spacer(40 - w, '.');
    cout << "[" << meter << spacer << "] " << nPerc << "%\r";
    cout.flush();
}


BOOL CPgcDemuxApp::ParseCommandLine(int argc, char *argv[])
{
/*
PgcDemux [option1] [option2] ... [option12] <ifo_input_file> <destination_folder>
option1: [-pgc, <pgcnumber>].      Selects the PGC number (from 1 to nPGCs). Default 1
option2: [-ang, <angnumber>].      Selects the Angle number (from 1 to n). Default 1
option3: [-vid, <vobid>].          Selects the Angle number (from 1 to n). Default 1
option4: [-cid, <vobid> <cellid>]. Selects a cell vobid (from 1 to n). Default 1
option5: {-m2v, -nom2v}. Extracts/No extracts video file. Default NO
option6: {-aud, -noaud}. Extracts/No extracts audio streams. Default YES
option7: {-sub, -nosub}. Extracts/No extracts subs streams. Default YES
option8: {-vob, -novob}. Generates a single PGC VOB. Default NO
option9: {-customvob <flags>}. Generates a custom VOB file. Flags:
           b: split VOB: one file per vob_id
		   n: write nav packs
		   v: write video packs
		   a: write audio packs
		   s: write subs packs
		   i: only first Iframe
		   l: patch LBA number
option10:{-cellt, -nocellt}. Generates a Celltimes.txt file. Default YES
option10:{-endt, -noendt}. Includes Last end time in Celltimes.txt. Default NO
option11:{-log, -nolog}. Generates a log file. Default YES
option12:{-menu, -title}. Domain. Default Title (except if filename is VIDEO_TS.IFO)
*/
	int i;
	string csPar,csPar2;
	string csAux,csAux1,csAux2;


//	m_bCheckAud=m_bCheckSub=m_bCheckLog=m_bCheckCellt=TRUE;
//	m_bCheckVid=m_bCheckVob=m_bCheckVob2=m_bCheckEndTime=FALSE;

//	m_bCheckIFrame=FALSE;
//	m_bCheckLBA=m_bCheckVideoPack=m_bCheckAudioPack=m_bCheckNavPack=m_bCheckSubPack=TRUE;


	m_nSelPGC=m_nSelAng=0;
	m_iMode=PGCMODE;
	m_iDomain=TITLES;

	if (argc < 3) return -1;

	for (i =1 ; i<(argc)-1 ; i++)
	{
		csPar=argv[i];
                transform(csPar.begin(), csPar.end(), csPar.begin(), ::tolower);

		if ( csPar=="-pgc" && argc >i+1 )
		{
			sscanf ( argv[i+1], "%d", &m_nSelPGC);
			if (m_nSelPGC <1 || m_nSelPGC >255)
			{
				MyErrorBox( "Invalid pgc number!");
				return FALSE;
			}
			m_iMode=PGCMODE;
			i++;
			m_nSelPGC--; // internally from 0 to nPGCs-1.
		}
		else if ( csPar=="-ang" && argc >i+1 )
		{
			sscanf ( argv[i+1], "%d", &m_nSelAng);
			if (m_nSelAng <1 || m_nSelAng >9)
			{
				MyErrorBox( "Invalid angle number!");
				return FALSE;
			}
			i++;
			m_nSelAng--; // internally from 0 to nAngs-1.
		}
		else if ( csPar=="-vid" && argc >i+1 )
		{
			sscanf ( argv[i+1], "%d", &m_nVid);
			if (m_nVid <1 || m_nVid >32768)
			{
				MyErrorBox( "Invalid Vid number!");
				return FALSE;
			}
			m_iMode=VIDMODE;
			i++;
		}
		else if ( csPar=="-cid" && argc >i+2 )
		{
			sscanf ( argv[i+1], "%d", &m_nVid);
			sscanf ( argv[i+2], "%d", &m_nCid);
			if (m_nVid <1 || m_nVid >32768)
			{
				MyErrorBox( "Invalid Vid number!");
				return FALSE;
			}
			if (m_nCid <1 || m_nCid >255)
			{
				MyErrorBox( "Invalid Cid number!");
				return FALSE;
			}

			m_iMode=CIDMODE;
			i+=2;
		}
		else if ( csPar=="-customvob" && argc >i+1 )
		{
			csPar2 = argv[i+1];
                        transform(csPar2.begin(), csPar2.end(), csPar2.begin(), ::tolower);

			m_bCheckVob=TRUE;
// n: write nav packs
// v: write video packs
// a: write audio packs
// s: write subs packs
// i: only first Iframe
// b: split file per vob_id
// l: Patch LBA number

			if (csPar2.find('b')!=string::npos)  m_bCheckVob2=TRUE;
			else m_bCheckVob2=FALSE;
			if (csPar2.find('v')!=string::npos)  m_bCheckVideoPack=TRUE;
			else m_bCheckVideoPack=FALSE;
			if (csPar2.find('a')!=string::npos)  m_bCheckAudioPack=TRUE;
			else m_bCheckAudioPack=FALSE;
			if (csPar2.find('n')!=string::npos)  m_bCheckNavPack=TRUE;
			else m_bCheckNavPack=FALSE;
			if (csPar2.find('s')!=string::npos)  m_bCheckSubPack=TRUE;
			else m_bCheckSubPack=FALSE;
			if (csPar2.find('i')!=string::npos)  m_bCheckIFrame=TRUE;
			else m_bCheckIFrame=FALSE;
			if (csPar2.find('l')!=string::npos)  m_bCheckLBA=TRUE;
			else m_bCheckLBA=FALSE;
			i++;
		}
		else if ( csPar=="-m2v" )  m_bCheckVid=1;
		else if ( csPar=="-vob" )  m_bCheckVob=1;
		else if ( csPar=="-aud" )  m_bCheckAud=1;
		else if ( csPar=="-sub" )  m_bCheckSub=1;
		else if ( csPar=="-log" )  m_bCheckLog=1;
		else if ( csPar=="-cellt" )  m_bCheckCellt=1;
		else if ( csPar=="-endt" )  m_bCheckEndTime=1;
		else if ( csPar=="-nom2v" )  m_bCheckVid=0;
		else if ( csPar=="-novob" )  m_bCheckVob=0;
		else if ( csPar=="-noaud" )  m_bCheckAud=0;
		else if ( csPar=="-nosub" )  m_bCheckSub=0;
		else if ( csPar=="-nolog" )  m_bCheckLog=0;
		else if ( csPar=="-nocellt" )  m_bCheckCellt=0;
		else if ( csPar=="-noendt" )  m_bCheckEndTime=0;
		else if ( csPar=="-menu" )	m_iDomain=MENUS;
		else if ( csPar=="-title" )  m_iDomain=TITLES;
	}
	m_csInputIFO=argv[(argc) -2];
	m_csOutputPath=argv[(argc) -1];

	m_csInputPath=m_csInputIFO.substr(0, m_csInputIFO.find_last_of("/\\"));

	csAux2=m_csInputIFO.substr(m_csInputIFO.find_last_of("/\\") + 1);
        transform(csAux2.begin(), csAux2.end(), csAux2.begin(), ::toupper);
	csAux1=csAux2.substr(0, 4);
	csAux=csAux2.substr(csAux2.size() - 6);
	if ( (csAux!="_0.IFO" || csAux1 != "VTS_" ) && csAux2 !="VIDEO_TS.IFO")
	{
		MyErrorBox( "Invalid input file!");
		return FALSE;
	}

	if (csAux2=="VIDEO_TS.IFO")
	{
		m_bVMGM=true;
		m_iDomain=MENUS;
	}
	else m_bVMGM=false;

	return TRUE;
}



// Exit Value
int CPgcDemuxApp::ExitInstance()
{
	if (m_pIFO!=NULL)  delete[]  m_pIFO;

        cout << endl;

	return m_iRet;

}


///////////////////////////////////////////////////////////////////////
///////////////  MAIN CODE ////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

int CPgcDemuxApp::InsertCell (ADT_CELL_LIST myADT_Cell, int iDomain)
{
	int iArraysize,i,ii;
	bool bIsHigher;

	if (iDomain==TITLES)
	{
		iArraysize=m_AADT_Cell_list.size();
		ii=iArraysize;
		for (i=0,bIsHigher=true; i<iArraysize && bIsHigher ; i++)
		{
			if (myADT_Cell.VID < m_AADT_Cell_list[i].VID )  {ii=i; bIsHigher=false;}
			else if (myADT_Cell.VID > m_AADT_Cell_list[i].VID )  bIsHigher=true;
			else
			{
				if (myADT_Cell.CID < m_AADT_Cell_list[i].CID ) {ii=i; bIsHigher=false;}
				else if (myADT_Cell.CID > m_AADT_Cell_list[i].CID )  bIsHigher=true;
			}

		}
		m_AADT_Cell_list.insert(m_AADT_Cell_list.begin() + ii, myADT_Cell);
	}
	if (iDomain==MENUS)
	{
		iArraysize=m_MADT_Cell_list.size();
		ii=iArraysize;
		for (i=0,bIsHigher=true; i<iArraysize && bIsHigher ; i++)
		{
			if (myADT_Cell.VID < m_MADT_Cell_list[i].VID ) {ii=i; bIsHigher=false;}
			else if (myADT_Cell.VID > m_MADT_Cell_list[i].VID )  bIsHigher=true;
			else
			{
				if (myADT_Cell.CID < m_MADT_Cell_list[i].CID )   {ii=i; bIsHigher=false;}
				else if (myADT_Cell.CID > m_MADT_Cell_list[i].CID )  bIsHigher=true;
			}

		}
//		if (i>0 && bIsHigher) i--;
		m_MADT_Cell_list.insert(m_MADT_Cell_list.begin() + ii, myADT_Cell);
	}
	return ii;
}

void CPgcDemuxApp::FillDurations()
{
	int iArraysize;
	int i,j,k;
	int VIDa,CIDa,VIDb,CIDb;
	bool bFound;
	int iVideoAttr, iFormat;


	iArraysize=m_AADT_Cell_list.size();

	for (i=0; i<iArraysize; i++)
	{
		VIDb=m_AADT_Cell_list[i].VID;
		CIDb=m_AADT_Cell_list[i].CID;
		for (j=0,bFound=false;j<m_nPGCs && !bFound; j++)
		{
			for (k=0;k<m_nCells[j];k++)
			{
				VIDa=GetNbytes(2,&m_pIFO[m_C_POST[j]+k*4]);
				CIDa=m_pIFO[m_C_POST[j]+k*4+3];
				if (VIDa==VIDb && CIDa==CIDb)
				{
					bFound=true;
					m_AADT_Cell_list[i].dwDuration=GetNbytes(4,&m_pIFO[m_C_PBKT[j]+0x18*k+4]);
				}
			}
		}
		if (!bFound)
		{
			iVideoAttr=m_pIFO[0x200]*256+m_pIFO[0x201];
			iFormat=(iVideoAttr & 0x1000) >> 12;
			if (iFormat == 0 ) // NTSC
				m_AADT_Cell_list[i].dwDuration=0xC0;
			else // PAL
				m_AADT_Cell_list[i].dwDuration=0x40;
		}
	}

	iArraysize=m_MADT_Cell_list.size();

	for (i=0; i<iArraysize; i++)
	{
		VIDb=m_MADT_Cell_list[i].VID;
		CIDb=m_MADT_Cell_list[i].CID;
		for (j=0,bFound=false;j<m_nMPGCs && !bFound; j++)
		{
			for (k=0;k<m_nMCells[j];k++)
			{
				VIDa=GetNbytes(2,&m_pIFO[m_M_C_POST[j]+k*4]);
				CIDa=m_pIFO[m_M_C_POST[j]+k*4+3];
				if (VIDa==VIDb && CIDa==CIDb)
				{
					bFound=true;
					m_MADT_Cell_list[i].dwDuration=GetNbytes(4,&m_pIFO[m_M_C_PBKT[j]+0x18*k+4]);
				}
			}
		}
		if (!bFound)
		{
			iVideoAttr=m_pIFO[0x100]*256+m_pIFO[0x101];
			iFormat=(iVideoAttr & 0x1000) >> 12;
			if (iFormat == 0 ) // NTSC
				m_MADT_Cell_list[i].dwDuration=0xC0;
			else // PAL
				m_MADT_Cell_list[i].dwDuration=0x40;
		}
	}

}

////////////////////////////////////////////////////////////////////////////////
/////////////  ReadIFO /////
////////////////////////////////////////////////////////////////////////////////
int CPgcDemuxApp::ReadIFO()
{
        stringstream csAux;
	string csAux2;
	int i,j,kk,nCell;
        unsigned nVIDs;
	ADT_CELL_LIST myADT_Cell;
	ADT_VID_LIST myADT_Vid;
	int nTotADT, nADT, VidADT,CidADT;
	int iArraysize;
	bool bAlready, bEndAngle;
	FILE * in;
	int iIniSec,iEndSec;
	struct  _stati64 statbuf;
	int iCat;
	int iIFOSize;


	if (_stati64 ( m_csInputIFO.c_str(), &statbuf)==0)
		iIFOSize= (int) statbuf.st_size;

	if ( iIFOSize > MAXLENGTH)
	{
		csAux.str("");
                csAux << "IFO too big " << m_csInputIFO;
		MyErrorBox (csAux.str().c_str());
		return -1;
	}

	in=fopen(m_csInputIFO.c_str(),"rb");
	if (in == NULL)
	{
		csAux.str("");
		csAux << "Unable to open " << m_csInputIFO;
		MyErrorBox (csAux.str().c_str());
		return -1;
	}

	if (m_pIFO!=NULL)  delete[]  m_pIFO;

	m_pIFO = new uchar[iIFOSize+2048];

// Read IFO


	for (i=0;!feof(in) && i< MAXLENGTH ;i++)
		m_pIFO[i]=fgetc(in);
	m_iIFOlen=i-1;
	fclose (in);

	m_AADT_Cell_list.clear();
	m_MADT_Cell_list.clear();
	m_AADT_Vid_list.clear();
	m_MADT_Vid_list.clear();


// Get Title Cells
	if (m_bVMGM)
	{
		m_iVTS_PTT_SRPT=   0;
		m_iVTS_PGCI=       0;
		m_iVTSM_PGCI=      2048*GetNbytes(4,&m_pIFO[0xC8]);
		m_iVTS_TMAPTI=     0;
		m_iVTSM_C_ADT=     2048*GetNbytes(4,&m_pIFO[0xD8]);
		m_iVTSM_VOBU_ADMAP=2048*GetNbytes(4,&m_pIFO[0xDC]);
		m_iVTS_C_ADT=      0;
		m_iVTS_VOBU_ADMAP= 0;
	}
	else
	{
		m_iVTS_PTT_SRPT=   2048*GetNbytes(4,&m_pIFO[0xC8]);
		m_iVTS_PGCI=       2048*GetNbytes(4,&m_pIFO[0xCC]);
		m_iVTSM_PGCI=      2048*GetNbytes(4,&m_pIFO[0xD0]);
		m_iVTS_TMAPTI=     2048*GetNbytes(4,&m_pIFO[0xD4]);
		m_iVTSM_C_ADT=     2048*GetNbytes(4,&m_pIFO[0xD8]);
		m_iVTSM_VOBU_ADMAP=2048*GetNbytes(4,&m_pIFO[0xDC]);
		m_iVTS_C_ADT=      2048*GetNbytes(4,&m_pIFO[0xE0]);
		m_iVTS_VOBU_ADMAP= 2048*GetNbytes(4,&m_pIFO[0xE4]);
	}
	if (m_bVMGM)
		m_nPGCs=0;
	else
		m_nPGCs=GetNbytes(2,&m_pIFO[m_iVTS_PGCI]);


// Title PGCs
	if (m_nPGCs > MAX_PGC)
	{
		csAux.str("");
		csAux << "ERROR: Max PGCs limit (" << MAX_PGC << ") has been reached.";
		MyErrorBox (csAux.str().c_str());
		return -1;
	}
	for (int k = 0; k < m_nPGCs; k++)
	{
		m_iVTS_PGC[k]=GetNbytes(4,&m_pIFO[m_iVTS_PGCI+0x04+(k+1)*8])+m_iVTS_PGCI;
		m_dwDuration[k]=(DWORD)GetNbytes(4,&m_pIFO[m_iVTS_PGC[k]+4]);

		m_C_PBKT[k]=GetNbytes(2,&m_pIFO[m_iVTS_PGC[k]+0xE8]);
		if (m_C_PBKT[k]!=0  ) m_C_PBKT[k]+=m_iVTS_PGC[k];

		m_C_POST[k]=GetNbytes(2,&m_pIFO[m_iVTS_PGC[k]+0xEA]);
		if (m_C_POST[k]!=0  ) m_C_POST[k]+=m_iVTS_PGC[k];

		m_nCells[k]=m_pIFO[m_iVTS_PGC[k]+3];


		m_nAngles[k]=1;

		for (nCell=0,bEndAngle=false; nCell<m_nCells[k] && bEndAngle==false; nCell++)
		{
			iCat=GetNbytes(1,&m_pIFO[m_C_PBKT[k]+24*nCell]);
			iCat=iCat & 0xF0;
//			0101=First; 1001=Middle ;	1101=Last
			if      (iCat == 0x50)
				m_nAngles[k]=1;
			else if (iCat == 0x90)
				m_nAngles[k]++;
			else if (iCat == 0xD0)
			{
				m_nAngles[k]++;
				bEndAngle=true;
			}
		}
		auto const duration = m_dwDuration[k];
		SPDLOG_INFO("PGC # {:02d}--> {:02X}:{:02X}:{:02X}.{:02X}", k+1,
			duration/(256*256*256),(duration/(256*256))%256,(duration/256)%256,(duration%256) & 0x3f);
	}


// Menu PGCs
	if( m_iVTSM_PGCI==0 )
		m_nLUs=0;
	else
		m_nLUs=GetNbytes(2,&m_pIFO[m_iVTSM_PGCI]);

	m_nMPGCs=0;
	if (m_nLUs > MAX_LU)
	{
                csAux.str("");
		csAux << "ERROR: Max LUs limit (" << MAX_LU << ") has been reached.";
		MyErrorBox (csAux.str().c_str());
		return -1;
	}

	for (nLU=0; nLU<m_nLUs;nLU++)
	{
  		m_iVTSM_LU[nLU]=   GetNbytes(4,&m_pIFO[m_iVTSM_PGCI+0x04+(nLU+1)*8])+m_iVTSM_PGCI;
		m_nPGCinLU[nLU]=   GetNbytes(2,&m_pIFO[m_iVTSM_LU[nLU]]);
		m_nIniPGCinLU[nLU]= m_nMPGCs;

		for (j=0; j < m_nPGCinLU[nLU]; j++)
		{
			if ((m_nMPGCs + m_nPGCinLU[nLU]) > MAX_MPGC)
			{
                                csAux.str("");
				csAux << "ERROR: Max MPGCs limit (" << MAX_MPGC << ") has been reached.";
				MyErrorBox (csAux.str().c_str());
				return -1;
			}
			nAbsPGC=j+m_nMPGCs;
			m_nLU_MPGC[nAbsPGC]=nLU;
			m_iMENU_PGC[nAbsPGC]= GetNbytes(4,&m_pIFO[m_iVTSM_LU[nLU]+0x04+(j+1)*8])+m_iVTSM_LU[nLU];

			m_M_C_PBKT[nAbsPGC]  =GetNbytes(2,&m_pIFO[m_iMENU_PGC[nAbsPGC]+0xE8]);
			if (m_M_C_PBKT[nAbsPGC] !=0) m_M_C_PBKT[nAbsPGC] += m_iMENU_PGC[nAbsPGC];
			m_M_C_POST[nAbsPGC]  =GetNbytes(2,&m_pIFO[m_iMENU_PGC[nAbsPGC]+0xEA]);
			if (m_M_C_POST[nAbsPGC] !=0) m_M_C_POST[nAbsPGC] +=m_iMENU_PGC[nAbsPGC];

			m_nMCells[nAbsPGC]=m_pIFO[m_iMENU_PGC[nAbsPGC]+3];

			if ( (m_M_C_PBKT[nAbsPGC]==0 || m_M_C_POST[nAbsPGC] ==0) &&  m_nMCells[nAbsPGC]!=0)
// There is something wrong...
			{
				m_nMCells[nAbsPGC]=0;
                                csAux.str("");
				csAux << "ERROR: There is something wrong in number of cells in LU " << nLU << ", Menu PGC " << j << ".";
				MyErrorBox (csAux.str().c_str());
				return -1;
			}
			m_dwMDuration[nAbsPGC]=(DWORD)GetNbytes(4,&m_pIFO[m_iMENU_PGC[nAbsPGC]+4]);

		} // For PGCs
		m_nMPGCs+=m_nPGCinLU[nLU];
	}


///////////// VTS_C_ADT  ///////////////////////
	if (m_iVTS_C_ADT==0) nTotADT=0;
	else
	{
		nTotADT=GetNbytes(4,&m_pIFO[m_iVTS_C_ADT+4]);
		nTotADT=(nTotADT-7)/12;
	}

//Cells
	for (nADT=0; nADT <nTotADT; nADT++)
	{
		VidADT=GetNbytes(2,&m_pIFO[m_iVTS_C_ADT+8+12*nADT]);
		CidADT=m_pIFO[m_iVTS_C_ADT+8+12*nADT+2];

		iArraysize=m_AADT_Cell_list.size();
		for (int k = 0, bAlready = false; k < iArraysize; k++)
		{
			if (CidADT==m_AADT_Cell_list[k].CID &&
				VidADT==m_AADT_Cell_list[k].VID )
			{
				bAlready=true;
				kk=k;
			}
		}
		if (!bAlready)
		{
			myADT_Cell.CID=CidADT;
			myADT_Cell.VID=VidADT;
			myADT_Cell.iSize=0;
			myADT_Cell.iIniSec=0x7fffffff;
			myADT_Cell.iEndSec=0;
			kk=InsertCell (myADT_Cell, TITLES);
//			m_AADT_Cell_list.SetAtGrow(iArraysize,myADT_Cell);
//			kk=iArraysize;
		}
		iIniSec=GetNbytes(4,&m_pIFO[m_iVTS_C_ADT+8+12*nADT+4]);
		iEndSec=GetNbytes(4,&m_pIFO[m_iVTS_C_ADT+8+12*nADT+8]);
		if (iIniSec < m_AADT_Cell_list[kk].iIniSec) m_AADT_Cell_list[kk].iIniSec=iIniSec;
		if (iEndSec > m_AADT_Cell_list[kk].iEndSec) m_AADT_Cell_list[kk].iEndSec=iEndSec;
		m_AADT_Cell_list[kk].iSize+=(iEndSec-iIniSec+1);
	}

///////////// VTSM_C_ADT  ///////////////////////
	if (m_iVTSM_C_ADT==0) nTotADT=0;
	else
	{
		nTotADT=GetNbytes(4,&m_pIFO[m_iVTSM_C_ADT+4]);
		nTotADT=(nTotADT-7)/12;
	}

// Cells
	for (nADT=0; nADT <nTotADT; nADT++)
	{
		VidADT=GetNbytes(2,&m_pIFO[m_iVTSM_C_ADT+8+12*nADT]);
		CidADT=m_pIFO[m_iVTSM_C_ADT+8+12*nADT+2];

		iArraysize=m_MADT_Cell_list.size();
		for (int k = 0, bAlready=false; k < iArraysize; k++)
		{
			if (CidADT==m_MADT_Cell_list[k].CID &&
				VidADT==m_MADT_Cell_list[k].VID )
			{
				bAlready=true;
				kk=k;
			}
		}
		if (!bAlready)
		{
			myADT_Cell.CID=CidADT;
			myADT_Cell.VID=VidADT;
			myADT_Cell.iSize=0;
			myADT_Cell.iIniSec=0x7fffffff;
			myADT_Cell.iEndSec=0;
			kk=InsertCell (myADT_Cell, MENUS);
//			m_MADT_Cell_list.SetAtGrow(iArraysize,myADT_Cell);
//			kk=iArraysize;
		}
		iIniSec=GetNbytes(4,&m_pIFO[m_iVTSM_C_ADT+8+12*nADT+4]);
		iEndSec=GetNbytes(4,&m_pIFO[m_iVTSM_C_ADT+8+12*nADT+8]);
		if (iIniSec < m_MADT_Cell_list[kk].iIniSec) m_MADT_Cell_list[kk].iIniSec=iIniSec;
		if (iEndSec > m_MADT_Cell_list[kk].iEndSec) m_MADT_Cell_list[kk].iEndSec=iEndSec;
		m_MADT_Cell_list[kk].iSize+=(iEndSec-iIniSec+1);
	}

	FillDurations();

//////////////////////////////////////////////////////////////
/////////////   VIDs
// VIDs in Titles
	iArraysize=m_AADT_Cell_list.size();
	for (i=0; i <iArraysize; i++)
	{
		VidADT=m_AADT_Cell_list[i].VID;

		nVIDs=m_AADT_Vid_list.size();
		for (int k = 0, bAlready = false; k < int(nVIDs); k++)
		{
			if (VidADT==m_AADT_Vid_list[k].VID )
			{
				bAlready=true;
				kk=k;
			}
		}
		if (!bAlready)
		{
			myADT_Vid.VID=VidADT;
			myADT_Vid.iSize=0;
			myADT_Vid.nCells=0;
			myADT_Vid.dwDuration=0;
                        if(nVIDs < m_AADT_Vid_list.size())
                                m_AADT_Vid_list.at(nVIDs) = myADT_Vid;
                        else
                                m_AADT_Vid_list.insert(m_AADT_Vid_list.begin() + nVIDs, myADT_Vid);
			kk=nVIDs;
		}
		m_AADT_Vid_list[kk].iSize+=m_AADT_Cell_list[i].iSize;
		m_AADT_Vid_list[kk].nCells++;
		m_AADT_Vid_list[kk].dwDuration=AddDuration(m_AADT_Cell_list[i].dwDuration,m_AADT_Vid_list[kk].dwDuration);
	}

// VIDs in Menus
	iArraysize=m_MADT_Cell_list.size();
	for (i=0; i <iArraysize; i++)
	{
		VidADT=m_MADT_Cell_list[i].VID;

		nVIDs=m_MADT_Vid_list.size();
		for (int k = 0, bAlready = false; k < int(nVIDs); k++)
		{
			if (VidADT==m_MADT_Vid_list[k].VID )
			{
				bAlready=true;
				kk=k;
			}
		}
		if (!bAlready)
		{
			myADT_Vid.VID=VidADT;
			myADT_Vid.iSize=0;
			myADT_Vid.nCells=0;
			myADT_Vid.dwDuration=0;
                        if(nVIDs < m_MADT_Vid_list.size())
                                m_MADT_Vid_list.at(nVIDs) = myADT_Vid;
                        else
                                m_MADT_Vid_list.insert(m_MADT_Vid_list.begin() + nVIDs, myADT_Vid);
			kk=nVIDs;
		}
		m_MADT_Vid_list[kk].iSize+=m_MADT_Cell_list[i].iSize;
		m_MADT_Vid_list[kk].nCells++;
		m_MADT_Vid_list[kk].dwDuration=AddDuration(m_MADT_Cell_list[i].dwDuration,m_MADT_Vid_list[kk].dwDuration);
	}

// Fill VOB file size
	if (m_bVMGM)
	{
		m_nVobFiles=0;

		for (int k = 0; k < 10; k++)
			m_i64VOBSize[k]=0;

		csAux2 = m_csInputIFO.substr(0, m_csInputIFO.size() - 3);
                csAux.str("");
		csAux << csAux2 << "VOB";
		if (_stati64 ( csAux.str().c_str(), &statbuf)==0)
			m_i64VOBSize[0]= statbuf.st_size;
	}
	else
	{
		for (int k = 0; k < 10; k++)
		{
			csAux2 = m_csInputIFO.substr(0, m_csInputIFO.size() - 5);
                        csAux.str("");
			csAux << csAux2 << k << ".VOB";
			if (_stati64 ( csAux.str().c_str(), &statbuf)==0)
			{
				m_i64VOBSize[k]= statbuf.st_size;
				m_nVobFiles=k;
			}
			else
				m_i64VOBSize[k]=0;
		}
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
/////////////   Demuxing Code  : Process Pack, called by all demuxing methods
/////////////                    bWrite: If true Write files, if not used for delay checking
////////////////////////////////////////////////////////////////////////////////

int CPgcDemuxApp::ProcessPack(bool bWrite)
{
	int sID;
	static int nPack=0;
	static int nFirstRef=0;
	bool bFirstAud;
	int nBytesOffset;

	if ( bWrite && m_bCheckVob )
	{
		if (IsNav(m_buffer))
		{
			if (m_bCheckLBA) ModifyLBA (m_buffer,m_i64OutputLBA);
			m_nVidout= (int) GetNbytes(2,&m_buffer[0x41f]);
			m_nCidout= (int) m_buffer[0x422];
			nFirstRef= (int) GetNbytes(4,&m_buffer[0x413]);
			nPack=0;

			bNewCell=false;
			if (m_nVidout!= m_nLastVid || m_nCidout!= m_nLastCid)
			{
				bNewCell=true;
				m_nLastVid=m_nVidout;
				m_nLastCid=m_nCidout;
			}
		}
		else
			nPack++;
		if ( (IsNav(m_buffer)   && m_bCheckNavPack)   ||
		     (IsAudio(m_buffer) && m_bCheckAudioPack) ||
		     (IsSubs (m_buffer) && m_bCheckSubPack)   )
				WritePack(m_buffer);
		else if ( IsVideo(m_buffer) && m_bCheckVideoPack)
		{
			if (!m_bCheckIFrame)
				WritePack(m_buffer);
			else
			{
//				if (nFirstRef == nPack)
//					if ( ! PatchEndOfSequence(m_buffer))
//						WritePack (Pad_pack);
				if (bNewCell && nFirstRef >= nPack ) WritePack(m_buffer);
			}
		}

	}
	if (IsNav(m_buffer))
	{
// do nothing
		m_nNavPacks++;
		m_iNavPTS0= (int) GetNbytes(4,&m_buffer[0x39]);
		m_iNavPTS1= (int) GetNbytes(4,&m_buffer[0x3d]);
		if (m_iFirstNavPTS0==0) m_iFirstNavPTS0=m_iNavPTS0;
		if (m_iNavPTS1_old> m_iNavPTS0)
		{
// Discontinuity, so add the offset
			m_iOffsetPTS+=(m_iNavPTS1_old-m_iNavPTS0);
		}
		m_iNavPTS0_old=m_iNavPTS0;
		m_iNavPTS1_old=m_iNavPTS1;
	}
	else if (IsVideo(m_buffer))
	{
		m_nVidPacks++;
		if (m_buffer[0x15] & 0x80)
		{
			m_iVidPTS= readpts(&m_buffer[0x17]);
			if (m_iFirstVidPTS==0) m_iFirstVidPTS=m_iVidPTS;
		}
		if ( bWrite && m_bCheckVid) demuxvideo(m_buffer);
	}
	else if (IsAudio(m_buffer))
	{
		m_nAudPacks++;
		nBytesOffset=0;

		sID=getAudId(m_buffer) & 0x07;

		bFirstAud=false;

		if (m_buffer[0x15] & 0x80)
		{
			if (m_iFirstAudPTS[sID]==0)
			{
				bFirstAud=true;
				m_iAudPTS= readpts(&m_buffer[0x17]);
				m_iFirstAudPTS[sID]=m_iAudPTS;
//				m_iAudIndex[sID]=m_buffer[0x17+m_buffer[0x16]];
				m_iAudIndex[sID]=getAudId (m_buffer);
			}
		}
		if (bFirstAud)
		{
			nBytesOffset=GetAudHeader(m_buffer);
			if (nBytesOffset<0)
// This pack does not have an Audio Frame Header, so its PTS is  not valid.
				m_iFirstAudPTS[sID]=0;
		}

		if ( bWrite && m_bCheckAud && m_iFirstAudPTS[sID]!=0)
		{
			demuxaudio(m_buffer,nBytesOffset);
		}
	}
	else if (IsSubs(m_buffer))
	{
		m_nSubPacks++;
		sID=m_buffer[0x17+m_buffer[0x16]] & 0x1F;

		if (m_buffer[0x15] & 0x80)
		{
			m_iSubPTS= readpts(&m_buffer[0x17]);
			if (m_iFirstSubPTS[sID]==0)
				m_iFirstSubPTS[sID]=m_iSubPTS;
		}
		if (bWrite && m_bCheckSub) demuxsubs(m_buffer);
	}
	else if (IsPad(m_buffer))
	{
		m_nPadPacks++;
	}
	else
	{
		m_nUnkPacks++;
	}
	return 0;
}


////////////////////////////////////////////////////////////////////////////////
/////////////   Audio Delay  Code:
////////////////////////////////////////////////////////////////////////////////
int CPgcDemuxApp::GetAudHeader(uchar* buffer)
// Returns the number of bytes from audio start until first header
// If no header found  returns -1
{
	int i,start,nbytes;
	uchar streamID;
	int firstheader,nHeaders;
	bool bFound;

	start=0x17+buffer[0x16];
	nbytes=buffer[0x12]*256+buffer[0x13]+0x14;
	if (IsAudMpeg(buffer))
		streamID=buffer[0x11];
	else
	{
		streamID=buffer[start];
		start+=4;
	}

	firstheader=0;

// Check if PCM
	if ( streamID>=0xa0 && streamID<= 0xa7 ) return 0;
	if ( streamID>=0x80 && streamID<= 0x8f )
	{
// Stream is AC3 or DTS...
		nHeaders=buffer[start-3];
		if (nHeaders !=0)
		{
			bFound=true;
			firstheader= buffer[start-2]*256+ buffer[start-1]-1;
		}
		else
			bFound=false;
	}
	else if ( streamID>=0xc0 && streamID<= 0xc7 )
	{
// Stream is MPEG ...
		for (i=start, bFound=false ; i< (nbytes-1) && bFound==false ; i++)
		{
//			if ( buffer[start+i] == 0xFF && (buffer[start+1+i] & 0xF0 )== 0xF0 )
			if ( buffer[i] == 0xFF && (buffer[i+1] & 0xF0 )== 0xF0 )
			{
				bFound=true;
				firstheader=i-start;
			}
		}
	}

	if ((start+firstheader) >= nbytes) bFound=false;

	if (bFound)
		return firstheader;
	else
		return -1;

}

int CPgcDemuxApp::GetAudioDelay(int iMode, int nSelection)
{
	int VID,CID;
	unsigned k;
        int nCell;
	__int64 i64IniSec,i64EndSec;
	__int64 i64sectors;
	int nVobin;
        stringstream csAux;
	string csAux2;
	FILE *in;
	__int64 i64;
	bool bMyCell;
	int iRet;

	IniDemuxGlobalVars();

	if (iMode==PGCMODE)
	{
		if (nSelection >= m_nPGCs)
		{
			MyErrorBox("Error: GetAudioDelay: PGC does not exist");
			return -1;
		}
		nCell=0;
		VID=GetNbytes(2,&m_pIFO[m_C_POST[nSelection]+4*nCell]);
		CID=m_pIFO[m_C_POST[nSelection]+3+4*nCell];
	}
	else if (iMode==VIDMODE)
	{
		if (nSelection >= int(m_AADT_Vid_list.size()))
		{
			MyErrorBox("Error: VID does not exist");
			return -1;
		}
		VID=m_AADT_Vid_list[nSelection].VID;
		CID=-1;
		for (k=0;k<m_AADT_Cell_list.size() && CID==-1; k++)
		{
			if (VID==m_AADT_Cell_list[k].VID)
				CID=m_AADT_Cell_list[k].CID;
		}

	}
	else if (iMode==CIDMODE)
	{
		if (nSelection >= int(m_AADT_Cell_list.size()))
		{
			MyErrorBox("Error: CID does not exist");
			return -1;
		}
		VID=m_AADT_Cell_list[nSelection].VID;
		CID=m_AADT_Cell_list[nSelection].CID;
	}

	for (k=0,nCell=-1; k < m_AADT_Cell_list.size() && nCell==-1; k++)
	{
		if (VID==m_AADT_Cell_list[k].VID &&
			CID==m_AADT_Cell_list[k].CID)
			nCell=k;
	}

	if (nCell<0)
	{
		MyErrorBox("Error: VID/CID not found!.");
		return -1;
	}
//
// Now we have VID; CID; and the index in Cell Array "nCell".
// So we are going to open the VOB and read the delays using ProcessPack(false)
	i64IniSec=m_AADT_Cell_list[nCell].iIniSec;
	i64EndSec=m_AADT_Cell_list[nCell].iEndSec;

	iRet=0;
	for (k=1,i64sectors=0;k<10;k++)
	{
		i64sectors+=(m_i64VOBSize[k]/2048);
		if (i64IniSec<i64sectors)
		{
			i64sectors-=(m_i64VOBSize[k]/2048);
			nVobin=k;
			k=20;
		}
	}
	csAux2 = m_csInputIFO.substr(0, m_csInputIFO.size() - 5);
        csAux.str("");
        csAux << csAux2 << nVobin << ".VOB";
	in=fopen(csAux.str().c_str(),"rb");
	if (in ==NULL)
	{
		MyErrorBox(("Error opening input VOB: " + csAux.str()).c_str());
		iRet=-1;
	}
	if (iRet==0) fseek(in, (long) ((i64IniSec-i64sectors)*2048), SEEK_SET);

	for (i64=0,bMyCell=true; iRet==0 && i64< (i64EndSec-i64IniSec+1) && i64< MAXLOOKFORAUDIO;i64++)
	{
	//readpack
		if (readbuffer(m_buffer,in)!=2048)
		{
			if (in!=NULL) fclose (in);
			nVobin++;
			csAux2 = m_csInputIFO.substr(0, m_csInputIFO.size() - 5);
                        csAux.str("");
                        csAux << csAux2 << nVobin << ".VOB";
			in = fopen(csAux.str().c_str(), "rb");
			if (readbuffer(m_buffer,in)!=2048)
			{
				MyErrorBox("Input error: Reached end of VOB too early");
				iRet=-1;
			}
		}

		if (iRet==0)
		{
			if (IsSynch(m_buffer) != true)
			{
				MyErrorBox("Error reading input VOB: Unsynchronized");
				iRet=-1;
			}
			if ((iRet==0) && IsNav(m_buffer))
			{
				if (m_buffer[0x420]==(uchar)(VID%256) &&
					m_buffer[0x41F]==(uchar)(VID/256) &&
					m_buffer[0x422]==(uchar) CID)
					bMyCell=true;
				else
					bMyCell=false;
			}

			if (iRet==0 && bMyCell)
			{
				iRet=ProcessPack(false);
			}
		}
	} // For readpacks
	if (in!=NULL) fclose (in);
	in=NULL;

	return iRet;
}


int CPgcDemuxApp::GetMAudioDelay(int iMode, int nSelection)
{
	int VID,CID;
	unsigned k;
        int nCell;
	__int64 i64IniSec,i64EndSec;
	string csAux,csAux2;
	FILE *in;
	__int64 i64;
	bool bMyCell;
	int iRet;

	IniDemuxGlobalVars();

	if (iMode==PGCMODE)
	{
		if (nSelection >= m_nMPGCs)
		{
			MyErrorBox("Error: GetMAudioDelay: PGC does not exist");
			return -1;
		}
		nCell=0;
		VID=GetNbytes(2,&m_pIFO[m_M_C_POST[nSelection]+4*nCell]);
		CID=m_pIFO[m_M_C_POST[nSelection]+3+4*nCell];
	}
	else if (iMode==VIDMODE)
	{
		if (nSelection >= int(m_MADT_Vid_list.size()))
		{
			MyErrorBox("Error: VID does not exist");
			return -1;
		}
		VID=m_MADT_Vid_list[nSelection].VID;
		CID=-1;
		for (k=0;k<m_MADT_Cell_list.size() && CID==-1; k++)
		{
			if (VID==m_MADT_Cell_list[k].VID)
				CID=m_MADT_Cell_list[k].CID;
		}

	}
	else if (iMode==CIDMODE)
	{
		if (nSelection >= int(m_MADT_Cell_list.size()))
		{
			MyErrorBox("Error: CID does not exist");
			return -1;
		}
		VID=m_MADT_Cell_list[nSelection].VID;
		CID=m_MADT_Cell_list[nSelection].CID;
	}

	for (k=0,nCell=-1; k < m_MADT_Cell_list.size() && nCell==-1; k++)
	{
		if (VID==m_MADT_Cell_list[k].VID &&
			CID==m_MADT_Cell_list[k].CID)
			nCell=k;
	}

	if (nCell<0)
	{
		MyErrorBox("Error: VID/CID not found!.");
		return -1;
	}
//
// Now we have VID; CID; and the index in Cell Array "nCell".
// So we are going to open the VOB and read the delays using ProcessPack(false)
	i64IniSec=m_MADT_Cell_list[nCell].iIniSec;
	i64EndSec=m_MADT_Cell_list[nCell].iEndSec;

	iRet=0;

	if (m_bVMGM)
	{
		csAux2 = m_csInputIFO.substr(0, m_csInputIFO.size() - 3);
		csAux=csAux2+"VOB";
	}
	else
	{
		csAux2 = m_csInputIFO.substr(m_csInputIFO.size() - 5);
		csAux=csAux2+"0.VOB";
	}
	in = fopen(csAux.c_str(), "rb");
	if (in ==NULL)
	{
		MyErrorBox(("Error opening input VOB: " + csAux).c_str());
		iRet=-1;
	}
	if (iRet==0) fseek(in, (long) ((i64IniSec)*2048), SEEK_SET);

	for (i64=0,bMyCell=true; iRet==0 && i64< (i64EndSec-i64IniSec+1) && i64< MAXLOOKFORAUDIO;i64++)
	{
//readpack
		if (readbuffer(m_buffer,in)!=2048)
		{
			MyErrorBox("Input error: Reached end of VOB too early");
			iRet=-1;
		}

		if (iRet==0)
		{
			if (IsSynch(m_buffer) != true)
			{
				MyErrorBox("Error reading input VOB: Unsynchronized");
				iRet=-1;
			}
			if ((iRet==0) && IsNav(m_buffer))
			{
				if (m_buffer[0x420]==(uchar)(VID%256) &&
					m_buffer[0x41F]==(uchar)(VID/256) &&
					m_buffer[0x422]==(uchar) CID)
					bMyCell=true;
				else
					bMyCell=false;
			}

			if (iRet==0 && bMyCell)
			{
				iRet=ProcessPack(false);
			}
		}
	} // For readpacks
	if (in!=NULL) fclose (in);
	in=NULL;

	return iRet;
}

////////////////////////////////////////////////////////////////////////////////
/////////////   Demuxing Code : DEMUX BY PGC
////////////////////////////////////////////////////////////////////////////////
void CPgcDemuxApp::IniDemuxGlobalVars()
{
	int k;
	string csAux;

	// clear PTS
	for (k=0;k<32;k++)
		m_iFirstSubPTS[k]=0;
	for (k=0;k<8;k++)
	{
		m_iFirstAudPTS[k]=0;
		nchannels[k]=-1;
		nbitspersample[k]=-1;
		fsample[k]=-1;
	}
	m_iFirstVidPTS=0;
	m_iFirstNavPTS0=0;
	m_iNavPTS0_old=m_iNavPTS0=0;
	m_iNavPTS1_old=m_iNavPTS1=0;

	m_nNavPacks=m_nVidPacks=m_nAudPacks=m_nSubPacks=m_nUnkPacks=m_nPadPacks=0;
	m_i64OutputLBA=0;
	m_nVobout=m_nVidout=m_nCidout=0;
	m_nLastVid=m_nLastCid=0;

	m_nCurrVid=0;
	m_iOffsetPTS=0;
	bNewCell=false;
}

int CPgcDemuxApp::OpenVideoFile()
{
	string csAux;

	if (m_bCheckVid)
	{
		csAux=m_csOutputPath+ '/' + "VideoFile.m2v";
		fvid = fopen(csAux.c_str(), "wb");
		if (fvid==NULL) return -1;
	}

	return 0;
}


int CPgcDemuxApp::PgcDemux(int nPGC, int nAng)
{
	int nTotalSectors;
	int nSector,nCell;
	int k,iArraysize;
	int CID,VID;
	__int64 i64IniSec,i64EndSec;
	__int64 i64sectors;
	int nVobin;
        stringstream csAux;
	string csAux2;
	FILE *in, *fout;
	__int64 i64;
	bool bMyCell;
	int iRet;
	DWORD dwCellDuration;
	int nFrames;
	int nCurrAngle, iCat;

	if (nPGC >= theApp.m_nPGCs)
	{
		MyErrorBox("Error: PgcDemux: PGC does not exist");
		m_bInProcess=false;
		return -1;
	}

	IniDemuxGlobalVars();
	if (OpenVideoFile()) return -1;
	m_bInProcess=true;

// Calculate  the total number of sectors
	nTotalSectors=0;
	iArraysize=m_AADT_Cell_list.size();
	for (nCell=nCurrAngle=0; nCell<m_nCells[nPGC]; nCell++)
	{
		VID=GetNbytes(2,&m_pIFO[m_C_POST[nPGC]+4*nCell]);
		CID=m_pIFO[m_C_POST[nPGC]+3+4*nCell];

		iCat=m_pIFO[m_C_PBKT[nPGC]+24*nCell];
		iCat=iCat & 0xF0;
//		0101=First; 1001=Middle ;	1101=Last
		if (iCat == 0x50)
			nCurrAngle=1;
		else if ((iCat == 0x90 || iCat == 0xD0) && nCurrAngle!=0)
			nCurrAngle++;
		if (iCat==0 || (nAng+1) == nCurrAngle)
		{
			for (k=0; k< iArraysize ;k++)
			{
				if (CID==m_AADT_Cell_list[k].CID &&
					VID==m_AADT_Cell_list[k].VID )
				{
					nTotalSectors+= m_AADT_Cell_list[k].iSize;
				}
			}
		}
		if (iCat == 0xD0)	nCurrAngle=0;
	}

	nSector=0;
	iRet=0;
	for (nCell=nCurrAngle=0; nCell<m_nCells[nPGC] && m_bInProcess==true; nCell++)
	{
		iCat=m_pIFO[m_C_PBKT[nPGC]+24*nCell];
		iCat=iCat & 0xF0;
//		0101=First; 1001=Middle ;	1101=Last
		if (iCat == 0x50)
			nCurrAngle=1;
		else if ((iCat == 0x90 || iCat == 0xD0) && nCurrAngle!=0)
			nCurrAngle++;
		if (iCat==0 || (nAng+1) == nCurrAngle)
		{

			VID=GetNbytes(2,&m_pIFO[m_C_POST[nPGC]+4*nCell]);
			CID=m_pIFO[m_C_POST[nPGC]+3+4*nCell];

			i64IniSec=GetNbytes(4,&m_pIFO[m_C_PBKT[nPGC]+nCell*24+8]);
			i64EndSec=GetNbytes(4,&m_pIFO[m_C_PBKT[nPGC]+nCell*24+0x14]);
			for (k=1,i64sectors=0;k<10;k++)
			{
				i64sectors+=(m_i64VOBSize[k]/2048);
				if (i64IniSec<i64sectors)
				{
					i64sectors-=(m_i64VOBSize[k]/2048);
					nVobin=k;
					k=20;
				}
			}
			csAux2 = m_csInputIFO.substr(0, m_csInputIFO.size() - 5);
                        csAux.str("");
                        csAux << csAux2 << nVobin << ".VOB";
			in = fopen(csAux.str().c_str(), "rb");
			if (in ==NULL)
			{
				MyErrorBox(("Error opening input VOB: " + csAux.str()).c_str());
				m_bInProcess=false;
				iRet=-1;
			}
			if (m_bInProcess) fseek(in, (long) ((i64IniSec-i64sectors)*2048), SEEK_SET);

			for (i64=0,bMyCell=true;i64< (i64EndSec-i64IniSec+1) && m_bInProcess==true;i64++)
			{
			//readpack
				if ((i64%MODUPDATE) == 0) UpdateProgress((int)((100*nSector)/nTotalSectors) );
				if (readbuffer(m_buffer,in)!=2048)
				{
					if (in!=NULL) fclose (in);
					nVobin++;
					csAux2 = m_csInputIFO.substr(0, m_csInputIFO.size() - 5);
                                        csAux.str("");
                                        csAux << csAux2 << nVobin << ".VOB";
					in = fopen(csAux.str().c_str(), "rb");
					if (readbuffer(m_buffer,in)!=2048)
					{
						MyErrorBox("Input error: Reached end of VOB too early");
						m_bInProcess=false;
						iRet=-1;
					}
				}

				if (m_bInProcess==true)
				{
					if (IsSynch(m_buffer) != true)
					{
						MyErrorBox("Error reading input VOB: Unsynchronized");
						m_bInProcess=false;
						iRet=-1;
					}
					if (IsNav(m_buffer))
					{
						if (m_buffer[0x420]==(uchar)(VID%256) &&
							m_buffer[0x41F]==(uchar)(VID/256) &&
							m_buffer[0x422]==(uchar) CID)
							bMyCell=true;
						else
							bMyCell=false;
					}

					if (bMyCell)
					{
						nSector++;
						iRet=ProcessPack(true);
					}

				}
			} // For readpacks
			if (in!=NULL) fclose (in);
			in=NULL;
		}  // if (iCat==0 || (nAng+1) == nCurrAngle)
		if (iCat == 0xD0) nCurrAngle=0;
	}	// For Cells

	CloseAndNull();
	nFrames=0;

	if (m_bCheckCellt && m_bInProcess==true)
	{
		csAux.str(m_csOutputPath + '/' + "Celltimes.txt");
		fout = fopen(csAux.str().c_str(), "w");
		for (nCell=0,nCurrAngle=0; nCell<m_nCells[nPGC] && m_bInProcess==true; nCell++)
		{
			dwCellDuration=GetNbytes(4,&m_pIFO[m_C_PBKT[nPGC]+24*nCell+4]);

			iCat=m_pIFO[m_C_PBKT[nPGC]+24*nCell];
			iCat=iCat & 0xF0;
//			0101=First; 1001=Middle ;	1101=Last
			if (iCat == 0x50)
				nCurrAngle=1;
			else if ((iCat == 0x90 || iCat == 0xD0) && nCurrAngle!=0)
				nCurrAngle++;
			if (iCat==0 || (nAng+1) == nCurrAngle)
			{
				nFrames+=DurationInFrames(dwCellDuration);
				if (nCell!=(m_nCells[nPGC]-1) || m_bCheckEndTime )
					fprintf(fout,"%d\n",nFrames);
			}

			if (iCat == 0xD0) nCurrAngle=0;
		}
		fclose(fout);
	}

	m_nTotalFrames=nFrames;

	if (m_bCheckLog && m_bInProcess==true) OutputLog(nPGC, nAng, TITLES);

	return iRet;
}

int CPgcDemuxApp::PgcMDemux(int nPGC)
{
	int nTotalSectors;
	int nSector,nCell;
	int k,iArraysize;
	int CID,VID;
	__int64 i64IniSec,i64EndSec;
	string csAux,csAux2;
	FILE *in,*fout;
	__int64 i64;
	bool bMyCell;
	int iRet;
	DWORD dwCellDuration;
	int nFrames;


	if (nPGC >= theApp.m_nMPGCs)
	{
		MyErrorBox("Error: PgcMDemux: PGC does not exist");
		m_bInProcess=false;
		return -1;
	}

	IniDemuxGlobalVars();
	if (OpenVideoFile()) return -1;
	m_bInProcess=true;

// Calculate  the total number of sectors
	nTotalSectors=0;
	iArraysize=m_MADT_Cell_list.size();
	for (nCell=0; nCell<m_nMCells[nPGC]; nCell++)
	{
		VID=GetNbytes(2,&m_pIFO[m_M_C_POST[nPGC]+4*nCell]);
		CID=m_pIFO[m_M_C_POST[nPGC]+3+4*nCell];
		for (k=0; k< iArraysize ;k++)
		{
			if (CID==m_MADT_Cell_list[k].CID &&
				VID==m_MADT_Cell_list[k].VID )
			{
				nTotalSectors+= m_MADT_Cell_list[k].iSize;
			}
		}
	}

	nSector=0;
	iRet=0;

	for (nCell=0; nCell<m_nMCells[nPGC] && m_bInProcess==true; nCell++)
	{
		VID=GetNbytes(2,&m_pIFO[m_M_C_POST[nPGC]+4*nCell]);
		CID=m_pIFO[m_M_C_POST[nPGC]+3+4*nCell];

		i64IniSec=GetNbytes(4,&m_pIFO[m_M_C_PBKT[nPGC]+nCell*24+8]);
		i64EndSec=GetNbytes(4,&m_pIFO[m_M_C_PBKT[nPGC]+nCell*24+0x14]);

		if (m_bVMGM)
		{
			csAux2 = m_csInputIFO.substr(0, m_csInputIFO.size() - 3);
			csAux=csAux2+"VOB";
		}
		else
		{
			csAux2 = m_csInputIFO.substr(0, m_csInputIFO.size() - 5);
			csAux=csAux2+"0.VOB";
		}
		in = fopen(csAux.c_str(), "rb");
		if (in ==NULL)
		{
			MyErrorBox(("Error opening input VOB: " + csAux).c_str());
			m_bInProcess=false;
			iRet=-1;
		}
		if (m_bInProcess) fseek(in, (long) ((i64IniSec)*2048), SEEK_SET);

		for (i64=0,bMyCell=true;i64< (i64EndSec-i64IniSec+1) && m_bInProcess==true;i64++)
		{
			//readpack
			if ((i64%MODUPDATE) == 0) UpdateProgress((int)((100*nSector)/nTotalSectors) );
			if (readbuffer(m_buffer,in)!=2048)
			{
				if (in!=NULL) fclose (in);
				MyErrorBox("Input error: Reached end of VOB too early");
				m_bInProcess=false;
				iRet=-1;
			}

			if (m_bInProcess==true)
			{
				if (IsSynch(m_buffer) != true)
				{
					MyErrorBox("Error reading input VOB: Unsynchronized");
					m_bInProcess=false;
					iRet=-1;
				}
				if (IsNav(m_buffer))
				{
					if (m_buffer[0x420]==(uchar)(VID%256) &&
						m_buffer[0x41F]==(uchar)(VID/256) &&
						m_buffer[0x422]== (uchar) CID)
						bMyCell=true;
					else
						bMyCell=false;
				}

				if (bMyCell)
				{
					nSector++;
					iRet=ProcessPack(true);
				}
			}
		} // For readpacks
		if (in!=NULL) fclose (in);
		in=NULL;
	}	// For Cells

	CloseAndNull();

	nFrames=0;

	if (m_bCheckCellt && m_bInProcess==true)
	{
		csAux=m_csOutputPath+ '/' + "Celltimes.txt";
		fout = fopen(csAux.c_str(), "w");
		for (nCell=0; nCell<m_nMCells[nPGC] && m_bInProcess==true; nCell++)
		{
			dwCellDuration=GetNbytes(4,&m_pIFO[m_M_C_PBKT[nPGC]+24*nCell+4]);
			nFrames+=DurationInFrames(dwCellDuration);
			if (nCell!=(m_nMCells[nPGC]-1) || m_bCheckEndTime )
				fprintf(fout,"%d\n",nFrames);
		}
		fclose(fout);
	}

	m_nTotalFrames=nFrames;

	if (m_bCheckLog && m_bInProcess==true) OutputLog(nPGC, 1 , MENUS);

	return iRet;
}

////////////////////////////////////////////////////////////////////////////////
/////////////   Demuxing Code : DEMUX BY VOBID
////////////////////////////////////////////////////////////////////////////////

int CPgcDemuxApp::VIDDemux(int nVid)
{
	int nTotalSectors;
	int nSector,nCell;
	int k,iArraysize;
	int CID,VID,nDemuxedVID;
	__int64 i64IniSec,i64EndSec;
	__int64 i64sectors;
	int nVobin;
	std::string filename(m_csInputIFO);
	filename.replace(filename.size() - 3, 3, "VOB");
	FILE *in, *fout;
	__int64 i64;
	bool bMyCell;
	int iRet;
	int nFrames;
	int nLastCell;

	if (nVid >= int(m_AADT_Vid_list.size()))
	{
		MyErrorBox("Error: Selected Vid does not exist");
		m_bInProcess=false;
		return -1;
	}

	IniDemuxGlobalVars();
	if (OpenVideoFile()) return -1;
	m_bInProcess=true;

// Calculate  the total number of sectors
	nTotalSectors= m_AADT_Vid_list[nVid].iSize;
	nSector=0;
	iRet=0;
	nDemuxedVID=m_AADT_Vid_list[nVid].VID;

	iArraysize=m_AADT_Cell_list.size();
	for (nCell=0; nCell<iArraysize && m_bInProcess==true; nCell++)
	{
		VID=m_AADT_Cell_list[nCell].VID;
		CID=m_AADT_Cell_list[nCell].CID;

		if (VID==nDemuxedVID)
		{
			i64IniSec=m_AADT_Cell_list[nCell].iIniSec;
			i64EndSec=m_AADT_Cell_list[nCell].iEndSec;
			for (k=1,i64sectors=0;k<10;k++)
			{
				i64sectors+=(m_i64VOBSize[k]/2048);
				if (i64IniSec<i64sectors)
				{
					i64sectors-=(m_i64VOBSize[k]/2048);
					nVobin=k;
					k=20;
				}
			}
			filename[filename.size() - 5] = '0' + nVobin;
			SPDLOG_INFO("Opening {}", filename);
			in = fopen(filename.c_str(), "rb");
			if (in ==NULL)
			{
				MyErrorBox(("Error opening input VOB: " + filename).c_str());
				m_bInProcess=false;
				iRet=-1;
			}
			if (m_bInProcess) fseek(in, (long) ((i64IniSec-i64sectors)*2048), SEEK_SET);

			for (i64=0,bMyCell=true;i64< (i64EndSec-i64IniSec+1) && m_bInProcess==true;i64++)
			{
			//readpack
				if ((i64%MODUPDATE) == 0) UpdateProgress((int)((100*nSector)/nTotalSectors) );
				if (readbuffer(m_buffer,in)!=2048)
				{
					if (in!=NULL) fclose (in);
					nVobin++;
					filename[filename.size() - 5] = '0' + nVobin;
					SPDLOG_INFO("Opening {}", filename);
					in = fopen(filename.c_str(), "rb");
					if (readbuffer(m_buffer,in)!=2048)
					{
						MyErrorBox("Input error: Reached end of VOB too early");
						m_bInProcess=false;
						iRet=-1;
					}
				}

				if (m_bInProcess==true)
				{
					if (IsSynch(m_buffer) != true)
					{
						MyErrorBox("Error reading input VOB: Unsynchronized");
						m_bInProcess=false;
						iRet=-1;
					}
					if (IsNav(m_buffer))
					{
						if (m_buffer[0x420]==(uchar)(VID%256) &&
							m_buffer[0x41F]==(uchar)(VID/256) &&
							m_buffer[0x422]==(uchar) CID)
							bMyCell=true;
						else
							bMyCell=false;
					}

					if (bMyCell)
					{
						nSector++;
						iRet=ProcessPack(true);
					}
				}
			} // For readpacks
			if (in!=NULL) fclose (in);
			in=NULL;
		}  // if (VID== DemuxedVID)
	}	// For Cells

	CloseAndNull();
	nFrames=0;

	if (m_bCheckCellt && m_bInProcess==true)
	{
		filename.clear();
		fmt::format_to(std::back_inserter(filename), "{}/Celltimes.txt", m_csOutputPath);
		fout = fopen(filename.c_str(), "w");

		nDemuxedVID=m_AADT_Vid_list[nVid].VID;

		iArraysize=m_AADT_Cell_list.size();
		for (nCell=nLastCell=0; nCell<iArraysize && m_bInProcess==true; nCell++)
		{
			VID=m_AADT_Cell_list[nCell].VID;
			if (VID==nDemuxedVID)
				nLastCell=nCell;
		}

		for (nCell=0; nCell<iArraysize && m_bInProcess==true; nCell++)
		{
			VID=m_AADT_Cell_list[nCell].VID;

			if (VID==nDemuxedVID)
			{
				nFrames+=DurationInFrames(m_AADT_Cell_list[nCell].dwDuration);
				if (nCell!=nLastCell || m_bCheckEndTime )
					fprintf(fout,"%d\n",nFrames);
			}
		}
		fclose(fout);
	}

	m_nTotalFrames=nFrames;

	if (m_bCheckLog && m_bInProcess==true) OutputLog(nVid, 1, TITLES);

	return iRet;
}

int CPgcDemuxApp::VIDMDemux(int nVid)
{
	int nTotalSectors;
	int nSector,nCell;
	int iArraysize;
	int CID,VID,nDemuxedVID;
	__int64 i64IniSec,i64EndSec;
	string csAux,csAux2;
	FILE *in, *fout;
	__int64 i64;
	bool bMyCell;
	int iRet;
	int nFrames;
	int nLastCell;

	if (nVid >= int(m_MADT_Vid_list.size()))
	{
		MyErrorBox("Error: Selected Vid does not exist");
		m_bInProcess=false;
		return -1;
	}

	IniDemuxGlobalVars();
	if (OpenVideoFile()) return -1;
	m_bInProcess=true;

// Calculate  the total number of sectors
	nTotalSectors= m_MADT_Vid_list[nVid].iSize;
	nSector=0;
	iRet=0;
	nDemuxedVID=m_MADT_Vid_list[nVid].VID;

	iArraysize=m_MADT_Cell_list.size();
	for (nCell=0; nCell<iArraysize && m_bInProcess==true; nCell++)
	{
		VID=m_MADT_Cell_list[nCell].VID;
		CID=m_MADT_Cell_list[nCell].CID;

		if (VID==nDemuxedVID)
		{
			i64IniSec=m_MADT_Cell_list[nCell].iIniSec;
			i64EndSec=m_MADT_Cell_list[nCell].iEndSec;
			if (m_bVMGM)
			{
				csAux2 = m_csInputIFO.substr(0, m_csInputIFO.size() - 3);
				csAux=csAux2+"VOB";
			}
			else
			{
				csAux2 = m_csInputIFO.substr(0, m_csInputIFO.size() - 5);
				csAux=csAux2+"0.VOB";
			}
			in = fopen(csAux.c_str(), "rb");
			if (in ==NULL)
			{
				MyErrorBox(("Error opening input VOB: " + csAux).c_str());
				m_bInProcess=false;
				iRet=-1;
			}
			if (m_bInProcess) fseek(in, (long) ((i64IniSec)*2048), SEEK_SET);

			for (i64=0,bMyCell=true;i64< (i64EndSec-i64IniSec+1) && m_bInProcess==true;i64++)
			{
	//readpack
				if ((i64%MODUPDATE) == 0) UpdateProgress((int)((100*nSector)/nTotalSectors) );
				if (readbuffer(m_buffer,in)!=2048)
				{
					if (in!=NULL) fclose (in);
					MyErrorBox("Input error: Reached end of VOB too early");
					m_bInProcess=false;
					iRet=-1;
				}

				if (m_bInProcess==true)
				{
					if (IsSynch(m_buffer) != true)
					{
						MyErrorBox("Error reading input VOB: Unsynchronized");
						m_bInProcess=false;
						iRet=-1;
					}
					if (IsNav(m_buffer))
					{
						if (m_buffer[0x420]==(uchar)(VID%256) &&
							m_buffer[0x41F]==(uchar)(VID/256) &&
							m_buffer[0x422]== (uchar) CID)
							bMyCell=true;
						else
							bMyCell=false;
					}

					if (bMyCell)
					{
						nSector++;
						iRet=ProcessPack(true);
					}
				}
			} // For readpacks
			if (in!=NULL) fclose (in);
			in=NULL;
		} // If (VID==DemuxedVID)
	}	// For Cells

	CloseAndNull();

	nFrames=0;

	if (m_bCheckCellt && m_bInProcess==true)
	{
		csAux=m_csOutputPath+ '/' + "Celltimes.txt";
		fout = fopen(csAux.c_str(), "w");

		nDemuxedVID=m_MADT_Vid_list[nVid].VID;

		iArraysize=m_MADT_Cell_list.size();

		for (nCell=nLastCell=0; nCell<iArraysize && m_bInProcess==true; nCell++)
		{
			VID=m_MADT_Cell_list[nCell].VID;
			if (VID==nDemuxedVID) nLastCell=nCell;
		}


		for (nCell=0; nCell<iArraysize && m_bInProcess==true; nCell++)
		{
			VID=m_MADT_Cell_list[nCell].VID;

			if (VID==nDemuxedVID)
			{
				nFrames+=DurationInFrames(m_MADT_Cell_list[nCell].dwDuration);
				if (nCell!=nLastCell || m_bCheckEndTime )
					fprintf(fout,"%d\n",nFrames);
			}
		}
		fclose(fout);
	}

	m_nTotalFrames=nFrames;

	if (m_bCheckLog && m_bInProcess==true) OutputLog(nVid, 1 , MENUS);

	return iRet;
}

////////////////////////////////////////////////////////////////////////////////
/////////////   Demuxing Code : DEMUX BY CELLID
////////////////////////////////////////////////////////////////////////////////
int CPgcDemuxApp::CIDDemux(int nCell)
{
	int nTotalSectors;
	int nSector;
	int k;
	int CID,VID;
	__int64 i64IniSec,i64EndSec;
	__int64 i64sectors;
	int nVobin;
        stringstream csAux;
	string csAux2;
	FILE *in, *fout;
	__int64 i64;
	bool bMyCell;
	int iRet;
	int nFrames;

	if (nCell >= int(m_AADT_Cell_list.size()))
	{
		MyErrorBox("Error: Selected Cell does not exist");
		m_bInProcess=false;
		return -1;
	}

	IniDemuxGlobalVars();
	if (OpenVideoFile()) return -1;
	m_bInProcess=true;

// Calculate  the total number of sectors
	nTotalSectors= m_AADT_Cell_list[nCell].iSize;
	nSector=0;
	iRet=0;

	VID=m_AADT_Cell_list[nCell].VID;
	CID=m_AADT_Cell_list[nCell].CID;

	i64IniSec=m_AADT_Cell_list[nCell].iIniSec;
	i64EndSec=m_AADT_Cell_list[nCell].iEndSec;
	for (k=1,i64sectors=0;k<10;k++)
	{
		i64sectors+=(m_i64VOBSize[k]/2048);
		if (i64IniSec<i64sectors)
		{
			i64sectors-=(m_i64VOBSize[k]/2048);
			nVobin=k;
			k=20;
		}
	}
	csAux2 = m_csInputIFO.substr(0, m_csInputIFO.size() - 5);
        csAux.str("");
        csAux << csAux2 << nVobin << ".VOB";
	in = fopen(csAux.str().c_str(), "rb");
	if (in ==NULL)
	{
		MyErrorBox(("Error opening input VOB: " + csAux.str()).c_str());
		m_bInProcess=false;
		iRet=-1;
	}
	if (m_bInProcess) fseek(in, (long) ((i64IniSec-i64sectors)*2048), SEEK_SET);

	for (i64=0,bMyCell=true;i64< (i64EndSec-i64IniSec+1) && m_bInProcess==true;i64++)
	{
	//readpack
		if ((i64%MODUPDATE) == 0) UpdateProgress((int)((100*nSector)/nTotalSectors) );
		if (readbuffer(m_buffer,in)!=2048)
		{
			if (in!=NULL) fclose (in);
			nVobin++;
			csAux2 = m_csInputIFO.substr(0, m_csInputIFO.size() - 5);
                        csAux.str("");
                        csAux << csAux2 << nVobin << ".VOB";
			in = fopen(csAux.str().c_str(), "rb");
			if (readbuffer(m_buffer,in)!=2048)
			{
				MyErrorBox("Input error: Reached end of VOB too early");
				m_bInProcess=false;
				iRet=-1;
			}
		}

		if (m_bInProcess==true)
		{
			if (IsSynch(m_buffer) != true)
			{
				MyErrorBox("Error reading input VOB: Unsynchronized");
				m_bInProcess=false;
				iRet=-1;
			}
			if (IsNav(m_buffer))
			{
				if (m_buffer[0x420]==(uchar)(VID%256) &&
					m_buffer[0x41F]==(uchar)(VID/256) &&
					m_buffer[0x422]==(uchar) CID)
					bMyCell=true;
				else
					bMyCell=false;
			}

			if (bMyCell)
			{
				nSector++;
				iRet=ProcessPack(true);
			}
		}
	} // For readpacks
	if (in!=NULL) fclose (in);
	in=NULL;

	CloseAndNull();

	nFrames=0;

	if (m_bCheckCellt && m_bInProcess==true)
	{
		csAux.str(m_csOutputPath + '/' + "Celltimes.txt");
		fout = fopen(csAux.str().c_str(), "w");
		nFrames=DurationInFrames(m_AADT_Cell_list[nCell].dwDuration);
		if (m_bCheckEndTime )
			fprintf(fout,"%d\n",nFrames);
		fclose(fout);
	}

	m_nTotalFrames=nFrames;

	if (m_bCheckLog && m_bInProcess==true) OutputLog(nCell, 1, TITLES);

	return iRet;
}

int CPgcDemuxApp::CIDMDemux(int nCell)
{
	int nTotalSectors;
	int nSector;
	int CID,VID;
	__int64 i64IniSec,i64EndSec;
	string csAux,csAux2;
	FILE *in, *fout;
	__int64 i64;
	bool bMyCell;
	int iRet;
	int nFrames;

	if (nCell >= int(m_MADT_Cell_list.size()))
	{
		MyErrorBox("Error: Selected Cell does not exist");
		m_bInProcess=false;
		return -1;
	}

	IniDemuxGlobalVars();
	if (OpenVideoFile()) return -1;
	m_bInProcess=true;

// Calculate  the total number of sectors
	nTotalSectors= m_MADT_Cell_list[nCell].iSize;
	nSector=0;
	iRet=0;

	VID=m_MADT_Cell_list[nCell].VID;
	CID=m_MADT_Cell_list[nCell].CID;

	i64IniSec=m_MADT_Cell_list[nCell].iIniSec;
	i64EndSec=m_MADT_Cell_list[nCell].iEndSec;
	if (m_bVMGM)
	{
		csAux2 = m_csInputIFO.substr(0, m_csInputIFO.size() - 3);
		csAux=csAux2+"VOB";
	}
	else
	{
		csAux2 = m_csInputIFO.substr(0, m_csInputIFO.size() - 5);
		csAux=csAux2+"0.VOB";
	}
	in = fopen(csAux.c_str(), "rb");
	if (in ==NULL)
	{
		MyErrorBox(("Error opening input VOB: " + csAux).c_str());
		m_bInProcess=false;
		iRet=-1;
	}
	if (m_bInProcess) fseek(in, (long) ((i64IniSec)*2048), SEEK_SET);

	for (i64=0,bMyCell=true;i64< (i64EndSec-i64IniSec+1) && m_bInProcess==true;i64++)
	{
	//readpack
		if ((i64%MODUPDATE) == 0) UpdateProgress((int)((100*nSector)/nTotalSectors) );
		if (readbuffer(m_buffer,in)!=2048)
		{
			if (in!=NULL) fclose (in);
			MyErrorBox("Input error: Reached end of VOB too early");
			m_bInProcess=false;
			iRet=-1;
		}
		if (m_bInProcess==true)
		{
			if (IsSynch(m_buffer) != true)
			{
				MyErrorBox("Error reading input VOB: Unsynchronized");
				m_bInProcess=false;
				iRet=-1;
			}
			if (IsNav(m_buffer))
			{
				if (m_buffer[0x420]==(uchar)(VID%256) &&
					m_buffer[0x41F]==(uchar)(VID/256) &&
					m_buffer[0x422]== (uchar) CID)
					bMyCell=true;
				else
					bMyCell=false;
			}

			if (bMyCell)
			{
				nSector++;
				iRet=ProcessPack(true);
			}
		}
	} // For readpacks
	if (in!=NULL) fclose (in);
	in=NULL;

	CloseAndNull();

	nFrames=0;

	if (m_bCheckCellt && m_bInProcess==true)
	{
		csAux=m_csOutputPath+ '/' + "Celltimes.txt";
		fout = fopen(csAux.c_str(), "w");
		nFrames=DurationInFrames(m_MADT_Cell_list[nCell].dwDuration);
		if (m_bCheckEndTime )
			fprintf(fout,"%d\n",nFrames);
		fclose(fout);
	}

	m_nTotalFrames=nFrames;

	if (m_bCheckLog && m_bInProcess==true) OutputLog(nCell, 1 , MENUS);

	return iRet;
}



////////////////////////////////////////////////////////////////////////////////
/////////////   Aux Code : Log
////////////////////////////////////////////////////////////////////////////////
void CPgcDemuxApp::OutputLog(int nItem, int nAng, int iDomain)
{
	string csFilePath, csAux, csAux1, csAux2;
	int k;
	int AudDelay;

	csFilePath=m_csOutputPath+ '/' + "LogFile.txt";

        ofstream log(csFilePath.c_str(), ios_base::out | ios_base::trunc);

        log << "[General]" << endl;

	log << "Total Number of PGCs   in Titles=" << m_nPGCs << endl;
	log << "Total Number of PGCs   in  Menus=" << m_nMPGCs << endl;
	log << "Total Number of VobIDs in Titles=" << m_AADT_Vid_list.size() << endl;
	log << "Total Number of VobIDs in  Menus=" << m_MADT_Vid_list.size() << endl;
	log << "Total Number of Cells  in Titles=" << m_AADT_Cell_list.size() << endl;
	log << "Total Number of Cells  in  Menus=" << m_MADT_Cell_list.size() << endl;

	if (m_iMode==PGCMODE) csAux="by PGC";
	else if (m_iMode==VIDMODE) csAux="by VOB Id";
	else if (m_iMode==CIDMODE) csAux="Single Cell";

	log << "Demuxing   Mode=" << csAux << endl;

	if (iDomain==TITLES) csAux="Titles";
	else  csAux="Menus";

	log << "Demuxing Domain=" << csAux << endl;

	log << "Total Number of Frames=" << m_nTotalFrames << endl;

	if (m_iMode==PGCMODE)
	{
		log << "Selected PGC=" << nItem + 1 << endl;
		log << "Number of Cells in Selected PGC=" << (iDomain==TITLES ? m_nCells[nItem] : m_nMCells[nItem]) << endl;
		log << "Selected VOBID=" << "None" << endl;
		log << "Number of Cells in Selected VOB=" << "None" << endl;

	}
	if (m_iMode==VIDMODE)
	{
		log << "Selected VOBID=" << (iDomain==TITLES ? m_AADT_Vid_list[nItem].VID : m_MADT_Vid_list[nItem].VID) << endl;
		log << "Number of Cells in Selected VOB=" << (iDomain==TITLES ? m_AADT_Vid_list[nItem].nCells : m_MADT_Vid_list[nItem].nCells) << endl;
		log << "Selected PGC=" << "None" << endl;
		log << "Number of Cells in Selected PGC=" << "None" << endl;
	}
	if (m_iMode==CIDMODE)
	{
		log << "Selected VOBID=" << "None" << endl;
		log << "Number of Cells in Selected VOB=" << "None" << endl;
		log << "Selected PGC=" << "None" << endl;
		log << "Number of Cells in Selected PGC=" << "None" << endl;
	}

        log << endl << "[Demux]" << endl;

	log << "Number of Video Packs=" << m_nVidPacks << endl;
	log << "Number of Audio Packs=" << m_nAudPacks << endl;
	log << "Number of Subs  Packs=" << m_nSubPacks << endl;
	log << "Number of Nav   Packs=" << m_nNavPacks << endl;
	log << "Number of Pad   Packs=" << m_nPadPacks << endl;
	log << "Number of Unkn  Packs=" << m_nUnkPacks << endl;

        log << showbase << internal << setfill('0');

        log << endl << "[Audio Streams]" << endl;

	for (k=0;k<8;k++)
	{
                log << "Audio_" << k + 1 << "=";
		if (m_iFirstAudPTS[k])
			log << hex << setw(2) << uppercase << m_iAudIndex[k] << dec << setw(0) << nouppercase << endl;
		else
			log << "None" << endl;
        }

        log << endl << "[Audio Delays]" << endl;

	for (k=0;k<8;k++)
	{
		if (m_iFirstAudPTS[k])
		{
//			AudDelay=m_iFirstAudPTS[k]-m_iFirstVidPTS;
			AudDelay=m_iFirstAudPTS[k]-m_iFirstNavPTS0;

			if (AudDelay <0)
				AudDelay-=44;
			else
				AudDelay+=44;
			AudDelay/=90;
                        log << "Audio_" << k + 1 << "=" << AudDelay << endl;
		}
	}

        log << endl << "[Subs Streams]" << endl;

	for (k=0;k<32;k++)
	{
                log << "Subs_" << setw(2) << k+1 << setw(0) << "=";
		if (m_iFirstSubPTS[k])
			log << hex << setw(2) << uppercase << k + 0x20 << dec << setw(0) << nouppercase << endl;
		else
			log << "None" << endl;
	}
}

void CPgcDemuxApp::WritePack(uchar* buffer)
{
	stringstream csAux;

	if (m_bInProcess==true)
	{
		if (m_bCheckVob2)
		{
			if  (fvob==NULL || m_nVidout != m_nCurrVid )
			{
				m_nCurrVid=m_nVidout;
				if (fvob != NULL) fclose(fvob);
                                csAux.str("");
				csAux << m_csOutputPath << '/';
				if (m_iDomain==TITLES)
					csAux << "VTS_01_1_" << setfill('0') << setw(3) << m_nVidout << ".VOB";
				else
					csAux << "VTS_01_0_" << setfill('0') << setw(3) << m_nVidout << ".VOB";
				fvob = fopen(csAux.str().c_str(), "wb");
			}
		}
		else
		{
			if  (fvob==NULL || ((m_i64OutputLBA)%(512*1024-1))==0 )
			{
				if (fvob != NULL) fclose(fvob);
                                csAux.str("");
				csAux << m_csOutputPath << '/';
				if (m_iDomain==TITLES)
				{
					m_nVobout++;
					csAux << "VTS_01_" << setw(0) << m_nVobout << ".VOB";
				}
				else
					csAux << "VTS_01_0.VOB";
				fvob = fopen(csAux.str().c_str(), "wb");
			}
		}

		if (fvob !=NULL) writebuffer(buffer,fvob, 2048);
		m_i64OutputLBA++;
	}

}


void  CPgcDemuxApp::CloseAndNull()
{
	int i;
	unsigned int byterate, nblockalign;
	struct  _stati64 statbuf;
	__int64 i64size;


	if (fvob !=NULL)
	{
		fclose(fvob);
		fvob=NULL;
	}
	if (fvid !=NULL)
	{
		fclose(fvid);
		fvid=NULL;
	}
	for (i=0;i<32;i++)
		if (fsub[i]!=NULL)
		{
			fclose(fsub[i]);
			fsub[i]=NULL;
		}
	for (i=0;i<8;i++)
		if (faud[i]!=NULL)
		{
			if (m_audfmt[i]==WAV)
			{
				i64size=0;
				fclose(faud[i]);

				if (_stati64 ( m_csAudname[i].c_str(), &statbuf)==0)
					i64size= statbuf.st_size;

				if (i64size >= 8) i64size-=8;

				faud[i] = fopen(m_csAudname[i].c_str(), "r+b");

				fseek(faud[i],4,SEEK_SET);
				fputc ((uchar)(i64size%256),faud[i]);
				fputc ((uchar)((i64size>>8) %256),faud[i]);
				fputc ((uchar)((i64size>>16)%256),faud[i]);
				fputc ((uchar)((i64size>>24)%256),faud[i]);

//				# of channels (2 bytes!!)
				fseek(faud[i],22,SEEK_SET);
				fputc ((uchar)(nchannels[i]%256),faud[i]);

//				Sample rate ( 48k / 96k in DVD)
				fseek(faud[i],24,SEEK_SET);
				fputc ((uchar)(fsample[i]%256),faud[i]);
				fputc ((uchar)((fsample[i]>>8) %256),faud[i]);
				fputc ((uchar)((fsample[i]>>16)%256),faud[i]);
				fputc ((uchar)((fsample[i]>>24)%256),faud[i]);

//				Byte rate ( 4 bytes)== SampleRate * NumChannels * BitsPerSample/8
//                    6000* NumChannels * BitsPerSample
				byterate=(fsample[i]/8)*nchannels[i]*nbitspersample[i];
				fseek(faud[i],28,SEEK_SET);
				fputc ((uchar)(byterate%256),faud[i]);
				fputc ((uchar)((byterate>>8) %256),faud[i]);
				fputc ((uchar)((byterate>>16)%256),faud[i]);
				fputc ((uchar)((byterate>>24)%256),faud[i]);


//				Block align ( 2 bytes)== NumChannels * BitsPerSample/8
				nblockalign=nbitspersample[i]*nchannels[i]/8;
				fseek(faud[i],32,SEEK_SET);
				fputc ((uchar)(nblockalign%256),faud[i]);
				fputc ((uchar)((nblockalign>>8) %256),faud[i]);

//				Bits per sample ( 2 bytes)
				fseek(faud[i],34,SEEK_SET);
				fputc ((uchar)(nbitspersample[i]%256),faud[i]);

				if (i64size >= 36) i64size-=36;
				fseek(faud[i],40,SEEK_SET);
//				fseek(faud[i],54,SEEK_SET);
				fputc ((uchar)(i64size%256),faud[i]);
				fputc ((uchar)((i64size>>8) %256),faud[i]);
				fputc ((uchar)((i64size>>16)%256),faud[i]);
				fputc ((uchar)((i64size>>24)%256),faud[i]);
			}
			fclose(faud[i]);
			faud[i]=NULL;
		}

}

int CPgcDemuxApp::check_sub_open (uchar i)
{
	stringstream csAux;

	i-=0x20;

	if ( i> 31) return -1;

	if (fsub[i]==NULL)
	{
                csAux.str("");
                csAux << m_csOutputPath << '/';
		csAux << "Subpictures_" << hex << setfill('0') << setw(2) << uppercase << i+0x20 << nouppercase << ".sup";
        if ((fsub[i] = fopen(csAux.str().c_str(), "wb")) == NULL)
		{
			MyErrorBox(("Error opening output subs file:" + csAux.str()).c_str());
			m_bInProcess=false;
			return 1;
		}
		else return 0;
	}
	else
	  return 0;
}

int CPgcDemuxApp::check_aud_open (uchar i)
{
	stringstream csAux;
	uchar ii;
/*
0x80-0x87: ac3  --> ac3
0x88-0x8f: dts  --> dts
0x90-0x97: sdds --> dds
0x98-0x9f: unknown
0xa0-0xa7: lpcm  -->wav
0xa8-0xaf: unknown
0xb0-0xbf: unknown
0xc0-0xc8: mpeg1 --> mpa
0xc8-0xcf: unknown
0xd0-0xd7: mpeg2 --> mpb
0xd8-0xdf: unknown
---------------------------------------------
SDSS   AC3   DTS   LPCM   MPEG-1   MPEG-2

 90    80    88     A0     C0       D0
 91    81    89     A1     C1       D1
 92    82    8A     A2     C2       D2
 93    83    8B     A3     C3       D3
 94    84    8C     A4     C4       D4
 95    85    8D     A5     C5       D5
 96    86    8E     A6     C6       D6
 97    87    8F     A7     C7       D7
---------------------------------------------
*/

	ii=i;

	if (ii <0x80) return -1;

	i=i&0x7;

	if (faud[i]==NULL)
	{
		csAux.str("");
                csAux << m_csOutputPath << '/' << "AudioFile_";
                csAux << hex << setw(2);

		if (ii >= 0x80 &&  ii <= 0x87)
		{
			csAux << uppercase << i + 0x80 << nouppercase << ".ac3";
			m_audfmt[i]=AC3;
		}
		else if (ii >= 0x88 &&  ii <= 0x8f )
		{
			csAux << uppercase << i + 0x88 << nouppercase << ".dts";
			m_audfmt[i]=DTS;
		}
		else if (ii >= 0x90 &&  ii <= 0x97)
		{
			csAux << uppercase << i + 0x90 << nouppercase << ".dds";
			m_audfmt[i]=DDS;
		}
		else if (ii >= 0xa0 &&  ii <= 0xa7)
		{
			csAux << uppercase << i + 0xa0 << nouppercase << ".wav";
			m_audfmt[i]=WAV;
		}
		else if (ii >= 0xc0 &&  ii <= 0xc7)
		{
			csAux << uppercase << i + 0xc0 << nouppercase << ".mpa";
			m_audfmt[i]=MP1;
		}
		else if (ii >= 0xd0 &&  ii <= 0xd7)
		{
			csAux << uppercase << i + 0xd0 << nouppercase << ".mpa";
			m_audfmt[i]=MP2;
		}
		else
		{
			csAux << uppercase << ii << nouppercase << ".unk";
			m_audfmt[i]=UNK;
		}

		m_csAudname[i] = csAux.str();

        if ((faud[i] = fopen(csAux.str().c_str(), "wb")) == NULL)
		{
			MyErrorBox(("Error opening output audio file:" + csAux.str()).c_str());
			m_bInProcess=false;
			return 1;
		}

		if (m_audfmt[i]==WAV)
		{
			fwrite(pcmheader,sizeof(uchar),44,faud[i]);
		}

		return 0;
	}
	else
	  return 0;
}



void  CPgcDemuxApp::demuxvideo(uchar* buffer)
{

	int start,nbytes;

	start=0x17+buffer[0x16];
	nbytes=buffer[0x12]*256+buffer[0x13]+0x14;

	writebuffer(&buffer[start],fvid,nbytes-start);

}

void  CPgcDemuxApp::demuxaudio(uchar* buffer, int nBytesOffset)
{
	int start,nbytes,i,j;
	int nbit,ncha;
	uchar streamID;
	uchar mybuffer[2050];

	start=0x17+buffer[0x16];
	nbytes=buffer[0x12]*256+buffer[0x13]+0x14;
	if (IsAudMpeg(buffer))
		streamID=buffer[0x11];
	else
	{
		streamID=buffer[start];
		start+=4;
	}

// Open File descriptor if it isn't open
	if (check_aud_open(streamID)==1)
	    return;

// Check if PCM
	if ( streamID>=0xa0 && streamID<= 0xa7 )
	{
		start +=3;

		if (nchannels[streamID & 0x7] == -1)
			nchannels[streamID & 0x7]=(buffer[0x17+buffer[0x16]+5] & 0x7) +1;

		nbit=(buffer[0x17+buffer[0x16]+5] >> 6) & 0x3;

		if (nbit==0) nbit=16;
		else if (nbit==1) nbit=20;
		else if (nbit==2) nbit=24;
		else nbit=0;

		if (nbitspersample[streamID & 0x7] ==-1)
			nbitspersample[streamID & 0x7]=nbit;
		if (nbitspersample[streamID & 0x7]!=nbit)
			nbit=nbitspersample[streamID & 0x7];

		if (fsample[streamID & 0x7] ==-1)
		{
			fsample[streamID & 0x7]=(buffer[0x17+buffer[0x16]+5] >> 4) & 0x3;
			if  (fsample[streamID & 0x7]==0 ) fsample[streamID & 0x7]=48000;
			else  fsample[streamID & 0x7]=96000;
		}

		ncha=nchannels[streamID & 0x7];
		if (nbit==24)
		{
			for (j=start; j< (nbytes-6*ncha+1) ; j+=(6*ncha))
			{
				for (i=0; i<2*ncha; i++)
				{
					mybuffer[j+3*i+2]=buffer[j+2*i];
					mybuffer[j+3*i+1]=buffer[j+2*i+1];
					mybuffer[j+3*i]=  buffer[j+4*ncha+i];
				}
			}

		}
		else if ( nbit==16 )
		{
			for (i=start; i< (nbytes-1) ; i+=2)
			{
				mybuffer[i]=buffer[i+1];
				mybuffer[i+1]=buffer[i];
			}
		}
		else if (nbit==20)
		{
			for (j=start; j< (nbytes-5*ncha+1) ; j+=(5*ncha))
			{
				for (i=0; i<ncha; i++)
				{
					mybuffer[j+5*i+0] = (hi_nib(buffer[j+4*ncha+i])<<4) + hi_nib(buffer[j+4*i+1]);
					mybuffer[j+5*i+1] = (lo_nib(buffer[j+4*i+1])<<4) + hi_nib(buffer[j+4*i+0]);
					mybuffer[j+5*i+2] = (lo_nib(buffer[j+4*i+0])<<4) + lo_nib(buffer[j+4*ncha+i]);
					mybuffer[j+5*i+3] = buffer[j+4*i+3];
					mybuffer[j+5*i+4] = buffer[j+4*i+2];
				}
			}
		}

		if ((nbit==16 && ((nbytes-start)%2)) ||
			(nbit==24 && ((nbytes-start)%(6*ncha))) ||
			(nbit==20 && ((nbytes-start)%(5*ncha))) )

			MyErrorBox("Error: Uncompleted PCM sample");

// if PCM do not take into account nBytesOffset
		writebuffer(&mybuffer[start],faud[streamID & 0x7],nbytes-start);
	}
	else
	{
// Very easy, no process at all, but take into account nBytesOffset...
		start+=nBytesOffset;
		writebuffer(&buffer[start],faud[streamID & 0x7],nbytes-start);

	}

}


void  CPgcDemuxApp::demuxsubs(uchar* buffer)
{
	int start,nbytes;
	uchar streamID;
	int k;
	uchar mybuff[10];
	int iPTS;

	start=0x17+buffer[0x16];
	nbytes=buffer[0x12]*256+buffer[0x13]+0x14;
	streamID=buffer[start];

	if (check_sub_open(streamID)==1)
	    return;
	if ((buffer[0x16]==0) || (m_buffer[0x15] & 0x80) != 0x80)
		writebuffer(&buffer[start+1],fsub[streamID & 0x1F],nbytes-start-1);
	else
	{
     // fill 10 characters
	    for (k=0; k<10;k++)
			mybuff[k]=0;

		iPTS=m_iSubPTS-m_iFirstNavPTS0+m_iOffsetPTS;

		mybuff[0]=0x53;
		mybuff[1]=0x50;
		mybuff[2]=iPTS%256;
		mybuff[3]=(iPTS >> 8)%256;
		mybuff[4]=(iPTS >> 16)%256;
		mybuff[5]=(iPTS >> 24)%256;

		writebuffer(mybuff,fsub[streamID & 0x1F],10);
		writebuffer(&buffer[start+1],fsub[streamID & 0x1F],nbytes-start-1);
	}
}
