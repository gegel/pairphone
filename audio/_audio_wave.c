 ///////////////////////////////////////////////
//
// **************************
//
// Project/Software name: X-Phone
// Author: "Van Gegel" <gegelcopy@ukr.net>
//
// THIS IS A FREE SOFTWARE  AND FOR TEST ONLY!!!
// Please do not use it in the case of life and death
// This software is released under GNU LGPL:
//
// * LGPL 3.0 <http://www.gnu.org/licenses/lgpl.html>
//
// You’re free to copy, distribute and make commercial use
// of this software under the following conditions:
//
// * You have to cite the author (and copyright owner): Van Gegel
// * You have to provide a link to the author’s Homepage: <http://torfone.org>
//
///////////////////////////////////////////////


//This file contains low-level procedure for wave audio
//Win32 system for X-Phone (PairPhone) project

//FOR WIN32 ONLY!

#include <basetsd.h>
#include <windows.h>
#include <commctrl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "ophmconsts.h"

#include "_audio_wave.h"
#define UNREF(x) (void)(x)

#define _DEFCONF "conf.txt"  //configuration filename
#define _SampleRate 48000  //Sample Rate
#define _BitsPerSample 16  //PCM16 mode
#define _Channels 1     // mono
#define _CHSIZE 320*6 //chunk size in bytes (2 * sample125us)
#define _CHNUMS 16  //number of chunks for buffer size 2400 samples (300 mS) can be buffered
#define _ROLLMASK (_CHNUMS-1) //and-mask for roll buffers while pointers incremented
#define _MELP_CHSIZE 360*6
//=======================================================

HWAVEIN _In=0;                   // Wave input device
HWAVEOUT _Out=0;                 // Wave output device
WAVEFORMATEX _Format;          // Audio format structure
UINT _BytesPerSample;          // Bytes in sample (2 for PCM16)
HANDLE _WorkerThreadHandle=0;    // Wave thread
DWORD _WorkerThreadId=0;         // ID of wave thread

int _IsSound=0;   //flag: sound OK
int _IsGo=0;     //flag: input runs
int _Dev_InN=0;    //system input number device specified in configure file
int _Dev_OutN=0;   //system output number device specified in configure file
int _ChSize=_CHSIZE; //chunk size
int _ChNums=_CHNUMS; //number of chunks

//audio input
WAVEHDR *_Hdr_in[2]; //headers for 2 chunks
unsigned char _wave_in[_CHNUMS][_CHSIZE+40*6];  //output buffers for 16 frames 20 mS each
volatile unsigned char _p_in=0; //pointer to chunk will be returned by input device
unsigned char _g_in=0; //pointer to chunk will be readed by application
int _n_in=0; //number of bytes ready in this frame

//audio output
WAVEHDR *_Hdr_out[_CHNUMS]; //headers for each chunk
unsigned char _wave_out[_CHNUMS][_CHSIZE+40*6]; //output buffers for 16 frames 20 mS each
unsigned char _p_out=0;  //pointer to chunk will be passed to output device
int _n_out=0;  //number of bytes already exist in this frame
volatile unsigned char _g_out=0;  //pointer to chunk will be returned by output device

//_Internal procedures
int _OpenDevices (void); //open in & out devices
void _StopDevices (void); //paused
void _CloseDevices (void); //closed devised (finalize)
void _ReleaseBuffers(void); //de - allocate memory

int _rdcfg(void); //read configure file and select devices from list
void _dlg_init(void); //ini audio
int _dlg_start(void); //create tread

