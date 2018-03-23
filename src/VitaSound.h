#if HEADER_MLGVITASOUND
#warning reincluded
#else
#define HEADER_MLGVITASOUND
#endif

#if PLATFORM
	#define INCLUDETESTCODE 0
#else
	#define INCLUDETESTCODE 1
#endif


#include <stdint.h>
#include <math.h>
#include <stdlib.h>

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
//65472 is SCE_AUDIO_MAX_LEN
// 32000 is good
// 19200 is probably good
// 4800 is godo for fadeout
#define AUDIO_BUFFER_LENGTH 4800

#define FILE_FORMAT_NONE 0
#define FILE_FORMAT_OGG 1

#define SHORT_SYNC_DELAY_MICRO 10000

#define STREAMING_BUFFERS 2

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

typedef struct fruhfreuir{
	void* mainAudioStruct; // malloc'd and needs a free function
	void** audioBuffers;  // malloc'd

	float* tempFloatSource; // Temp buffer to hold converted audio data
	float* tempFloatConverted; // malloc'd. This is the destination buffer for the converted audio data before it's copied back to the main buffer.

	SRC_DATA usualConverterData;
	SRC_STATE* personalConverter; // Needs a free function

	unsigned int unscaledSamples; // Number of samples to load from the file and convert
	unsigned int scaledSamples; // Number of samples to play and size of destination buffer

	signed char totalPlaying; // Non-streamed data can be played from multiple threads.

	unsigned int numBuffers; // Number of audio buffers we can swap between.
	signed char fileFormat; // Tells you what the file format is, OGG, WAV, etc.
	signed char shouldLoop; // bool
	SceUID playerThreadId; // Thread ID for the thread that's playing this sound.
	signed int audioPort;

	signed char isFirstTime;
	signed char quitStatus; // 0 if should not quit, 1 if should quit, -1 if already quit
	signed char isFadingOut;
	unsigned int fadeoutPerSwap; // In Vita volume units
	signed int volume;
	signed int lastVolume; // Used to detect volume changes
	signed char myStreamStatus; // If we should stream or not
}NathanAudio;
int lastConverterError=0;

//signed char nextLoadableSoundBuffer=1;
//signed char lastLoadedSoundBuffer=1;

// TODO - See what happens if I play Higurashi sounds in this test project. By that I mean start some BGM and let me play a bunch of DIFFERENT voice lines, each one needing to free the last one's memory because they're stored in the same variable.
// TODO - libmpg123, use it.

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

void mlgsnd_restartAudio(NathanAudio* _passedAudio){
	if (_passedAudio->fileFormat==FILE_FORMAT_OGG){
		ov_raw_seek(_passedAudio->mainAudioStruct,0);
	}
}

// _fadeoutTime is in milliseconds
unsigned int mlgsnd_getFadeoutPerSwap(unsigned int _fadeoutTime, unsigned int _startVolume){
	// (AUDIO_BUFFER_LENGTH/48000) - Time between swaps in seconds
	// _fadeoutTime * (AUDIO_BUFFER_LENGTH/48000) - Correct time between swaps
	return (unsigned int)(_startVolume/(_fadeoutTime / ((AUDIO_BUFFER_LENGTH/(double)48000)*1000)));
}

