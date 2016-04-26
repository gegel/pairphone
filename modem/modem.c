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

//This file contains procedures for modulate and demodulate data using pseudo-voice modem
//suitable for GSM-FR compressed channel
//This is BPSK modem with carrier parameters strongly adapted for GSM FR codec engine with VAD
//Some tricks are empirically discovered and can be not optimal. Now modem in development and can be 
//improved with new ideas. Also code can be not optimal by performance and style due to continuous changes,
//Floating point is used for ease debugging only and can be replaced by fixing point later. 

//some switches for equalizer algorithm
#define newalgo
#define newalgos
//#define newalgog



#include "stdlib.h"
#include "memory.h"
#include "math.h"

#if defined(_MSC_VER) || defined(__BORLANDC__)
typedef unsigned __int64 ulong64;
typedef signed __int64 long64;
#else
typedef unsigned long long ulong64;
typedef signed long long long64;
#endif

//globals
static unsigned int r[9]; //shifted symbols for parity check
static unsigned int rr=0; //shifted parity bits
static float fr[90];      //weights of lags
static float fd[90];      //metrics of bits
static float ffd[90];     //metrics of bits
static int lag=0;         //lag of block (last bit position in the stream)
static int cnt=0;         //counter of PCM frames in block
static int u=0;           //average DC level
static int cq=0;          //phase jitter filtering coefficient for frequency adjustment
static float mlag=0;      //average tx/rx sampling rate difference
static float qq=0;        //quality of fixing 'thin" lag   (bit aligned - phase lock)
static char oldq=0;       //'thin' phase correction value of last processed frame
static char blk=0;        //flag of block ready
static float f180=0;      //average delta Pi phase value
static float fg[4][36]={{0},{0}};	//36 equalizing coefficients depends previous bit
static float ffg[4][36]={{0},{0}};	//36 equalizing coefficients depends previous bit
static unsigned int dr=0; //shifted received hard decision bits
static char lock=0;       //phase locked flag
static char align=1;      //flag of frame synchronisation locked
static float falign=50;   //average raw BER as a quality of frame synch

static int lastb=0;		//value of the last transmitted bit for ISI
static int vadtr=0;		//flag of the periodic whole frame muting for anti-VAD trick