//Transcode cp866(ru) to latin
void _r2tru(char* in, char* out)
{
 static char tbl_win[64]={
     -32, -31, -30, -29, -28, -27, -26, -25,
     -24, -23, -22, -21, -20, -19, -18, -17,
     -16, -15, -14, -13, -12, -11, -10, -9,
     -8, -7, -4, -5, -6, -3, -2, -1,
     -64, -63, -62, -61, -60, -59, -58, -57,
     -56, -55, -54, -53, -52, -51, -50, -49,
     -48, -47, -46, -45, -44, -43, -42, -41,
     -40, -39, -36, -37, -38, -35, -34, -33
 };
 static char* tbl_trn="abvgdezzijklmnoprstufhccss'y'ejjABVGDEZZIIKLMNOPRSTUFHCCSS'I'EJJ";
 int i,j;
 char*p=out;

 for(i=0;i<1+(int)strlen(in); i++)
 {
  if(in[i]>=0) (*p++)=in[i];
  else
  {
   for(j=0; j<64; j++)
   {
    if(in[i]==tbl_win[j])
    {
     (*p++)=tbl_trn[j];
     if((j==6)||(j==23)||(j==24)||(j==38)||(j==55)||(j==56)) (*p++)='h'; //zh, ch, sh
     if((j==25)||(j==57)) (*p++)='c'; //sc
     if((j==30)||(j==62)) (*p++)='u'; //ju
     if((j==31)||(j==63)) (*p++)='a'; //sc
     break;
    }
    if(j==64) (*p++)='?';
   }
  }
 }
}


//=============================Level 0==================


//*****************************************************************************
//Parse configure file for param and copy value to param, return length of value
//zero if not found and error code if no configure file
int  _parseconfa(char* param)
{
 FILE *fpp;
 char buf[256];
 char* p=NULL;
 int i;

 //open configuration file
 fpp = fopen(_DEFCONF, "rt");
 if (fpp == NULL)
 {
  perror("Cannot open configure file");
  return -1;
 }
  //read it sting-by-string
  while( fgets(buf, sizeof(buf), fpp) )
  {
   if((buf[0]=='#')||(buf[0]==0)) continue; //skip comments and emty stings
   p=strstr(buf, param); //search specified parameter in string
   if(!p) continue; //process next string up to eof
   p=strchr(buf, '='); //search separator
   if(!p) continue; //no value
   p++; //set pointer to value
   break;
  }
  fclose(fpp);
  param[0]=0; //clear input string
  if(p) //if parameter found
  {  //truncate value string to first space or end of string
   for(i=0;i<(int)strlen(p);i++)
   if( (p[i]=='\r')||(p[i]=='\n')||(p[i]==' ') ) break;
   p[i]=0;
   strncpy(param, p, 31); //replace input parameter by it's value
   param[31]=0;
  }
  return (strlen(param)); //length of value's string or null
}



//read configuration file and get specified input and output device numbers
int _rdcfg(void)
{
 //FILE *fpp;
 char buf[256];
 char* p=0;
 //set default devices for use
 _Dev_InN=0;
 _Dev_OutN=0;
 //Load buffer parameters
 strcpy(buf, "_AudioChunks");
 if(0>=_parseconfa(buf)) strcpy(buf, "#");
 if(buf[0]!='#') p=strchr(buf, '*');  //pointer to ascii chunk numbers
 if(p)
 {
   _ChNums=0;  //number of chunks
   _ChSize=0; //chunk size in bytes
   p[0]=0;
   _ChNums=atoi(++p); //to integer
   if(_ChNums<2) _ChNums=2; //defaults
   _ChNums=ceil(log2(_ChNums));
   _ChNums=ceil(exp2(_ChNums));
   _ChSize=2*atoi(buf);
   if(_ChSize<80) _ChSize=80; //defaults
 }

 strcpy(buf, "_NPP7");
 _parseconfa(buf);
 if(buf[0]=='1') _ChSize=_MELP_CHSIZE;
 if(_ChSize>(_CHSIZE+40*6)) _ChSize=_CHSIZE+40*6;
 printf("Period size %d, Buffer size %d\r\n", _ChSize/2, _ChNums*_ChSize/2);

 strcpy(buf, "_AudioInput"); //number of Audio_In in list
 if(0<_parseconfa(buf))
 {
   p=strchr(buf, ':'); //search separator
   if(p) _Dev_InN=atoi(++p); //string to integer from next char after it
 }
 strcpy(buf, "_AudioOutput"); //number of Audio_Out in list
 if(0<_parseconfa(buf))
 {
   p=strchr(buf, ':');  //search separator
   if(p) _Dev_OutN=atoi(++p); //string to integer from next char after it
 }
 printf("%d/%d in/out wave devices used\r\n", _Dev_InN, _Dev_OutN);
 return 0;
}


