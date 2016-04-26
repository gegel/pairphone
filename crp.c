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

//This file contains crypto related procedures for PairPhone:

//Triple Diffie-Hellmann Initial Key Exchange in two steps: 
//parties exchange 224 bits of keys first, then the remaining 32 bits
//instead commitment for preventing influence shared secret 
//to obtain a predetermined short fingerprint by mounting MitM
//This IKE provide PFS and implicit authentication.

//Protected identification searching appropriate certificate for known contact in adressbook
//provide wPFS, UKS and KCI resistance

//Explicit authentication using shared password

//Block-stream encryption (CTR mode) with no error spreading

//All crypto procedures designed in mind for significant bit errors environment 
//and provides soft decision instead rejection of incorrect data 



 #include <stdlib.h>
 #include <stdio.h>
 #include <time.h>
 #include <string.h>

#ifdef _WIN32 
 #include <stddef.h>
 #include <stdlib.h>
 #include <basetsd.h>
 #include <stdint.h>
 #include <windows.h>
 #include <time.h>
 #include <conio.h>
#else //Linux
 #include <time.h>
 #include <sys/time.h>
 #include <sys/types.h>
 #include <sys/stat.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
 
#include "crypto/libcrp.h"
#include "fec/fec_golay2412.h"
#include "crp.h"



//definitions
#define SALT (char*)"$xphonesalt" //salt for PKDF
#define SALTLEN 12
//#define GUESTID 3895017989
#define GUESTID -399949307    //signed
//variables
static char mute=-1; //flag for voice can be transmitted
static char talk=-1; //flag of the last VAD decission
static char step=0;  //stage of connection process
static char role=0;  //depends comparing ephemeral public keys
static unsigned int  cnt_out=0; //counter of outgoing packets
static unsigned int cnt_in=0;  //synchronized counter of incoming packets
static float finv=0;  //soft flag of physical channel inversion
static float fcrc=0;  //soft level of sequence type (on DH, AU or work steps)
static unsigned int lastid=0;  //id of the outgoing contact specified by originator of the call
static unsigned int ourhid=0; //our hidden id
static unsigned char secret[32];  //our session secret key
static unsigned char pubkey[32]; //our session public key
static unsigned char skey[32]; //their session public key or agreed session symmetric keys
static unsigned char akey[32]; //keys derives from shared password
static unsigned char udata[32]; //temporary data array
static float fdata[288]={0}; //accumulator for soft bits decisions of received data
static KECCAK512_DATA cspng; //Keccak state (global for security reason)

//constants
static const unsigned char bitmask[8]={1,2,4,8,16,32,64,128}; //bit mask tab
//bits set lookup table
static const unsigned char BitsSetTable256[256] =
{
  #define B2(n) n,     n+1,     n+1,     n+2
  #define B4(n) B2(n), B2(n+1), B2(n+1), B2(n+2)
  #define B6(n) B4(n), B4(n+1), B4(n+1), B4(n+2)
    B6(0), B6(1), B6(1), B6(2)
};
//internal procedures level 1
static int MakeDH(unsigned char* pkt);
static int ProcessDH(unsigned char* pkt);
static int MakeAU(unsigned char* pkt);
static int ProcessAU(unsigned char* pkt);
static int MakeCtr(unsigned char* pkt);
static int ProcessCtr(unsigned char* pkt);
static int AgreedKey(void);
static int SearchContact(unsigned char* id, char* name);
static int ResetCT(int state);
//internal procedures level 2
static void VoiceEnc(unsigned char *pkt);
static void VoiceDec(unsigned char *pkt);
static int EncodeBlock(unsigned char* block, unsigned char* data);
static int DecodeBlock(unsigned char* data, unsigned char* block);
static void AssembleSoftBits(unsigned char* data, int bitstart, int bitlen);
static void UpdateSoftBits(unsigned char* data, int bitstart, int bitlen, int weight);
static unsigned int BytesToInt(unsigned char* bytes);
static void IntToBytes(unsigned char* bytes, unsigned int value);
static unsigned int crc32(unsigned int crc, const void *buf, int size);
static unsigned char crc8(unsigned char crc, const void *buf, unsigned length);

//*************************************************
//Connection's steps:
//0-idle (send/receive key's 224 bits parts but not send checksum)
//1-in key exchange (equal 0, but also send checksum algo 1)
//2-have first 224 bits part of their public key (equal 1, but send checksum algo 2)
//3-wait for last 32 bits part of their public key (key7)
//4-have key, wait for id
//5-acceptor have secret, wait for slow sync

//8-work mode (voice transmission available)


//transmitted packets depends state
//step 0 - key parts 0-223 bit without crc
//step 1 - key part 0-223 bit + crc1
//step 2 - key part 0-223 bit + crc0
//step 3 - crc0 + key7 (bits 224-255)
//step 4 - orig: key7 + hid, accept: crc0 + key7
//step 5 - key7 + hid
//step 8 - control/voice data

//processed incoming packets depends state
//steps 0,1 - key parts, crc0, crc1
//step 2 - crc1
//step 3 - key7
//step 4 - hid
//steps 5 - control data
//step 8 - control/voice data

//packets types for key data (32 bits payload payload):

//types 0-6: first 224 bits as a parts of session public key
//type 7: checksum 1 of transmitted key's parts 0-6
//type 8: checksum 0 of transmitted key's parts 0-6
//Note: for checksum computing uses algo 0 while we haven't remote key,
//otherwise uses algo 1

//type 9: last 32 bits as a parts of session public key
//type 10: our hidden id for this contact


//Transporting format of the data packet
//packet contains:
// 32 bits of payload,
// 4 bits label is packet's type (DH/AU stages), or 4 LSB of payload  (CTR stage)
// 8 bits tag is transporting crc8 (DH/AU stages), or authenticator (CTR stage)
//Note: transporting crc8 computes as a crc8 of  32 bits payload
//on the DH stages (0-2) and inverted one on the AU stages (3-4)
//This this allows receiver to obtain actual state of transmitter

//the 81 bits packet of transport layer format:
//8 bits of header is crc8/authenticator
//3*24 bits of FEC-protected(1/2) and greyed 32-bits payload + 4 bits packets type
//1 bit always 0 for obtaining inversion of physical channel

//Note: FEC is 3 Golay24/12 codes protects 36 bits (32 bits payload and 4 bits type)
//36 bits (as a hash of 4 bits type field) uses for greying 32 bits of payload


