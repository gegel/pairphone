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


#ifndef TRUE
#define TRUE	1
#endif
#ifndef FALSE
#define FALSE	0
#endif

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <alsa/asoundlib.h>
#include <sys/time.h>
#include "_audio_alsa.h"
//#include "cntrls.h"
#define _DEFCONF "conf.txt"  //configuration filename
#define _SAMPLE_RATE 	48000
#define _DEFBUFSIZE 6400
#define _DEFPERIODS 4

#define _DEF_devAudioInput "plughw:0,0"
#define _DEF_devAudioOutput "plughw:0,0"
#define _DEF_devAudioControl "default"
#define _DEF_capture_mixer_elem "Capture"
#define _DEF_playback_mixer_elem "PCM"

static char _devAudioInput[32];
static char _devAudioOutput[32];
static char _devAudioControl[32];
static char _capture_mixer_elem[32];
static char _playback_mixer_elem[32];

/* - setup - */
static int _snd_rate = _SAMPLE_RATE;
static int _snd_format = SND_PCM_FORMAT_S16_LE;// SND_PCM_FORMAT_MU_LAW;
static int _snd_channels = 1;
static int _verbose = 0;		/* DEBUG! */
static int _quiet_mode = 0;	/* Show info when _suspending.  Not relevant as
				   this application doesn't _suspend. */

static unsigned _buffer_time = 300 * 1000;	/* Total size of buffer: 900 ms */
static unsigned _period_time = 5 * 1000;	/* a.k.a. fragment size: 30 ms */
static unsigned _bufsize = _DEFBUFSIZE;
static unsigned _periods = _DEFPERIODS;
static int _showparam=0;

static int _sleep_min = 0;
static int _avail_min = -1;
static int _start_delay = 0;
static int _stop_delay = 0;

/* - record/playback - */
static snd_pcm_t *_pcm_handle_in = NULL; //record
static snd_pcm_t *_pcm_handle_out = NULL; //playback
static snd_pcm_uframes_t _chunk_size, _buffer_size;
static size_t _bits_per_sample, _bits_per_frame;
static size_t _chunk_bytes;

/* - _mixer - */
static int _mixer_failed = 0;
static snd_mixer_t *_mixer = NULL;
static snd_mixer_elem_t *_capture_elem = NULL;
static snd_mixer_elem_t *_playback_elem = NULL;
static int _mixer_capture_failed=0, _mixer_playback_failed=0;
static long _reclevel_min, _reclevel_max;
static long _playlevel_min, _playlevel_max;

/* - misc - */
static snd_output_t *_log;

//VAD
int _Vad_c=0;

int _IsGo=0;     //flag: input runs



//*****************************************************************************
//Parse config file for param and copy value to param, return length of value
//zero if not found and error code if no config file
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
  perror("Cannot open config file");
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



//read specified alsa buffer parameters from config file
static int _rdcfg(void)
{
 char buf[256];
 char* p=NULL;
 
 //set defaults
 _periods=_DEFPERIODS;
 _bufsize=_DEFBUFSIZE;
 strcpy(_devAudioInput,_DEF_devAudioInput);
 strcpy(_devAudioOutput,_DEF_devAudioOutput);
 strcpy(_devAudioControl,_DEF_devAudioControl);
 strcpy(_capture_mixer_elem,_DEF_capture_mixer_elem);
 strcpy(_playback_mixer_elem,_DEF_playback_mixer_elem);

 //read from config file:

 //chunk size and cunks in buffer
 strcpy(buf, "_AudioChunks");
 if(0>=_parseconfa(buf)) strcpy(buf, "#");
 p=strchr(buf, '*');
 if((buf[0]=='#')||(!p)) _showparam=1;
 else
 { 
   _showparam=0;
   _periods=0;
   p[0]=0;
   _periods=atoi(++p);
   printf("PPPeriods=%u\r\n", _periods);
   if(!_periods) _periods=_DEFPERIODS;
   _bufsize=_periods*atoi(buf);
   printf("BBBufsize=%u\r\n", _bufsize);
   if(!_bufsize) _bufsize=_DEFBUFSIZE; 
 }
 
//device names
 strcpy(buf, "_AudioInput");
 if(0>=_parseconfa(buf)) strcpy(buf, "#");
 if(buf[0]!='#') strcpy(_devAudioInput,buf);
 
 strcpy(buf, "_AudioOutput");
 if(0>=_parseconfa(buf)) strcpy(buf, "#");
 if(buf[0]!='#') strcpy(_devAudioOutput,buf);
 
 strcpy(buf, "_AudioControl");
 if(0>=_parseconfa(buf)) strcpy(buf, "#");
 if(buf[0]!='#') strcpy(_devAudioControl,buf);
 
 strcpy(buf, "_CaptureMixer");
 if(0>=_parseconfa(buf)) strcpy(buf, "#");
 if(buf[0]!='#') strcpy(_capture_mixer_elem, buf);

 strcpy(buf, "_PlaybackMixer");
 if(0>=_parseconfa(buf)) strcpy(buf, "#");
 if(buf[0]!='#') strcpy(_playback_mixer_elem, buf);
 
 return 0; 
}