static const unsigned char mask[8]={1,2,4,8,16,32,64,128}; //bit mask table


    //waveform table
    const short wave[8][36]={
        //Normal//
        //bit 0
        {0, 2778, 5472, 7999, 10284, 12256, 13856, 15035, 15756,
        16000, 15756, 15035, 13856, 12256, 10284, 7999, 5472, 2778,
        0, -2778, -5472, -7999, -10284, -12256, -13856, -15035, -15756,
        -16000, -15756, -15035, -13856, -12256, -10284, -8000, -5472, -2778},
        //bit 1
        {0, -2778, -5472, -7999, -10284, -12256, -13856, -15035, -15756,
        -16000, -15756, -15035, -13856, -12256, -10284, -8000, -5472, -2778,
        0, 2778, 5472, 7999, 10284, 12256, 13856, 15035, 15756,
        16000, 15756, 15035, 13856, 12256, 10284, 7999, 5472, 2778},

        //Shaped//
        //bit 0
        {0, 244, 965, 2144, 3744, 5716, 8001, 10528, 13222,
        16000, 13222, 10528, 8001, 5716, 3744, 2144, 965, 244,
        0, -244, -965, -2144, -3744, -5716, -8001, -10528, -13222,
        -16000, -13222, -10528, -8001, -5716, -3744, -2144, -965, -244},
        //bit 1
        {0, -244, -965, -2144, -3744, -5716, -8001, -10528, -13222,
        -16000, -13222, -10528, -8001, -5716, -3744, -2144, -965, -244,
        0, 244, 965, 2144, 3744, 5716, 8001, 10528, 13222,
        16000, 13222, 10528, 8001, 5716, 3744, 2144, 965, 244},

        //Muted//
        //bit 0
        {0, 1389, 2736, 3999, 5142, 6128, 6928, 7517, 7878,
        8000, 7878, 7517, 6928, 6128, 5142, 3999, 2736, 1389,
        0, -1389, -2736, -3999, -5142, -6128, -6928, -7517, -7878,
        -8000, -7878, -7517, -6928, -6128, -5142, -4000, -2736, -1389},
        //bit 1
        {0, -1389, -2736, -3999, -5142, -6128, -6928, -7517, -7878,
        -8000, -7878, -7517, -6928, -6128, -5142, -4000, -2736, -1389,
        0, 1389, 2736, 3999, 5142, 6128, 6928, 7517, 7878,
        8000, 7878, 7517, 6928, 6128, 5142, 3999, 2736, 1389},
		
        //Shaped and muted//
        //bit 0
        {0/2, 244/2, 965/2, 2144/2, 3744/2, 5716/2, 8001/2, 10528/2, 13222/2,
        16000/2, 13222/2, 10528/2, 8001/2, 5716/2, 3744/2, 2144/2, 965/2, 244/2,
        0/2, -244/2, -965/2, -2144/2, -3744/2, -5716/2, -8001/2, -10528/2, -13222/2,
        -16000/2, -13222/2, -10528/2, -8001/2, -5716/2, -3744/2, -2144/2, -965/2, -244/2},
        //bit 1
        {0/2, -244/2, -965/2, -2144/2, -3744/2, -5716/2, -8001/2, -10528/2, -13222/2,
        -16000/2, -13222/2, -10528/2, -8001/2, -5716/2, -3744/2, -2144/2, -965/2, -244/2,
        0/2, 244/2, 965/2, 2144/2, 3744/2, 5716/2, 8001/2, 10528/2, 13222/2,
        16000/2, 13222/2, 10528/2, 8001/2, 5716/2, 3744/2, 2144/2, 965/2, 244/2}

    };

   //SAMPLERATE 48000 KHz//
   //**********************************************************
   //modulator of BPSK 1333 Hz carrier (36 48KHz PCM samples per bit)
   //modulate 81 bits in uchar array to 3240 short PCM samples
   //returns number of outputted samples (3240)
   //input: 81 data bits in data[0]:data[10](LSB)
   //output: 3240 short PCM samples of baseband

   int Modulate(unsigned char* data, short* frame)
   {
    //48KHz waveforms for bit=0 and 1: BPSK 1333Hz carrier
    //with asymmetric, ISI and periodic muting

    int i, j, k, ii; 	//general
    int b;
    unsigned char p[9]; //parity bits array for 9 symbols
    short* sp=frame;    //pointer to PCM
    char isi=0; 		//flag of controlled ISI

    //modulator: 81 bits  in 9 symbols, add parity bit to each symbol
    //interleaves symbols: transmit bits 0 of each symbol, then bits 1,... last parity bits
    memset(p, 0, 8); //init parity bits array for symbols
    p[8]=1; //parity is odd only for last symbol
    for(i=0;i<10;i++)  //output bit counter: 10 bits per symbol
    {
     for(j=0;j<9;j++) //output symbol counter: 9 symbols
     {
      if(i==9) b=p[j];  //use parity bit (the last in symbol)
      else
      {
       k=j*9+i; //input bit number (0-80)
       if(data[k/8]&mask[k%8]) b=1; else b=0; //use input bit
       p[j]^=b; //add bit to parity
      }
      isi=b^lastb; //check for current bit is different previous bit
      lastb=b; //save current bit for next
      if(isi) b+=2; //ISI trick: shaping
      if(vadtr) b+=4; //VAD trick: muting
      for(ii=0;ii<36;ii++) sp[ii]=wave[b][ii]; //modulate bit to 36 samples waveform
      for(ii=0;ii<18;ii++) sp[ii]=sp[ii]/2; //applies some asymmetric of wave
      sp+=36; //move PCM pointer to the next bit position
     }
    }
    vadtr^=1; //change muting flag for next frame: 67.5 mS normal, 67.5 mS muted
    return (int)(sp-frame); //number of outputted samples
   }
