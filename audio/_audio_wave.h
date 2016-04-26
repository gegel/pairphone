// Contact: <torfone@ukr.net>
// Author: Van Gegel
//
// THIS IS A FREE SOFTWARE
//
// This software is released under GNU LGPL:
//
// * LGPL 3.0 <http://www.gnu.org/licenses/lgpl.html>
//
// You're free to copy, distribute and make commercial use
// of this software under the following conditions:
//
// * You have to cite the author (and copyright owner): Van Gegel
// * You have to provide a link to the author's Homepage: <http://torfone.org/>
//
///////////////////////////////////////////////

   int _soundinit(void);
   int _soundgrab(char *buf, int len);
   int _soundplay(int len, unsigned char *buf);
   void _soundterm(void);
   int _getdelay(void);
   int _getchunksize(void);
   int _getbufsize(void);
   void _soundflush(void);
   int _soundrec(int on);