//returns chunk size
int _getchunksize(void)
{
 return (int) _chunk_size;
}

//returns buffer size
int _get_bufsize(void)
{
 return (int) _buffer_size;
}

/*
//returns current count of samples in output buffers 
int _getdelay(void)
{
 int rc;
 snd_pcm_sframes_t frames;
 rc=snd_pcm_delay(_pcm_handle_out, &frames);
 if(rc) 
 {
  fprintf(stderr, "GetDelay error=%d", rc);
  return 0;
 }
 return frames;
}
*/

int _getdelay(void)
{
 int i=_buffer_size;
 return (i-snd_pcm_avail_update(_pcm_handle_out));
}



/*
 * Open the sound peripheral and initialise for access.  Returns TRUE if
 * successful, FALSE otherwise.
 *
 * iomode: O_RDONLY (sfmike) or O_WRONLY (sfspeaker).
 */
static int _soundinit_2(int iomode)
{
	snd_pcm_stream_t stream;
	snd_pcm_hw_params_t *hwparams;
	snd_pcm_sw_params_t *swparams;
	snd_pcm_uframes_t xfer_align, start_threshold, stop_threshold;
	size_t n;
	char *device_file;
	
	snd_pcm_t *pcm_handle=NULL;
        
        int ret;

        //apply users config
        _rdcfg();

	// might be used in case of error even without _verbose.
	snd_output_stdio_attach(&_log, stderr, 0);

	if (iomode != 0) {
		stream = SND_PCM_STREAM_CAPTURE;
		_start_delay = 1;
		device_file = _devAudioInput;
		
	} else {
		stream = SND_PCM_STREAM_PLAYBACK;
		device_file = _devAudioOutput;
		
	}

	if (snd_pcm_open(&pcm_handle, device_file, stream, SND_PCM_NONBLOCK) < 0) {
		fprintf(stderr, "Error opening PCM device %s\n", device_file);
		return FALSE;
	}

	snd_pcm_hw_params_alloca(&hwparams);
	if (snd_pcm_hw_params_any(pcm_handle, hwparams) < 0) {
		fprintf(stderr, "Can't configure the PCM device %s\n",
			_devAudioInput);
		return FALSE;
	}


	/* now try to configure the device's hardware parameters */
	if (snd_pcm_hw_params_set_access(pcm_handle, hwparams,
		SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
		fprintf(stderr, "Error setting interleaved access mode.\n");
		return FALSE;
	}

	/* Here we request mu-law sound format.  ALSA can handle the
	 * conversion to linear PCM internally, if the device used is a
	 * "plughw" and not "hw". */
	if (snd_pcm_hw_params_set_format(pcm_handle, hwparams, _snd_format) < 0) {
		fprintf(stderr, "Error setting PCM format\n");
		return FALSE;
	}

	if (snd_pcm_hw_params_set_channels(pcm_handle, hwparams, _snd_channels) < 0) {
		fprintf(stderr, "Error setting channels to %d\n",
			_snd_channels);
		return FALSE;
	}

	if (snd_pcm_hw_params_set_rate_near(pcm_handle, hwparams, (unsigned int*)&_snd_rate, NULL) < 0) {
		fprintf(stderr, "The rate %d Hz is not supported.  "
			"Try a plughw device.\n", _snd_rate);
		return FALSE;
	}

// buffer and period size can be set in bytes, or also in time.  


	if (snd_pcm_hw_params_set_buffer_time_near(pcm_handle, hwparams, &_buffer_time, 0) < 0) {
		fprintf(stderr, "Error setting buffer time to %u\n", _buffer_time);
		return FALSE;
	}

	if (snd_pcm_hw_params_set_period_time_near(pcm_handle, hwparams, &_period_time, 0) < 0) {
		fprintf(stderr, "Error setting period time to %u\n", _period_time);
		return FALSE;
	}

 if(!_showparam) //read config file
 {      //sets specified buffer parameters
	if (snd_pcm_hw_params_set_buffer_size(pcm_handle, hwparams, _bufsize) < 0) {
		fprintf(stderr, "Error setting buffer size to %u\n", _bufsize);
		return FALSE;
	}
	if (snd_pcm_hw_params_set_periods(pcm_handle, hwparams, _periods, 0) < 0) {
		fprintf(stderr, "Error setting _periods.\n");
		return FALSE;
	}
 }
 
	if ((ret=snd_pcm_hw_params(pcm_handle, hwparams)) < 0) {
		fprintf(stderr, "Error setting hardware parameters=%d\n", ret);
		return FALSE;
	}

	/* check the hw setup */
	snd_pcm_hw_params_get_period_size(hwparams, &_chunk_size, 0);
	snd_pcm_hw_params_get_buffer_size(hwparams, &_buffer_size);
	if (_chunk_size == _buffer_size) {
		fprintf(stderr, "Can't use period equal to buffer size (%lu)\n",
			_chunk_size);
		return FALSE;
	}
	
	
	//if (_verbose||_showparam)
		printf("Device=%s Period size=%lu, Buffer size=%lu\n\r",
			device_file, _chunk_size, _buffer_size);

	/* now the software setup */
	/* This is from aplay, and I don't really understand what it's good
	 * for... */
	snd_pcm_sw_params_alloca(&swparams);
	snd_pcm_sw_params_current(pcm_handle, swparams);

	if (snd_pcm_sw_params_get_xfer_align(swparams, &xfer_align) < 0) {
		fprintf(stderr, "Unable to obtain xfer align\n");
		return FALSE;
	}
	if (_sleep_min)
		xfer_align = 1;
	if (snd_pcm_sw_params_set_sleep_min(pcm_handle, swparams, _sleep_min) < 0) {
		fprintf(stderr, "Unable to set _sleep_min to %d\n", _sleep_min);
		return FALSE;
	}

	if (_avail_min < 0)
		n = _chunk_size;
	else
		n = (double) _snd_rate * _avail_min / 1000000;
	if (snd_pcm_sw_params_set_avail_min(pcm_handle, swparams, n) < 0) {
		fprintf(stderr, "Can't set _avail_min to %d\n", n);
		return FALSE;
	}

	/* round up to closest transfer boundary */
	n = (_buffer_size / xfer_align) * xfer_align;
	if (_start_delay <= 0) {
		start_threshold = n + (double) _snd_rate * _start_delay / 1000000;
	} else
		start_threshold = (double) _snd_rate * _start_delay / 1000000;
	if (start_threshold < 1)
		start_threshold = 1;
	if (start_threshold > n)
		start_threshold = n;
	if (_verbose)
		printf("Start threshold would be %lu\n\r", start_threshold);
#if 0
	// NOTE: this makes playback not work.  Dunno why.
	if (snd_pcm_sw_params_set_start_threshold(pcm_handle, swparams, start_threshold) < 0) {
		fprintf(stderr, "Can't set start threshold\n");
		return FALSE;
	}
#endif

	if (_stop_delay <= 0) 
		stop_threshold = _buffer_size + (double) _snd_rate * _stop_delay / 1000000;
	else
		stop_threshold = (double) _snd_rate * _stop_delay / 1000000;
	if (snd_pcm_sw_params_set_stop_threshold(pcm_handle, swparams, stop_threshold) < 0) {
		fprintf(stderr, "Can't set stop threshold\n");
		return FALSE;
	}

	if (snd_pcm_sw_params_set_xfer_align(pcm_handle, swparams, xfer_align) < 0) {
		fprintf(stderr, "Can't set xfer align\n");
		return FALSE;
	}

	if (snd_pcm_sw_params(pcm_handle, swparams) < 0) {
		fprintf(stderr, "unable to install sw params:\n");
		snd_pcm_sw_params_dump(swparams, _log);
		return FALSE;
	}


	/* ready to enter the SND_PCM_STATE_PREPARED status */
	if (snd_pcm_prepare(pcm_handle) < 0) {
		fprintf(stderr, "Can't enter prepared state\n");
		return FALSE;
	}

	if (_verbose)
		snd_pcm_dump(pcm_handle, _log);

	_bits_per_sample = snd_pcm_format_physical_width(_snd_format);
	_bits_per_frame = _bits_per_sample * _snd_channels;
	_chunk_bytes = _chunk_size * _bits_per_frame / 8;

	if (_verbose)
		printf("Audio buffer size should be %d bytes\n\r", _chunk_bytes);

//	audiobuf = realloc(audiobuf, _chunk_bytes);
//	if (audiobuf == NULL) {
//		error("not enough memory");
//		exit(EXIT_FAILURE);
//	}

        
        if (iomode != 0) {
		_pcm_handle_in=pcm_handle;
	} else {
		_pcm_handle_out=pcm_handle;
	}


	return TRUE;
}

/* Helper function.  If the sound setup fails, release the device, because if
 * we try to open it twice, the application will block */
int _soundinit(void)
{
    int rc;
	
    //list audio devices
    rc=system("aplay --list-devices");
	
    printf("\r\n--------------Initialise Line audio-----------\r\n");
    //Open ALSA
    rc=1;
    printf("Audio input: ");
	if(!_soundinit_2(1)) //record
    {
     printf("Record not avaloable\r\n");
     rc=0;
    }
    printf("Audio output: ");
	if(!_soundinit_2(0)) //playback
    {
     printf("Playback not avaliable\r\n");
     rc=0;
    }
    if (rc != TRUE && (_pcm_handle_in || _pcm_handle_out)) 
    {	
     _soundterm();
    }
    return rc;
}

/* Close the audio device and the _mixer */
void _soundterm(void)
{
	if(_pcm_handle_in) snd_pcm_close(_pcm_handle_in);
	_pcm_handle_in = NULL;
	if(_pcm_handle_out) snd_pcm_close(_pcm_handle_out);
	_pcm_handle_out = NULL;
	if (_mixer) {
		snd_mixer_close(_mixer);
		_mixer = NULL;
	}
}

/* This is a hack to open the audio device once with O_RDWR, and then give the
 * descriptors to sfmike/sfspeaker.  Use this when opening the same device
 * twice, once read only and once write only, doesn't work.  AFAIK, not an
 * issue with ALSA.
 */
void _sound_open_file_descriptors(int *audio_io, int *audio_ctl)
{
	(void)audio_io;
	(void)audio_ctl;
	return;
}


/* I/O error handler */
/* grabbed from alsa-utils-0.9.0rc5/aplay/aplay.c */
/* what: string "overrun" or "underrun", just for user information */
static void _xrun(char *what, snd_pcm_t *pcm_handle)
{
	(void)what;

	snd_pcm_status_t *status;
	int res;
	
	snd_pcm_status_alloca(&status);
	if ((res = snd_pcm_status(pcm_handle, status))<0) {
		fprintf(stderr, "status error: %s\n", snd_strerror(res));
		exit(EXIT_FAILURE);
	}
	if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
		struct timeval now, diff, tstamp;
		gettimeofday(&now, 0);
		snd_pcm_status_get_trigger_tstamp(status, &tstamp);
		timersub(&now, &tstamp, &diff);
 
		//fprintf(stderr, "Buffer %s!!! (at least %.3f ms long)\n",
		//	what,
		//	diff.tv_sec * 1000 + diff.tv_usec / 1000.0);

		if (_verbose) {
			fprintf(stderr, "Status:\n");
			snd_pcm_status_dump(status, _log);
		}
                //if(_pcm_handle_out==pcm_handle) snd_pcm_drop(_pcm_handle_out);


		if ((res = snd_pcm_prepare(pcm_handle))<0) {
			fprintf(stderr, "_xrun: prepare error: %s\n",
				snd_strerror(res));
			exit(EXIT_FAILURE);
		}
		

		return;		/* ok, data should be accepted again */
	}
	fprintf(stderr, "read/write error\n");
	exit(EXIT_FAILURE);
}

