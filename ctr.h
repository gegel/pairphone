

// KEY codes and ESC-sequences for Windows and Linux
#ifdef _WIN32

#define KEY_INS 1
#define KEY_DEL 2
#define KEY_BREAK 127
#define KEY_UP 4
#define KEY_DOWN 5
#define KEY_RIGHT 6
#define KEY_LEFT 7
#define KEY_STAB 3
#define KEY_TAB 9
#define KEY_ENTER 13
#define KEY_ESC 27
#define KEY_BACK 8

#define KEY_F1 16
#define KEY_F2 17
#define KEY_F3 18
#define KEY_F4 19
#define KEY_F5 20
#define KEY_F6 21
#define KEY_F7 22
#define KEY_F8 23
#define KEY_F9 24
#define KEY_F10 25


#define EKEY_UP 0x00001B48
#define EKEY_DOWN 0x00001B50
#define EKEY_RIGHT 0x00001B4D
#define EKEY_LEFT 0x00001B4B
#define EKEY_STAB 0x00001B94
#define EKEY_INS 0x00001B52
#define EKEY_DEL 0x00001B53

#define EKEY_F1 0x00001B3B
#define EKEY_F2 0x00001B3C
#define EKEY_F3 0x00001B3D
#define EKEY_F4 0x00001B3E
#define EKEY_F5 0x00001B3F
#define EKEY_F6 0x00001B40
#define EKEY_F7 0x00001B41
#define EKEY_F8 0x00001B42
#define EKEY_F9 0x00001B43
#define EKEY_F10 0x00001B44

#else //Linux

#define KEY_INS 1
#define KEY_DEL 2
#define KEY_BREAK 3
#define KEY_UP 4
#define KEY_DOWN 5
#define KEY_RIGHT 6
#define KEY_LEFT 7
#define KEY_STAB 8
#define KEY_TAB 9
#define KEY_ENTER 13
#define KEY_ESC 27
#define KEY_BACK 127

#define KEY_F1 16
#define KEY_F2 17
#define KEY_F3 18
#define KEY_F4 19
#define KEY_F5 20
#define KEY_F6 21
#define KEY_F7 22
#define KEY_F8 23
#define KEY_F9 24
#define KEY_F10 25

#define EKEY_UP 0x001B5B41
#define EKEY_DOWN 0x001B5B42
#define EKEY_RIGHT 0x001B5B43
#define EKEY_LEFT 0x001B5B44
#define EKEY_STAB 0x001B5B5A
#define EKEY_INS 0x1B5B327E
#define EKEY_DEL 0x1B5B337E

#define EKEY_F1 0x1B5B3234
#define EKEY_F2 0x001B4F51
#define EKEY_F3 0x001B4F52
#define EKEY_F4 0x001B4F53
#define EKEY_F5 0x1B5B3135
#define EKEY_F6 0x1B5B3137
#define EKEY_F7 0x1B5B3138
#define EKEY_F8 0x1B5B3139
#define EKEY_F9 0x1B5B3230
#define EKEY_F10 0x1B5B3231

#endif


//control
void tty_rawmode(void); //set console to raw mode (linux only)
void tty_normode(void); //restore normal mode
void helpscr(void); //output help screen
//runtime
int ctr(int job); //check for key pressed, process it, returns state of input buffer. Suspend thread if no job.