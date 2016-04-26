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

//This is a main procedure of PairPhone testing software
//one-thread implementation as a infinite loop contained procedures for:
//-receiving baseband, demodulating, decrypting, decompressing, playing over earphones (RX)
//-recording voice from mike, compressing, encrypting, modulating, sending baseband into line (TX)
//-scan keyboard, processing. Suspending thread if no job (CTR)
//---------------------------------------------------------------------------
#ifdef _WIN32

 #include <stdlib.h>
 #include <stdio.h>
 #include <stddef.h>
 #include <basetsd.h>
 #include <stdint.h>
 #include <windows.h>
 #include <time.h>
 #include <conio.h>
 #include <string.h>
 #include "memory.h"
 #include "math.h"
  
 #else //linux
 
 #include <stdlib.h>
 #include <stdio.h>
 #include <string.h>
 #include <time.h>
 #include "memory.h"
 #include "math.h"
 
 #include <sys/time.h>
 
 #endif

#include "audio/audio.h"  //low-level alsa/wave audio 
#include "crypto/libcrp.h" //cryptography primitives 

#include "crp.h" //key agreement, authentication, encryption/decryption, frame synchronization
#include "ctr.h" //scan keyboard, processing. Suspending thread if no job (CTR)
#include "rx.h"  //receiving baseband, demodulating, decrypting, decompressing, playing over earphones
#include "tx.h"	 //recording voice from mike, compressing, encrypting, modulating, sending baseband into line

int main(int argc, char* argv[])
{
 int i=0;
    
 printf("---------------------------------------------------------------\r\n");
 printf("   PairPhone v0.1a  Van Gegel, 2016  MailTo: torfone@ukr.net\r\n");
 printf("     P2P private talk over GSM-FR compressed voice channel\r\n");  
 printf("---------------------------------------------------------------\r\n");
   
   randInit(0,0); //init SPRNG
   if(audio_init()) return -1;  //init audio
   tty_rawmode(); //init console IO
   HangUp(); //set idle mode
   
   //main loop 
   do
   {
    i=rx(i);   //receiving
    i=tx(i);   //transmitting
    i=ctr(1);  //controlling
   }
   while(i);
   
   tty_normode(); //restore console
   audio_fin(); //close audio
   return 0;
}
//---------------------------------------------------------------------------