/* Input _suspend handler */
/* grabbed from alsa-utils-0.9.0rc5/aplay/aplay.c */
static void _suspend(void)
{
	int res;
    snd_pcm_t *pcm_handle=_pcm_handle_in;
    
	//if (!_quiet_mode)
		fprintf(stderr, "Suspended. Trying resume. "); fflush(stderr);
	while ((res = snd_pcm_resume(pcm_handle)) == -EAGAIN)
		sleep(1);	/* wait until _suspend flag is released */
	if (res < 0) {
		if (!_quiet_mode)
			fprintf(stderr, "Failed. Restarting stream. "); fflush(stderr);
		if ((res = snd_pcm_prepare(pcm_handle)) < 0) {
			fprintf(stderr, "_suspend: prepare error: %s\n", snd_strerror(res));
			exit(EXIT_FAILURE);
		}
	}
	if (!_quiet_mode)
		fprintf(stderr, "Done.\n");
}


/*
 * Play a sound (update playbuffer asynchronously).  Buf contains ULAW encoded audio, 8000 Hz, mono, signed bytes
 * grabbed from alsa-utils-0.9.0rc5/aplay/aplay.c
 *
 * buf: where the samples are, if stereo then interleaved
 * len: number of samples (not bytes).  Doesn't matter for mono 8 bit.
 */
