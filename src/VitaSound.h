//zlib license (c) mylegguy
// oldest to newest
//https://pastebin.com/4R59NH12
//https://pastebin.com/raw/m9k41uD7
//https://pastebin.com/raw/SP4tpNga
#if HEADER_MLGVITASOUND
#warning reincluded
#else
#define HEADER_MLGVITASOUND
#endif

// If this is libGeneralGood
#if PLATFORM
	#define INCLUDETESTCODE 0
#else
	#define INCLUDETESTCODE 1
#endif

#define SNDPLAT_VITA 1
#define SNDPLAT_LINUX 2
#if __gnu_linux__
	#define SNDPLATFORM SNDPLAT_LINUX
#else
	#define SNDPLATFORM SNDPLAT_VITA
#endif

#include <stdint.h>
#include <math.h>
#include <stdlib.h>
// memset and memcpy
#include <string.h>

#if SNDPLATFORM == SNDPLAT_VITA
	#include <psp2/ctrl.h>
	#include <psp2/audioout.h>
	#include <psp2/kernel/processmgr.h>
	#include <psp2/kernel/threadmgr.h>
	#include <vitasdk.h>
#endif

#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

#include <mpg123.h>

#include <samplerate.h>

#include "legarchive.h"

#if INCLUDETESTCODE && SNDPLATFORM == SNDPLAT_VITA
	#include "debugScreen.h"
	#define printf psvDebugScreenPrintf
#endif

// Works best with small buffers
// 65472 is SCE_AUDIO_MAX_LEN
// 32000 is good
// 19200 is probably good
// 9600
// 4800 is godo for fadeout
#define AUDIO_BUFFER_LENGTH 4800

// Best be even to avoid OV_EINVAL
#define BIGBUFFERSCALE 1.30

// SRC_SINC_MEDIUM_QUALITY
// SRC_SINC_FASTEST
#define SAMPLERATECONVERSIONQUALITY SRC_SINC_FASTEST

/*
Format specific functions:

_mlgsnd_getLengthInSamples
mlgsnd_restartAudio
mlgsnd_getNumChannels
mlgsnd_getSampleRate
_mlgsnd_disposeMusicMainStruct
*/
#define FILE_FORMAT_NONE 0
#define FILE_FORMAT_OGG 1
#define FILE_FORMAT_MP3 2
// Planned
#define FILE_FORMAT_WAV 3

#define SHORT_SYNC_DELAY_MICRO 10000

#define STREAMING_BUFFERS 3

#define NAUDIO_QUITSTATUS_PLAYING 1
#define NAUDIO_QUITSTATUS_SHOULDQUIT 2
#define NAUDIO_QUITSTATUS_PAUSED 3
#define NAUDIO_QUITSTATUS_QUITTED 5 // I know "quitted" is wierd, but I don't want to make this constant sound like a command.
#define NAUDIO_QUITSTATUS_PREPARING 6 // Getting audio ports and such ready
#define NAUDIO_QUITSTATUS_FREED 7

// if value is >= STREAMSTATUS_DONTSTREAM the do not stream
#define STREAMSTATUS_STREAMING 1
#define STREAMSTATUS_DONTSTREAM 2
#define STREAMSTATUS_MINISTREAM 3 // Streams until it reaches the end of the allocated buffers
//#define STREAMSTATUS_DONESTREAMING 4 // Done with ministream

#if SNDPLATFORM == SNDPLAT_LINUX
	#include <pthread.h>
	//#include <clock.h>

	// usleep
	#include <unistd.h>

	#define sceKernelDelayThread usleep
	
	#define SceCtrlData void*
	#define SCE_CTRL_SQUARE 1
	#define SceUID pthread_t*
	#define SceSize int
	#define SceUInt unsigned int
	#define SceAudioOutMode signed int

	#define SCE_AUDIO_OUT_MODE_MONO 0
	#define SCE_AUDIO_OUT_MODE_STEREO 0
	#define SCE_AUDIO_OUT_PORT_TYPE_MAIN 0
	#define SCE_AUDIO_VOLUME_FLAG_L_CH 0
	#define SCE_AUDIO_VOLUME_FLAG_R_CH 0
	#define SCE_AUDIO_OUT_MAX_VOL 500

	int sceAudioOutOpenPort(int _a, int _b, int _c, int _d){
		return 0;
	}
	int sceAudioOutSetConfig(int _a, int _b, int _c, int _d){
		return 0;
	}
	void sceAudioOutReleasePort(int _a){}
	void sceAudioOutSetVolume(int _a, int _b, int _c[2]){}
	void sceKernelExitDeleteThread(int _a){pthread_exit(NULL);}
	char wasJustPressed(void* bla,int _passedValue){
		return 0;
	}
	void sceKernelWaitThreadEnd(pthread_t* happy, int* _a, int* _b){
		pthread_join(*happy,NULL);
	}
	void psvDebugScreenInit() {}
	void sceCtrlPeekBufferPositive() {};
	int getTicks(){
		struct timespec _myTime;
		clock_gettime(CLOCK_MONOTONIC, &_myTime);
		return _myTime.tv_nsec/1000000;
	}
	int _lastOutputTicks=0;
	int sceAudioOutOutput(int _a, void* _b){
		int _newOutputTicks = getTicks();
		if (_lastOutputTicks!=0){
			//100
			if (_newOutputTicks<_lastOutputTicks+100){
				usleep(((_lastOutputTicks+100)-_newOutputTicks)*1000);
			}
		}
		return 0;
	}
	int mlgsnd_soundPlayingThread(SceSize args, void *argp);
	void* _soundThreadPthreadWrapper(void* _argument){
		printf("before %p\n",_argument);
		mlgsnd_soundPlayingThread(1,_argument);
		return NULL;
	}
	//SceUID mySoundPlayingThreadID = sceKernelCreateThread("SOUNDPLAY", mlgsnd_soundPlayingThread, 0, 0x10000, 0, 0, NULL);
	//pthread_t* sceKernelCreateThread(){
	//	pthread_t* _returnThread = malloc(sizeof(pthread_t));
	//	pthread_create(_returnThread,NULL,_soundThreadPthreadWrapper,NULL);
	//	return _returnThread;
	//}
#endif

typedef struct{
	void* mainAudioStruct; // malloc'd and needs a free function
	void** audioBuffers;  // malloc'd

	float* tempFloatSource; // Temp buffer to hold converted audio data
	float* tempFloatConverted; // malloc'd. This is the destination buffer for the converted audio data before it's copied back to the main buffer.

	SRC_DATA usualConverterData;
	SRC_STATE* personalConverter; // Needs a free function

	signed int nextBufferLoadOffset;

	unsigned int unscaledSamples; // Number of samples to load from the file and convert
	unsigned int scaledSamples; // Number of samples to play and size of destination buffer

	unsigned int numBuffers; // Number of audio buffers we can swap between.
	signed char fileFormat; // Tells you what the file format is, OGG, WAV, etc.
	
	SceUID playerThreadId; // Thread ID for the thread that's playing this sound.
	signed int audioPort;

	signed char shouldLoop; // bool
	signed int volume; // Current volume
	signed char myStreamStatus; // If we should stream or not

	signed char totalPlaying; // Non-streamed data can be played from multiple threads.
	signed char isFirstTime;
	signed char quitStatus; // 0 if should not quit, 1 if should quit, -1 if already quit
	signed char isFadingOut;
	unsigned int fadeoutPerSwap; // In Vita volume units
	signed int lastVolume; // Used to detect volume changes

	FILE* _internalFilePointer; // Do not mess with normally please
}NathanAudio;

int lastConverterError=0;
signed char haveInitMpg123=0;

//signed char nextLoadableSoundBuffer=1;
//signed char lastLoadedSoundBuffer=1;


//int _debugFilterPort=0;

// TODO - Try downscaling high sample rate
	// Pretty sure my 125% buffer system will break it.

void fixLegArchiveFile(legArchiveFile* _passedFile){
	fclose(_passedFile->fp);
	_passedFile->fp = fopen(_passedFile->filename,"r");
	fseek(_passedFile->fp,_passedFile->startPosition+_passedFile->internalPosition,SEEK_SET);
}