//*************************************************************************
   //Demodulate 36*6=216 or in range (35-37)*6 short PCM samples in subframe
   //input: must be at least 36*7 samples forward!!!
   //output: 81 bits of payload in data[0]-data[10](LSB) while ready flag (event) is set 
   //7 MSB of data[10] is a synchronization lag (0-90)
   //4 LSB of data[11] is a number of symbols errors in block (0-9)
   //MSBs of data[11] are:
   //bit 7 is a flag of payload ready (receiving event)
   //bit 6 is a flag of block synchronization locked
   //bit 5 is a flag of phase locked
   //bit 4 is a flag of frequency locked
   //returns the number of actually processed samples
   //(software must move PCM pointer to this value for next)
   int Demodulate( short* frame, unsigned char* data)
   {
    #define ad_coef 0.95
    //general
    int i,j,k,q=0,b,p,pj;
    long64 ge;
    int ii, jj, kk, pp=0, bb;
    float f;
    short* sp=frame+9;  //set pointer to processed frame (39*6 samples) with the ability of 'thin' phase correction
    float fgr, fgl; //correlation of first and second half periods of carrier
    int eg[36]; //correlation results for each sample position point during a period of carrier

    float fgrm, fglm, fgg, fgra, fgla;
    
    int fgrb, fglb;

    float ffg0, ffg1, ffg2, ffg3;
    float spn[36]; //normalized waveform
    char bbb;
    char lastbit=0;

//=====================================
//coherent demodulator of BPSK 1333 Hz carrier (6*6 48KHz PCM samples per bit)
//with soft FEC r=9/10 and payloads bitrate 1200 bps
//output 81 bits aligned to block of 67.5mS (6*540 PCM 48000 samples)
//corresponds to MELPE 1200bps speech frame
//=====================================

//---------------------------------------
//The first we correlate input stream with square signal 1333Hz
//for phase locking and frequency adjustment
//The phase corrects with step += 2pi/36 (we have 36 48KHz PCM samples per carrier period)
//The frequency correct by skipping/doubling using a sample in input stream
//For tune the frequency the first we must transform phase jitter to vander
//Time of this filter is adaptive with absolute sampling rate difference
//of modulator / demodulator
//---------------------------------------

    //correlate 36*6 samples frame with square 1333Hz (carrier frequency)
    //signals with shifting steps Pi/18
    for(i=0;i<36;i++) eg[i]=0; //clear for new correlation results
    for(i=0;i<24;i++) //look for 4 subframes (24 periods) forwards
    {
      for(j=0; j<36; j++) //each period (2*PI=36 samples)
      {
       k=i*36+j; //pointer for next step PI/18
       eg[j]+=abs(sp[k+0]-sp[k+18]); //averages the results of multiplying with square signal
      }
    }

    //search the best (probably correct) phase for this frame
    k=0;
    for(j=0;j<36;j++) //36 possibles shifts with step PI/18
    {
     if(eg[j]>k) //search best correlation value
     {
      k=eg[j]; //best value
      q=j;     //best phase pointer
     }
    }

    //search correct Pi shifting lag of continuous input stream
    f180*=0.9; //average +-PI shifting lag
    if(q>17) //+PI the best
    {
     q-=18; //shift stream back
     f180-=1; //averages
    }
    else  f180+=1; //the result is positive for lag 0 and negative for lag Pi
    if(fabs(f180)<1) //no good carrier detected
    {
     if(lock) for(i=0;i<90;i++) fr[i]=0; //the lost of carrier: clear lags array
     lock=0; //clear lock flag
    }
    else if(fabs(f180)>9) lock=1; //carrier excellent: set lock flag

    //lock the phase
    q-=9; //set -Pi/18 - 0 - +Pi/18 'thin' phase adjusting for this frame
    if(f180<0) sp+=18; //correct frame pointer to actual Pi phase lag
    sp+=q; //correct frame pointer to actual Pi/18 phase lag


    //frequency tuning (by sampling rate difference between modulator and demodulator)
    //The first transform phase jitter to vander
    qq*=0.999; //averages quality of 'thin' synch
    mlag*=0.99; //averages absolute sampling rate difference value
    if(oldq==q) qq+=1; //phase not changed compared previous frame: increase synch quality
    else
    {
     oldq=q;  //store current phase (lag)
     mlag+=q; //averages delta of phase from previous frame
    }
    kk=96-(3*abs(mlag)); //average sampling rate difference coefficient (time of jitter2vander filter)
    if((kk<1)||(qq<200)) kk=0; //jitter filtering coefficient: 1 if big rate difference or bad phase synch
    cq+=q; //the duration of the non-zero phase lag (the window for compensation phase jitter)
    //this is for 8KHz only, seems no extra filtering needed for 48KHz sampling:
    //if(abs(cq)>kk) cq=0; else q=0; //jitter to vander: correct stream (skip or use twice one sample)

    //now input is 36*6 samples frame aligned to correct phase point
    //and can be demodulated coherently
    data[11]&=0x7F; //clear data ready flag on output bytes array
    for(k=0;k<6;k++) //process 6 * 2 triplets (6*6 samples to bit and 6 bits in 36*6 samples)
    {
     pj=k*36; //pointer to first sample in current triplet of processed frame

     //Averages DC level of input stream: add current period
     u=504*u;
     for(i=0;i<36;i++) u+=sp[pj+i];
     u=u/540;

     //For compensate channel characteristic we must use corresponds
     //coefficients during correlation (equalizing coefficients)
     //This coefficients dynamically updated depending channel statistic
     //The time of updating corresponds to GSM codecs frame and set empirically
	 //Note: Modulator applied controlled ISI: the bit just changed producing wave period applying filter 
	 //Demodulator must equalize this predistors given the previously received bit

#ifndef newalgo


     //look for channel statistic:
     //averages samples weights for adjusting of equalizer depends previously received bits
     f=0;
     for(i=0;i<36;i++) //36 coefficients (for 36 samples) in a carrier period
     {
      fg[dr&3][i]*=0.95;  //adjusting time corresponds GSM codec properties
      fg[dr&3][i]+=fabs((float)(sp[pj+i]-u)); //average equalizing coefficients
      f+=fg[dr&3][i]; //total amplitude of period
     }
     f/=48; //will be uses for normalizing of coefficients

     //now we can correlate the input wave period with equalized
     //coherent signal for bit decision
     fgl=0;
     fgr=0;
     fgg=0;
     fglm=0;
     fgrm=0;
     fgla=0;
     fgra=0;

     for(i=0;i<18;i++)  //first half-period
     {
      fgl=(((float)(sp[pj+i]-u))*fg[dr&3][i]/f); //with equalizing
      fgla+=fgl;
      if(fabs(fgl)>fabs(fglm)) fglm=fgl;
     }
        fglm=fgla;
     for(i=18;i<36;i++) //second halfperiod
     {
      fgr=(((float)(sp[pj+i]-u))*fg[dr&3][i]/f); //with equalizing
      fgra+=fgr;
      if(fabs(fgr)>fabs(fgrm)) fgrm=fgr;
     }
     fgrm=sp[pj+27]-u;
     fgrm=fgra;
 
     fgr=0;
     fgl=fglm;
     
     fgr=fgra;
     fgl=fgla;
     
     if(fgr>fgl) b=1; else b=0;  //hard decission of bit

#endif


#ifdef newalgo

     for(i=0;i<36;i++) spn[i]=(float)(sp[pj+i]-u); //eliminate DC
     //correlation with 4 dynamic tables depends previous received and expected current bit
     ffg0=0; //correlation for case current bit equal last bit
     ffg1=0; //not equal
     ffg2=0;
     ffg3=0;



#ifndef newalgog
     for(i=0;i<36;i++) ffg0+=spn[i]*ffg[0][i]; //with 0 adaptive table 0
     for(i=0;i<36;i++) ffg0-=spn[i]*ffg[1][i]; //with 1 adaptive table 0
     for(i=0;i<36;i++) ffg1+=spn[i]*ffg[2][i]; //with 0 adaptive table 1
     for(i=0;i<36;i++) ffg1-=spn[i]*ffg[3][i]; //with 1 adaptive table 1
     if(fabs(ffg1)>fabs(ffg0)) ffg0=ffg1; //maximal absolute correlation
     if(ffg0>=0) bbb=0; else bbb=1; //hard bit decision
#else
     for(i=0;i<36;i++) ffg0+=spn[i]*ffg[0][i];
     for(i=0;i<36;i++) ffg1+=spn[i]*ffg[1][i];
     for(i=0;i<36;i++) ffg2+=spn[i]*ffg[2][i];
     for(i=0;i<36;i++) ffg3+=spn[i]*ffg[3][i];

     bbb=0;  //Max

     if(ffg1>ffg0)
     {
      ffg0=ffg1;
      bbb=1;
     }

     if(ffg2>ffg0)
     {
      ffg0=ffg2;
      bbb=0;
     }

     if(ffg3>ffg0)
     {
      ffg0=ffg3;
      bbb=1;
     }

     if(bbb) ffg0=-ffg0; //set hard decision to metric
#endif



     lastbit=bbb+((lastbit^bbb)<<1); //index of selected correlation table
     f=0;
     for(i=0;i<36;i++) //36 coefficients (for 36 samples) in a carrier period
     {
      ffg[(int)lastbit][i]*=0.95;  //adjusting time corresponds GSM codec properties
      ffg[(int)lastbit][i]+=(float)(sp[pj+i]-u); //average equalizing coefficients
      f+=fabs(ffg[(int)lastbit][i]); //averages amplitude of period
     }
     f/=48;
     if(!f) f=1; //prevention division by zero on start
     ffg0/=f; //normalizing
     lastbit=bbb;  //store current bit for  next

     b=bbb;   //!!!!!!!!!!!!!

#endif


     dr<<=1;				//shift early received bits
     dr|=b;					//add bit for correct equalizer for next
                           

     //Now we have a received bit and can be check parity of received data
     //So each block contains 540*6 48KHz samples (15 frames 36*6 samples/6 bits each)
     //we have 90 bits totally and split they to 9 symbols 10 bits each.
     //Each symbol contain 9 payload bits and one parity bit
     //The parity bit of last symbol in block is odd, others are even
     //Before transmission data bits are interleaved:
     //the first transmits bits 0 of all 9 symbols, follow bits 1... and last parity bits
     //During bits receiving we will be check parities of all previously received 89 bits
     //so assuming a current bit as a last bit in the block
     //The result is comparing with correct parity word 000000001
     //complete only block fully received. For checking the level of
     //correctness we can compute the number of matching parity bits
     //So we averages all possible bit-aligned lags (90 positions) during bit stream received
     //and can find the best lag pointed the position of the last bit in block
     //This synchronization will be probably correct after 2 full blocks and we can
     //output the first correct block after maximum 270 mS of stream processed
     //starting at any time without any synch-sequences or other bit rate overheads

     //"thick" synch using parity bits
     j=cnt*6+k; //current stream lag (aligned to bit)
     i=j%9;  //the virtual number of probably symbol in block
     r[i]=(r[i]<<1)|b; //push received bit to virtual symbol

     //check parity of 10-bits symbol for now
     p=0x3FF&r[i];
     p^=(p>>1);
     p^=(p>>2);
     p^=(p>>4);
     p^=(p>>8);
     rr=(rr<<1)|(p&1);  //push probably parity bit to virtual parity word

     //averages matches parity block
     if(lock)
     fr[j]*=0.999; //average weight of current lag
     else fr[j]*=0.99; //for fast synch while phase not locked good
     if(rr&1)  //if current parity 1 (probably the end of packet)
     {
      //calculi the number of previous zero parity (must be 8)
      p=rr;
      for(i=0; i<8; i++)
      {
       p>>=1;
       if(!(p&1)) fr[j]+=1; //check for zero and add to current position lag metrics array 
      }
     }
     //after continuous stream receiving (at least 135 mS)
     //the index of maximum value in fr[90] array corresponds the last bit in block
     //This checked at the end of processing each 36 samples subframe by
     //searching maximal value in fr[90] array and set this index as a lag value

     //Now we assumed than lag value already points to last bit of block
     //Using lag value and bit counter we can compute the actual position
     //of currently received bit in block and put received bit to its place in bit array

     //process received bit
      i=(j-lag)-1; //current bit number in block
      if(i<0) i+=90; //over boundaries of block

      //compute common energy of bit for correlation:
      //for first and second half-period of BPSK waveform


#ifndef newalgos
      ge=0;
      for(ii=0;ii<36;ii++) ge+=((sp[pj+ii]-u)*(sp[pj+ii]-u)); //total energy of the processed subframe
      ge=sqrt(ge); //for correlation
      if(ge==0) ge=2000;  //prevention the division by zero
      //compute the value corresponds soft metric of bit (correlation coefficient)
      fd[i]=( ((float)(fgra-fgla)) / ge);
      fd[i]=fabs(fd[i]); //absolute correlation
      if(!b) fd[i]=-fd[i]; //applies hard bit decision for signed soft bit value 
#endif

  //=========================================
#ifdef newalgos
     //computes LLR
     ge=0; //common energy of bit for normalization
     for(ii=0;ii<36;ii++) ge+=(spn[ii]*spn[ii]);
     ge=sqrt(ge);
     if(ge<1) ge=1;  //prevention the division by zero
     fd[i]=(ffg0/ge);  //soft metric of bit (LLR)
#endif

      //now we have bit array aligned to block boundaries with  soft  bits
      //note the block synchronization is invariant for channel inversion
      //but BPSK demodulation result is NOT invariant: the inversion of channel
      //cause the inversion of all payload bits in outputted block
      //So channel inversion is stable and depended the used physical channel only
      //the software cam be look and set inversion flag later and invert full modem output continuously

      //Checks for received bit is the last bit in block
      //In this case we must process all 90 previously received bits
      //(all of them are in his places in fd[90] arrays) and clear array for receiving next block

      //output full block
      if(j==lag) //this was the last bit of packet: output packet
      {
       memset(data, 0, 12); //clear bytes output
       kk=0; //clear output bits counter
       bb=0; //clear BER counter

       for(ii=0;ii<9;ii++) //for input symbol counter
       {                   //ii is number of 9-bits symbol (0-8)
        f=100000; //maximal possible metric value for fec
        if(ii==8) p=1; else p=0; //init parity bit (parity in last symbol is odd for syn purpose)

        //note: input bit-stream is interleaved, deinterleave there
        for(jj=0;jj<10; jj++) //for input bit counter in symbol
        {                     //jj is number of bit in symbol (0-8 info and 9 is parity)
         j=jj*9+ii; //pointer to input stream bit
         if(fd[j]>0) b=1; else b=0; //hard decision of the bit

         //now we have a complete 10-bits symbol and proceed the simple FEC:
         //check for symbol parity and flip bit with minimal metric if parity is wrong
         //so we can probably correct 1 error in 10 received bits
         //since stream was interleaved this FEC can correct burst up to
         //9 subsequent errors (6.75 mS) in ANY place of 67.5mS block.
         //This burst is longer then GSM codec subframe (5 mS) and not a multiple of it
         //and also not a multiple of modems 36 samples (4.5 mS) subframe.

         if(fabs(fd[j])<=f) //check for input bit with minimal metric in symbol
         {
          f=fabs(fd[j]); //this is a minimal metric
          pp=kk; //output position of bit with minimal metric
         }

         //output information bits in symbols and check last parity bit
         if(jj<9) //this is information bit: output it
         {
          if(b) data[kk/8]^=mask[kk%8]; //store in bytes output
          p^=b; //add to parity
          kk++; //move output pointer to next bit

         } //this is parity bit (last in symbol)
         else if(b!=p) //if parity is wrong
         {
          bb++; //count bit error
          if(pp!=kk)
          {
           data[pp/8]^=mask[pp%8];   //flip bit with minimal metric
          }

         }

        } //end of bit loop //for(jj=0;jj<10; jj++)//
       }  //end of symbol loop //for(ii=0;ii<9;ii++)//

       data[11]=bb;	//output raw BER counter
       falign*=0.9; //averages raw BER
       falign+=bb;  //add the BER of current block to average raw BER value
       if((falign>40)&&(align)) //check for raw BER is hight and synch flag was set: probably lost of frame synch
       {
        align=0; //clear synch flag
        for(i=0;i<90;i++) fr[i]=0; //the lost of synch: clear lags metrics
        for(i=0;i<36;i++) //set default dynamic equalizer
        {
         ffg[0][i]=wave[0][i];  //for bit 0
         ffg[1][i]=wave[1][i];  //for bit 1
         ffg[2][i]=ffg[0][i];   //copy for case expected bit not equal previous bit received
         ffg[3][i]=ffg[1][i];
        }
        i=0;
       }
       else if((falign<30)&&(!align)) align=1; //small BER: set synch flag

       //now we have full block of payload ready for output
       //since we must strongly counted the received blocks (for strong cryptographic synchronization)
       //we can not skip block or output twice even jumps of lag occurred
       //(for example, after  loss of frames with synchronization failing and restoring
       //on the hight rate difference). So we use flag of block was be outputted and output block
       //immediately after the last bit received but only once to incoming 67.5mS.
       //If the lag matching event not occurred during this time we still outputs incorrect
       //block for continuous counting
       if(!blk) //output block only once per time: check flag
       {
        data[11]|=0x80; //set ready flag (receiving event)  for output this block
        blk=1; //set flag: block was been outputted
        //software must check this flag after every demodulates processing
        //and read 81 bit of output data if flag is set
        //flag will be clear by demodulator in next processing procedure
       }

      } //end of output full block //if(j==lag)//
    } //end of input 36-samples frame //for(k=0;k<6;k++)//

    //frame processed: prepares for next frame
    cnt++; //counter of frames 36*6 samples each
    //we split incoming stream to virtual blocks of 540*6 samples
    //for cryptographic strong synchronisation properties
    if(cnt>=15)
    {
     //15 frame processed
     cnt=0; //reset frame counter
     //correct lag: search best value for alignment to block boundaries
     f=0; //minimal possible value
     for(i=0;i<90;i++) if(f<fr[i]) //search for all possible lags
     {
      f=fr[i]; //new maximal value
      lag=i; //the best lag
     }

     //we must output certainly one block every 67.5 mS
     //but immediately after last bit of block will be received
     //if lag condition was not matched during last block
     //we must still output incorrect block for synchronization properties
     //check block been outputted per this 540*6 samples frame
     if(!blk) data[11]|=0x8F; //if no output incorrect block (for continuous block counting)
     else blk=0; //clear flag for new frame
    }

    //add status to output data array
    data[10]+=(lag<<1); //7 MSB of byte 10 is actual lag of block (0-89)
    if(align) data[11]|=0x40; //block lag lock flag
    if(lock) data[11]|=0x20; //phase lock flag
    if(qq>50) data[11]|=0x10; //frequency lock flag

    //returns the number of actually processed samples
    //(corrected for frequency adjustment)
    //software must use this value to move samples pointer for next processing
    return 216+q;
   }
//**************************************************************************