//*********************************Level 0: auxiliary*******************************
//==================================================================================
//vanilla crc8
static unsigned char crc8(unsigned char crc, const void *buf, unsigned length)
{
 static const unsigned char crc_table[256] =
 {
  0x00, 0x25, 0x4A, 0x6F, 0x94, 0xB1, 0xDE, 0xFB,
  0x0D, 0x28, 0x47, 0x62, 0x99, 0xBC, 0xD3, 0xF6,
  0x1A, 0x3F, 0x50, 0x75, 0x8E, 0xAB, 0xC4, 0xE1,
  0x17, 0x32, 0x5D, 0x78, 0x83, 0xA6, 0xC9, 0xEC,
  0x34, 0x11, 0x7E, 0x5B, 0xA0, 0x85, 0xEA, 0xCF,
  0x39, 0x1C, 0x73, 0x56, 0xAD, 0x88, 0xE7, 0xC2,
  0x2E, 0x0B, 0x64, 0x41, 0xBA, 0x9F, 0xF0, 0xD5,
  0x23, 0x06, 0x69, 0x4C, 0xB7, 0x92, 0xFD, 0xD8,
  0x68, 0x4D, 0x22, 0x07, 0xFC, 0xD9, 0xB6, 0x93,
  0x65, 0x40, 0x2F, 0x0A, 0xF1, 0xD4, 0xBB, 0x9E,
  0x72, 0x57, 0x38, 0x1D, 0xE6, 0xC3, 0xAC, 0x89,
  0x7F, 0x5A, 0x35, 0x10, 0xEB, 0xCE, 0xA1, 0x84,
  0x5C, 0x79, 0x16, 0x33, 0xC8, 0xED, 0x82, 0xA7,
  0x51, 0x74, 0x1B, 0x3E, 0xC5, 0xE0, 0x8F, 0xAA,
  0x46, 0x63, 0x0C, 0x29, 0xD2, 0xF7, 0x98, 0xBD,
  0x4B, 0x6E, 0x01, 0x24, 0xDF, 0xFA, 0x95, 0xB0,
  0xD0, 0xF5, 0x9A, 0xBF, 0x44, 0x61, 0x0E, 0x2B,
  0xDD, 0xF8, 0x97, 0xB2, 0x49, 0x6C, 0x03, 0x26,
  0xCA, 0xEF, 0x80, 0xA5, 0x5E, 0x7B, 0x14, 0x31,
  0xC7, 0xE2, 0x8D, 0xA8, 0x53, 0x76, 0x19, 0x3C,
  0xE4, 0xC1, 0xAE, 0x8B, 0x70, 0x55, 0x3A, 0x1F,
  0xE9, 0xCC, 0xA3, 0x86, 0x7D, 0x58, 0x37, 0x12,
  0xFE, 0xDB, 0xB4, 0x91, 0x6A, 0x4F, 0x20, 0x05,
  0xF3, 0xD6, 0xB9, 0x9C, 0x67, 0x42, 0x2D, 0x08,
  0xB8, 0x9D, 0xF2, 0xD7, 0x2C, 0x09, 0x66, 0x43,
  0xB5, 0x90, 0xFF, 0xDA, 0x21, 0x04, 0x6B, 0x4E,
  0xA2, 0x87, 0xE8, 0xCD, 0x36, 0x13, 0x7C, 0x59,
  0xAF, 0x8A, 0xE5, 0xC0, 0x3B, 0x1E, 0x71, 0x54,
  0x8C, 0xA9, 0xC6, 0xE3, 0x18, 0x3D, 0x52, 0x77,
  0x81, 0xA4, 0xCB, 0xEE, 0x15, 0x30, 0x5F, 0x7A,
  0x96, 0xB3, 0xDC, 0xF9, 0x02, 0x27, 0x48, 0x6D,
  0x9B, 0xBE, 0xD1, 0xF4, 0x0F, 0x2A, 0x45, 0x60
 };

 const unsigned char* ptr;

 ptr = buf;

 while (length--)
 crc = crc_table[crc ^ *ptr++];
 return crc;
}

//====================================================
//vanilla crc32
static unsigned int crc32(unsigned int crc, const void *buf, int size)
{
        static const unsigned int crc32_tab[] = {
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
	0xe963a535, 0x9e6495a3,	0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
	0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
	0xf3b97148, 0x84be41de,	0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,	0x14015c4f, 0x63066cd9,
	0xfa0f3d63, 0x8d080df5,	0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
	0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,	0x35b5a8fa, 0x42b2986c,
	0xdbbbc9d6, 0xacbcf940,	0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
	0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,	0x76dc4190, 0x01db7106,
	0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
	0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
	0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
	0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
	0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
	0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
	0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
	0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
	0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
	0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
	0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
	0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
	0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
	0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
	0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
	0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
	0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
	0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
	0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
	0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
	0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
	0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
	0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

	const unsigned char *p;

	p = buf;
	crc = crc ^ ~0U;

	while (size--)
		crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);

	return crc ^ ~0U;
}

//====================================================
//Converts 4 bytes array to unsigned integer
//This is more portable then the cast of pointers
static unsigned int BytesToInt(unsigned char* bytes)
{
 unsigned int d;

 d=bytes[0]+(bytes[1]<<8)+(bytes[2]<<16)+(bytes[3]<<24);
 return d;
}

//====================================================
//Converts unsigned integer to 4 bytes array
static void IntToBytes(unsigned char* bytes, unsigned int value)
{
 bytes[0]=value&0xFF;
 bytes[1]=(value>>8)&0xFF;
 bytes[2]=(value>>16)&0xFF;
 bytes[3]=(value>>24)&0xFF;
}

//===================================================================================
//Encode 36 data bits to 81 bits block (to transporting format)
static int EncodeBlock(unsigned char* block, unsigned char* data)
{
 unsigned int d;

 //input: 32 bits of payload in data[0-3], 4 bits of tag in data[4], 8 bits of header in data[5]
 //output: the 81 bits packet of transport layer format:
 //8 bits of header,
 //3*24 bits of FEC-protected and greyed 32-bits payload + 4 bits packets type
 //1 bit always 0 for obtain inversion of physical channel

 //greying the tag
 data[4]&=0x0F;
 if(1&data[4]) data[4]^=0x0E;

 //graying the payload
 d=data[4]; //greyed tag
 d=crc32(0, data+4, 1);  //mask derives from greyed tag
 data[0]^=d&0xFF;
 data[1]^=(d>>8)&0xFF;
 data[2]^=(d>>16)&0xFF;
 data[3]^=(d>>24)&0xFF;

 //encode data to block
 d=data[0]+((0xF&data[1])<<8); //first 12 bits of data
 d=fec_golay2412_encode_symbol(d);  //24 bits codeword
 block[1]=d&0xFF;                  //add to block
 block[2]=(d>>8)&0xFF;
 block[3]=(d>>16)&0xFF;

 d=(data[1]>>4)+(data[2]<<4);   //second 12 bits of data
 d=fec_golay2412_encode_symbol(d); //24 bits codeword
 block[4]=d&0xFF;              //add to block
 block[5]=(d>>8)&0xFF;
 block[6]=(d>>16)&0xFF;

 d=data[3]+((0xF&data[4])<<8);     //third 12 bits of data
 d=fec_golay2412_encode_symbol(d);   //24 bits codeword
 block[7]=d&0xFF;                 //add to block
 block[8]=(d>>8)&0xFF;
 block[9]=(d>>16)&0xFF;

 block[0]=data[5];  //add header (unchanged)
 block[10]=0;       //polarity bit always 0

 return 0;
}