size_t oggReadCallback(void* ptr, size_t size, size_t nmemb, void* datasource){
	//printf("read. %d;%d\n",size,nmemb);
	legArchiveFile* _passedFile = (legArchiveFile*)datasource;
	if (_passedFile->inArchive==1){
		if (_passedFile->internalPosition+(size*nmemb)>=_passedFile->totalLength){
			nmemb = (_passedFile->totalLength-_passedFile->internalPosition)/size;
		}
	}
	size_t _readElements = fread(ptr,size,nmemb,((legArchiveFile*)datasource)->fp);
	if (_readElements==0 && nmemb!=0 && feof(_passedFile->fp)==0){
		printf("necesito fix.\n");
		fixLegArchiveFile(_passedFile);
		return oggReadCallback(ptr,size,nmemb,datasource);
	}
	_passedFile->internalPosition += size*_readElements;
	return _readElements;
}
int oggCloseCallback(void *datasource){
	fclose(((legArchiveFile*)datasource)->fp);
	if (((legArchiveFile*)datasource)->filename!=NULL){
		free(((legArchiveFile*)datasource)->filename);
	}
	free(datasource);
	return 0;
}
long oggTellCallback(void *datasource){
	if (((legArchiveFile*)datasource)->inArchive){
		return ((legArchiveFile*)datasource)->internalPosition;
	}else{
		return ftell(((legArchiveFile*)datasource)->fp);
	}
}
int oggSeekCallback(void *datasource, ogg_int64_t offset, int whence){
	//printf("seek\n");
	legArchiveFile* _passedFile = (legArchiveFile*)datasource;
	int _seekReturnValue;
	if (_passedFile->inArchive){
		if (whence==SEEK_SET){
			//printf("set %d;%d\n",offset,_passedFile->totalLength);
			_seekReturnValue = fseek(_passedFile->fp,_passedFile->internalPosition*-1+offset,SEEK_CUR);
			if (_seekReturnValue==0){
				_passedFile->internalPosition=offset;
			}
		}else if (whence==SEEK_CUR){
			//printf("cur %d\n",offset);
			if (_passedFile->internalPosition+offset>_passedFile->totalLength){
				printf("too long.");
			}
			_seekReturnValue = fseek(_passedFile->fp,offset,SEEK_CUR);
			if (_seekReturnValue==0){
				_passedFile->internalPosition+=offset;
			}
		}else if (whence==SEEK_END){
			//printf("end %d\n",offset);
			_seekReturnValue = fseek(_passedFile->fp,_passedFile->totalLength-_passedFile->internalPosition+offset,SEEK_CUR);
			if (_seekReturnValue==0){
				_passedFile->internalPosition=_passedFile->totalLength+offset;
			}
		}else{
			printf("Unknown seek request %d\n",whence);
			_seekReturnValue=0;
		}
	}else{
		_seekReturnValue = fseek(_passedFile->fp,offset,whence);
	}
	if (_seekReturnValue!=0 && feof(_passedFile->fp)==0){
		fixLegArchiveFile(_passedFile);
		return oggSeekCallback(datasource,offset,whence);
	}
	return 0;
}
long mpgReadCallback(void* _passedData, void* _passedBuffer, size_t _passedBytes){
	return oggReadCallback(_passedBuffer,1,_passedBytes,_passedData);
}
void mpgCloseCallback(void* _passedData){
	oggCloseCallback(_passedData);
}
off_t mpgSeekCallback(void* _passedData, off_t offset, int whence){
	oggSeekCallback(_passedData,offset,whence);
	return oggTellCallback(_passedData);
}

signed char mlgsnd_getNextBufferIndex(NathanAudio* _passedAudio, signed char _audioBufferSlot){
	_audioBufferSlot++;
	if (_audioBufferSlot==_passedAudio->numBuffers){
		return 0;
	}
	return _audioBufferSlot;
}

int64_t _mlgsnd_getLengthInSamples(NathanAudio* _passedAudio){
	if (_passedAudio->fileFormat == FILE_FORMAT_OGG){
		return ov_pcm_total(_passedAudio->mainAudioStruct,-1);
	}else if (_passedAudio->fileFormat == FILE_FORMAT_MP3){
		int64_t _foundLength = mpg123_length(*((mpg123_handle**)_passedAudio->mainAudioStruct));
		if (_foundLength==MPG123_ERR){
			_foundLength=-1;
		}
		return _foundLength;
	}else{
		return 0;
	}
}

signed int _mlgsnd_vitaSoundValueToGeneralGood(signed int _volumeValue){
	return (int)((_volumeValue/(double)SCE_AUDIO_OUT_MAX_VOL)*128);
}

void _mlgsnd_waitWhilePreparing(NathanAudio* _passedAudio){
	while (_passedAudio->quitStatus == NAUDIO_QUITSTATUS_PREPARING){
		sceKernelDelayThread(SHORT_SYNC_DELAY_MICRO);
	}
}

void mlgsnd_setVolume(NathanAudio* _passedAudio, signed int _newVolume){
	//_mlgsnd_waitWhilePreparing(_passedAudio);
	//if (_passedAudio->audioPort!=-1 && _passedAudio->playerThreadId!=0 && _passedAudio->quitStatus!=NAUDIO_QUITSTATUS_FREED){ // If is alive audio
	_passedAudio->volume = (int)((_newVolume/(double)128)*SCE_AUDIO_OUT_MAX_VOL);
	//	sceAudioOutSetVolume(_passedAudio->audioPort, SCE_AUDIO_VOLUME_FLAG_L_CH |SCE_AUDIO_VOLUME_FLAG_R_CH, (int[]){_passedAudio->volume,_passedAudio->volume}); 
	//}	
}

/* From the currnet spot, seek a certian number of samples. Negative numbers supported.
void mlgsnd_seekSamplesRelative(NathanAudio* _passedAudio, signed int _relativeSampleSeek){
	if (_passedAudio->fileFormat==FILE_FORMAT_OGG){
		ogg_int64_t _currentPosition = ov_pcm_tell(_passedAudio->mainAudioStruct);
		if (ov_pcm_seek(_passedAudio->mainAudioStruct,_currentPosition+_relativeSampleSeek)!=0){
			printf("error.\n");
		}
	}
}*/

void mlgsnd_restartAudio(NathanAudio* _passedAudio){
	if (_passedAudio->fileFormat==FILE_FORMAT_OGG){
		if (ov_raw_seek(_passedAudio->mainAudioStruct,0)!=0){
			printf("error.\n");
		}
	}else if (_passedAudio->fileFormat==FILE_FORMAT_MP3){
		mpg123_seek(*((mpg123_handle**)_passedAudio->mainAudioStruct),0,SEEK_SET);
	}
}

// _fadeoutTime is in milliseconds
unsigned int mlgsnd_getFadeoutPerSwap(unsigned int _fadeoutTime, unsigned int _startVolume){
	// (AUDIO_BUFFER_LENGTH/48000) - Time between swaps in seconds
	// _fadeoutTime * (AUDIO_BUFFER_LENGTH/48000) - Correct time between swaps
	return (unsigned int)(_startVolume/(_fadeoutTime / ((AUDIO_BUFFER_LENGTH/(double)48000)*1000)));
}

int mlgsnd_getNumChannels(NathanAudio* _passedMusic){
	if (_passedMusic->fileFormat==FILE_FORMAT_OGG){
		vorbis_info* vi=ov_info((_passedMusic->mainAudioStruct),-1);
		return vi->channels;
	}else if (_passedMusic->fileFormat==FILE_FORMAT_MP3){
		long _rateResult;
		int _channelsResult, _encResult;
		mpg123_getformat(*((mpg123_handle**)_passedMusic->mainAudioStruct), &_rateResult, &_channelsResult, &_encResult);
		return _channelsResult;
	}else{
		return 0;
	}
}