//*****************************************************************************
//discover wave input and output in system and print devices list
void _dlg_init(void)
{
 int NumWaveDevs;
 int i;
 char str[256];
 
 printf("\r\n------------------Audio input-----------------\r\n");
 //get number of wave input devices in system
 NumWaveDevs = waveInGetNumDevs ();
 for (i = 0; i < NumWaveDevs + 1; i++)
 {
  WAVEINCAPS DevCaps;
  waveInGetDevCaps (i - 1, &DevCaps, sizeof (DevCaps)); //get devices name
  _r2tru(DevCaps.szPname, str);
  printf("WaveInDevice %d: %s\r\n", i, str);

 }
 
 printf("\r\n-----------------Audio output-----------------\r\n");
 //get number of wave output devices in system
 NumWaveDevs = waveOutGetNumDevs ();
 for (i = 0; i < NumWaveDevs + 1; i++)
 {
  WAVEOUTCAPS DevCaps;
  waveOutGetDevCaps (i - 1, &DevCaps, sizeof (DevCaps)); //get devices name
  _r2tru(DevCaps.szPname, str);
  printf("WaveOutDevice %d: %s\r\n", i, str); //print it
 }
}

//*****************************************************************************
//Stop audio input/output
void _StopDevices (void)
{
  waveInReset (_In);
  waveOutReset (_Out);
}

//*****************************************************************************
//Close audio input/output
void _CloseDevices (void)
{
 if (_In) waveInClose (_In);
 _In = NULL;
 if (_Out) waveOutClose (_Out);
 _Out = NULL;
}

//*****************************************************************************
//release dynamically allocated wave header
void _ReleaseBuffers(void)
{
 volatile int i;
 if(_WorkerThreadId) PostThreadMessage (_WorkerThreadId, WM_QUIT, 0, 0);
 for(i=0;i<_CHNUMS; i++) if(_Hdr_out[i]) LocalFree ((HLOCAL)_Hdr_out[i]);
 for(i=0;i<_CHNUMS; i++) if(_Hdr_in[i]) LocalFree ((HLOCAL)_Hdr_in[i]);
}

