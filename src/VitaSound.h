//zlib license (c) mylegguy
// oldest to newest
//https://pastebin.com/4R59NH12
//https://pastebin.com/raw/m9k41uD7
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


#include <stdint.h>
#include <math.h>
#include <stdlib.h>
// memset and memcpy
#include <string.h>

#include <psp2/ctrl.h>
#include <psp2/audioout.h>
#include <psp2/kernel/processmgr.h>

#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>
#include <psp2/kernel/threadmgr.h> 

#include <samplerate.h>

#if INCLUDETESTCODE
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
#define SAMPLERATECONVERSIONQUALITY SRC_SINC_MEDIUM_QUALITY

#define FILE_FORMAT_NONE 0
#define FILE_FORMAT_OGG 1

#define SHORT_SYNC_DELAY_MICRO 10000

#define STREAMING_BUFFERS 3

// Planned
#define FILE_FORMAT_WAV 2
#define FILE_FORMAT_MP3 3

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
}NathanAudio;
int lastConverterError=0;

//signed char nextLoadableSoundBuffer=1;
//signed char lastLoadedSoundBuffer=1;


//int _debugFilterPort=0;

// TODO - See what happens if I play Higurashi sounds in this test project. By that I mean start some BGM and let me play a bunch of DIFFERENT voice lines, each one needing to free the last one's memory because they're stored in the same variable.
// TODO - libmpg123, use it.
// TODO - Try downscaling high sample rate
	// Pretty sure my 125% buffer system will break it.

// TODO - To solve the audio popping issue, actually change stuff depending on input_frames_used.	
	// Often not all the samples are used, so some data is skipped. This causes the pop.
	// IDEA - Shove all my audio data in one big buffer. I can start loading from any position and load any amount. So say 200 less samples are used, I can start loading from 200 index and load the normal loading amount minus 200.
		// Have a few things. (1) big buffer that is the size of two smaller buffers. (2) variable for position in buffer that I'm currently playing (3) variable for position in buffer I'm reading to.
		
		// What to do when not all input data is used:
			/*
				(Assuming buffer 0 has 300 samples not used)
				Start playing buffer 1 at whatever offset
				Copy from (buffer 2 start) - 300 to (buffer 0 start)
				start loading in buffer position (buffe 0 start) + 300
					load ((buffer 2 start - 300)/(sigle buffer length)) samples
			*/
	
	// Take a look at this: https://stackoverflow.com/questions/1201319/what-is-the-difference-between-memmove-and-memcpy

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

// From the currnet spot, seek a certian number of samples. Negative numbers supported.
void mlgsnd_seekSamplesRelative(NathanAudio* _passedAudio, signed int _relativeSampleSeek){
	if (_passedAudio->fileFormat==FILE_FORMAT_OGG){
		ogg_int64_t _currentPosition = ov_pcm_tell(_passedAudio->mainAudioStruct);
		if (ov_pcm_seek(_passedAudio->mainAudioStruct,_currentPosition+_relativeSampleSeek)!=0){
			printf("error.\n");
		}
	}
}

void mlgsnd_restartAudio(NathanAudio* _passedAudio){
	if (_passedAudio->fileFormat==FILE_FORMAT_OGG){
		if (ov_raw_seek(_passedAudio->mainAudioStruct,0)!=0){
			printf("error.\n");
		}
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
	}else{
		return 0;
	}
}