//===================================================================================
//decode 81 bits block to 36 data bits
//returns polarity 1/-1 or 0 if block erred
static int DecodeBlock(unsigned char* data, unsigned char* bblock)
{
 int i;
 unsigned int d;
 unsigned char block[10];

 //set block polarity
 memcpy(block, bblock, 10);
 if(1&bblock[10]) for(i=0;i<10;i++) block[i]^=0xFF;

 //FEC first 24 bits symbol
 d=block[1]+(block[2]<<8)+(block[3]<<16);
 d=fec_golay2412_decode_symbol(d);
 if(d&0x1000) return 0; //uncorrectable
 data[0]=d&0xFF;
 data[1]=(d>>8);

 //FEC second 24 bits symbol
 d=block[4]+(block[5]<<8)+(block[6]<<16);
 d=fec_golay2412_decode_symbol(d);
 if(d&0x1000) return 0; //uncorrectable
 data[1]|=(d<<4)&0xF0;
 data[2]=d>>4;

 //FEC third 24 bits symbol
 d=block[7]+(block[8]<<8)+(block[9]<<16);
 d=fec_golay2412_decode_symbol(d);
 if(d&0x1000) return 0; //uncorrectable
 data[3]=d&0xFF;
 data[4]=(d>>8)&0x0F;

 //un-greyed payload
 d=crc32(0, data+4, 1);  //mask derives from greyed tag
 data[0]^=d&0xFF;
 data[1]^=(d>>8)&0xFF;
 data[2]^=(d>>16)&0xFF;
 data[3]^=(d>>24)&0xFF;

 //un-greyed tag
 if(1&data[4]) data[4]^=0x0E;

 //copy header (unchanged)
 data[5]=block[0];

 //returns actual polarity
 if(1&bblock[10]) return -1;
 else return 1;
}

//===========================================================
//update soft bits array using common metric
static void UpdateSoftBits(unsigned char* data, int bitstart, int bitlen, int weight)
{
 //input: data points to a uchar source bit sequence, bitstart is a start bit in this sequence
 //bitlen is a number of updated bits, weight is a metric value in range -4 to 4 
 
 int i;
 for(i=0;i<bitlen;i++)
 {
  fdata[i+bitstart]*=0.95;  //accumulate soft metrics each update
  if(data[i/8]&bitmask[i%8]) fdata[i+bitstart]+=weight; else fdata[i+bitstart]-=weight; //updates soft bits depends metric 
 }
}

//read soft bits
static void AssembleSoftBits(unsigned char* data, int bitstart, int bitlen)
{
 //input: bitstart is a start bit in bit sequence
 //bitlen is a number of bits will be read
 //output: data points to a uchar as a destination of bits 
 
 int i;
 
 if(bitlen%8) memset(data, 0, bitlen/8+1); else memset(data, 0, bitlen/8); //clear destination array
 for(i=0;i<bitlen;i++) if(fdata[i+bitstart]>0) data[i/8]|=bitmask[i%8]; //hard decision of soft bits
}


//**************************************Level 1: private*****************************
//===================================================================================
//Reset crypto engine
static int ResetCT(int state) 
{
 int i;
 //clear counters and set specified stage
 //set all secrets as random
 //clear temp memory

 step=state; //set specified stage
 role=0;    //clear role flag
 ourhid=0; //clear our hidden id
 cnt_in=0;  //clear counters
 cnt_out=7; //set initial counter value for packet's type 7 (key's crc)
 mute=-1;  //no voice transmission allowed

 randFetch(akey, 32);  //set authentication keys at random
 randFetch(skey, 32);  //set encryption keys at random
 randFetch(secret, 32); //peak new session secret key at random
 get_pubkey(pubkey, secret); //computes corresponds public key
 for(i=0;i<32;i++) udata[i]=0;  //clear temporary data array
 memset(&cspng, 0, sizeof(cspng));  //clear Keccak state
 finv=0; //clear channel inversion flag
 for(i=0;i<288;i++) fdata[i]=0; //clear accumulator of received soft bits
 printf("\r\nCryptoengine reinitialized\r\n");
 return 0;
}

//===================================================================================
//make packet of DH sequence
static int MakeDH(unsigned char* pkt)
{
 //input: ignores
 //output DH packet ready for sending
 //returns: -1 for key packet ready, 0 for error
 //int i;
 unsigned int d=0;
 if(cnt_out>14) cnt_out=7;
 if(cnt_out>7) cnt_out=0; //roll packets type 0-8 for key sequence
 if((!step)&&(cnt_out>6)) cnt_out=0; //not send type8 packets (checksum) while not active
 if(cnt_out==7)  //type7: checksum (algo 1) of 224 bits of our session (ephemeral) public key transmitted on DH stage
 {
  if(step==2) //on step 2 we replace type7 by type8 (checksum algo 0)
  {
   d=crc32(0, pubkey, 28); //summ0: indicates that we have a complete their key
   cnt_out=8;
  }
  else d=crc32(1, pubkey, 28); //summ1: we not have a complete their key yet
 }
 else d=BytesToInt(pubkey+4*(cnt_out&0x07)); //types 0-6: parts of our public session key

 IntToBytes(udata, d); //32 bits of payload
 udata[4]=cnt_out&0x0F; //packets type
 udata[5]=crc8(0, udata, 4); //transporting crc8
 EncodeBlock(pkt, udata); //assemble FEC-protected packet
 cnt_out++; //counter for sequence of packet's types
 return -1;
}

//===================================================================================
//process incoming packets on DH stage of connection
static int ProcessDH(unsigned char* pkt)
{
 //input: received packet
 //output: none
 //returns: //returns: -1 for DH packet ready, 0 for error
 int i, w, ptype;
 unsigned char c;

 //count incoming packets
 if(step==2) cnt_in++;  //counter of total packets received on initial steps
 if(cnt_in>30) //check for time-out of step 2
 {
  printf("\r\nTimeout of key exchange\r\n");
  cnt_out=65535; //set counter full for force reset
  step=6; //set stage for controls
  return 0;
 }
 //decode packet and obtain channel polarity
 i=DecodeBlock(udata, pkt); //FEC the received data
 if(!i) return 0; //uncorrectable
 finv+=i; //accumulate inversion of physical channel
 //obtain packet's type
 ptype=udata[4]&0x0F; //obtain type of packet
 if(ptype==15) //check for 'initial' ptype15 causes clearing of soft bits
 {
  for(i=0;i<288;i++) fdata[i]=0; //clear bits accumulator
 }

 if(ptype>8) return 0; //only types 0-8 valid for key sequence
 //check crc8 and computes payload's metric
 c=crc8(0, udata, 4);  //check transport checksum
 w=4-BitsSetTable256[c^udata[5]]; //number of erred bits in it
 //accumulate metric to detection of current step on remote side
 fcrc*=0.95;
 fcrc+=w; //-1 for DH, 1 for AU, 0 for CTR/voice

 //check for DH sequence detected during idle state
 if((!step)&&(w>3)&&(ptype==7)) //activates on acceptor side if metric is hight for DH sequence
 {
  step=1; //set active
  cnt_out=7; //set counter for the DH transmission from crc8 packet
  printf("\r\nIncoming detected\r\n");
 }
 //accumulate soft bits (only for crc0 after valid crc1 received)
 if((step==1)||(ptype==8)) UpdateSoftBits(udata, 32*ptype, 32, w); //accumulate received bits while whole key received
 AssembleSoftBits(udata, 0, 288); //read currently received 0-6 key data
 //try crc1 algo for checking validity of already received key0-6
 if((step!=2)&&(BytesToInt(udata+28)==crc32(1,udata, 28))) //from step 1 to 2
 {
  step=2; //we have valid key, no receive more
 }
 //try crc0 algo for obtaining have remote party our key0-6 or not yet
 if(BytesToInt(udata+32)!=crc32(0,udata, 28)) return -1; //other part not have our key yet
 //now remote party have a our key0-6 and we have their key0-6: proceed next step
 memcpy(skey, udata, 28); //temporary store part0-6 of their session public key

 //clear soft data for next stage
 for(i=0;i<288;i++) fdata[i]=0;
 fcrc=0;
 cnt_in=0;
 cnt_out=10;
 step=3; //set next state
 return -1;
}
//===================================================================================
//make packet on the identification (AU) stages (3,4,5)
static int MakeAU(unsigned char* pkt)
{
 //input: ignores
 //output AU packet ready for sending
 //returns: -2 for AU packet ready, 0 for error
 unsigned int d=0;

 //two packet types sends in loop
 if((cnt_out>10)||(cnt_out<9)) cnt_out=9; //roll packets types
 //set packets type depends state
 if(cnt_out==10) //replace hid type by crc0 type
 {
  if(step==3) cnt_out=8;  //in state 3
  if((step==4)&&(!lastid)) cnt_out=8; //in state 4 on acceptor side
 }
 //set payload depends current packets type
 if(cnt_out==8) d=0xFFFFFFFF^crc32(0, pubkey, 28); //type8: crc0 of our pubkey (for finalize key sequence on the remote side)
 else if(cnt_out==9) d=BytesToInt(pubkey+28); //type9: part7 of our pubkey
 else d=ourhid; //type 10: our hid

 //compose payload
 IntToBytes(udata, d);    //payload
 udata[4]=cnt_out&0x0F;  //packet type
 udata[5]=0xFF^crc8(0, udata, 4); //transporting crc8

 EncodeBlock(pkt, udata); //assemble FEC-protected packet
 cnt_out++; //counter for sequence of packet's types
 return -2;
}