long mlgsnd_getSampleRate(NathanAudio* _passedMusic){
	if (_passedMusic->fileFormat==FILE_FORMAT_OGG){
		vorbis_info* vi=ov_info(_passedMusic->mainAudioStruct,-1);
		return vi->rate;
	}else if (_passedMusic->fileFormat==FILE_FORMAT_MP3){
		long _rateResult;
		int _channelsResult, _encResult;
		mpg123_getformat(*((mpg123_handle**)_passedMusic->mainAudioStruct), &_rateResult, &_channelsResult, &_encResult);
		return _rateResult;
	}else{
		return 0;
	}
}
long mlgsnd_getBufferSize(NathanAudio* _passedAudio, unsigned int _numberOfSamples, char _sizeOfUint){
	return _numberOfSamples*_sizeOfUint*mlgsnd_getNumChannels(_passedAudio);
}
// Raw audio data stored as short
long mlgsnd_getShortSourceSize(NathanAudio* _passedAudio){
	return mlgsnd_getBufferSize(_passedAudio,_passedAudio->unscaledSamples,sizeof(short));
}
// Raw audio data stored as float
long mlgsnd_getFloatSourceSize(NathanAudio* _passedAudio){
	return mlgsnd_getBufferSize(_passedAudio,_passedAudio->unscaledSamples,sizeof(float));
}
// Converted audio data stored as float
long mlgsnd_getFloatDestSize(NathanAudio* _passedAudio){
	return mlgsnd_getBufferSize(_passedAudio,_passedAudio->scaledSamples,sizeof(float));
}
// Do not use directly, use mlgsnd_getShortSrcDestSize
// Converted audio data stored as short
long __mlgsnd_getShortDestSize(NathanAudio* _passedAudio){
	return mlgsnd_getBufferSize(_passedAudio,_passedAudio->scaledSamples,sizeof(short));
}
// Get a buffer size that can hold short data before or after it's converted
long mlgsnd_getShortSrcDestSize(NathanAudio* _passedAudio){
	if (_passedAudio->scaledSamples>_passedAudio->unscaledSamples){ // If we upscale
		//printf("")
		return __mlgsnd_getShortDestSize(_passedAudio);
	}else{ // We downscale
		
		return mlgsnd_getShortSourceSize(_passedAudio);
	}
}
// Should be the size of the first buffer. This will get 125% of a short source buffer if it's bigger than the short dest buffer
long mlgsnd_getBigShortSrcDestSize(NathanAudio* _passedAudio){
	long _possibleBufferSizeOne = mlgsnd_getShortSourceSize(_passedAudio)*BIGBUFFERSCALE;
	long _possibleBufferSizeTwo = __mlgsnd_getShortDestSize(_passedAudio);
	return (_possibleBufferSizeOne > _possibleBufferSizeTwo ? _possibleBufferSizeOne : _possibleBufferSizeTwo);
}
long mlgsnd_getBigFloatSrcSize(NathanAudio* _passedAudio){
	return mlgsnd_getFloatSourceSize(_passedAudio)*BIGBUFFERSCALE;
}

#if !defined(PLATFORM) && SNDPLATFORM != SNDPLAT_LINUX
int getTicks(){
	return  (sceKernelGetProcessTimeWide()/1000);
}
#endif

char* getErrorName(int _errorCode){
	switch (_errorCode){
		case OV_EREAD:
			return "OV_EREAD";
		case OV_ENOTVORBIS:
			return "OV_ENOTVORBIS";
		case OV_EVERSION:
			return "OV_EVERSION";
		case OV_EBADHEADER:
			return "OV_EBADHEADER";
		case OV_EFAULT:
			return "OV_EFAULT";
		case OV_HOLE:
			return "OV_HOLE";
		case OV_EBADLINK:
			return "OV_EBADLINK";
		case OV_EINVAL:
			return "OV_EINVAL";
		case 0:
			return "ok";
		default:
			return "???";
	}
	return "why is this here?";
}

// Returns 1 for end of file or error
// Return values
// 0 - OK
// 1 - End of file
// 2 - Error
signed char mlgsnd_readOGGData(OggVorbis_File* _fileToReadFrom, char* _destinationBuffer, int _totalBufferSize, char _passedShouldLoop, FILE* _internalFilePointer){
	int _soundBufferWriteOffset=0;
	int _currentSection;
	int _bytesLeftInBuffer = _totalBufferSize;
	while(1){
		// Read from my OGG file, at the correct offset in my buffer, I can read 4096 bytes at most, big endian, 16-bit samples, and signed samples
		long ret=ov_read(_fileToReadFrom,&(_destinationBuffer[_soundBufferWriteOffset]),_bytesLeftInBuffer,0,2,1,&_currentSection);
		if (ret == 0) { // EOF
			if (_passedShouldLoop==0){
				if (_soundBufferWriteOffset==0){
					return 1;
				}else{ // Just return what we have
					memset(&(_destinationBuffer[_soundBufferWriteOffset]),0,_bytesLeftInBuffer);
					return 0;
				}
			}else{
				// Alright, we're going to loop.
				// When the user exits to LiveArea and goes back in, this code will thinks it hit EOF. That is because the file pointer won't work anymore.
				// Calling ov_raw_seek with a broken file pointer crashes. Standard functions won't crash when interacting with the file, though.
				// So, because we think we're at the end of the file, we can probably go back one byte.
				// If it doesn't work, we know the file pointer is broken and we should cancel loading.
				// If it does work, we can continue with the loading after fixing our seeking.
				if (fseek(_internalFilePointer,-1,SEEK_CUR)!=0){
					// File has been broken, quit.
					return 2;
				}else{
					fseek(_internalFilePointer,1,SEEK_CUR);
				}
				if (ov_raw_seek(_fileToReadFrom,0)!=0){
					return 1;
				}
			}
		} else if (ret < 0) {
			printf("Error: %ld\n",ret);
			if (ret==OV_HOLE){
				printf("OV_HOLE\n");
			}else if(ret==OV_EBADLINK){
				printf("OV_EBADLINK\n");
			}else if (ret==OV_EINVAL){
				printf("OV_EINVAL with %d bytes left\n",_bytesLeftInBuffer);
			}
			return 2;
		} else {
			_bytesLeftInBuffer-=ret;
			// Move pointer in buffer
			_soundBufferWriteOffset+=ret;
			if (_bytesLeftInBuffer<=0){
				break;
			}
		}
	}
	return 0;
}

// Returns 1 for end of file or error
// Return values
// 0 - OK
// 1 - End of file
// 2 - Error
signed char _mlgsnd_readMp3Data(mpg123_handle* _fileToReadFrom, char* _destinationBuffer, int _totalBufferSize, char _passedShouldLoop){
	int _soundBufferWriteOffset=0;
	int _bytesLeftInBuffer = _totalBufferSize;
	while(1){
		size_t _bytesDecoded;
		int _possibleErrorCode = mpg123_read(_fileToReadFrom,(unsigned char*)&(_destinationBuffer[_soundBufferWriteOffset]),_bytesLeftInBuffer,&_bytesDecoded);
		if (_possibleErrorCode==MPG123_DONE){
			if (_passedShouldLoop==0){
				if (_soundBufferWriteOffset==0){
					return 1;
				}else{ // Just return what we have
					memset(&(_destinationBuffer[_soundBufferWriteOffset]),0,_bytesLeftInBuffer);
					return 0;
				}
			}else{
				mpg123_seek(_fileToReadFrom,0,SEEK_SET);
			}
		}else if (_possibleErrorCode!=MPG123_OK){
			printf("error, %s\n",mpg123_plain_strerror(_possibleErrorCode));
			//return 2;
		}
		// Update position in buffer based on how much read
		_bytesLeftInBuffer-=_bytesDecoded;
		_soundBufferWriteOffset+=_bytesDecoded;
		if (_bytesLeftInBuffer<=0){
			break;
		}
		
	}
	return 0;
}

void mlgsnd_fadeoutMusic(NathanAudio* _passedAudio, unsigned int _fadeoutTime){
	_passedAudio->fadeoutPerSwap = mlgsnd_getFadeoutPerSwap(_fadeoutTime, _passedAudio->volume);
	_passedAudio->isFadingOut=1;
}
void mlgsnd_stopMusic(NathanAudio* _passedAudio){
	if (_passedAudio->totalPlaying == 0){ // If there are none to stop then don't stop.
		return;
	}
	_passedAudio->quitStatus = NAUDIO_QUITSTATUS_SHOULDQUIT;
	_passedAudio->volume = 0;
	if (_passedAudio->audioPort!=-1){
		sceAudioOutSetVolume(_passedAudio->audioPort, SCE_AUDIO_VOLUME_FLAG_L_CH |SCE_AUDIO_VOLUME_FLAG_R_CH, (int[]){0,0}); // Make it seem like the audio is stopped while it actually stops
	}
	
	if (_passedAudio->totalPlaying==1){ // Only wait for thread exit if we have only one thread.
		SceUInt _tempStoreTimeout = 50000000;
		int _storeExitStatus = 0;
		sceKernelWaitThreadEnd(_passedAudio->playerThreadId,&_storeExitStatus,&_tempStoreTimeout); // Timeout is 5 seconds
	}else{ // If multiple then wait for all of them to stop.
		while (_passedAudio->totalPlaying!=0){
			sceKernelDelayThread(SHORT_SYNC_DELAY_MICRO);
		}
	}
	_passedAudio->playerThreadId=-1;
	_passedAudio->volume = SCE_AUDIO_OUT_MAX_VOL; // If we replay this sound
	_passedAudio->lastVolume = _passedAudio->volume;
}
void _mlgsnd_disposeMusicMainStruct(NathanAudio* _passedAudio){
	if (_passedAudio->fileFormat==FILE_FORMAT_OGG){
		ov_clear(_passedAudio->mainAudioStruct);
	}else if (_passedAudio->fileFormat==FILE_FORMAT_MP3){
		mpg123_delete(*((mpg123_handle**)_passedAudio->mainAudioStruct));
		fclose(_passedAudio->_internalFilePointer);
	}
}
void mlgsnd_freeMusic(NathanAudio* _passedAudio){
	if (_passedAudio->quitStatus == NAUDIO_QUITSTATUS_FREED){ // Avoid audio double free. Actually, this line is useless and will still crash on double free.
		return;
	}
	// Don't free playing music
	mlgsnd_stopMusic(_passedAudio);
	// Different for each file format
	_mlgsnd_disposeMusicMainStruct(_passedAudio);
	// Free converter
	src_delete(_passedAudio->personalConverter);
	// Free main audio buffers
	int i;
	for (i=0;i<_passedAudio->numBuffers;i++){
		free(_passedAudio->audioBuffers[i]);
	}
	free(_passedAudio->audioBuffers);
	// Free temp conversion buffers
	free(_passedAudio->tempFloatConverted);
	free(_passedAudio->tempFloatSource);
	// Phew, almost forgot these
	free(_passedAudio->mainAudioStruct);
	free(_passedAudio);
}


