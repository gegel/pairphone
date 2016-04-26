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


int _getdelay(void);
int _getchunksize(void);
int _getbufsize(void);
int _soundinit(void);
void _soundterm(void);
void _sound_open_file_descriptors(int *audio_io, int *audio_ctl);
int _soundplay(int len, unsigned char *buf);
void _soundplayvol(int value);
void _soundrecgain(int value);
void _sounddest(int where);
int _soundgrab(char *buf, int len);
void _soundflush(void);
void _soundflush1(void);
int _soundrec(int on);

