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


//This file contains receiving procedures for PairPhone:
//We records some 48KHz audio samples from Line, collects in the input buffer,
//synchronously demodulates while receives full 81 bits block,
//check for data type is voice or control, decrypt/decode or put silence,
//resampled depends actual buffer state and play 8KHz audio over Speaker.

//Estimated overall voice latency

//On the transmission side:
//delay of recording Mike buffer is 22.5mS
//delay of MELPE coder frame is 67.5 mS
//delay of Line playing buffer is 45 mS

//On the transport: channel latency of GSM depends external conditions

//On the receiving side:
//delay of Line receiving buffer is 22.5 mS
//delay of MELPE decoder frame is 67.5 mS
//delay of Speaker playing buffer is 45 mS

//Total latency average 270 mS + GSM latency (typically 180 mS)



#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32

#include <stddef.h>
#include <stdlib.h>
#include <basetsd.h>
#include <stdint.h>

 #include <windows.h>
 #include <time.h>
 
 #else
 #include <time.h>
 #include <sys/time.h>
 #include <sys/types.h>
 #include <sys/stat.h>
 
#endif


#include "audio/audio.h"  //low-level audio input/output
#include "modem/modem.h"  //modem
#include "melpe/melpe.h"  //audio codec
#include "crp.h"          //data processing
#include "rx.h"           //this

//Global variables

 //baseband processing
 static short speech[360*6];    //PCM 48KHz input buffer for samples received from the line
 static short *samples=speech;  //pointer to samples ready for processing
 static int cnt=0;              //the number of unprocessed samples
 static unsigned char buf[12];  //demodulators data output
 short sp[544]; //ouputted speech frame

 //resampling
 float qff=1.0;  //resampling ratio
 static float up_pos=1.0; //resampler fractional position
 static short left_sample=0; //base sample
  
 //playing
 static short jit_buf[800]; //PCM 8KHz buffer for samples ready for playing over Speaker
 //static short* p_jit_buf=jit_buf; //pointer to unplayed samples in the buffer
 static short l_jit_buf=0; //number of unplayed samples in the buffer 
 static int q_jit_buf=0; //pointer to unplayed samples in the buffer
 static float fdelay=7200; //averages playing delay
 //accumulators
 static float fber=0; //Average bit error rate
 static float fau=0;  //Average authentication level
 
//internal procedures 
 static int resample(short* src, short* dest, float fstep); //resumpling before playing
 static int playjit(void); //playing buffered samples

////////////////////////////////////////////////////////////////////////////
//for TEST
//**********************************************************
 int rxbit=0;
 int rxerr=0;
 static float verrs=0;
 //bits set lookup table
 static const unsigned char BitsSetTable256[256] =
 {
  #   define B2(n) n,     n+1,     n+1,     n+2
  #   define B4(n) B2(n), B2(n+1), B2(n+1), B2(n+2)
  #   define B6(n) B4(n), B4(n+1), B4(n+1), B4(n+2)
    B6(0), B6(1), B6(1), B6(2)
 };

 //emulation of encoder for test
void melpe_s_emu( short* ssp, unsigned char* sbuf)
{
 int i=0;
 int j=0;
 float fi;
 
 for(i=0;i<10;i++) j+=BitsSetTable256[sbuf[i]];
 verrs*=0.9;
 verrs+=j;
 rxbit+=80;
 rxerr+=j;
 
 for(i=0;i<540;i++)
 {
  fi=i;
  fi=sin(2*M_PI*fi/9);
  ssp[i]=(short)(4000*fi);
 }

}

//Change samples rate (from PGPFone)
int RateChange(short *src, short *dest, int srcLen, int srcRate, int destRate)
{
        //int srcRate = 8000;
        short *sourceShortPtr;
	short *destShortPtr;
	float nSrcRate, nDestRate;
	nSrcRate = srcRate;
	nDestRate = destRate;

	// All samples are 16 bit signed values. Pass in the number of source samples,
	// the sample rate of the source data, and the required destination sample
	// rate.
	if(!srcLen)
		return 0;
	// If the sample rates are identical, just copy the data over
	if(srcRate == destRate)
	{
		memcpy((char*)dest, (char*)src, srcLen*2);
		return srcLen;
	}
	sourceShortPtr = src;
	destShortPtr = dest;

	// Downsample
	if(srcRate > destRate)
	{
		float destStep = nDestRate / nSrcRate;
		float position = 0;

		while(srcLen)
		{
			int destSample = 0;
			int count = 0;
			
			// Accumulate source samples, until the fractional destination position
			// crosses a boundary (or we run out of source data)
			while(srcLen && (position < 1.0))
			{
				destSample += *sourceShortPtr++;
				srcLen--;
				position += destStep;
				count++;
			}
			position = position - 1.0;
			*destShortPtr++ = (destSample/count);
		}
	}
	else // Upsample
	{
		float sourceStep = nSrcRate / nDestRate;
		float position = 0;

		while(--srcLen)
		{
			int leftSample = *sourceShortPtr++;
			int sampleDifference = *sourceShortPtr-leftSample;

			while(position < 1.0)
			{
				*destShortPtr++ = leftSample + ((float)sampleDifference * position);
				position += sourceStep;
			}
			position = position - 1.0;
		}
	}
	// Return the number of samples written
	// Note that this code will sometimes (often?) write one sample too many, so make
	// sure your destination buffer is oversized by one.
	return destShortPtr - dest;
}
//End of test area
////////////////////////////////////////////////////////////////////////////