// Returns NOT 0 if audio is done playing or error, 0 otherwise
signed char _mlgsnd_loadUnprocessedData(signed char _passedFormat, void* mainAudioStruct, char* _destinationBuffer, int _totalBufferSize, char _passedShouldLoop, NathanAudio* _passedAudio){
	signed char _possibleReturnValue=-1;
	if (_passedFormat==FILE_FORMAT_OGG){
		_possibleReturnValue = mlgsnd_readOGGData(mainAudioStruct,_destinationBuffer,_totalBufferSize,_passedShouldLoop,_passedAudio->_internalFilePointer);
	}else if (_passedFormat==FILE_FORMAT_MP3){
		_possibleReturnValue = _mlgsnd_readMp3Data(*((mpg123_handle**)mainAudioStruct),_destinationBuffer,_totalBufferSize,_passedShouldLoop);
	}
	return _possibleReturnValue;
}
/*void _mlgsnd_processData(SRC_STATE* _passedConverter, SRC_DATA* _passedConverterData, short* _shortBuffer, float* _floatBufferTempSource, float* _floatBufferTempDest, unsigned int _smallBufferSamples, unsigned int _longBufferSamples, signed char _numChannels){
	// Update data object
	_passedConverterData->data_in = _floatBufferTempSource;
	_passedConverterData->data_out = _floatBufferTempDest;
	
	// Keep these as is, secret rabbit code already accounts for channels. Stolen code: data->data_in + data->input_frames * psrc->channels
	_passedConverterData->input_frames = _smallBufferSamples;
	_passedConverterData->output_frames = _longBufferSamples;

	// Change our loaded short data into loaded float data
	src_short_to_float_array(_shortBuffer,_floatBufferTempSource,_smallBufferSamples*_numChannels);
	// Convert loaded float data into converted float data with proper sample rate in temp buffer
	int _possibleErrorCode = src_process(_passedConverter,_passedConverterData);
	if (_possibleErrorCode!=0){
		printf("%s\n",src_strerror(_possibleErrorCode)); // SRC_STATE pointer is NULL
	}
	// Change converted float data in temp buffer to short data with proper sample rate in proper buffer
	src_float_to_short_array(_floatBufferTempDest,_shortBuffer,_longBufferSamples*_numChannels);
}*/

// samples is _passedAudio->unscaledSamples
// bytes is mlgsnd_getShortSourceSize(_passedAudio)
signed int mlgsnd_loadMoreData(NathanAudio* _passedAudio, unsigned char _audioBufferSlot, unsigned int _passedSamples, unsigned int _passedSourceBufferSize){
	signed char _possibleReturnValue = _mlgsnd_loadUnprocessedData(_passedAudio->fileFormat,_passedAudio->mainAudioStruct, &(((char**)_passedAudio->audioBuffers)[_audioBufferSlot][_passedAudio->nextBufferLoadOffset]),_passedSourceBufferSize-_passedAudio->nextBufferLoadOffset,_passedAudio->shouldLoop,_passedAudio);
	_passedAudio->nextBufferLoadOffset=0;
	if (_possibleReturnValue!=0){
		return -1;
	}
	if (_passedAudio->usualConverterData.src_ratio!=1){

		// Update data object
		_passedAudio->usualConverterData.data_in = _passedAudio->tempFloatSource;
		_passedAudio->usualConverterData.data_out = _passedAudio->tempFloatConverted;
		// Keep these as is, secret rabbit code already accounts for channels. Stolen code: data->data_in + data->input_frames * psrc->channels
		_passedAudio->usualConverterData.input_frames = _passedSamples;
		_passedAudio->usualConverterData.output_frames = _passedAudio->scaledSamples;
	
		// Change our loaded short data into loaded float data
		src_short_to_float_array(_passedAudio->audioBuffers[_audioBufferSlot],_passedAudio->tempFloatSource,_passedSamples*mlgsnd_getNumChannels(_passedAudio));
		// Convert loaded float data into converted float data with proper sample rate in temp buffer
		int _possibleErrorCode = src_process(_passedAudio->personalConverter,&(_passedAudio->usualConverterData));
		if (_possibleErrorCode!=0){
			printf("%s\n",src_strerror(_possibleErrorCode)); // SRC_STATE pointer is NULL
		}
		
		//printf("%d;%d\n",_passedAudio->usualConverterData.input_frames_used,_passedAudio->usualConverterData.output_frames_gen);
		// Save unused samples
		if (_passedSamples>_passedAudio->usualConverterData.input_frames_used && _passedAudio->numBuffers!=1 ){
			signed char _nextBuffer = mlgsnd_getNextBufferIndex(_passedAudio,_audioBufferSlot);
			signed int _possibleNextLoadOffset = (_passedSamples-_passedAudio->usualConverterData.input_frames_used)*sizeof(short)*mlgsnd_getNumChannels(_passedAudio);
			if (_possibleNextLoadOffset>_passedSourceBufferSize){
				printf("too much data unused, so the only logical thing to do is discard it all\n");
			}else{
				_passedAudio->nextBufferLoadOffset = _possibleNextLoadOffset;
				memcpy( _passedAudio->audioBuffers[_nextBuffer], &(((char**)_passedAudio->audioBuffers)[_audioBufferSlot][_passedSourceBufferSize-_passedAudio->nextBufferLoadOffset]), _passedAudio->nextBufferLoadOffset );
			}
		}
	
		// Change converted float data in temp buffer to short data with proper sample rate in proper buffer
		src_float_to_short_array(_passedAudio->tempFloatConverted,_passedAudio->audioBuffers[_audioBufferSlot],_passedAudio->usualConverterData.output_frames_gen*mlgsnd_getNumChannels(_passedAudio));

	}

	/*_mlgsnd_processData(_passedAudio->personalConverter,&(_passedAudio->usualConverterData),_passedAudio->audioBuffers[_audioBufferSlot],_passedAudio->tempFloatSource,_passedAudio->tempFloatConverted,_passedAudio->unscaledSamples,_passedAudio->scaledSamples, mlgsnd_getNumChannels(_passedAudio) );
	if (_passedAudio->usualConverterData.input_frames_used!=_passedAudio->unscaledSamples){
		printf("failed.\n");
	}*/
	return 0;
}

// Process pause trigger
void _mlgsnd_waitWhilePaused(NathanAudio* _passedAudio){
	while (_passedAudio->quitStatus == NAUDIO_QUITSTATUS_PAUSED){
		sceKernelDelayThread(SHORT_SYNC_DELAY_MICRO);
	}
}

