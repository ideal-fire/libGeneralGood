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

// Planned
#define FILE_FORMAT_WAV 2
#define FILE_FORMAT_MP3 3

#define NAUDIO_QUITSTATUS_PLAYING 1
#define NAUDIO_QUITSTATUS_SHOULDQUIT 2
#define NAUDIO_QUITSTATUS_PAUSED 3
#define NAUDIO_QUITSTATUS_QUITTED 5 // I know "quitted" is wierd, but I don't want to make this constant sound like a command.
#define NAUDIO_QUITSTATUS_PREPARING 6 // Getting audio ports and such ready
#define NAUDIO_QUITSTATUS_FREED 7

typedef struct fruhfreuir{
	void* mainAudioStruct; // malloc'd and needs a free function
	void** audioBuffers;  // malloc'd

	float* tempFloatConverted; // malloc'd. This is the destination buffer for the converted audio data before it's copied back to the main buffer.

	SRC_DATA usualConverterData;
	SRC_STATE* personalConverter; // Needs a free function

	unsigned int unscaledSamples; // Number of samples to load from the file and convert
	unsigned int scaledSamples; // Number of samples to play and size of destination buffer

	signed char numBuffers; // Number of audio buffers we can swap between. Must be exactly 2.
	signed char fileFormat; // Tells you what the file format is, OGG, WAV, etc.
	signed char shouldLoop; // bool
	SceUID playerThreadId; // Thread ID for the thread that's playing this sound.
	signed int audioPort;

	signed char quitStatus; // 0 if should not quit, 1 if should quit, -1 if already quit
	signed char isFadingOut;
	unsigned int fadeoutPerSwap; // In Vita volume units
	signed int volume;
	signed int lastVolume; // Used to detect volume changes
}NathanAudio;
int lastConverterError=0;

//signed char nextLoadableSoundBuffer=1;
//signed char lastLoadedSoundBuffer=1;

// TODO - Maybe make the sound threads before they're needed.
// TODO - Support for libGeneralGood

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
uint64_t getTicks(){
	return  (sceKernelGetProcessTimeWide());
}
#endif