int _soundplay(int len, unsigned char *buf)
{
	int rc;
    snd_pcm_t *pcm_handle=_pcm_handle_out;
	/* the function expects the number of frames, which is equal to bytes
	 * in this case */
	
		rc = snd_pcm_writei(pcm_handle, buf, len);
	
		
		if (rc == -EAGAIN || (rc >= 0 && rc < len)) {
			fprintf(stderr, "Playback uncompleet: %d form %d\n", rc, len);
			//snd_pcm_wait(pcm_handle, 1000);
		} else if (rc == -EPIPE) {
			 //Experimental: when a buffer underrun happens, then
			 //wait some extra time for more data to arrive on the
			 //network.  The one skip will be longer, but less
			 //buffer underruns will happen later.  Or so he
			 //thought... 
			//usleep(100000);
                        //fprintf(stderr, "underrun\n");
                        printf("_underrun\n");
			_xrun("underrun", pcm_handle);
                       
		} else if (rc == -ESTRPIPE) {
                        fprintf(stderr, "_suspend\n");
			_suspend();
		} else if (rc < 0) {
			fprintf(stderr, "Write error: %s\n", snd_strerror(rc));
			return -EIO;
		}

	     return rc;
}

/* Try to open the _mixer */
/* returns FALSE on failure */
static int _mixer_open_2()
{
	int rc;

	if ((rc=snd_mixer_open(&_mixer, 0)) < 0) {
		fprintf(stderr, "Can't open _mixer: %s\n", snd_strerror(rc));
		return FALSE;
	}

	if ((rc=snd_mixer_attach(_mixer, _devAudioControl)) < 0) {
		fprintf(stderr, "Mixer attach error to %s: %s\n",
			_devAudioControl, snd_strerror(rc));
		return FALSE;
	}

	if ((rc=snd_mixer_selem_register(_mixer, NULL, NULL)) < 0) {
		fprintf(stderr, "Mixer register error: %s\n", snd_strerror(rc));
		return FALSE;
	}

	if ((rc=snd_mixer_load(_mixer)) < 0) {
		fprintf(stderr, "Mixer load error: %s\n", snd_strerror(rc));
		return FALSE;
	}

	return TRUE;
}