int mlgsnd_soundPlayingThread(SceSize args, void *argp){
	#if SNDPLATFORM == SNDPLAT_VITA
		argp = *((void***)argp);
	#endif
	NathanAudio* _passedAudio=(((void**)argp)[0]);
	int _currentPort = (int)(((void**)argp)[1]);
	free(argp);

	signed short i;
	for (i=0;_passedAudio->quitStatus != NAUDIO_QUITSTATUS_SHOULDQUIT;){
		// Detect volume changes
		if (_passedAudio->lastVolume!=_passedAudio->volume){
			sceAudioOutSetVolume(_currentPort, SCE_AUDIO_VOLUME_FLAG_L_CH |SCE_AUDIO_VOLUME_FLAG_R_CH, (int[]){_passedAudio->volume,_passedAudio->volume});
			_passedAudio->lastVolume=_passedAudio->volume;
		}

		// Fadeout if needed
		if (_passedAudio->isFadingOut){
			_passedAudio->volume-=_passedAudio->fadeoutPerSwap;
			// If fadeout is over
			if (_passedAudio->volume<0){
				break;
			}
			sceAudioOutSetVolume(_currentPort, SCE_AUDIO_VOLUME_FLAG_L_CH |SCE_AUDIO_VOLUME_FLAG_R_CH, (int[]){_passedAudio->volume,_passedAudio->volume});
		}

		//if (_currentPort!=_debugFilterPort){
		//	printf("%d;%d\n",_currentPort,sceAudioOutGetRestSample(_currentPort));
		//}
		//if (<=0){
		//	printf("Bad.\n");
		//}
		//if (sceAudioOutGetRestSample(_currentPort)<=64){
		//	printf("bad.\n");
		//}

		//SceUInt64 _startTicks = sceKernelGetProcessTimeWide();
		// Start playing audio
		if (sceAudioOutOutput(_currentPort, _passedAudio->audioBuffers[i])<0){
			printf("Output error.\n");
		}
		
		// Change buffer variable
		++i;
		// If the next buffer is out of bounds
		if (i==_passedAudio->numBuffers){
			if (_passedAudio->myStreamStatus>=STREAMSTATUS_DONTSTREAM){ // If we don't stream then we need to quit as we have no buffers left
				_passedAudio->quitStatus = NAUDIO_QUITSTATUS_SHOULDQUIT;
				--i; // Buffer variable is now the last known working buffer. If the quit status is changed between now and the end of the loop then the last working buffer will replay.
			}else{ // If we do stream
				i=0;
			}
		}
		
		// Stream audio data if we need to.
		if (_passedAudio->myStreamStatus == STREAMSTATUS_STREAMING || _passedAudio->myStreamStatus == STREAMSTATUS_MINISTREAM){
			// Start loading next audio into the other buffer while the current audio buffer is playing.
			signed char _loadMoreResult = mlgsnd_loadMoreData(_passedAudio,i,_passedAudio->unscaledSamples,mlgsnd_getShortSourceSize(_passedAudio));
			if (_passedAudio->myStreamStatus == STREAMSTATUS_MINISTREAM && i==_passedAudio->numBuffers-1){ // If we just loaded the last buffer.
				_passedAudio->myStreamStatus = STREAMSTATUS_DONTSTREAM;
			}
			if (_loadMoreResult==-1){ // If error or end of file without looping
				break; // Break out
			}
		}
		
		// Wait for audio to finish playing in the remaining time
		//sceAudioOutOutput(_currentPort, NULL);
		//printf("%ld\n",(long)(sceKernelGetProcessTimeWide()-_startTicks));

		// Check low priority triggers
		_mlgsnd_waitWhilePaused(_passedAudio);
	}

	sceAudioOutOutput(_currentPort, NULL); // Wait for audio to finish playing
	sceAudioOutReleasePort(_currentPort); // Release the port now that there's no audio
	if (_passedAudio->audioPort==_currentPort){
		_passedAudio->audioPort=-1; // Reset port variable
	}
	//_passedAudio->quitStatus = NAUDIO_QUITSTATUS_QUITTED; // Set quit before exiting
	_passedAudio->playerThreadId=-1;
	--_passedAudio->totalPlaying;
	sceKernelExitDeleteThread(0);
	return 0;
}

