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


//This file contains desktop interface related procedures for PairPhone
//Console input is performed in raw mode (NO_DELAY), esc-sequences for control keys detects by timer

char cmdbuf[256];       	//console input buffer
int cmdptr=0;           	//pointer to last inputted char
unsigned int esc_time=0; 	//time after asc char detected
unsigned int esc_key=0;  	//chars getted after esc during some time
char lastname[32]; 			//last outgoing call party




#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>


#ifdef _WIN32

 #include <stddef.h>
 #include <basetsd.h>
 #include <stdint.h>
 #include <windows.h>
 #include <conio.h>

 #define close fclose
 #define usleep Sleep

 //for Windows emulation of gettimeofday
#ifndef _TIMEZONE_DEFINED /* also in sys/time.h */
#define _TIMEZONE_DEFINED
struct timezone
{
  int  tz_minuteswest; //minutes W of Greenwich
  int  tz_dsttime;     //type of dst correction 
};
#endif /* _TIMEZONE_DEFINED */

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
  FILETIME ft;
  const __int64 DELTA_EPOCH_IN_MICROSECS= 11644473600000000;
  unsigned __int64 tmpres = 0;
  unsigned __int64 tmpres_h = 0;
  //static int tzflag;

  if (NULL != tv)
  {
    GetSystemTimeAsFileTime(&ft);

    tmpres |= ft.dwHighDateTime;
    tmpres <<= 32;
    tmpres |= ft.dwLowDateTime;

    //converting file time to unix epoch
    tmpres /= 10;  //convert into microseconds
    tmpres -= DELTA_EPOCH_IN_MICROSECS;

    tmpres_h=tmpres / 1000000UL; //sec
    tv->tv_sec = (long)(tmpres_h);
    tv->tv_usec = (long)(tmpres % 1000000UL);
  }
  return 0;
}


#else  //Linux
 #include <sys/types.h>
 #include <sys/stat.h>
 #include <fcntl.h>
 #include <fcntl.h>
 #include <assert.h>
 #include <ctype.h>
 #include <sys/types.h>
 #include <sys/time.h>
 #include <sys/wait.h>
 #include <sys/stat.h>
 #include <unistd.h>
 #include <errno.h>
 #include <signal.h>
 #include <termios.h>
 #include <termio.h>

 static struct termio old_term_params;
 
 
 static struct timespec us_to_timespec(unsigned long long _us)
 {
	struct timespec ret;
	time_t seconds = _us / 1000000ULL;
	_us -= seconds * 1000000ULL;
	ret.tv_sec = seconds;
	ret.tv_nsec = _us * 1000ULL;
	return ret;
 }

 static void ophh_time_ussleep(unsigned long long _us)
 {
	struct timespec requested_time = us_to_timespec(_us);
	while (nanosleep(&requested_time, &requested_time) == -1 && errno == EINTR)
	continue;
 }
 
 
#endif

#include "crp.h"
#include "ctr.h"
 
 //*****************************************************************************
 //set raw mode for terminal (Linux only)
void tty_rawmode(void)
{
#ifndef _WIN32   
   struct termio term_params;

   ioctl(fileno(stdin), TCGETA, &old_term_params);
   term_params = old_term_params;
   term_params.c_iflag &= ~(ICRNL|IXON|IXOFF);	// no cr translation 
   term_params.c_iflag &= ~(ISTRIP);   // no stripping of high order bit 
   term_params.c_oflag &= ~(OPOST);    // no output processing 	
   term_params.c_lflag &= ~(ISIG|ICANON|ECHO); // raw mode 
   term_params.c_cc[4] = 1;  // satisfy read after 1 char 
   ioctl(fileno(stdin), TCSETAF, &term_params);
   fcntl(fileno(stdin), F_SETFL, O_NDELAY);
#endif
 return;
}
//*****************************************************************************
// Restore tty mode (Linux only)
void tty_normode(void)
{
#ifndef _WIN32   
 ioctl(fileno(stdin), TCSETAF, &old_term_params);
#endif
 return;
}
//*****************************************************************************
//suspend executing of the thread  for paus msec
void psleep(int paus)
{
 #ifdef _WIN32
    Sleep(paus);
 #else
    ophh_time_ussleep(paus*10);
 #endif
}
 //*****************************************************************************
 //returns timestamp: average mS*10
unsigned int getmsec(void)
{
     struct timeval tt1;
     gettimeofday(&tt1, NULL);
     return (unsigned int) (((tt1.tv_sec)<<8) | ((tt1.tv_usec)>>12));
}