static snd_mixer_elem_t *_get_mixer_elem(char *name, int index)
{
	snd_mixer_selem_id_t *sid;
	snd_mixer_elem_t *elem;

	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_id_set_index(sid, index);
	snd_mixer_selem_id_set_name(sid, name);

	elem= snd_mixer_find_selem(_mixer, sid);
	if (!elem) {
		fprintf(stderr, "Control '%s',%d not found.\n",
			snd_mixer_selem_id_get_name(sid),
			snd_mixer_selem_id_get_index(sid));
	}
	return elem;
}


/* Try to open the _mixer, but only once. */
static int _mixer_open()
{
	if (_mixer_failed)
		return FALSE;

	if (_mixer)
		return TRUE;

	if (_mixer_open_2() == TRUE)
		return TRUE;

	if (_mixer) {
		snd_mixer_close(_mixer);
		_mixer = NULL;
	}
	_mixer_failed ++;
	return FALSE;
}

/* Set the playback volume from 0 (silence) to 100 (full on). */
void _soundplayvol(int value)
{
	long vol;
   

	if (_mixer_open() != TRUE)
		return;
	if (_mixer_playback_failed)
		return;
	if (!_playback_elem) {
		_playback_elem = _get_mixer_elem(_playback_mixer_elem, 0);
		if (!_playback_elem) {
			_mixer_playback_failed = 1;
			return;
		}
		snd_mixer_selem_get_playback_volume_range(_playback_elem,
			&_playlevel_min, &_playlevel_max);
	}

	vol = _playlevel_min + (_playlevel_max - _playlevel_min) * value / 100;
	snd_mixer_selem_set_playback_volume(_playback_elem, 0, vol);
	snd_mixer_selem_set_playback_volume(_playback_elem, 1, vol);
}