NathanAudio* _mlgsnd_loadAudioFILE(legArchiveFile _passedFile, char _passedFileFormat, char _passedShouldLoop, char _passedShouldStream){
	NathanAudio* _returnAudio = malloc(sizeof(NathanAudio));

	if (_passedFileFormat==FILE_FORMAT_MP3){
		_returnAudio->fileFormat = FILE_FORMAT_MP3;

		int _lastMpg123Error;
		// https://www.mpg123.de/api/mpglib_8c_source.shtml
		if (!haveInitMpg123){
			printf("Init mp3\n");
			mpg123_init();
			haveInitMpg123=1;
		}
		mpg123_handle* _newHandle = mpg123_new(NULL, &_lastMpg123Error);
		if(_newHandle == NULL){
			printf("Unable to create mpg123 handle: %s\n", mpg123_plain_strerror(_lastMpg123Error));
			return NULL;
		}
		_returnAudio->mainAudioStruct = malloc(sizeof(mpg123_handle*));
		*((mpg123_handle**)_returnAudio->mainAudioStruct)=_newHandle;

		if (_passedFile.fp==NULL){
			printf("bad.\n");
		}

		//if (_passedFile.totalLength!=0){
			legArchiveFile* _toPass = malloc(sizeof(legArchiveFile));
			*_toPass = _passedFile;
			_lastMpg123Error = mpg123_replace_reader_handle(_newHandle,mpgReadCallback,mpgSeekCallback,mpgCloseCallback);
			_lastMpg123Error = mpg123_open_handle(_newHandle,_toPass);
		//}else{
		//	if (mpg123_open_fd(*((mpg123_handle**)_returnAudio->mainAudioStruct),fileno(_passedFile.fp))!=MPG123_OK){
		//		printf("open error for mpg123.\n");
		//		return NULL;
		//	}
		//}

		// https://www.mpg123.de/api/group__mpg123__output.shtml
		_lastMpg123Error = mpg123_format_none(*((mpg123_handle**)_returnAudio->mainAudioStruct));
		//44100
		_lastMpg123Error = mpg123_format(*((mpg123_handle**)_returnAudio->mainAudioStruct),48000,MPG123_STEREO,MPG123_ENC_SIGNED_16);
		if (_lastMpg123Error!=MPG123_OK){
			printf("error, %d;%s\n",_lastMpg123Error,mpg123_plain_strerror(_lastMpg123Error));
		}
	}else if (_passedFileFormat == FILE_FORMAT_OGG){
		OggVorbis_File myVorbisFile;
		int _possibleErrorCode;
		//if (_passedFile.totalLength!=0){

			ov_callbacks myCallbacks;
			myCallbacks.read_func = oggReadCallback;
			myCallbacks.seek_func = oggSeekCallback;
			myCallbacks.close_func = oggCloseCallback;
			myCallbacks.tell_func = oggTellCallback;
	
			legArchiveFile* _toPass = malloc(sizeof(legArchiveFile));
			*_toPass = _passedFile;
	
			//int _possibleErrorCode = ov_open(_passedFile.fp, &myVorbisFile, NULL, 0);
			_possibleErrorCode = ov_open_callbacks(_toPass, &myVorbisFile, NULL, 0,myCallbacks);
		//}else{
		//	_possibleErrorCode = ov_open(_passedFile.fp, &myVorbisFile, NULL, 0);
		//}
		if(_possibleErrorCode != 0){
			fclose(_passedFile.fp);
			printf("open not worked!\n");
			printf("Error: %d;%s",_possibleErrorCode,getErrorName(_possibleErrorCode));
			return NULL;
		}
		_returnAudio->mainAudioStruct = malloc(sizeof(OggVorbis_File));
		*((OggVorbis_File*)(_returnAudio->mainAudioStruct)) = myVorbisFile;
		_returnAudio->fileFormat = FILE_FORMAT_OGG;
	}else{
		printf("Bad file format %d\n",_passedFileFormat);
		return NULL;
	}
	_returnAudio->_internalFilePointer = _passedFile.fp;

	_returnAudio->quitStatus=NAUDIO_QUITSTATUS_PREPARING;
	_returnAudio->isFadingOut=0;
	_returnAudio->volume = SCE_AUDIO_OUT_MAX_VOL; // SCE_AUDIO_OUT_MAX_VOL is actually max volume, not 0 decibels.
	_returnAudio->lastVolume = _returnAudio->volume;
	_returnAudio->shouldLoop = _passedShouldLoop;
	_returnAudio->playerThreadId=-1;
	_returnAudio->audioPort=-1;
	_returnAudio->isFirstTime=1;
	_returnAudio->totalPlaying=0;
	_returnAudio->nextBufferLoadOffset=0;
	
	// Setup sample rate ratios
	double _oldRateToNewRateRatio = (double)mlgsnd_getSampleRate(_returnAudio)/48000;
	_returnAudio->unscaledSamples = AUDIO_BUFFER_LENGTH*_oldRateToNewRateRatio;
	_returnAudio->scaledSamples = AUDIO_BUFFER_LENGTH;
	
	// Malloc temp buffers
	_returnAudio->tempFloatSource = malloc(mlgsnd_getBigFloatSrcSize(_returnAudio));  // Also needs to be able to hold big first buffer as float data
	_returnAudio->tempFloatConverted = malloc(mlgsnd_getFloatDestSize(_returnAudio));
	
	// Init converter object data argument now that temp buffer exists
	_returnAudio->usualConverterData.data_in = _returnAudio->tempFloatSource;
	_returnAudio->usualConverterData.data_out = _returnAudio->tempFloatConverted;
	_returnAudio->usualConverterData.input_frames = _returnAudio->unscaledSamples;
	_returnAudio->usualConverterData.output_frames= _returnAudio->scaledSamples;
	_returnAudio->usualConverterData.src_ratio = 48000/(double)mlgsnd_getSampleRate(_returnAudio);
	_returnAudio->usualConverterData.end_of_input=0;

	// Init converter object
	_returnAudio->personalConverter = src_new(SAMPLERATECONVERSIONQUALITY,mlgsnd_getNumChannels(_returnAudio),&lastConverterError);
	if (lastConverterError!=0){
		printf("%s\n",src_strerror(lastConverterError)); // Bad converter number
	}

	// Init data for audio
	_returnAudio->myStreamStatus = (_passedShouldStream ? STREAMSTATUS_STREAMING : STREAMSTATUS_DONTSTREAM);

	// How many buffers we would need if we were streaming
	_returnAudio->numBuffers = ceil((_mlgsnd_getLengthInSamples(_returnAudio)*_returnAudio->usualConverterData.src_ratio)/(double)AUDIO_BUFFER_LENGTH);

	if (_returnAudio->numBuffers<=0){
		printf("ohnozies\n");
		_returnAudio->numBuffers = STREAMING_BUFFERS+1;
	}
	
	// If we could fit our streaming sound in just our swap buffers then don't stream
	if (_returnAudio->numBuffers<=STREAMING_BUFFERS){
		_returnAudio->myStreamStatus=STREAMSTATUS_MINISTREAM;
	}else if (_returnAudio->numBuffers==1){
		_returnAudio->myStreamStatus=STREAMSTATUS_DONTSTREAM;
	}
	// Change the numBuffers variable if we're streaming
	if (_returnAudio->myStreamStatus==STREAMSTATUS_STREAMING){ // We are actually streaming
		_returnAudio->numBuffers=STREAMING_BUFFERS;
	}else{
		// We're not streaming, don't change the numBuffers variable
	}

	// Malloc audio buffers depending on if we want streaming or not
	_returnAudio->audioBuffers = malloc(sizeof(void*)*_returnAudio->numBuffers); // Array

	// First buffer exception
	_returnAudio->audioBuffers[0] = malloc(mlgsnd_getBigShortSrcDestSize(_returnAudio)); // Needs to be able to hold the 125% unscaled and 100% scaled. TODO - We can't know which one is bigger.
	// Normal buffers
	int i;
	for (i=1;i<_returnAudio->numBuffers;i++){
		_returnAudio->audioBuffers[i] = malloc(mlgsnd_getShortSrcDestSize(_returnAudio)); // The array elements, the audio buffers
	}
	////mlgsnd_loadMoreData(_returnAudio,0);
	//// No matter what, we'll need the first buffer.
	//// Special code to load the first buffer
	//_mlgsnd_loadUnprocessedData(_returnAudio->fileFormat,_returnAudio->mainAudioStruct,_returnAudio->audioBuffers[0],mlgsnd_getInitBufferSize(_returnAudio,_returnAudio->unscaledSamples,sizeof(short)),1); // Always loop for now.
	////void _mlgsnd_processData(SRC_STATE* _passedConverter, SRC_DATA* _passedConverterData, short* _shortBuffer, float* _floatBufferTempSource, float* _floatBufferTempDest, unsigned int _smallBufferSamples, unsigned int _longBufferSamples){
	//_mlgsnd_processData(_returnAudio->personalConverter,&(_returnAudio->usualConverterData),_returnAudio->audioBuffers[0],_returnAudio->tempFloatSource,_returnAudio->tempFloatConverted,_mlgsnd_getInitSamples(_returnAudio->unscaledSamples),_returnAudio->scaledSamples,mlgsnd_getNumChannels(_returnAudio));
	//if (_returnAudio->usualConverterData.output_frames_gen!=AUDIO_BUFFER_LENGTH){
	//	printf("will pop.\n");
	//}
	//// No idea why this is never called. I guess everything just turns out okay?
	//if (_returnAudio->usualConverterData.input_frames_used!=_mlgsnd_getInitSamples(_returnAudio->unscaledSamples)){ // If we loaded too much
	//	printf("seek %d\n",-1*(_mlgsnd_getInitSamples(_returnAudio->unscaledSamples)-_returnAudio->usualConverterData.input_frames_used));
	//	mlgsnd_seekSamplesRelative(_returnAudio,-1*(_mlgsnd_getInitSamples(_returnAudio->unscaledSamples)-_returnAudio->usualConverterData.input_frames_used));
	//}
	mlgsnd_loadMoreData(_returnAudio,0,_returnAudio->unscaledSamples*BIGBUFFERSCALE,mlgsnd_getBigShortSrcDestSize(_returnAudio));

	// Fill more buffers depending on if we want streaming or not
	if (_returnAudio->myStreamStatus==STREAMSTATUS_DONTSTREAM){ // Fill all buffers
		short i;
		for (i=1;i<_returnAudio->numBuffers;i++){
			mlgsnd_loadMoreData(_returnAudio,i,_returnAudio->unscaledSamples,mlgsnd_getShortSourceSize(_returnAudio));
		}
	}
	return _returnAudio;
}
NathanAudio* _mlgsnd_loadAudioFilename(char* _passedFilename, char _passedShouldLoop, char _passedShouldStream){
	char _foundFileFormat = FILE_FORMAT_NONE;
	if (strlen(_passedFilename)>=4 && strcmp(&(_passedFilename[strlen(_passedFilename)-4]),".mp3")==0){
		_foundFileFormat = FILE_FORMAT_MP3;
	}else if (strlen(_passedFilename)>=4 && strcmp(&(_passedFilename[strlen(_passedFilename)-4]),".wav")==0){
		// Not yet supported
		//_foundFileFormat = FILE_FORMAT_WAV;
	}else if (strlen(_passedFilename)>=4 && strcmp(&(_passedFilename[strlen(_passedFilename)-4]),".ogg")==0){
		_foundFileFormat = FILE_FORMAT_OGG;
	}
	if (_foundFileFormat!=FILE_FORMAT_NONE){
		legArchiveFile _passFile;
		_passFile.fp = fopen(_passedFilename,"r");
		_passFile.totalLength=0;
		_passFile.internalPosition=0;
		_passFile.inArchive=0;
		_passFile.filename = malloc(strlen(_passedFilename)+1);
			strcpy(_passFile.filename,_passedFilename);
		return _mlgsnd_loadAudioFILE(_passFile, _foundFileFormat, _passedShouldLoop, _passedShouldStream);
	}else{
		return NULL;
	}
}
NathanAudio* mlgsnd_loadMusicFilename(char* filename){
	return _mlgsnd_loadAudioFilename(filename,1,1);
}
NathanAudio* mlgsnd_loadSoundFilename(char* filename){
	return _mlgsnd_loadAudioFilename(filename,0,0);
}
NathanAudio* mlgsnd_loadMusicFILE(legArchiveFile _passedFile, char _passedFileFormat){
	return _mlgsnd_loadAudioFILE(_passedFile,_passedFileFormat,1,1);
}
NathanAudio* mlgsnd_loadSoundFILE(legArchiveFile _passedFile, char _passedFileFormat){
	return _mlgsnd_loadAudioFILE(_passedFile,_passedFileFormat,0,0);
}