//*****************************************************************************
//clears input text string
void doclr(int cl)
{
   int i;
   cmdbuf[cmdptr]=0;
   if(cmdptr)
   {
    printf("\r");
    for(i=0;i<cmdptr;i++) printf(" ");
    printf("\r");
    if(cl)
    {
     for(i=0;i<cmdptr;i++) cmdbuf[i]=0;
     cmdptr=0;
    }
   }
}

//*****************************************************************************
//look for ESC-sequence (control keys) and returns input char, null or ctrl code(1-8)
int goesc(int cc)
{
 unsigned int ii;

 if(cc==KEY_ESC)  //if esc char detected
 {
  ii=getmsec(); //time now
  if(ii>esc_time) //if elapsed more then 1000 ms after last char
  {
   esc_time=ii+256; //set next time 1 sec in future
   esc_key=KEY_ESC; //init ctrl sequence
   cc=0; //no current char
  } //else current char is esc
  else esc_key=0; //break esc sequence and return esc
 }
 else if(cc)//other char detected
 {
  if(esc_key) //already existed some sequence
  {
   ii=getmsec(); //time now
   if(ii>esc_time) esc_key=0; //if elapsed more then 1000 ms after last char: reset sequence
   else //less then 1000 mS after last char: add char to sequence
   {
    esc_key=(esc_key<<8)+(unsigned char)cc; //add char
    ii=0; //clears for ctrl code
    
    switch(esc_key) //check for complete sequence  and use ctrl code
    {
     case EKEY_UP:
     {
      ii=KEY_UP;
     }
     break;
     case EKEY_DOWN:
     {
      ii=KEY_DOWN;
     }
     break;
     case EKEY_RIGHT:
     {
      ii=KEY_RIGHT;
     }
     break;
     case EKEY_LEFT:
     {
      ii=KEY_LEFT;
     }
     break;
     case EKEY_STAB:
     {
      ii=KEY_STAB;
     }
     break;
     case EKEY_INS:
     {
      ii=KEY_INS;
     }
     break;
     case EKEY_DEL:
     {
      ii=KEY_DEL;
     }
     break;
     case EKEY_F1:
     {
      ii=KEY_F1;
     }
     break;
     case EKEY_F2:
     {
      ii=KEY_F2;
     }
     break;
     case EKEY_F3:
     {
      ii=KEY_F3;
     }
     break;
     case EKEY_F4:
     {
      ii=KEY_F4;
     }
     break;
     case EKEY_F5:
     {
      ii=KEY_F5;
     }
     break;
     case EKEY_F6:
     {
      ii=KEY_F6;
     }
     break;
     case EKEY_F7:
     {
      ii=KEY_F7;
     }
     break;
     case EKEY_F8:
     {
      ii=KEY_F8;
     }
     break;
     case EKEY_F9:
     {
      ii=KEY_F9;
     }
     break;
     case EKEY_F10:
     {
      ii=KEY_F10;
     }
     break;
    }

    if(ii) //if complete sequence matched
    {
     cc=ii; //return resulting ctrl code
     //esc_key=0;
     //esc_time=0; //reset sequence
    }
    else cc=0; //sequence in process: no chars returns yes

   }
  }
 }
 return cc; //resulting char or ctrl code
}
//*****************************************************************************
//process inputted key or char
static int gochar(int c)
{
 int i;
 if(!c) return 0; //no char for processing
 
 switch(c)
 {
  case KEY_ENTER: //menu, command, chat or swith talk/mute
  {
   doclr(1); //delete inputted chars
   printf("\r\nText SMS not supported yet\r\n");
  }
  break;
  case KEY_DEL: //clear string
  {
   doclr(1); //delete inputted chars
  }
  break;
  case KEY_ESC:
  {
   doclr(1); //delete inputted chars
   c=KEY_BREAK;//hung up, exit
  }
  break;
  case KEY_TAB: //on/off mute
  {
   doclr(0); //clear typed string on the screen
   i=Mute(0); //obtaing state
   if(i<0) i=1; else i=-1; //invert value
   i=Mute(i); //try to set new state and obtaing actual
   if(i>0) printf("TALK\r\n"); else printf("MUTE\r\n"); //note state
   if(cmdptr) printf("%s",cmdbuf); //restore typed string
  }
  break;
  case KEY_F1:
  {
   //help screen
   doclr(0); //clear typed string on the screen
   helpscr(); //output help info
   if(cmdptr) printf("%s",cmdbuf); //restore typed string
  }  
  break;
  case KEY_F2:
  {
   // pair:
   doclr(0); //clear typed string on the screen
   AddContact(cmdbuf); //try to add specified name to adressbook
   doclr(1); //delete inputted chars

  }
  break;
  case KEY_F3:
  {
  // list
   doclr(0); //clear typed string on the screen
   ListContact(cmdbuf); //output contacts list matches specified mask
   doclr(1); //delete inputted chars
  }
  break;
  case KEY_F4:
  {
  // origanate outgoing call by name
   for(i=0;i<31;i++) lastname[i]=cmdbuf[i];  //copy up to 31 characters of name
   lastname[32]=0; //terminate if inputted string longer then 31 chars
   doclr(1); //delete inputted chars
   SetupCall(lastname); //originate call to specified contact

   //Note!!!! Copy last name to global string for redialling!
  }
  break;
  case KEY_F5:
  {
   // redial  Note!!!! use glbal name string for redial here!!!!
   doclr(1); //delete inputted chars
   SetupCall(0); //repeat last outgoing call
  }
  break;
  case KEY_F6:
  {
  // pin
   doclr(1); //delete inputted chars
   printf("\r\nPIN not supported yet\r\n");
  }
  break;
  case KEY_F7:
  {
  // password
   doclr(0); //clear typed string on the screen
   SetPassword(cmdbuf); //derives keys from specified passphrase
   doclr(1); //delete inputted chars
  }
  break;
  case KEY_F8:
  {
  // hangup
   doclr(1); //delete inputted chars
   printf("\r\nCall terminated by user\r\n");
   HangUp(); //terminates current call and clears all related data
  }
  break;
  case KEY_F9:
  {
  // reconnect
   doclr(1); //delete inputted chars
   printf("\r\nCall reset by user\r\n");
   ResetCall();  //reconnect current call with new key exchange
  }
  break;
  case KEY_F10:
  {
   doclr(1); //delete inputted chars
   printf("\r\nExited\r\n");
   c=KEY_BREAK;//exit
  }
  break;
  case KEY_BACK:   //corrects string: delete last character
  {
   if(cmdptr) //if inputted string not empty
   {
    printf("%c %c", 8, 8); //delete from screen
    cmdptr--; //delete from buffer
   }
  }
  break;
  default:    //print char to screen ans store to buffer
  {
   if((!cmdptr)&&(c=='?')) helpscr();
   if( (cmdptr<255)&&(c>27) ) //only printable chars
   {
    if(!cmdptr)  printf("\r                                            \r"); //clear field at screen before first char
    cmdbuf[cmdptr]=c; //store char in buffer
    cmdptr++; //buffer pointer
    printf("%c",c); //print char at screen
   }
  }
 }
 return c;
}
//*****************************************************************************
//read char from console input asynchronously
//temporary suspend thread if no job
//returns 0 for exiting on ctrl-break, <0 for input buffer is empty, otherwise >0
int ctr(int job)
{
 char c;
 int j = 0;
 
 //get char from raw terminal
 do
 {
  c=0;
#ifdef _WIN32
  if(kbhit()) //if key was pressed
  {
   j=getch(); //read char
   c=j;  //convert int to char type
   if((!c)||(c==-32)) c=KEY_ESC;  //zero is control char now
  }
#else
  j = read(fileno(stdin), &c, 1); //read asynchronously
#endif
 }
 while(0);
 
 c=goesc(c); //process esc-sequences as control characters

 if((j>0)&&(c>0)) //have a character
 {
  c=gochar(c); //process character
  fflush(stdout); //print notifications
  if(c==KEY_BREAK) return 0; // break (ctrl+C), force exiting
  job=1; //set job
 }
 
 if(!job) psleep(1); //suspend thread while no jobs were doing in the loop
 
 if(cmdptr) j=1; else j=-1; //set flag of command buffer is empty
 if(Mute(0)>0) j*=2; //apply mute flag 
 return j;
}

//*****************************************************************************
//outputs help screen
void helpscr(void)
{
 printf("---------------------------------------------------------------\r\n");
 printf("   PairPhone v0.1a  Van Gegel, 2016  MailTo: torfone@ukr.net\r\n");
 printf("     P2P private talk over GSM-FR compressed voice channel\r\n");  
 printf("---------------------------------------------------------------\r\n");
 printf("<Enter>: SMS (not suported yet), <Tab>: talk/mute, <Del>: clean\r\n");
 printf("F1: show this help screen, more on http://torfone.org/pairphone\r\n");
 printf("F2: pair devices during active call (name must be specified)\r\n");
 printf("F3: search contacts (mask can be specified)\r\n");
 printf("F4: outgoing call (name can be specified, otherwise 'guest')\r\n");
 printf("F5: repeat the last outgoing call\r\n");
 printf("F6: apply pin code access to the book (not released yet)\r\n");
 printf("F7: apply preshared passphrase (1 or 2 words or empty defaults)\r\n");
 printf("F8: terminate active call to iddle state (hang up)\r\n");
 printf("F9: reconnect active call (agreed new keys)\r\n");
 printf("F10: exit\r\n");
 printf("---------------------------------------------------------------\r\n");
} 