//===================================================================================
//process incoming packets on AU stage of connection
static int ProcessAU(unsigned char* pkt)
{
 //input: received packet
 //output: none
 //returns: -2 for AU packet processed, 0 for error
 int i, w, ptype;
 unsigned char c;

 cnt_in++;  //counter of total packets received
 if(cnt_in>50) //check for limit of stages 3-4
 {
  printf("\r\nTimeout of negotiation\r\n");
  cnt_out=65535; //set counter full for force reset
  step=6;   //set control stage
  return 0;
 }
 i=DecodeBlock(udata, pkt); //FEC data
 if(!i) return 0; //uncorrectable
 finv+=i;  //accumulates channel polarity flag
 //check packets type: 9-14 are valid for AU stage
 ptype=udata[4]&0x0F; //obtain packet's type
 if((ptype<9)||(ptype>10)) return 0; //unexpected packet type
 //computes metric
 c=crc8(0, udata, 4);  //check transport checksum
 w=BitsSetTable256[c^udata[5]]-4; //number of erred bits in it
 fcrc*=0.95; //accumulate metric for detection of current step on remote side
 fcrc+=w; //-1 for DH, 1 for AU, 0 for CTR/voice

 AssembleSoftBits(udata+4, 32*(ptype-9), 32); //extracts existed soft data
 UpdateSoftBits(udata, 32*(ptype-9), 32, w); //updates by new data;

 //process last part of key (key7) only in step 3
 if((step==3)&&(ptype==9))
 {
  if(memcmp(udata, udata+4, 4)) return -2; //compare new data with existed
  memcpy(skey+28, udata, 4);
  step=4; //data is same: probably correct
  //set role depend public key comparing
  for(i=0;i<32;i++)
  {
   if(skey[i]!=pubkey[i]) break; //byte-by-byte comparing
  }
  if(pubkey[i]>skey[i]) role=1; else role=0; //role 1 for our key greater then their
  //check for originator side have lastid
  if(!lastid) return -2;
  //further for originator side only: mask our id
  //computes secret with Diffie-Hellmann: curve25519_donna(s=Y^x, x, Y)
  curve25519_donna(udata, secret, skey); //compute shared secret to udata
  //derive idmask=H(secret)
  Sponge_init(&cspng, 0, 0, 0, 0);
  Sponge_data(&cspng, udata, 32, 0, SP_NORMAL); //absorb shared secret
  Sponge_finalize(&cspng, udata, 8);
  ourhid^=BytesToInt(udata+4*role); //mask our id (actual for originator only)
  return -2;
 }

 if((step!=4)||(ptype!=10)) return -2; //process hid only in step 4
 if(memcmp(udata, udata+4, 4)) return -2;
 //udata contains 4 bytes hid
 i=AgreedKey(); //computes session symmetric key and control codes
 if(i<0) //adressbook error
 {
  if(i==-1) printf("\r\nIdentification error\r\n");
  else if(i==-2) printf("\r\nAdressbook file not found\r\n");
  else if(i==-3) printf("\r\nImpersonation of contact\r\n");
  else printf("\r\nUnknown error\r\n");
  cnt_out=65535; //set counter full force reset
  step=6; //set control stage
  return 0;
 }
 //clears data for next stage
 for(i=0;i<288;i++) fdata[i]=0;
 fcrc=0;
 cnt_in=0;
 if(!lastid) step=5; //acceptor proceeds to step 5
 else  //originator of the call
 {
  step=6; //6 for originator: send ctr, receive ctr
  cnt_out=200; //set start values for initial synchronization of counters
 }
 return -2;
}