char _mlgsnd_initPort(NathanAudio* _passedAudio){
	// SCE_AUDIO_MAX_LEN is 65472. 65472/44100 = 1.485. That feels like around how much I've loaded.
	// The port returned from this must be kept safe for future use
	_passedAudio->audioPort = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, AUDIO_BUFFER_LENGTH, 48000, mlgsnd_getNumChannels(_passedAudio) == 1 ? SCE_AUDIO_OUT_MODE_MONO : SCE_AUDIO_OUT_MODE_STEREO);
	if (_passedAudio->audioPort<0){ // Probably no ports avalible
		return 0;
	}
	// More audio init
	sceAudioOutSetVolume(_passedAudio->audioPort, SCE_AUDIO_VOLUME_FLAG_L_CH |SCE_AUDIO_VOLUME_FLAG_R_CH, (int[]){_passedAudio->volume,_passedAudio->volume});
	sceAudioOutSetConfig(_passedAudio->audioPort, -1, -1, (SceAudioOutMode)-1); // This line is probablly useless. A -1 argument means that the specific property isn't changed. But I saw my god, Rinnegatamante, do it, so I'll do it too.
	return 1;
}
void mlgsnd_play(NathanAudio* _passedAudio){
	// We may need to rewind if we're streaming and this isn't our first time because buffers and file positions change.
	if ((_passedAudio->myStreamStatus == STREAMSTATUS_STREAMING) && !_passedAudio->isFirstTime){
		if (_passedAudio->totalPlaying!=0){ // If we stream then we write to buffers then we can't have two threads reading from the same buffer, wait for stop.
			mlgsnd_stopMusic(_passedAudio);
		}
		// If this is our second time playing this audio
		mlgsnd_restartAudio(_passedAudio);
		// Reload first buffer
		mlgsnd_loadMoreData(_passedAudio,0,_passedAudio->unscaledSamples,mlgsnd_getShortSourceSize(_passedAudio));
	}

	if (_passedAudio->isFirstTime){
		_passedAudio->isFirstTime=0;
	}
	// Initialize audio ports and such
	if (_mlgsnd_initPort(_passedAudio)==0){
		return;
	}
	// We just used the volume variable
	_passedAudio->lastVolume = _passedAudio->volume;

	// Set this flag as late as possible
	_passedAudio->quitStatus = NAUDIO_QUITSTATUS_PLAYING;

	// Do it before we start playing
	++_passedAudio->totalPlaying;

	// Arguments for our new thread
	void** _newArgs = malloc(sizeof(void*)*2);
	_newArgs[0]=_passedAudio;
	_newArgs[1]=(void*)(_passedAudio->audioPort);
	#if SNDPLATFORM == SNDPLAT_VITA
		// Initialize sound playing thread, but don't start it.
		SceUID mySoundPlayingThreadID = sceKernelCreateThread("SOUNDPLAY", mlgsnd_soundPlayingThread, 0, 0x10000, 0, 0, NULL);
		_passedAudio->playerThreadId = mySoundPlayingThreadID;
		// Start sound thread
		sceKernelStartThread(_passedAudio->playerThreadId,sizeof(void**),&_newArgs);
	#elif SNDPLATFORM == SNDPLAT_LINUX
		_passedAudio->playerThreadId = malloc(sizeof(pthread_t));
		pthread_create(_passedAudio->playerThreadId,NULL,_soundThreadPthreadWrapper,_newArgs);
	#endif
}

#if INCLUDETESTCODE==1
SceCtrlData lastCtrl;
void printAudioDataStart(NathanAudio* _passedAudio, signed char _whichBuffer){
	return;
	
	printf("Values in buffer %d.\n",_whichBuffer);
	int j;
	for (j=0;j<10;j++){
		printf("%d,",(((short**)_passedAudio->audioBuffers[_whichBuffer]))[j]);
	}
	printf("\n");
}
#if SNDPLATFORM == SNDPLAT_VITA
	char wasJustPressed(SceCtrlData ctrl, int _possibleControl){
		if ((ctrl.buttons & _possibleControl) && !(lastCtrl.buttons & _possibleControl)){
			return 1;
		}
		return 0;
	}
#endif
void printGuide(){
	printf("Right - Swap sound filename\nLeft - Rewind\nDown - Toggle first time flag\nTriangle - Load\nSquare - Free\nCircle - Stop\nX - Play\n\n");
}
#if SNDPLATFORM == SNDPLAT_VITA
	signed char checkFileExist(const char* location){
		SceUID fileHandle = sceIoOpen(location, SCE_O_RDONLY, 0777);
		if (fileHandle < 0){
			return 0;
		}else{
			sceIoClose(fileHandle);
			return 1;
		}
	}
	void directoryTest(char* _path){
		int j;
		int _start = getTicks();
		for (j=0;j<100;++j){
			checkFileExist(_path);
		}
		int _total = (getTicks()-_start);
		printf("Total:%d\n",_total);
		printf("Avg: %d\n",_total/100);
	}
