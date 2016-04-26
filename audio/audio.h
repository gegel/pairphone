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


   int soundinit(void);
   int soundgrab(char *buf, int len);
   int soundplay(int len, unsigned char *buf);
   void soundterm(void);
   int getdelay(void);
   int getchunksize(void);
   int getbufsize(void);
   void soundflush(void);
   void soundflush1(void);
    int soundrec(int on);
    
    int _soundinit(void);
   int _soundgrab(char *buf, int len);
   int _soundplay(int len, unsigned char *buf);
   void _soundterm(void);
   int _getdelay(void);
   int _getchunksize(void);
   int _getbufsize(void);
   void _soundflush(void);
   void _soundflush1(void);
    int _soundrec(int on);