//*****************************************************************************
//Open and init wave input and output
int _OpenDevices (void)
{
  MMRESULT Res;
  volatile int i;
  WAVEHDR *Hdr;

  InitCommonControls (); //init system controls
  _In = NULL; //clear devices pointers
  _Out = NULL;
  //set audio format
  _BytesPerSample = (_BitsPerSample + 7) / 8;
  _Format.wFormatTag = WAVE_FORMAT_PCM;
  _Format.nChannels = _Channels;
  _Format.wBitsPerSample = _BitsPerSample;
  _Format.nBlockAlign = (WORD)(_Channels * _BytesPerSample);
  _Format.nSamplesPerSec = _SampleRate;
  _Format.nAvgBytesPerSec = _Format.nSamplesPerSec * _Format.nBlockAlign;
  _Format.cbSize = 0;

  //---------------------Open Devices----------------
  //_Output
  Res = waveOutOpen (
      (LPHWAVEOUT)&_Out,
      _Dev_OutN - 1,
      &_Format,
      (DWORD)_WorkerThreadId,
      0,
      CALLBACK_THREAD
    );
  if (Res != MMSYSERR_NOERROR) return FALSE;
  waveOutPause (_Out); //stop output device

  //_Input
  Res = waveInOpen (
      (LPHWAVEIN)&_In,
      _Dev_InN - 1,
      &_Format,
      (DWORD)_WorkerThreadId,
      0,
      CALLBACK_THREAD
    );
  if (Res != MMSYSERR_NOERROR) return FALSE;

  //------------Allocates wave headers----------------

  //For output device allocate header for each frame in buffer
  for(i=0; i<_CHNUMS; i++)
  {
   Hdr = (WAVEHDR *)LocalAlloc (LMEM_FIXED, sizeof (*Hdr));
   if (Hdr)
   {
    Hdr->lpData = (char*)_wave_out[i];
    Hdr->dwBufferLength = _ChSize;
    Hdr->dwFlags = 0;
    Hdr->dwLoops = 0;
    Hdr->dwUser = 0;
    _Hdr_out[i]=Hdr;
   }
   else
   {
    for(i=0;i<_CHNUMS; i++) if(_Hdr_out[i]) LocalFree ((HLOCAL)_Hdr_out[i]);
    return 0;
   }
  }
  //init output pointers
   _p_out=0;
   _g_out=0;
   _n_out=0;

  //-------------------------init wave input---------------
  //Allocates 2 headers
  for(i=0; i<2; i++)
  {
   Hdr = (WAVEHDR *)LocalAlloc (LMEM_FIXED, sizeof (*Hdr));
   if (Hdr)
   {
    Hdr->lpData = (char*)_wave_in[i];
    Hdr->dwBufferLength = _ChSize;
    Hdr->dwFlags = 0;
    Hdr->dwLoops = 0;
    Hdr->dwUser = 0;
    _Hdr_in[i]=Hdr;
    waveInPrepareHeader (_In, Hdr, sizeof (WAVEHDR));
    Res = waveInAddBuffer (_In, Hdr, sizeof (WAVEHDR));
   }
   if( (!Hdr) || (Res != MMSYSERR_NOERROR) )
   {
    for(i=0;i<_CHNUMS; i++) if(_Hdr_out[i]) LocalFree ((HLOCAL)_Hdr_out[i]);
    for(i=0;i<_CHNUMS; i++) if(_Hdr_in[i]) LocalFree ((HLOCAL)_Hdr_in[i]);
    return 0;
   }
  }
  //init input pointers
  _p_in=0;
  _g_in=0;
  _n_in=0;
  return 1;
}

//========================Top level=================

//*****************************************************************************
//init wave devices
int _soundinit(void)
{
  _IsSound=0; //set sound flag to OK
  _dlg_init(); //print list of available sound devices
  
  printf("\r\n--------------Initialise Line audio-----------\r\n");
  _rdcfg();  //read sound devices numbers from configure file
  _IsSound=_dlg_start(); //open wave input and wave output
  return _IsSound;
}

//*****************************************************************************
//terminate wave devices
void _soundterm(void)
{
 CloseHandle (_WorkerThreadHandle); //stop audio thread
 _WorkerThreadId = 0;
 _WorkerThreadHandle = NULL;
 //_StopDevices ();  //paused
 _CloseDevices (); //close devices
 Sleep(500); //some time for close complete
 _ReleaseBuffers(); //release memory
 //SetPriorityClass (GetCurrentProcess (), DefPrioClass); //restore priority of main procedure
 printf("Wave Thread stopped!\r\n");
 _IsSound=0;
}

//*****************************************************************************
//Get number of samples in output queue
int _getdelay(void)
{
 int i=(int)_g_out; //read volatile value: pointer to next buffer will be released
 i=-i;
 i=i+_p_out; //buffers in queue now
 if(i<0) i=_CHNUMS+i; //correct roll
 i=i-2; //skip two work buffers
 if(i<0) i=0; //correct
 i=i*_ChSize; //bytes in queue
 i=i+_n_out; //add tail in current buffer
 i=i/2; //samples in queue
 return i;
}

//*****************************************************************************
//get number of samples in chunk (frame)
int _getchunksize(void)
{
 return _ChSize/2;
}