//===================================================================================
//computes session symmetric keys (for tx and rx directions)
//computes our control code and code for checking other side
static int AgreedKey()
{
 int i;
 unsigned int d;
 unsigned char sert[64];
 unsigned char buf[32];
 char str[32];

 //store their hidden id
 for(i=0;i<4;i++) sert[i]=udata[i];
 //compute shared secret as a Diffie-Hellmann: curve25519_donna(s=Y^x, x, Y)
 curve25519_donna(buf, secret, skey); //compute shared secret to udata
 //derive idmask=H(secret)
 Sponge_init(&cspng, 0, 0, 0, 0);
 Sponge_data(&cspng, buf, 32, 0, SP_NORMAL); //absorb shared secret
 Sponge_finalize(&cspng, udata, 8); //squeeze mask
 //unmask their hidden id and obtain their real id
 for(i=0;i<4;i++) sert[i]^=udata[i+4-4*role];
 //search in book, obtain certificate to buf and name to sdata
 str[0]=0;  //searching by id
 i=SearchContact(sert, str); //search certificate for this contact in addressbook
 if(i<0) //their id not found in our book, set up 'guest' call
 {
  printf("\r\nContact not in book, set up 'guest' call (UNTRUSTED)\r\n");
  IntToBytes(sert, GUESTID); //convert guest id to array
  i=SearchContact(sert, str); //obtain a certificate
  if(i<0) return i; //id not found: book error
 }
 else printf("\r\nContact identifies as %s\r\n", str); //presents name of the remote part
 //check for originator/acceptor of the call
 if(lastid) //for originator side
 {
  d=BytesToInt(sert); //their id
  if(d!=lastid) //compare with id uses for originate the call
  {             //difference occurs:
   if(d!=GUESTID) return -3; //their id not a guest: possible attack
   printf("\r\nContact not introduces, set up 'guest' call (UNTRUSTED)\r\n");
  }
 }
 else //for acceptor side
 {
  ourhid=BytesToInt(udata+4*role); //make mask for hide our id
  get_pubkey(udata, sert+32); //compute our long-term public from contact's certificate
  ourhid^=BytesToInt(udata);;  //mask our id
 }
 //derive session symmetric keys=encr||decr (128+128 bits)
 //=H(||shared_secret||their_sert^our_skey||their_pkey^our_sert)
 //This is TripleDH algo with implicit authentication
 Sponge_init(&cspng, 0, 0, 0, 0);
 //absorb pair 1 (ephemeral with ephemeral) as a shared secret
 Sponge_data(&cspng, buf, 32, 0, SP_NORMAL); //absorb shared secret
 //absorb pair 2 (long-term with ephemeral) depends role
 if(role) curve25519_donna(buf, secret, sert); //their_sert^our_skey
 else curve25519_donna(buf, sert+32, skey); //their_pkey^our_sert
 Sponge_data(&cspng, buf, 32, 0, SP_NORMAL); //absorb
 //absorb pair 3 (ephemeral with long-term)
 if(!role) curve25519_donna(buf, secret, sert); //their_sert^our_skey
 else curve25519_donna(buf, sert+32, skey); //their_pkey^our_sert
 Sponge_data(&cspng, buf, 32, 0, SP_NORMAL); //absorb
 //squeeze symmetric keys (256 bits)
 Sponge_finalize(&cspng, udata, 32);
 //swap keys for transmitting/receiving depends role
 if(role)
 memcpy(skey, udata, 32);  //0|1
 else                      //1|0
 {
  memcpy(skey, udata+16, 16);
  memcpy(skey+16, udata, 16);
 }
 //check for id derives from current session secret already exist in book
 i=AddContact(0); //id collision: must restart connecting for pairing be available
 if(i) printf("\r\nNote: you can't pair devices during this call!\r\n");
 //derives session control codes k=H(skey)
 Sponge_init(&cspng, 0, 0, 0, 0);
 Sponge_data(&cspng, skey, 16, 0, SP_NORMAL); //encryption key
 Sponge_finalize(&cspng, buf, 4); //SAS for answer
 d=BytesToInt(buf);
 printf("\r\nSay code: %04d     ",0x1FFF&d);
 Sponge_init(&cspng, 0, 0, 0, 0);
 Sponge_data(&cspng, skey+16, 16, 0, SP_NORMAL); //decryption key
 Sponge_finalize(&cspng, buf, 4); //SAS for checking
 d=BytesToInt(buf);
 printf("Check answer: %04d\r\n",0x1FFF&d);
 return 0;
}

//===================================================================================
//Build outgoing control or voice packet
static int MakeCtr(unsigned char* pkt)
{
 //input - voice packet or silence (with VAD flag)
 //output encrypted voice packet or control packet
 //returns -1 for voice or cnt_out for control

 //VAD marks silence by set byte 11
 //voice packet encrypts,
 //silence packet replaced by control packet

 //format of control packet:
 //32 bits payload is counter
 //4 bits tag is 4 LSB of counter
 //8 bits header is authenticator

 cnt_out++; //increment outgoing packet counter (never rewind!)

 //check for call time limit: outgoing counter full
 if(cnt_out>65534) //restart connection with exchanging new session keys
 {
  if(step>7) printf("\r\nLimit of call time, reconnected \r\n"); //call duration is elapsed
  if(lastid) printf("\r\nReconnecting\r\n"); //or reset forced by engine
  else printf("\r\nDisconnected, idle. Ready\r\n"); //or reset forced by engine
  ResetCT(0); //reset crypto to new values
  if(lastid) SetupCall(0); //set up our id for originator side
  cnt_out=15; //set packets type for clearing soft bits on remote side
 }
 //check for packet will be processed as a voice
 if((1&pkt[11])&&(step==8)) VoiceEnc(pkt);//check flag setted by Voice Active Detector
 else
 {
  //Make control packet: send our outgoing counter for sync remote party
  udata[0]=0xFF&cnt_out;
  udata[1]=0xFF&(cnt_out>>8);
  //copy of outgoing counter
  udata[2]=udata[0];
  udata[3]=udata[1];
  //tag as 4 lsb of counter
  udata[4]=0xF&udata[0]; //LSB of counter
  //computes header=H(akey|ekey|cnt) is authenticator
  Sponge_init(&cspng, 0, 0, 0, 0);
  Sponge_data(&cspng, akey, 16, 0, SP_NORMAL); //absorb key derived from password
  Sponge_data(&cspng, skey, 16, 0, SP_NORMAL); //absorb encryption session key
  Sponge_data(&cspng, udata, 2, 0, SP_NORMAL); //absorb 16 bit outgoing counter
  Sponge_finalize(&cspng, udata+5, 1); //au_tag 8 bit
  EncodeBlock(pkt, udata); //FEC data to block ready for transmitting
 }
 if(step>7) return cnt_out; //returns counter value for estimate time of the call
 else return -3; //or return -3 before connection completely established
}