#endif
int main(void) {
	psvDebugScreenInit();
	// TODO - To fix load dictionary loading, just put the dictionary in a seperate file.
	if (sizeof(float)!=4 || sizeof(short)!=2 || (sizeof(int)>sizeof(void*) || (sizeof(short)>sizeof(float)))){
		printf("Type sizes wrong, f;d, %d;%d\n",sizeof(float),sizeof(short));
	}
	//NathanAudio* _theGoodBGM = mlgsnd_loadMusic("ux0:data/SOUNDTEST/dppbattlemono.ogg");
	//mlgsnd_setVolume(_theGoodBGM,50);
	//mlgsnd_play(_theGoodBGM);
	//
	////_debugFilterPort=_theGoodBGM->audioPort;

	//int j;
	////for (j=0;j<2;++j){
	//int _start = getTicks();
	////for (j=0;j<20;++j){
	//	//checkFileExist("ux0:data/SOUNDTEST/438224");
	//	printf("%d\n",checkFileExist("ux0:data/OneThousand/438224"));
	////}
	//printf("Total:%d\n",(getTicks()-_start));
	////}
	////aadbs.ogg
	//_start = getTicks();
	////for (j=0;j<20;++j){
	//	//checkFileExist("ux0:data/SOUNDTEST/438224");
	//	printf("%d\n",checkFileExist("ux0:data/OneThousand/aadbs.ogg"));
	////}
	//printf("Total:%d\n",(getTicks()-_start));

	//int j;
	////for (j=0;j<2;++j){
	//int _start = getTicks();
	////for (j=0;j<20;++j){
	//	//checkFileExist("ux0:data/SOUNDTEST/438224");
	//	printf("%d\n",checkFileExist("ux0:data/OneThousand/438224"));
	////}
	//printf("Total:%d\n",(getTicks()-_start));
	////}
	////aadbs.ogg
	//_start = getTicks();
	////for (j=0;j<20;++j){
	//	//checkFileExist("ux0:data/SOUNDTEST/438224");
	//	printf("%d\n",checkFileExist("ux0:data/OneThousand/aadbs.ogg"));
	////}
	//printf("Total:%d\n",(getTicks()-_start));
	#if SNDPLATFORM == SNDPLAT_VITA
		//legArchive soundarch = loadLegArchive("ux0:data/SOUNDTEST/oggarchive");
		//legArchive soundarch = loadLegArchive("ux0:data/HIGURASHI/Games/base_install-converted/SEArchive.legArchive");
		//legArchive soundarch = loadLegArchive("uma0:data/HIGURASHI/games/SayaNoUta-converted/SEArchive.legArchive");
	#elif SNDPLATFORM == SNDPLAT_LINUX
		//legArchive soundarch = loadLegArchive("./uta");
	#endif
	//
	////https://www.mpg123.de/api/group__mpg123__lowio.shtml
	//
	////NathanAudio* _theGoodBGM2 = _mlgsnd_loadAudio("ux0:data/HIGURASHI/Games/umineko 4-converted/SE/11900015.ogg",0,1);
	#if SNDPLATFORM == SNDPLAT_VITA
		//NathanAudio* _theGoodBGM2 = _mlgsnd_loadAudioFilename("ux0:data/SOUNDTEST/battlerika.ogg",1,1);
		
		//NathanAudio* _theGoodBGM2 = _mlgsnd_loadAudioFILE(getAdvancedFile(soundarch,"music/s02.mp3"),FILE_FORMAT_MP3,1,1);
		NathanAudio* _theGoodBGM2 = _mlgsnd_loadAudioFilename("ux0:data/s02.mp3",1,1);
		//if (checkFileExist("ux0:data/HIGURASHI/Games/base_install-converted/SE/special/title.mp3")==0){
		//	printf("grueiwre\n");
		//}
		//NathanAudio* _theGoodBGM2 = _mlgsnd_loadAudioFilename("ux0:data/SOUNDTEST/BGM1.mp3",1,1);
	#elif SNDPLATFORM == SNDPLAT_LINUX
		//NathanAudio* _theGoodBGM2 = _mlgsnd_loadAudioFilename("./BGM1.mp3",1,1);
		
		//NathanAudio* _theGoodBGM2 = _mlgsnd_loadAudioFILE(getAdvancedFile(soundarch,"music/s02.mp3"),FILE_FORMAT_MP3,1,1);
		NathanAudio* _theGoodBGM2 = _mlgsnd_loadAudioFilename("./s02.mp3",1,1);
		
		//NathanAudio* _theGoodBGM2 = _mlgsnd_loadAudioFilename("./wa_038.ogg",0,1);
	#endif
	mlgsnd_setVolume(_theGoodBGM2,50);
	mlgsnd_play(_theGoodBGM2);
	//mlgsnd_freeMusic(_theGoodBGM2);
	//printf("a\n");
	//sceKernelDelayThread(1000);
	//printf("b\n");
	//_theGoodBGM2 = _mlgsnd_loadAudio("ux0:data/SOUNDTEST/smart.ogg",0,1);
	//mlgsnd_setVolume(_theGoodBGM2,50);
	//mlgsnd_play(_theGoodBGM2);

	//NathanAudio* _theGoodBGM3 = _mlgsnd_loadAudio("ux0:data/SOUNDTEST/rikatest.ogg",0,1);
	//mlgsnd_setVolume(_theGoodBGM3,50);
	//mlgsnd_play(_theGoodBGM3);

	//NathanAudio* _theGoodBGM3 = mlgsnd_loadMusic("ux0:data/SOUNDTEST/as.ogg");
	//mlgsnd_setVolume(_theGoodBGM3,50);
	//mlgsnd_play(_theGoodBGM3);

	//NathanAudio* _theGoodBGM3 = mlgsnd_loadMusic("ux0:data/SOUNDTEST/as.ogg");
	//mlgsnd_setVolume(_theGoodBGM3,50);
	//mlgsnd_play(_theGoodBGM3);
	/*
	NathanAudio* _lastSoundEffect=NULL;
	char* _firstPossibleSound="ux0:data/SOUNDTEST/yuna0023.ogg";
	char* _secondPossibleSound="ux0:data/SOUNDTEST/mion.ogg";

	//s20/01/440100006
	//s20/01/440100008

	char* _newSoundToPlay=_firstPossibleSound;
	//printf("X-Stop\nO-Free\nSquare-Pause\nTrinalge-Fadeout\nUp - play");
	
	printGuide();

	*/


	//SceUID _testfile = sceIoOpen("ux0:data/SOUNDTEST/a.txt",SCE_O_RDONLY,0777);
	//char readdata[5];
	//readdata[4]='\0';
	//sceIoRead(_testfile,readdata,4);

	while (1){
		SceCtrlData ctrl;
		sceCtrlPeekBufferPositive(0, &ctrl, 1);
		//printf("a\n");

		#if SNDPLATFORM == SNDPLAT_LINUX
			char _command[5];
			fgets(_command,5,stdin);
			_command[strlen(_command)-1]='\0';
			printf("%s\n",_command);
			if (_command[0]=='s'){
				printf("attmeptfree.\n");
				printf("Stop sound effect.\n");
				mlgsnd_stopMusic(_theGoodBGM2);
				printf("Free sound effect.\n");
				mlgsnd_freeMusic(_theGoodBGM2);
			}
			if (_command[0]=='p'){
				printf("fqwdw\n");
				mlgsnd_play(_theGoodBGM2);
			}
		#endif

		#if SNDPLATFORM == SNDPLAT_VITA
		if (wasJustPressed(ctrl,SCE_CTRL_SQUARE)){
			if (_theGoodBGM2!=NULL){
				printf("Stop sound effect.\n");
				mlgsnd_stopMusic(_theGoodBGM2);
				printf("Free sound effect.\n");
				mlgsnd_freeMusic(_theGoodBGM2);
			}
		}
		if (wasJustPressed(ctrl,SCE_CTRL_CROSS)){
			printf("pla\n");
			//NathanAudio* _theGoodBGM2 = _mlgsnd_loadAudioFILE(getAdvancedFile(soundarch,"msys20.ogg"),FILE_FORMAT_OGG,1,1);
			//mlgsnd_setVolume(_theGoodBGM2,50);
			mlgsnd_play(_theGoodBGM2);
		}
		if (wasJustPressed(ctrl,SCE_CTRL_TRIANGLE)){
			//sceIoRead(_testfile,readdata,4);
			//printf("Read: %s\n",readdata);
		}
		#endif



		/*

		if (wasJustPressed(ctrl,SCE_CTRL_RIGHT)){
			if (_newSoundToPlay==_firstPossibleSound){
				_newSoundToPlay = _secondPossibleSound;
			}else{
				_newSoundToPlay = _firstPossibleSound;
			}
			printf("Will play %s\n",_newSoundToPlay);
		}
		if (wasJustPressed(ctrl,SCE_CTRL_LEFT)){
			printf("Rewind audio\n");
			mlgsnd_restartAudio(_lastSoundEffect);
		}
		if (wasJustPressed(ctrl,SCE_CTRL_DOWN)){
			_lastSoundEffect->isFirstTime = !_lastSoundEffect->isFirstTime;
			printf("First time flag is %d\n",_lastSoundEffect->isFirstTime);
		}


		if (wasJustPressed(ctrl,SCE_CTRL_TRIANGLE)){
			printf("Load sound effect.\n");
			_lastSoundEffect = _mlgsnd_loadAudio(_newSoundToPlay,0,1);
		}
		if (wasJustPressed(ctrl,SCE_CTRL_SQUARE)){
			if (_lastSoundEffect!=NULL){
				printf("Free sound effect.\n");
				mlgsnd_freeMusic(_lastSoundEffect);
			}
		}
		if (wasJustPressed(ctrl,SCE_CTRL_CIRCLE)){
			printf("Stopping...\n");
			mlgsnd_stopMusic(_lastSoundEffect);
			//printf("Done.\n");
		}
		if (wasJustPressed(ctrl,SCE_CTRL_CROSS)){
			printf("Starting to play...\n");
			mlgsnd_play(_lastSoundEffect);
			//printf("Done.\n");
		}


		if (wasJustPressed(ctrl,SCE_CTRL_SELECT)){
			mlgsnd_restartAudio(_lastSoundEffect);
			printf("%d\n",mlgsnd_loadMoreData(_lastSoundEffect,0,_lastSoundEffect->unscaledSamples,mlgsnd_getShortSourceSize(_lastSoundEffect)));
			printAudioDataStart(_lastSoundEffect,0);
		}

		if (wasJustPressed(ctrl,SCE_CTRL_START)){
			printGuide();
		}
		*/

		/*
		if (wasJustPressed(ctrl,SCE_CTRL_UP)){
			printf("play sound %p\n",_newSoundToPlay);
			if (_lastSoundEffect!=NULL){
				mlgsnd_freeMusic(_lastSoundEffect);
			}
			_lastSoundEffect = _mlgsnd_loadAudio(_newSoundToPlay,0,1);
			mlgsnd_setVolume(_lastSoundEffect,100);
			_lastSoundEffect->isFirstTime=0;
			mlgsnd_play(_lastSoundEffect);
			if (_newSoundToPlay==_firstPossibleSound){
				_newSoundToPlay = _secondPossibleSound;
			}else{
				_newSoundToPlay = _firstPossibleSound;
			}
		}
		if (wasJustPressed(ctrl,SCE_CTRL_SQUARE)){
			if (_mlgsnd_initPort(_lastSoundEffect)==0){
				return 0;
			}
			SceUID mySoundPlayingThreadID = sceKernelCreateThread("SOUNDPLAY", mlgsnd_soundPlayingThread, 0, 0x10000, 0, 0, NULL);
			_lastSoundEffect->playerThreadId = mySoundPlayingThreadID;
		
			void** _newArgs = malloc(sizeof(void*)*2);
			_newArgs[0]=_lastSoundEffect;
			_newArgs[1]=(void*)(_lastSoundEffect->audioPort);
			_lastSoundEffect->quitStatus = NAUDIO_QUITSTATUS_PLAYING;
			sceKernelStartThread(_lastSoundEffect->playerThreadId,sizeof(void**),&_newArgs);
		}
		if (wasJustPressed(ctrl,SCE_CTRL_TRIANGLE)){
			mlgsnd_play(_lastSoundEffect);
		}
		if (wasJustPressed(ctrl,SCE_CTRL_RIGHT)){
			mlgsnd_loadMoreData(_lastSoundEffect,0);
		}
		if (wasJustPressed(ctrl,SCE_CTRL_LEFT)){
			mlgsnd_restartAudio(_lastSoundEffect);
		}
		if (wasJustPressed(ctrl,SCE_CTRL_DOWN)){
			_lastSoundEffect->numBuffers=1;
			_lastSoundEffect->myStreamStatus=STREAMSTATUS_DONTSTREAM;
			mlgsnd_loadMoreData(_lastSoundEffect,0);
			mlgsnd_play(_lastSoundEffect);
		}*/
		
		lastCtrl = ctrl;
	
		sceKernelDelayThread(10000);
	}

	#if SNDPLATFORM == SNDPLAT_VITA
		sceKernelExitProcess(0);
	#endif
	return 0;
}
#endif