SceCtrlData lastCtrl;
void mlgsnd_waitForCross(){
	while (1){
		SceCtrlData ctrl;
		sceCtrlPeekBufferPositive(0, &ctrl, 1);
		if ((ctrl.buttons & SCE_CTRL_CROSS) && !(lastCtrl.buttons & SCE_CTRL_CROSS)){
			lastCtrl = ctrl;
			break;
		}
		lastCtrl = ctrl;
	}
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
#if !defined(PLATFORM)
SceUInt64 getTicks(){
	return  (sceKernelGetProcessTimeWide());
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
				printf("OV_EINVAL\n");
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

long mlgsnd_getBufferSize(NathanAudio* _passedAudio, unsigned int _numberOfSamples, char _sizeOfUint){
	return (mlgsnd_getNumChannels(_passedAudio)==1 ? _numberOfSamples*_sizeOfUint : _numberOfSamples*_sizeOfUint*2);
}

// Returns not 0 if audio is done playing or error, 0 otherwise
signed char mlgsnd_loadMoreData(NathanAudio* _passedAudio, unsigned char _audioBufferSlot){
	if (_passedAudio->fileFormat==FILE_FORMAT_OGG){
		signed char _possibleReturnValue = mlgsnd_readOGGData(_passedAudio->mainAudioStruct,_passedAudio->audioBuffers[_audioBufferSlot],mlgsnd_getBufferSize(_passedAudio,_passedAudio->unscaledSamples,sizeof(short)),_passedAudio->shouldLoop);
		if (_possibleReturnValue!=0){
			return _possibleReturnValue;
		}
	}
	// Change our loaded short data into loaded float data
	src_short_to_float_array(_passedAudio->audioBuffers[_audioBufferSlot],_passedAudio->tempFloatSource,mlgsnd_getBufferSize(_passedAudio,_passedAudio->unscaledSamples,1));
	// Convert loaded float data into converted float data with proper sample rate in temp buffer
	int _possibleErrorCode = src_process(_passedAudio->personalConverter,&_passedAudio->usualConverterData);
	if (_possibleErrorCode!=0){
		printf("%s\n",src_strerror(_possibleErrorCode)); // SRC_STATE pointer is NULL
	}
	// Change converted float data in temp buffer to short data with proper sample rate in proper buffer
	src_float_to_short_array(_passedAudio->tempFloatConverted,_passedAudio->audioBuffers[_audioBufferSlot],mlgsnd_getBufferSize(_passedAudio,_passedAudio->scaledSamples,1));
	return 0;
}

// Process pause trigger
void _mlgsnd_waitWhilePaused(NathanAudio* _passedAudio){
	while (_passedAudio->quitStatus == NAUDIO_QUITSTATUS_PAUSED){
		sceKernelDelayThread(SHORT_SYNC_DELAY_MICRO);
	}
}

int mlgsnd_soundPlayingThread(SceSize args, void *argp){
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

		//SceUInt64 _startTicks = sceKernelGetProcessTimeWide();
		// Start playing audio
		sceAudioOutOutput(_currentPort, _passedAudio->audioBuffers[i]);
		
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
			signed char _streamMoreResult = mlgsnd_loadMoreData(_passedAudio,i);
			if (_passedAudio->myStreamStatus == STREAMSTATUS_MINISTREAM && i==_passedAudio->numBuffers-1){ // If we just loaded the last buffer.
				_passedAudio->myStreamStatus = STREAMSTATUS_DONTSTREAM;
			}
			if (_streamMoreResult!=0){ // If error or end of file without looping
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
	
	// Setup sample rate ratios
	double _oldRateToNewRateRatio = (double)mlgsnd_getSampleRate(_returnAudio)/48000;
	_returnAudio->unscaledSamples = AUDIO_BUFFER_LENGTH*_oldRateToNewRateRatio;
	_returnAudio->scaledSamples = AUDIO_BUFFER_LENGTH;
	
	// Malloc temp buffers
	_returnAudio->tempFloatConverted = malloc(mlgsnd_getBufferSize(_returnAudio,_returnAudio->scaledSamples,sizeof(float)));
	_returnAudio->tempFloatSource = malloc(mlgsnd_getBufferSize(_returnAudio,_returnAudio->unscaledSamples,sizeof(float)));
	
	// Init converter object data argument now that temp buffer exists
	_returnAudio->usualConverterData.data_in = _returnAudio->tempFloatSource;
	_returnAudio->usualConverterData.data_out = _returnAudio->tempFloatConverted;
	_returnAudio->usualConverterData.input_frames = _returnAudio->unscaledSamples;
	_returnAudio->usualConverterData.output_frames= _returnAudio->scaledSamples;
	_returnAudio->usualConverterData.src_ratio = 48000/(double)mlgsnd_getSampleRate(_returnAudio);
	_returnAudio->usualConverterData.end_of_input=0;

	// Init converter object
	_returnAudio->personalConverter = src_new(SRC_SINC_MEDIUM_QUALITY,mlgsnd_getNumChannels(_returnAudio),&lastConverterError);
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
	int i;
	for (i=0;i<_returnAudio->numBuffers;i++){
		_returnAudio->audioBuffers[i] = malloc(mlgsnd_getBufferSize(_returnAudio,_returnAudio->scaledSamples,sizeof(short))); // The array elements, the audio buffers	
	}

	// Fill a certian amount of audio buffers depending on if we want streaming or not
	if (_returnAudio->myStreamStatus==STREAMSTATUS_STREAMING || _returnAudio->myStreamStatus==STREAMSTATUS_MINISTREAM){ // Fill one buffer
		mlgsnd_loadMoreData(_returnAudio,0); // TODO - CRASH!
	}else if (_returnAudio->myStreamStatus==STREAMSTATUS_DONTSTREAM){ // Fill all buffers
		short i;
		for (i=0;i<_returnAudio->numBuffers;i++){
			mlgsnd_loadMoreData(_returnAudio,i);
		}
	}else{
		printf("error.\n");
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
		printf("Need to rewind\n");
		// If this is our second time playing this audio
		mlgsnd_restartAudio(_passedAudio);
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
int main(void) {
	psvDebugScreenInit();

	if (sizeof(float)!=4 || sizeof(short)!=2 || (sizeof(int)>sizeof(void*))){
		printf("Type sizes wrong, f;d, %d;%d\n",sizeof(float),sizeof(short));
	}

	NathanAudio* _theGoodBGM = mlgsnd_loadMusic("ux0:data/SOUNDTEST/dppbattlemono.ogg");
	mlgsnd_setVolume(_theGoodBGM,50);
	mlgsnd_play(_theGoodBGM);

	//NathanAudio* _theGoodBGM2 = mlgsnd_loadMusic("ux0:data/SOUNDTEST/as.ogg");
	//mlgsnd_setVolume(_theGoodBGM2,50);
	//mlgsnd_play(_theGoodBGM2);
//
	//NathanAudio* _theGoodBGM3 = mlgsnd_loadMusic("ux0:data/SOUNDTEST/as.ogg");
	//mlgsnd_setVolume(_theGoodBGM3,50);
	//mlgsnd_play(_theGoodBGM3);

	NathanAudio* _lastSoundEffect=NULL;
	char* _firstPossibleSound="ux0:data/SOUNDTEST/rikatest.ogg";
	char* _secondPossibleSound="ux0:data/SOUNDTEST/battlerika.ogg";

	char* _newSoundToPlay=_firstPossibleSound;

	printf("X-Stop\nO-Free\nSquare-Pause\nTrinalge-Fadeout\nUp - play");
	while (1){
		
		SceCtrlData ctrl;
		sceCtrlPeekBufferPositive(0, &ctrl, 1);
		if ((ctrl.buttons & SCE_CTRL_UP) && !(lastCtrl.buttons & SCE_CTRL_UP)){
			printf("play sound %p\n",_newSoundToPlay);
			if (_lastSoundEffect!=NULL){
				mlgsnd_freeMusic(_lastSoundEffect);
			}
			_lastSoundEffect = _mlgsnd_loadAudio(_newSoundToPlay,0,1);
			mlgsnd_setVolume(_lastSoundEffect,100);
			mlgsnd_play(_lastSoundEffect);
			//if (_newSoundToPlay==_firstPossibleSound){
			//	_newSoundToPlay = _secondPossibleSound;
			//}else{
			//	_newSoundToPlay = _firstPossibleSound;
			//}
		}
		
		lastCtrl = ctrl;
	
		sceKernelDelayThread(10000);
	}

	
	sceKernelExitProcess(0);
	return 0;
}
#endif