//===========================================================
//Process incoming voice/control packet
static int ProcessCtr(unsigned char* pkt)
{
 //input: encrypted voice or control pkt (auto detected)
 //output: decrypted voice or none
 //returns: -3 for voice or password authentication result (0-8) otherwise

 unsigned int d;
 int i, delta;
 unsigned char c;
 char au=-3;  //number of bits matches password's authenticator

 //increment incoming packets counter (newer rewind!)
 cnt_in++;
 //check for sync time limit
 if(cnt_in==200) //the last packet before synchronization
 {
  printf("\r\nTimeout of sync\r\n");
  cnt_out=65535; //set outgoing counter full for force reset
  step=6;   //set control stage
  return 0;
 }
 //check for incming counter full
 if(cnt_in>65534)
 {
  printf("\r\nLimit of packets counter\r\n");
  cnt_out=65535; //set outgoing counter full for force reset
  step=6;   //set control stage
  return 0;
 }
 //try process received data as a control packet
 i=DecodeBlock(udata, pkt); //FEC
 //accumulate crc8 matching for detect DH/AU sequence
 c=crc8(0, udata, 4);  //check transport checksum for DH/AU (not CTR/voice)
 fcrc*=0.95;
 fcrc+=(4-BitsSetTable256[c^udata[5]]); //averages 4 for DH, -4 for AU, 0 for CTR/voice;
 //check for probably key sequence (other party was resets)
 if(fcrc>60) //input DH sequence detected, force reset to receive new key
 {
  printf("\r\nRemote party force reset\r\n");
  cnt_out=65535; //set counter full for force reset
  step=6;  //set control stage
  return 0;
 }
 
 //check for label exactly matches at least one copy of the counter
 if(((0xF&udata[0])!=udata[4])&&((0xF&udata[2])!=udata[4])) i=0;
 //check for copies difference is less then a few bits
 if((BitsSetTable256[udata[0]^udata[2]]+BitsSetTable256[udata[1]^udata[3]])>2) i=0;
 //random probability for this conditions is 10^-17 (2.5 hours) 

 //check payload for control format
 if(!i)//packet is a voice data or errored
 {
  if(step!=8) return 0; //process voice only on final (work) stage
  VoiceDec(pkt); //decode this packet as a voice in work mode
  return -3;
 }
 //Now we have probably control packet, probably with erred bits
 //And also there are small probability (<1/16) this is a voice packet with random payload
 //occurs matching to a control format (this is will be detected later)

 //compute expected authenticator H(akey|dkey|cnt_in)
 IntToBytes(udata+8, cnt_in);  //our incoming counter
 Sponge_init(&cspng, 0, 0, 0, 0);
 Sponge_data(&cspng, akey+16, 16, 0, SP_NORMAL); //absorb key derived from password
 Sponge_data(&cspng, skey+16, 16, 0, SP_NORMAL);//absorb decryption session key
 Sponge_data(&cspng, udata+8, 2, 0, SP_NORMAL);  //absorb incoming counter
 Sponge_finalize(&cspng, udata+6, 1); //au_tag 8 bit
 //computes bits differences between expected tag and tag from packet
 if(step==8) au=(8-BitsSetTable256[udata[5]^udata[6]]);
 else au=0; //result is authentication level in range 0-8 and must be
            //accumulated from packet's sequence for evaluation of the level of identification
 
 //try synchronize our incoming counter with their outgoing
 d=udata[0]+(udata[1]<<8); //received value of remote counter
 //computes difference between their outgoing and our incoming counter
 delta=d-(cnt_in&0xFFFF); //positive for need forward correction, negative - for back
 if(delta>32767) delta-=65535; //obtain forward or back difference
 else if(delta<-32767) delta+=65535; //in range 32768 packets (correctable are +-30 min average)
 //accumulate bits of delta value during packets sequence (real delta must relatively constant)
 d=abs(delta); //convert format from integer to unsigned
 if(delta<0) d+=32768; //this format better for accumulation due small bits differences
 udata[6]=0xFF&d;
 udata[7]=0xFF&(d>>8);
 UpdateSoftBits(udata+6, 0, 32, 1); //averages deltas of packets sequence
 //fast correction: the current delta must be 1 or -1
 
 //fast correction
 //check for copies of counters the same and delta is small
 //random probability of this conditions is 10^-24 (10 hours)
 if((udata[0]==udata[2])&&(udata[1]==udata[3])&&(delta>-255)&&(delta<255))
 {
  if(step!=8) //initial sync allowed both back and forward
  {
    printf("\r\nConnected\r\n");
    if(cnt_out<200) cnt_out=201;
    step=8;  //set work state
  }
  else if(delta)//recovery of losses sync
  {
   printf("\r\nSync: %d\r\n", delta);
   if(delta<-1) delta=-1;  //freeze counter instead back correction
  }
  cnt_in+=delta;  //corrects counter
  if(delta) for(i=0;i<16;i++) fdata[i]=0; //clear accumulated soft bits of this delta
  fcrc=0; //clear accumulator of DH sequence
  return au; //delta is small: this is probably control packet, not voice
 }

 //slow correction: any constant delta
 AssembleSoftBits(udata+8, 0, 32);   //use averages bit of previous deltas
 d=0;
 if(step!=8) c=10; else c=17; //accumulation is faster before connection established
 for(i=0;i<16;i++) //bit-by-bit
 {
  if((fdata[i]<-c)||(fdata[i]>c)) d++; //check metric of each bit of averages delta
 }
 if(d==16) //all bits probably valid
 {
  //slow sync event
  d=udata[8]+(udata[9]<<8);  //hard decision of averages valid delta
  if(d>32767) delta=0-(0x7FFF&d); else delta=d; //restore integer format
  
  if(step!=8) //initial sync allowed both back and forward
  {
    printf("Connected slow\r\n");
    if(cnt_out<200) cnt_out=201;
    step=8;  //set work state
  }
  else if(delta)//recovery of losses sync
  {
   printf("Sync slow: %d\r\n", delta);
   if(delta<-1) delta=-1;  //freeze counter instead back correction
  }
  cnt_in+=delta;  //corrects counter
  for(i=0;i<16;i++) fdata[i]=0; //clear accumulated soft bits of this delta
  fcrc=0; //clear accumulator of DH sequence
 }
 //if no sync events occurs we must check this packet is a voice early incorrectly interpreted as control
 //bit-by-bit compare recently received delta with averages delta
 i=BitsSetTable256[udata[6]^udata[8]]+BitsSetTable256[udata[7]^udata[9]];
 if((i<3)||(step!=8)) return au; //14 or more bits from 16 are matching: this is probably control packet
 VoiceDec(pkt); //otherwise process this packet as a voice
 return -3;
}

//===========================================================
//encrypts voice data for transmission
static void VoiceEnc(unsigned char *pkt)
{
  int i;
  //input: 81 bits of vice data in pkt
  //output: encrypted 81 bits ready for transmission

  //use current outgoing counter (32 bits value) as IV
  IntToBytes(udata, cnt_out);
  //computes gamma=H(cnt_out||ekey)
  Sponge_init(&cspng, 0, 0, 0, 0);
  Sponge_data(&cspng, udata, 4, 0, SP_NORMAL); //absorbs counter
  Sponge_data(&cspng, skey, 16, 0, SP_NORMAL); //absorbs Enc key
  Sponge_finalize(&cspng, udata, 11); //squeeze gamma
  udata[10]&=0x01; //81 bits
  for(i=0;i<11;i++) pkt[i]^=udata[i]; //encrypts voice
}

//===========================================================
//decrypts received voice packet
static void VoiceDec(unsigned char *pkt)
{
  int i;
  //input: 81 bits received packet classified as a voice by not matching a control format
  //output: 81 bits voice data ready for decompressing by MELPE codec

  //apply channel polarity (usually is constant during call) 
  if(finv<0) //check for channel polarity obtained on agreed steps
  {
   for(i=0;i<10;i++) pkt[i]^=0xFF; //inverse 80 bits received data
   pkt[10]^=1; //inverse bit 81
  }
  //use current incoming counter (32 bits value) as IV
  IntToBytes(udata, cnt_in);
  //computes gamma=H(cnt_in||ekey)
  Sponge_init(&cspng, 0, 0, 0, 0);
  Sponge_data(&cspng, udata, 4, 0, SP_NORMAL); //absorbs counter
  Sponge_data(&cspng, skey+16, 16, 0, SP_NORMAL); //absorbs Dec key
  Sponge_finalize(&cspng, udata, 11); //squeeze gamma
  udata[10]&=0x01; //81 bits
  for(i=0;i<11;i++) pkt[i]^=udata[i]; //decrypts voice
}


//*******************************Level 2: public*************************************
//===================================================================================
//Print all contacts from address book matches mask
int ListContact(char* mask)
{
 int i,l,p;
 FILE* fl=0;
 char str[256];  //space for string from file
 int n=0;

 //try open bookfile for reading as text
 if(!(fl = fopen((char*)"contacts.txt", "rt" )))
 {
  printf("\r\nAddress book file not found!\r\n");
  return -2; //if specified addressbook not found return error code -1
 }

  //read strings while exhausted
  while(!feof(fl))
  {
   n++; //strings counter
   if(!fgets(str, 256, fl)) break; //read contact's string from addressbook
   l=strlen(str); //strings length
   p=0; //pointer to the start of info field
   for(i=0;i<l;i++) //search of certificate's boundaries for obtain name and info fields
   {
    if(str[i]=='}') p=i; //set pointer to the end of cert
    else if((str[i]=='{')||(str[i]<32)) str[i]=0; //truncate name field
   }
   if(p) p++; //move pointer to the info field
   //check for fields matches mask and output
   if( strstr(str, mask) || strstr(str+p, mask) ) printf("%d: '%s' [%s]\r\n", n, str, str+p);
  }
  fclose(fl); //close file
  return n;  //number of entries looks
}