//*****************************************************************************
//----------------Streaming resampler--------------------------------------------
static int resample(short* src, short* dest, float fstep)
{
 //resampled MELPE frame (540 short 8KHz samples) to specified rate
 //input: pointer to source and resulting short samples, resulting ratio
 //output: samples in dest resumpled from 8KHz to specified sample rate
 //returns: resulting length in samples

 int i, diff=0;
 short* sptr=src; //source
 short* dptr=dest; //destination

 //process 540 samples
 for(i=0;i<540;i++) //process MELPE frame
 {
  diff = *sptr-left_sample; //computes difference between current and basic samples
  while(up_pos <= 1.0) //while position not crosses a boundary
  {
   *dptr++ = left_sample + ((float)diff * up_pos); //set destination by basic, difference and position
    up_pos += fstep; //move position forward to fractional step
  }
  left_sample = *sptr++; //set current sample as a  basic
  up_pos = up_pos - 1.0; //move position back to one outputted sample
 }
 return dptr-dest;  //number of outputted samples
}

//*****************************************************************************
//*****************************************************************************
//--Playing over Speaker----------------------------------
static int playjit(void)
{
  //play 8KHz samples in buffer over Speaker
  int i=0;
  int job=0;

  if(l_jit_buf>0) //we have unplayed samples, try to play
  {
   i=soundplay(l_jit_buf, (unsigned char*)(jit_buf+q_jit_buf)); //play, returns number of played samples
   if(i) job+=2; //set job
   if((i<0)||(i>l_jit_buf)) i=0; //must play again if underrun (PTT mode etc.)
   l_jit_buf-=i; //decrease number of unplayed samples
   if(l_jit_buf<0) l_jit_buf=0;
   q_jit_buf+=i; //move pointer to unplayed samples
   if((l_jit_buf<180)&&(q_jit_buf>0)) //all samples played
   {
    //memcpy((char*)jit_buf, (char*)(jit_buf+q_jit_buf), 2*l_jit_buf);
    for(i=0;i<l_jit_buf;i++) jit_buf[i]=jit_buf[i+q_jit_buf]; //move tail
    q_jit_buf=0; //move pointer to the start of empty buffer
   }
  }
  return job; //job flag
}