//*****************************************************************************
//get total buffers size in samples
int _getbufsize(void)
{
 return ((_ChSize*(_CHNUMS-1))/2);
}

//*****************************************************************************
//skip all samples grabbed before
void _soundflush(void)
{
 //in: not released yet
}

//*****************************************************************************
//skip all unplayed samples
void _soundflush1(void)
{
 //out: not released yet
}

//*****************************************************************************
//------------------------------------------------
//grab up to len samples from wave input device
//return number of actually got samples
int _soundgrab(char *buf, int len)
{
 int i, l, d=0;
 unsigned char cpp, cp=_p_in; //read volatile value
 //_p_in points to first frame in work now
 //_p_in+1 frame also in work
 //_p_in-1 frame was last returned by input device

 //_g_in points to the most oldest unread frame
 //_n_in is number of unread bytes in it (tail)

 if(_IsSound && _IsGo)//check for device opened
 {
  cpp=cp+1;  //pointer to buffer passed to input device
  cpp&=_ROLLMASK;
  l=len*2; //length in bytes (for 16 bit audio samples)
  while(l>0) //process up to length
  { //check for pointed buffer not uses by input device now
   if((_g_in==cp)||(_g_in==cpp)) break; //2 chunks uses by input device at time
   i=_ChSize-_n_in; //ready bytes in this frame
   if(i>l) i=l;  //if we need less then exist
   memcpy(buf, &_wave_in[_g_in][_n_in], i); //copy to output
   d+=i; //bytes outed
   l-=i; //bytes remains
   _n_in+=i; //bytes processed in current frame
   if(_n_in>=_ChSize) //if all bytes of current frame processed
   {
    _g_in++;   //pointer to next frame
    _g_in=_g_in&_ROLLMASK; //roll mask (16 frames total)
    _n_in=0;  //no byte of this frame has not yet been read
   }
  }
 }
 //if(!d) Sleep(20);
 return (d/2); //returns number of outputted frames
}


//*****************************************************************************
//pass up to len samples from buf to wave output device for playing
//returns number of samples actually passed to wave output device
int _soundplay(int len, unsigned char *buf)
{
 #define STARTDELAY 2 //number of silence chunk passed to output before playing
 int d=0; //bytes passed
 int i, l;
 unsigned char cp, cg=_g_out; //read volatile value
 //_g_out points to last played (and empty now) frame
 //_p_out points to frame for writing data now
 //at-least one frame must be played at time (normally two and more)
 //if no frames played at time (_g_out==_p_out) that is underrun occurred
 //and we must push some frames of silence (for prevention next underrun)
 //and restart playing

 if(!_IsSound) return 0;  //check for device ready
 if(_p_out==cg) //underrun occurred
 {
  for(i=0;i<STARTDELAY;i++) //pass to device some silence frames
  {
   memset(_wave_out[i], 0, _ChSize); //put silence to first frame
   waveOutPrepareHeader (_Out, _Hdr_out[i], sizeof (WAVEHDR)); //prepare header
   waveOutWrite (_Out, _Hdr_out[i], sizeof (WAVEHDR));    //pass it to wave output device
  }
  _g_out=0; //reset pointer of returned headers
  _p_out=i; //set pointer to next header to pass to device
  _n_out=0; //it is empty
  waveOutRestart (_Out); //restart wave output process
  return -1; //return error code
 }

 l=len*2; //length in bytes (for 16 bit audio samples)
 while(l) //process all input data
 {
  cp=_p_out+1; //pointer to next frame
  cp=cp&_ROLLMASK; //roll mask
  if(cp==cg) break; //if next frame not returned yet (buffer full)
  i=_ChSize-_n_out; //number of empty bytes in this frame
  if(i>l) i=l; //if we have less bytes then empty
  memcpy(&_wave_out[_p_out][_n_out], buf+d, i); //copy input data to frame
  l-=i; //remain data bytes
  d+=i; //processed bytes
  _n_out+=i; //empty bytes remains in current frame
  if(_n_out>=_ChSize) //if chunk full
  {
   waveOutPrepareHeader (_Out, _Hdr_out[_p_out], sizeof (WAVEHDR)); //prepare header
   waveOutWrite (_Out, _Hdr_out[_p_out], sizeof (WAVEHDR));    //pass it to wave output device
   _p_out=cp; //pointer to next chunk
   _n_out=0;  //all bytes in this frame are empty
  }
 } //while(l)
 d=d/2;
 return (d);  //returns number of accepted samples
}