//===================================================================================
//Search contact entry in adressbook by contact's ID or name
static int SearchContact(unsigned char* idd, char* nname) 
{
 //input: 4 bytes array contains id (name is emty)
 //or string in name
 //output: last founded contact's certificate in id, contact's info in name
 //returns-2 if adressbook file not found,  -1 if id or name not found
 //or number of records matched if OK
 //NOTE: result is FIRST matched record in adressbook!
 //id or name can be NULL

  int i, n=0;
  int r=0;
  FILE* fl=0;
  char str[256];  //space for string from file
  char info[32]={0}; //name/info of last founded record
  char name[32]={0};
  unsigned char buf[64]; //certificate of last founded record

  //copy name string up to 31 characters
  if(nname) memcpy(name, nname, 32);
  name[31]=0; //terminate string

  //try open bookfile for reading as a text
  if(!(fl = fopen((char*)"contacts.txt", "rt" )))
  {
   printf("\r\nAddress book file not found!\r\n");
   return -2; //if specified addressbook not found return error code -1
  }

  //searching loop
  if(!name[0]) //name is empty: searching by id
  {
   while(!feof(fl))  //while end of file
   {
    if(!idd) break; //id must be specified if name is empty
    n++;  //next entry in book
    str[0]=0;  //clear resulting string
    if(!fgets(str, 256, fl)) continue; //read contact's string from addressbook
    if(!str[0]) continue; //skip empty strings
    if(64!=b64dstr(str, buf, 64)) continue; //decode b64->binary, must be 64
    if(memcmp(idd, buf, 4)) continue; //compare first 4 bytes of certificate
    for(i=0;i<strlen(str);i++) //search for end of name field in all string
    {
     if(str[i]=='{') break; //found start of certificate
    }
    if(!i) continue; //no name field
    str[i]=0; //truncate string: only name
    if(strlen(str)>31) continue; //name too long?
    strcpy(name, str); //copy name to info
    str[i]='{'; //restore start of certificate
    for(i=0;i<strlen(str);i++) //search for end of certificate field in all string
    {
     if(str[i]=='}') break;  //found end of certificate, start of info field
    }
    if(!i) continue; //format error
    if(strlen(str+i)>31) continue; //info field too long?
    strcpy(info, str+i+1); //copy info to info
    for(i=0;i<strlen(info);i++) if(info[i]<32) info[i]=0; //skip unprintable
    r=1; //result is OK
    break; //found
   }
  }
  else   //name is specified: search by name
  {
   while(!feof(fl)) //while end of file
   {
    n++; //next entry
    str[0]=0; //clear resulting string
    if(!fgets(str, 256, fl)) continue; //read contact's string from addressbook
    if(!str[0]) continue; //skip empty strings
    for(i=0;i<strlen(str);i++) //search for end of name field in all string
    {
     if(str[i]=='{') break; //found start of certificate
    }
    if(!i) continue; //no name field
    str[i]=0; //truncate string: only name
    if(!strcmp(name, str)) //compare name
    {
      str[i]='{'; //restore all string
      if(64!=b64dstr(str, buf, 64)) continue; //decode b64->binary
      for(i=0;i<strlen(str);i++) //search for end of certificate field in all string
      {
       if(str[i]=='}') break;  //found end of certificate, start of info field
      }
      if(!i) continue; //error
      if(strlen(str+i)>31) continue; //info field too long?
      strcpy(info, str+i+1); //copy info to info
      for(i=0;i<strlen(info);i++) if(info[i]<32) info[i]=0; //skip unprintable
      r=1; //result is OK
      break; //found
    }
   }
  }

  fclose(fl); //close file
  if(r) //if at-least one entry found
  {
   if(idd) memcpy(idd, buf, 64); //output certificate
   sprintf(str, "'%s' [%s]", name, info); //compose name and info
   if(nname) strcpy(nname, str); //output
   return n; //returns number of entries matches
  }
  else return -1; //or no one found
}

//===================================================================================
//set mute flag, returns outgoing counter value with applied mute flag
int Mute(int flag)
{
 if(step==8) //change flag only in work mode
 {
  if(flag==1) mute=1; //voice transmission allowed
  else if(flag==-1) mute=-1; //voice transmission denied
 }
 else mute=-1;
 return mute * cnt_out; //outgoing counter's value with sign depends actual muting state
}

//set talk flag (last VAD decission), returns step value with applie talk flag
int State(int flag)
{
 if(step==8) //change flag only in work mode
 {
  if(flag==1) talk=1;
  else if(flag==-1) talk=-1;
 }
 else talk=-1;
 return step*talk;
}

//===================================================================================
//wrapper for making outgoing packets
int MakePkt(unsigned char* pkt) //Make pkt from 81 bit(11 bytes) data, last bit is VAD decision
{
 //input: voice data with VAD flag in bit 81, or ignores in steps 0-7
 //output: packet ready for sending
 //returns: 0 for idle/error, -1 for DH, -2 for AU, -3 for voice or cnt_out
 int i;

 if(step>5) i=MakeCtr(pkt);  //send voice/control data after IKE complete
 else if(step>2) i=MakeAU(pkt); //send ID material on the identification stage
 else i=MakeDH(pkt); //send public keys material on the Diffie-Hellman agreement stage

 return i;
}

//===================================================================================
//wrapper for processing incoming packets
int ProcessPkt(unsigned char* pkt)
{
 //input: received packet 81 bits
 //output: decrypted voice data or nothing
 //returns: 0 for idle, -1 for DH, -2 for AU, -3 for voice, AU-level for control packets
 int i;

 if(step>4) i=ProcessCtr(pkt); //steps 5,6,7,8
 else if(step>2) i=ProcessAU(pkt); //steps 3,4
 else i=ProcessDH(pkt); //steps 0,1,2

 return i;
}

//===================================================================================
//reconnect current call with new key exchange 
void ResetCall(void)
{
 //printf("\r\nCall resetted by user\r\n");
 ResetCT(0); //reset crypto to new values
 if(lastid) SetupCall(0); //set-up our id for originator side
 cnt_out=15; //set packets type for clearing soft bits on remote side
}

//===================================================================================
//inviting call by originator
int SetupCall(char* nname)
{
 //input: contact name or empty for recall
 //output: contact's info from book
 //returns: 0 for OK, -1 for contact not found, -2 for adressbook file not found
 int i;
 //unsigned int d;
 unsigned char buf[64];
 char name[80]={0};

 if(nname) memcpy(name, nname, 32); //copy name up to 31 chars
 name[31]=0; //truncate string
 if(!(*name)) strcpy(name, (char*)"guest"); //use guest for empty name

 IntToBytes(buf, lastid); //search last outgoing contact id if name is empty
 i=SearchContact(buf, name); //search certificate for this contact in addressbook
 if(i<0) //no results of error
 {
  if(i==-1) printf("\r\nContact '%s' not in adressbook", name);
  else if(i==-2) printf("\r\nAdressbook file not found");
  else printf("\r\nUnknown error");
  printf(", call terminated\r\n");
  return i; //check for bookfily and specified contact exist
 }
 step=1; //set active
 lastid=BytesToInt(buf); //their id (save for recall)
 get_pubkey(buf, buf+32); //compute our long-term public from contact's certification
 ourhid=BytesToInt(buf);  //our id for this contact (not masked yet)
 printf("\r\nOutgoing call to %s\r\n", name);
 return i; //returns searching results
}