/* Set recording gain from 0 (minimum) to 100 (maximum). */
void _soundrecgain(int value)
{
	long vol;

	if (_mixer_open() != TRUE)
		return;
	if (_mixer_capture_failed)
		return;
	if (!_capture_elem) {
		_capture_elem = _get_mixer_elem(_capture_mixer_elem, 0);
		if (!_capture_elem) {
			_mixer_capture_failed = 1;
			return;
		}
		snd_mixer_selem_get_capture_volume_range(_capture_elem,
			&_reclevel_min, &_reclevel_max);
	}

	// maybe unmute, or enable "rec" switch and so forth...
	vol = _reclevel_min + (_reclevel_max - _reclevel_min) * value / 100;
	snd_mixer_selem_set_capture_volume(_capture_elem, 0, vol);
	snd_mixer_selem_set_capture_volume(_capture_elem, 1, vol);
}

/* select the output - speaker, or audio output jack.  Not implemented */
void _sounddest(int where)
{
	(void)where;

	return;
}

/* Record some audio non-blocking (as much as accessable and fits into the given buffer) */
int _soundgrab(char *buf, int len)
{
    size_t result = 0;
    if(_IsGo) {
        ssize_t r;
        //size_t count = len;
        snd_pcm_t *pcm_handle=_pcm_handle_in;
   

/*  I don't care about the chunk size.  We just read as much as we need here.
 *  Seems to work.
        if (_sleep_min == 0 && count != _chunk_size) {
            fprintf(stderr, "Chunk size should be %lu, not %d\n\r",
                _chunk_size, count);
		count = _chunk_size;
        }
*/

	// Seems not to be required.
        //int rc = snd_pcm_state(_pcm_handle_in);

        //if (rc == SND_PCM_STATE_PREPARED)
        //snd_pcm_start(pcm_handle);
        

       // do {
		      r = snd_pcm_readi(pcm_handle, buf, len);
	//    } while (r == -EAGAIN);
	

        if(r == -EAGAIN) return 0;
	    if(r>0) result=r;
	    
        if (r == -EPIPE) {
			printf("_overrun\n");
			_xrun("overrun", pcm_handle);
		} else if (r == -ESTRPIPE) {
                        fprintf(stderr, "_suspend");
			_suspend();
		} else if (r < 0) {
			if (r == -4) {
				/* This is "interrupted system call, which
				 * seems to happen quite frequently, but does
				 * no harm.  So, just return whatever has been
				 * already read. */
			}
			fprintf(stderr, "read error: %s (%d); state=%d\n\r",
				snd_strerror(r), r, snd_pcm_state(pcm_handle));
		}
    }
    return result;
}


/* This is called *before* starting to record.  Flush pending input.*/

void _soundflush(void)
{
//	printf("SOUND FLUSH\n\r");
        snd_pcm_drop(_pcm_handle_in);	/* this call makes the state go to "SETUP" */
	snd_pcm_prepare(_pcm_handle_in);	/* and now go to "PREPARE" state, ready for "RUNNING" */                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 
	
}

void _soundflush1(void)
{
//	printf("SOUND FLUSH\n\r");
        snd_pcm_drop(_pcm_handle_out);	/* this call makes the state go to "SETUP" */
	snd_pcm_prepare(_pcm_handle_out);	/* and now go to "PREPARE" state, ready for "RUNNING" */                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 
	
}


int _soundrec(int on)
{   
 _IsGo=on;
 return _IsGo;
}