//*****************************************************************************
//receiving loop: grab 48KHz baseband samples from Line,
//demodulate, decrypt, decode, play 8KHz voice over Speaker
int rx(int typing)
{
 //input: -1 for no typing chars, 1 - exist some chars in input buffer
 //output: 0 - no any jobs doing, 1 - some jobs were doing
 int i;
 float f;
 int job=0; //flag of any job were doing 
 char lag_flag=0; //block lag is locked (modems synchronization complete)
 //char lock_flag=0; //phase of carrier (1333Hz, 6 samples per period) is locked
 //char sync_flag=0; //the difference of frequency transmitter-to-receiver sampling rate is locked
 //char current_lag=0;  //block lag (0-90, aligned to last bit position, the 6 samples for bit)

 char info[8]={0}; //call info

 //regularly play Speaker's buffer
 job=playjit(); //the first try to play a tail of samples in buffer
 //check for we have enough samples for demodulation
 if(cnt<180*6) //check we haven't enough of unprocessed samples
 {
  //move tail to start of receiving buffer
  if(samples>speech) //check for tail
  {
   for(i=0; i<cnt; i++) speech[i]=samples[i]; //move tail to start of buffer
   samples=speech; //set pointer to start of buffer
  }
  //record
  i=_soundgrab((char*)(samples+cnt), 180*6);  //try to grab new 48KHZ samples from Line

  if((i>0)&&(i<=(180*6))) //some samples grabbed
  {
   cnt+=i;  //add grabbed  samples to account
   job+=4;  //set job
  }
 }
 else //we have enough samples for processing
 {
  i=Demodulate(samples, buf); //process samples: 36*6 (35-37)*6 samples
  samples+=i; //move pointer to next samples (with frequency adjusting)
  cnt-=i; //decrease the number of unprocessed samples
  if(0x80&buf[11]) //checks flag for output data block is ready
  {
   //check for synck and averages BER
   lag_flag=!(!(buf[11]&0x40)); //block lag is locked (synchronization compleet)
   //lock_flag=!(!(buf[11]&0x20)); //phaze of carrier (1333Hz, 6 samples per period) is locked
   //sync_flag=!(!(buf[11]&0x10)); //the differency of frequency transmitter-to-receiver sampling rate is locked
   //current_lag=buf[10]>>1;  //block lag (0-90, aligned to last bit position, the 6 samples for bit)
   if(lag_flag) //check modem sync
   {
    //averages BER
    i=(0x0F&buf[11]); //count symbols errors (only 1 error per 9-bit symbol can be detected)
    fber*=0.99; //fber in range 0-900
    fber+=i;  //in range 0-9 errored bits per 90 bits treceived
   }
   //output statistics
   if(typing<0) //output call's info if no characters were typed by user
   {
    f=Mute(0);   //get packets counter value
    i=State(0); //get current connection step * vad flag 
	
	//notification of state and voice output 
    if(!i) strcpy(info, (char*)"IDLE"); 
    else if(abs(i)<8) strcpy(info, (char*)"CALL");  
    else if(f<=0) strcpy(info, (char*)"MUTE");
    else if(i<0) strcpy(info, (char*)"PAUS");
    else strcpy(info, (char*)"TALK");

    if(f<0) f=-f; //absolute value
    i=f*0.0675; //computes total time of the call in sec: each packet 67,5 ms

    f=fau/4-100; //computes authentification level in %
    if(f<0) f=0; //only positive results have reason
    //current state notification
    if(lag_flag) printf("%s %dmin %dsec BER:%0.02f AU:%d%%\r", info, i/60, i%60, fber/90, (int)f);
    else printf("%s %dmin %dsec BER:---- AU:%d%%\r", info, i/60, i%60, (int)f); //lost of sync in modem
   }

   //process received packet detects voice/silence type
   buf[11]=0xFE; //set flag default as for silence descriptor
   if(lag_flag) //check modem sync
   {
    i=ProcessPkt(buf);  //decode received packet
    if(i>=0) //received packet is a control type
    {
     fau*=0.99;  //fau in range 0-800 (400 for random data)
     fau+=i; //averages authentication level
    }
    else if(i==-3) 
	{
	 buf[11]=0xFF; //set flag for voice data received 
	}
   } //end of sync ok, packets processing
  } //end of data block received
 } //end of a portion of sampless processing

 //check we have received data and output buffer is empty for decoding
 if((0x0E&buf[11])&&(l_jit_buf<=180))
 {
  //decode voice data or set silency
  job+=16; //set job
  if(1&buf[11]) //this is a voice frame, decode it
  {
   melpe_s(sp, buf); //decode 81 bits in 11 bytes to 540 8KHz samples 
  }
  else memset(sp, 0, 1080); //or output 67.5 mS of silence
  buf[11]=0; //clears flag: data buffer is processed

  //computes average playing delay
  i=getdelay()+l_jit_buf; //total number of unplayed samples in buffers
  fdelay*=0.9; //averages
  fdelay+=i;
  //computes optimal resapling ratio for the optimum delay
  f=fabs(fdelay/10-720)/10000000; //correction rate due inconsistency
  if(i<360) qff-=f;  //adjust current ratio
  else if(i>1080) qff+=f;
  if(qff<0.888) qff=0.888; //restrictions
  else if(qff>1.142) qff=1.142;

  //resample and play to Headset
  if(l_jit_buf>180) l_jit_buf=0; //prevent overflow
  l_jit_buf+=resample(sp, jit_buf+l_jit_buf, qff); //resample buffer for playing
  playjit(); //immediately try to play buffer
 }
 return job;
}
  
//----------------------Setup----------------------------------
//*****************************************************************************
//initialize audio devices
int audio_init(void)
{ 
 if(!_soundinit()) //init audio8 play/rec Headset and audio48 play/rec Line
 {
  printf("Error of 'Line' audio device initialization, application terminated\r\n");
  return 1;  //init Headset side 8KHz audio device
 }

 if(!soundinit()) 
 {
  return 2; //init Line side 48KHz audio device
  printf("Error of 'Headset' audio device initialization, application terminated\r\n");
 }
 printf("\r\n");
 
 _soundrec(1); //start records from Line
 soundrec(1); //start recrds from Mike

 melpe_i(); //init MELPE1200 codec engine
 return 0; //success  
}

//*****************************************************************************
//finalize audio devices
void audio_fin(void)
{
 _soundterm();
 soundterm();
}