long mlgsnd_getSampleRate(NathanAudio* _passedMusic){
	if (_passedMusic->fileFormat==FILE_FORMAT_OGG){
		vorbis_info* vi=ov_info(_passedMusic->mainAudioStruct,-1);
		return vi->rate;
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

#if !defined(PLATFORM)
int getTicks(){
	return  (sceKernelGetProcessTimeWide()/1000);
}
#endif

// Returns 1 for end of file or error
// Return values
// 0 - OK
// 1 - End of file
// 2 - Error
signed char mlgsnd_readOGGData(OggVorbis_File* _fileToReadFrom, char* _destinationBuffer, int _totalBufferSize, char _passedShouldLoop){
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
	}
}
void mlgsnd_freeMusic(NathanAudio* _passedAudio){
	if (_passedAudio->quitStatus == NAUDIO_QUITSTATUS_FREED){ // Avoid audio double free
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
signed char _mlgsnd_loadUnprocessedData(signed char _passedFormat, void* mainAudioStruct, char* _destinationBuffer, int _totalBufferSize, char _passedShouldLoop){
	signed char _possibleReturnValue=-1;
	if (_passedFormat==FILE_FORMAT_OGG){
		_possibleReturnValue = mlgsnd_readOGGData(mainAudioStruct,_destinationBuffer,_totalBufferSize,_passedShouldLoop);
	}
	return _possibleReturnValue;
}
void _mlgsnd_processData(SRC_STATE* _passedConverter, SRC_DATA* _passedConverterData, short* _shortBuffer, float* _floatBufferTempSource, float* _floatBufferTempDest, unsigned int _smallBufferSamples, unsigned int _longBufferSamples, signed char _numChannels){
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
}

// samples is _passedAudio->unscaledSamples
// bytes is mlgsnd_getShortSourceSize(_passedAudio)
signed int mlgsnd_loadMoreData(NathanAudio* _passedAudio, unsigned char _audioBufferSlot, unsigned int _passedSamples, unsigned int _passedSourceBufferSize){
	signed char _possibleReturnValue = _mlgsnd_loadUnprocessedData(_passedAudio->fileFormat,_passedAudio->mainAudioStruct, &(((char**)_passedAudio->audioBuffers)[_audioBufferSlot][_passedAudio->nextBufferLoadOffset]),_passedSourceBufferSize-_passedAudio->nextBufferLoadOffset,_passedAudio->shouldLoop);
	_passedAudio->nextBufferLoadOffset=0;
	if (_possibleReturnValue!=0){
		return -1;
	}
	
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
	//printf("====\n");
	argp = *((void***)argp);
	NathanAudio* _passedAudio=(((void**)argp)[0]);
	int _currentPort = (int)(((void**)argp)[1]);
	free(argp);

	++_passedAudio->totalPlaying;

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

NathanAudio* _mlgsnd_loadAudio(char* filename, char _passedShouldLoop, char _passedShouldStream){
	OggVorbis_File myVorbisFile;
	FILE* fp = fopen(filename,"r");
	if(ov_open(fp, &myVorbisFile, NULL, 0) != 0){
		fclose(fp);
		printf("open not worked!\n");
		return NULL;
	}

	NathanAudio* _returnAudio = malloc(sizeof(NathanAudio));
	_returnAudio->mainAudioStruct = malloc(sizeof(OggVorbis_File));
	*((OggVorbis_File*)(_returnAudio->mainAudioStruct)) = myVorbisFile;
	_returnAudio->fileFormat = FILE_FORMAT_OGG;
	
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

NathanAudio* mlgsnd_loadMusic(char* filename){
	return _mlgsnd_loadAudio(filename,1,1);
}
NathanAudio* mlgsnd_loadSound(char* filename){
	return _mlgsnd_loadAudio(filename,0,0);
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
	// Initialize sound playing thread, but don't start it.
	SceUID mySoundPlayingThreadID = sceKernelCreateThread("SOUNDPLAY", mlgsnd_soundPlayingThread, 0, 0x10000, 0, 0, NULL);
	_passedAudio->playerThreadId = mySoundPlayingThreadID;

	void** _newArgs = malloc(sizeof(void*)*2);
	_newArgs[0]=_passedAudio;
	_newArgs[1]=(void*)(_passedAudio->audioPort);

	// Set this flag as late as possible
	_passedAudio->quitStatus = NAUDIO_QUITSTATUS_PLAYING;

	// Start sound thread
	sceKernelStartThread(_passedAudio->playerThreadId,sizeof(void**),&_newArgs);
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
char wasJustPressed(SceCtrlData ctrl, int _possibleControl){
	if ((ctrl.buttons & _possibleControl) && !(lastCtrl.buttons & _possibleControl)){
		return 1;
	}
	return 0;
}
void printGuide(){
	printf("Right - Swap sound filename\nLeft - Rewind\nDown - Toggle first time flag\nTriangle - Load\nSquare - Free\nCircle - Stop\nX - Play\n\n");
}
int main(void) {
	psvDebugScreenInit();

	if (sizeof(float)!=4 || sizeof(short)!=2 || (sizeof(int)>sizeof(void*) || (sizeof(short)>sizeof(float)))){
		printf("Type sizes wrong, f;d, %d;%d\n",sizeof(float),sizeof(short));
	}
	//NathanAudio* _theGoodBGM = mlgsnd_loadMusic("ux0:data/SOUNDTEST/dppbattlemono.ogg");
	//mlgsnd_setVolume(_theGoodBGM,50);
	//mlgsnd_play(_theGoodBGM);
	//
	////_debugFilterPort=_theGoodBGM->audioPort;

	//NathanAudio* _theGoodBGM2 = mlgsnd_loadMusic("ux0:data/SOUNDTEST/as.ogg");
	//mlgsnd_setVolume(_theGoodBGM2,50);
	//mlgsnd_play(_theGoodBGM2);

	//NathanAudio* _theGoodBGM3 = mlgsnd_loadMusic("ux0:data/SOUNDTEST/as.ogg");
	//mlgsnd_setVolume(_theGoodBGM3,50);
	//mlgsnd_play(_theGoodBGM3);

	//NathanAudio* _theGoodBGM3 = mlgsnd_loadMusic("ux0:data/SOUNDTEST/as.ogg");
	//mlgsnd_setVolume(_theGoodBGM3,50);
	//mlgsnd_play(_theGoodBGM3);

	NathanAudio* _lastSoundEffect=NULL;
	char* _firstPossibleSound="ux0:data/SOUNDTEST/yuna0023.ogg";
	char* _secondPossibleSound="ux0:data/SOUNDTEST/mion.ogg";

	//s20/01/440100006
	//s20/01/440100008

	char* _newSoundToPlay=_firstPossibleSound;
	//printf("X-Stop\nO-Free\nSquare-Pause\nTrinalge-Fadeout\nUp - play");
	
	printGuide();


	while (1){
		SceCtrlData ctrl;
		sceCtrlPeekBufferPositive(0, &ctrl, 1);


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

	
	sceKernelExitProcess(0);
	return 0;
}
#endif