//===========================================================
//derive authentication keys from passphrase
void SetPassword(char* pass) //Set key for authentication by pre-shared password
{
 //input: passphrase can contains one or two words separated by space
 //one-word passphrase sets common key for both sides
 //two-word phrase sets two keys:
 //the key derives from the first word uses for checking their received
 //and second - for making our authenticator for transmission
 //so sides can pre-share same phrase but swap words place-by-place
 //A party under enforcement can change second word and hidden notice other side
 //whereas at its side there is no way to detect this

 #define PKDFSTEPS 16
 int i, l, k=0;
 unsigned char r=0;
 char password[256]={0};
 //password string can be one word or two words separated by space
 //the first word use for check incoming mac
 //the second - for compute outgoing mac
 //the parties must pre-shared phrase and one side must use it
 //as first-second word sequence but another - as second-first
 //Under enforcement participant must change SECOND word for
 //secure notification of another side about enforcement

 //copy constant input to string
 l=strlen(pass);
 if(l>255) l=255; //up to 255 characters in passphrase
 for(i=0;i<l;i++) password[i]=pass[i];
 password[l]=0; //terminate string

 //check for spaces in the string, split
 k=0;
 for(i=0;i<strlen(password);i++) if(password[i]==32)
 {
  password[i]=0;
  k=i; //position of the last space
 }
 if(k)
 {
  k++; //the start of last word
  r+=4;
 }

 //process second half (will be used for subscribing outgoing data)
 l=strlen(password+k); //last word (or single)
 if(!l) //phrase is empty: reset to default
 {
  strcpy(password+k, (char*)"0");
  l=1;
  r+=1;
 }
 //computes akey=H(salt||pasword||...||password)
 Sponge_init(&cspng, 0, 0, 0, 0);
 Sponge_data(&cspng, (unsigned char*)SALT, SALTLEN, 0, SP_NORMAL);
 for(i=0;i<PKDFSTEPS;i++) Sponge_data(&cspng, (unsigned char*)(password+k), l, 0, SP_NORMAL);
 Sponge_finalize(&cspng, akey, 16); //akey for subscribing

 //process first half (will be used for checking incoming data)
 l=strlen(password); //first word (or single)
 if((!l)||(r&1)) //phrase is empty: reset to default
 {
  strcpy(password, (char*)"0");
  l=1;
  r+=2;
 }
 //computes akey=H(password||...||password||salt)
 Sponge_init(&cspng, 0, 0, 0, 0);
 Sponge_data(&cspng, (unsigned char*)SALT, SALTLEN, 0, SP_NORMAL);
 for(i=0;i<PKDFSTEPS;i++) Sponge_data(&cspng, (unsigned char*)password, l, 0, SP_NORMAL);
 Sponge_finalize(&cspng, akey+16, 16); //akey for checking

 if(r&4) printf("\r\nPassphrase ");
 else printf("\r\nPassword ");
 if(r&3)printf("resetted to default\r\n");
 else printf("applied\r\n");
}

//===========================================================
//Add new contact to adressbook
int AddContact(char* name)
{
 char str[256];
 unsigned char buf[64];
 int i, p=0;
 FILE* fl=0;

 //adressbook uses for user-friendly automatic authentication
 //between parties after devices  have been paired
 //Safe pairing possible during untrusted guest call after
 //the parties were convinced of authenticity the channel
 //This can be done using the pre-shared password, for example,
 //passing it with PGP. Other way is visual checking of control
 //codes after the connection established while direct contact.
 //After pairing both sides have own private and their public keys
 //individually for this pair as a adressbook entry named for this contact.
 //Format: entry string as a name(up to 31 chars), b64 representation
 //of a 64 bytes of the keys in braces and optionally up to 31 chars info
 //(for example, mobile phone number for dialling).

 if((step!=8)&&(name))
 {
  printf("\r\nDevices can be paired only after connection complete\r\n");
  return 0;
 }

 if(name)  //if name not specified: only search for id collision
 {
  //scan name string for space separates name and info parts
  for(i=0; i<strlen(name); i++) if(name[i]==' ')
  {
   p=i; //points to the info part  (after last space)
   break;
  }
  //split to two strings: name and info
  //if(p) name[p]=0;
  //p++; //point to info string
  
  if(p) 
  {
   name[p]=0;
   p++; //point to info string
  }

  //check for name is not empty
  if(!name[0])
  {
   printf("\r\nError! ContactName is empty!\r\n");
   return -1;
  }
  //check for name is less then 31 chars
  if(strlen(name)>31)
  {
   printf("\r\nError! ContactName too long (>31)!\r\n");
   return -2;
  }
  //check for info is less then 31 chars
  if(strlen(name+p)>31)
  {
   printf("\r\nError! ContactPhone too long (>31)!\r\n");
   return -3;
  }
  //search contact by name
  strcpy(str, name);
  i=SearchContact(buf, str);
  //specified name already exist: user must change name
  if(i>0)
  {
   printf("\r\nName already exist! Change name and try again!\r\n");
   return -4;
  }
}

 //derive cert=their_public_cert||our_private_cert

 //their_public_cert = g ^ H(ekey||dkey)
 Sponge_init(&cspng, 0, 0, 0, 0);
 Sponge_data(&cspng, skey+16, 16, 0, SP_NORMAL);
 Sponge_data(&cspng, skey, 16, 0, SP_NORMAL);
 Sponge_finalize(&cspng, buf+32, 32);
 get_pubkey(buf, buf+32); //get_pubkey(pubkey, seckey);

 //check for ID must be non-zero
 if(!BytesToInt(buf))
 {
  printf("\r\nError! Collision: zeroed ID, pairing impossible\r\n");
  return -7;
 }
 //our_private_cert = H(dkey||ekey)
 Sponge_init(&cspng, 0, 0, 0, 0);
 Sponge_data(&cspng, skey, 16, 0, SP_NORMAL);
 Sponge_data(&cspng, skey+16, 16, 0, SP_NORMAL);
 Sponge_finalize(&cspng, buf+32, 32);

 //search for our id already exist in book (id collision)
 str[0]=0;
 i=SearchContact(buf, str);
 if(i>0)
 {
  if(name) printf("\r\nError! Collision for id=%d with contact '%s'\r\n", BytesToInt(buf), str);
  return -5;
 }

 if(!name) return 0;  //not add entry to book if name not specified

 //make book's entry as a string
 strcpy(str, name); //name
 b64estr(buf, 64, str+strlen(str)); //encode certificate
 if(p) strcpy(str+strlen(str), name+p); //phone info

 //open address book for append
 if(!(fl = fopen((char*)"contacts.txt", "at" ))) //open specified bookfile
 {
  printf("\r\nError opening address book file for append!\r\n");
  return -6;
 }
 fprintf(fl,"%s\n", str); //add entry
 fclose(fl);
 printf("\r\nContact '%s' added to adressbook\r\n", name);
 return 0;
}

//===========================================================
 //terminate call and reset crypto engine ready for new call
 void HangUp(void)
 {
  //if(step) printf("\r\nCall terminated by user\r\n");
  ResetCT(0); //reset crypto
  lastid=0;   //clear last id of originatr's outgoing contact sets side as an acceptor 
 }