// Returns 1 for end of file or error
char mlgsnd_readOGGData(OggVorbis_File* _fileToReadFrom, char* _destinationBuffer, int _totalBufferSize, char _passedShouldLoop){
	int _soundBufferWriteOffset=0;
	int _currentSection;
	int _bytesLeftInBuffer = _totalBufferSize;
	while(1){
		// Read from my OGG file, at the correct offset in my buffer, I can read 4096 bytes at most, big endian, 16-bit samples, and signed samples
		long ret=ov_read(_fileToReadFrom,&(_destinationBuffer[_soundBufferWriteOffset]),_bytesLeftInBuffer,0,2,1,&_currentSection);
		if (ret == 0) { // EOF
			if (!_passedShouldLoop || ov_raw_seek(_fileToReadFrom,0)!=0){
				return 1;
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
			return 1;
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
	if (_passedAudio->quitStatus == NAUDIO_QUITSTATUS_QUITTED || _passedAudio->playerThreadId == 0){
		return;
	}
	_passedAudio->quitStatus = NAUDIO_QUITSTATUS_SHOULDQUIT;
	SceUInt _tempStoreTimeout = 5000000;
	sceKernelWaitThreadEnd(_passedAudio->playerThreadId,0,&_tempStoreTimeout); // Timeout is 5 seconds
	_passedAudio->playerThreadId=0;
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
	// Free temp conversion buffers
	free(_passedAudio->tempFloatConverted);
	// Phew, almost forgot these
	free(_passedAudio->mainAudioStruct);
	free(_passedAudio);

	_passedAudio->quitStatus = NAUDIO_QUITSTATUS_FREED;
}

long mlgsnd_getBufferSize(NathanAudio* _passedAudio, unsigned int _numberOfSamples, char _sizeOfUint){
	return (mlgsnd_getNumChannels(_passedAudio)==1 ? _numberOfSamples*_sizeOfUint : _numberOfSamples*_sizeOfUint*2);
}

// Returns 1 if audio is done playing or error, 0 otherwise
char mlgsnd_loadMoreData(NathanAudio* _passedAudio, unsigned char _audioBufferSlot){
	if (_passedAudio->fileFormat==FILE_FORMAT_OGG){
		if (mlgsnd_readOGGData(_passedAudio->mainAudioStruct,_passedAudio->audioBuffers[_audioBufferSlot],mlgsnd_getBufferSize(_passedAudio,_passedAudio->unscaledSamples,sizeof(short)),_passedAudio->shouldLoop)!=0){
			return 1;
		}
	}

	// Change the sample rate converter input to my current audio buffer
	_passedAudio->usualConverterData.data_in = _passedAudio->audioBuffers[_audioBufferSlot];

	// Change our loaded short data into loaded float data
	src_short_to_float_array(_passedAudio->audioBuffers[_audioBufferSlot],_passedAudio->audioBuffers[_audioBufferSlot],mlgsnd_getBufferSize(_passedAudio,_passedAudio->unscaledSamples,1));
	
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
	NathanAudio* _passedAudio=*((NathanAudio**)argp);

	// If this is our second time playing this audio
	mlgsnd_restartAudio(_passedAudio);

	// SCE_AUDIO_MAX_LEN is 65472. 65472/44100 = 1.485. That feels like around how much I've loaded.
	// The port returned from this must be kept safe for future use
	_passedAudio->audioPort = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, AUDIO_BUFFER_LENGTH, 48000, mlgsnd_getNumChannels(_passedAudio) == 1 ? SCE_AUDIO_OUT_MODE_MONO : SCE_AUDIO_OUT_MODE_STEREO);
	if (_passedAudio->audioPort<0){ // Probably no ports avalible
		return 0;
	}

	// More audio init
	sceAudioOutSetVolume(_passedAudio->audioPort, SCE_AUDIO_VOLUME_FLAG_L_CH |SCE_AUDIO_VOLUME_FLAG_R_CH, (int[]){_passedAudio->volume,_passedAudio->volume});
	sceAudioOutSetConfig(_passedAudio->audioPort, -1, -1, (SceAudioOutMode)-1); // This line is probablly useless. A -1 argument means that the specific property isn't changed. But I saw my god, Rinnegatamante, do it, so I'll do it too.
	
	// If we already recieved a trigger then process it
	while (_passedAudio->quitStatus != NAUDIO_QUITSTATUS_PREPARING){
		// Process early triggers
		_mlgsnd_waitWhilePaused(_passedAudio);
		// Always at the end
		if (_passedAudio->quitStatus==NAUDIO_QUITSTATUS_SHOULDQUIT){
			break;
		}else{
			_passedAudio->quitStatus = NAUDIO_QUITSTATUS_PREPARING;
		}
	}
	

	// Only load starting audio and set the playing flag if we're not planning to quit right after
	if (_passedAudio->quitStatus!=NAUDIO_QUITSTATUS_SHOULDQUIT){
		if (_passedAudio->quitStatus==NAUDIO_QUITSTATUS_PREPARING){ // Check for rare case flag changes
			_passedAudio->quitStatus = NAUDIO_QUITSTATUS_PLAYING; // We are now officially playing.
		}else{ // Flag changed too quick
			printf("error.\n");
			return 0;
		}
		// Load some data
		mlgsnd_loadMoreData(_passedAudio,0);
	}

	int i;
	for (i=0;_passedAudio->quitStatus != NAUDIO_QUITSTATUS_SHOULDQUIT;){
		// Detect volume changes
		if (_passedAudio->lastVolume!=_passedAudio->volume){
			sceAudioOutSetVolume(_passedAudio->audioPort, SCE_AUDIO_VOLUME_FLAG_L_CH |SCE_AUDIO_VOLUME_FLAG_R_CH, (int[]){_passedAudio->volume,_passedAudio->volume}); 
			_passedAudio->lastVolume=_passedAudio->volume;
		}

		// Fadeout if needed
		if (_passedAudio->isFadingOut){
			_passedAudio->volume-=_passedAudio->fadeoutPerSwap;
			// If fadeout is over
			if (_passedAudio->volume<0){
				break;
			}
			sceAudioOutSetVolume(_passedAudio->audioPort, SCE_AUDIO_VOLUME_FLAG_L_CH |SCE_AUDIO_VOLUME_FLAG_R_CH, (int[]){_passedAudio->volume,_passedAudio->volume}); 
		}

		// Start playing audio
		sceAudioOutOutput(_passedAudio->audioPort, _passedAudio->audioBuffers[i]);

		// Swap buffer variable
		i=!i;
		
		// Start loading next audio into the other buffer while the current audio buffer is playing.
		if (mlgsnd_loadMoreData(_passedAudio,i)!=0){ // If error or end of file.
			break; // Break out
		}
		
		// Wait for audio to finish playing in the remaining time
		sceAudioOutOutput(_passedAudio->audioPort, NULL);

		// Check low priority triggers
		_mlgsnd_waitWhilePaused(_passedAudio);
	}

	sceAudioOutOutput(_passedAudio->audioPort, NULL); // Wait for audio to finish playing
	sceAudioOutReleasePort(_passedAudio->audioPort); // Release the port now that there's no audio
	_passedAudio->audioPort=-1; // Reset port variable
	_passedAudio->quitStatus = NAUDIO_QUITSTATUS_QUITTED; // Set quit before exiting
	sceKernelExitDeleteThread(0);
	return 0;
}

NathanAudio* mlgsnd_loadMusic(char* filename, char _passedShouldLoop){
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
	
	// Init data for audio
	_returnAudio->numBuffers=2;
	_returnAudio->quitStatus=NAUDIO_QUITSTATUS_PREPARING;
	_returnAudio->isFadingOut=0;
	_returnAudio->volume = SCE_AUDIO_OUT_MAX_VOL; // SCE_AUDIO_OUT_MAX_VOL is actually max volume, not 0 decibels.
	_returnAudio->lastVolume = _returnAudio->volume;
	_returnAudio->shouldLoop = _passedShouldLoop;
	_returnAudio->playerThreadId=0;

	// Setup sample rate ratios
	double _oldRateToNewRateRatio = (double)mlgsnd_getSampleRate(_returnAudio)/48000;
	_returnAudio->unscaledSamples = AUDIO_BUFFER_LENGTH*_oldRateToNewRateRatio;
	_returnAudio->scaledSamples = AUDIO_BUFFER_LENGTH;
	
	// Malloc audio buffers
	_returnAudio->tempFloatConverted = malloc(mlgsnd_getBufferSize(_returnAudio,_returnAudio->scaledSamples,sizeof(float))); // Temp conversion buffer
	_returnAudio->audioBuffers = malloc(sizeof(void*)*2); // Array
	int i;
	for (i=0;i<_returnAudio->numBuffers;i++){
		_returnAudio->audioBuffers[i] = malloc(mlgsnd_getBufferSize(_returnAudio,_returnAudio->scaledSamples,sizeof(float))); // The array elements, the audio buffers	
	}

	// Init converter object
	_returnAudio->personalConverter = src_new(SRC_SINC_MEDIUM_QUALITY,mlgsnd_getNumChannels(_returnAudio),&lastConverterError);
	if (lastConverterError!=0){
		printf("%s\n",src_strerror(lastConverterError)); // Bad converter number
	}

	// Init converter object data argument
	_returnAudio->usualConverterData.data_in = NULL; // Changed every time
	_returnAudio->usualConverterData.data_out = _returnAudio->tempFloatConverted;
	_returnAudio->usualConverterData.input_frames = _returnAudio->unscaledSamples;
	_returnAudio->usualConverterData.output_frames= _returnAudio->scaledSamples;
	_returnAudio->usualConverterData.src_ratio = 48000/(double)mlgsnd_getSampleRate(_returnAudio);
	_returnAudio->usualConverterData.end_of_input=0;

	return _returnAudio;
}

void mlgsnd_play(NathanAudio* _passedAudio){
	if (_passedAudio->playerThreadId!=0){
		mlgsnd_stopMusic(_passedAudio);
	}
	// Initialize sound playing thread, but don't start it.
	SceUID mySoundPlayingThreadID = sceKernelCreateThread("SOUNDPLAY", mlgsnd_soundPlayingThread, 0, 0x10000, 0, 0, NULL);
	_passedAudio->playerThreadId = mySoundPlayingThreadID;
	// Start sound thread
	sceKernelStartThread(_passedAudio->playerThreadId,sizeof(NathanAudio*),&_passedAudio);
}
#if INCLUDETESTCODE==1
int main(void) {
	psvDebugScreenInit();

	if (sizeof(float)!=4 || sizeof(short)!=2){
		printf("Type sizes wrong, f;d, %d;%d\n",sizeof(float),sizeof(short));
	}

	NathanAudio* myHappyAudio;
	//printf("Start tryna load music.\n");
	
	////NathanAudio* myHappyAudio2 = loadMusic("ux0:data/SOUNDTEST/dppbattlemono.ogg",0);
	//printf("Okay, start playing.\n");
	
	//nathanAudioPlay(myHappyAudio2);

	myHappyAudio = mlgsnd_loadMusic("ux0:data/SOUNDTEST/wa_038.ogg",0);
	mlgsnd_setVolume(myHappyAudio,128);
	mlgsnd_play(myHappyAudio);

	int i;

	//for (i=0;;++i){
	//	printf("Try %d\n",i);
	//	FILE* fp;
	//	fp = fopen("ux0:data/SOUNDTEST/olda.ogg","r");
	//	if (fp==NULL){
	//		printf("Failed.\n");
	//	}
	//	fclose(fp);
	//}

	NathanAudio* myHappyAudio2;
	
	//for (i=0;;++i){ // Fails starting on and after 272nd try. (273rd try on another attempt)
	//	if (i>250){
	//		printf("Try %d\n",i);
	//	}
	//	myHappyAudio2 = mlgsnd_loadMusic("ux0:data/SOUNDTEST/olda.ogg",1);
	//	mlgsnd_play(myHappyAudio2);
	//	mlgsnd_stopMusic(myHappyAudio2);
	//	mlgsnd_freeMusic(myHappyAudio2);
	//}

	while (1){
		
		SceCtrlData ctrl;
		sceCtrlPeekBufferPositive(0, &ctrl, 1);
		if ((ctrl.buttons & SCE_CTRL_CROSS) && !(lastCtrl.buttons & SCE_CTRL_CROSS)){
			printf("Stop music\n");
			mlgsnd_stopMusic(myHappyAudio);
		}
		if ((ctrl.buttons & SCE_CTRL_CIRCLE) && !(lastCtrl.buttons & SCE_CTRL_CIRCLE)){
			printf("free music\n");
			mlgsnd_freeMusic(myHappyAudio);
		}
		if ((ctrl.buttons & SCE_CTRL_SQUARE) && !(lastCtrl.buttons & SCE_CTRL_SQUARE)){
			printf("toggle pause");
			if (myHappyAudio->quitStatus == NAUDIO_QUITSTATUS_PAUSED){
				myHappyAudio->quitStatus = NAUDIO_QUITSTATUS_PLAYING;
			}else{
				myHappyAudio->quitStatus = NAUDIO_QUITSTATUS_PAUSED;
			}
		}
		if ((ctrl.buttons & SCE_CTRL_TRIANGLE) && !(lastCtrl.buttons & SCE_CTRL_TRIANGLE)){
			printf("fadeout");
			mlgsnd_fadeoutMusic(myHappyAudio,5000);
		}
		lastCtrl = ctrl;
	
		sceKernelDelayThread(10000);
	}

	
	sceKernelExitProcess(0);
	return 0;
}
#endif