//*****************************************************************************
//===================Wave task========================

DWORD WINAPI _WorkerThreadProc (void *Arg)
{
  MSG Msg; //system message
  //Set hight priority
  SetThreadPriority (GetCurrentThread (), THREAD_PRIORITY_TIME_CRITICAL);
  //work loop: wait for Message
  while (GetMessage (&Msg, NULL, 0, 0) == TRUE)
  {
   switch (Msg.message) //process system message from HW wave devices
   {
    case MM_WIM_DATA: //wave input buffer complete
    {
     WAVEHDR *Hdr = (WAVEHDR *)Msg.lParam;
     unsigned char bc;

     waveInUnprepareHeader (_In, Hdr, sizeof (*Hdr)); //unprepare returned wave header
     //_p_in is a pointer to returned frame
     bc=_p_in+1; //computes pointer to next frame (normally in use now)
     bc&=_ROLLMASK;  //roll mask
     _p_in=bc;  //set it as a pointer to next frame will be returns
     bc++;     //computes pointer to next frame for passes to input device
     bc&=_ROLLMASK; //roll mask
     //returns header with next frame to input device
     Hdr->lpData=(char*)_wave_in[bc]; //attach next buffer to this header
     waveInPrepareHeader (_In, Hdr, sizeof (*Hdr)); //prepare header
     waveInAddBuffer (_In, Hdr, sizeof (*Hdr)); //back to input device
     break;
    }

    case MM_WOM_DONE:  //wave output buffer played
    {
     WAVEHDR *Hdr = (WAVEHDR *)Msg.lParam;
     unsigned char bc;

     waveOutUnprepareHeader (_Out, Hdr, sizeof (*Hdr)); //unprepare returned wave header
     bc=_g_out; //pointer to returned header
     bc++; //pointer to next frame: normally it was early passed to device
     _g_out=bc&_ROLLMASK; //roll mask
     break;
    }//if(f_out[_g_out]==1)
   } //switch (Msg.message)
  }  //while (GetMessage (&Msg, NULL, 0, 0) == TRUE)
  UNREF (Arg);
  return 0;
}
//===============end of wave task=======================


//*****************************************************************************
//start audio
int _dlg_start(void)
{
   int Success=0;
   //up priority of main task WARNING: there are some problems of Win32 priority!!!
   //SetPriorityClass (GetCurrentProcess (), HIGH_PRIORITY_CLASS);
   //creates wave thread
   _WorkerThreadHandle = CreateThread (
              NULL,
              0,
              _WorkerThreadProc,
              NULL,
              0,
              &_WorkerThreadId
            );
   //open wave devices
   Success = _OpenDevices ();
/*   if (Success)
   {  //start audio input
    if (waveInStart(_In) != MMSYSERR_NOERROR)
    {  //if no inputs
     _CloseDevices ();
     _IsSound=0;
     printf("Starting audio input failed\r\n");
    }
   } */
   return Success;  //For PairPhone project: we can have only Line device (without Headset) in test mode
}

//start/stop audio input
int _soundrec(int on)
{
 if(on!=_IsGo)
 {
  MMRESULT Res;

  if(on) Res = waveInStart(_In); //start audio input
  else Res = waveInStop(_In);   //stop audio input
  if(Res != MMSYSERR_NOERROR) _IsGo=0; //error
  else _IsGo=on;                       //set flag
  //printf("Rec=%d\r\n", _IsGo);
 }
 return _IsGo;                        //return status
}
