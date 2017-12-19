// SNDPLAYER == SND_3DS
#ifndef TREEDEESHSOUND
#define TREEDEESHSOUND
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <3ds.h>
#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

// TODO - WAV playing causes problems. Probably a buffer overflow.

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#define BYTESPERSAMPLE 2

#define MAXBUFFERSIZE 2000000
#define SINGLEOGGREAD 4096

#define MUSICTYPE_NONE 0
#define MUSICTYPE_OGG 1
#define MUSICTYPE_WAV 2

typedef struct{
	void* _musicMainStruct; // Made with malloc, remember to free.
	char* _musicMusicBuffer[10]; // Only 2 used normally. Up 10 for single buffer sound effects
	ndspWaveBuf _musicWaveBuffer[10];
	char _musicIsTwoBuffers; // For sound effects, this is the number of buffers.
	unsigned char _musicChannel;
	unsigned char _musicShoudLoop;
	unsigned char _musicType;
	char _musicIsDone;
}NathanMusic;

// Returns -1 if error
// Returns 1 if EOF
// Returns 0 otherwise
signed char decoreMoreOGG(OggVorbis_File* _passedOggFile, char* _passedAudioBuffer, char _passedShouldLoop, u64* _soundBufferWriteOffset){
	int _passedCurrentSection=0;
	*_soundBufferWriteOffset=0;
	signed char _returnCode=0;
	// Load the actual OGG sound data
	while(1){
		// Read from my OGG file, at the correct offset in my buffer, I can read 4096 bytes at most, big endian, 16-bit samples, and signed samples
		long ret=ov_read(_passedOggFile,&(_passedAudioBuffer[*_soundBufferWriteOffset]),SINGLEOGGREAD,0,2,1,&_passedCurrentSection);
		if (ret == 0) { // EOF
			if (_passedShouldLoop==1){
				if (*_soundBufferWriteOffset==0){
					ov_raw_seek(_passedOggFile,0);
					*_soundBufferWriteOffset=0;
					continue;
				}else{ // Just return what little I have
					break;
				}
			}
			_returnCode=1;
			break;
		} else if (ret < 0) {
			printf("Error: %ld\n",ret);
			if (ret==OV_HOLE){
				printf("OV_HOLE\n");
			}else if(ret==OV_EBADLINK){
				printf("OV_EBADLINK\n");
			}else if (ret==OV_EINVAL){
				printf("OV_EINVAL\n");
			}
			_returnCode=-1;
			break;
		} else {
			// Move pointer in buffer
			*_soundBufferWriteOffset+=ret;
			if (*_soundBufferWriteOffset+SINGLEOGGREAD>=MAXBUFFERSIZE){
				break;
			}
		}
	}
	return _returnCode;
}
signed char decoreMoreWAV(drwav* _passedWavFile, char* _passedAudioBuffer, char _passedShouldLoop, u64* _soundBufferWriteOffset){
	*_soundBufferWriteOffset=0;
	signed char _returnCode=0;
	while(1){
		long ret = drwav_read_s16(_passedWavFile, SINGLEOGGREAD, (short int*)&(_passedAudioBuffer[*_soundBufferWriteOffset]));
		if (ret == 0) {
			// EOF
			if (_passedShouldLoop==1){
				if (*_soundBufferWriteOffset==0){
					drwav_seek_to_sample(_passedWavFile,0);
					continue;
				}else{ // Just return what little I have
					break;
				}
			}
			_returnCode=1;
			break;
		} else if (ret < 0) {
			printf("Error: %ld\n",ret);
			_returnCode=-1;
			break;
		} else {
			// Move pointer in buffer
			*_soundBufferWriteOffset+=ret;
			if (*_soundBufferWriteOffset+SINGLEOGGREAD>=MAXBUFFERSIZE){
				break;
			}
		}
	}
	return _returnCode;
}
int nathanGetMusicNumberOfChannels(NathanMusic* _passedMusic){
	if (_passedMusic->_musicType==MUSICTYPE_OGG){
		vorbis_info* vi=ov_info((_passedMusic->_musicMainStruct),-1);
		return vi->channels;
	}else if (_passedMusic->_musicType==MUSICTYPE_WAV){
		return ((drwav*)(_passedMusic->_musicMainStruct))->channels;
	}else{
		return 0;
	}
}
// Returns -1 if error
// Returns 1 if EOF
// Returns 0 otherwise
signed char nathannathanUpdateAudioBufferNathanMusic(NathanMusic* _passedMusic, int _passedBufferNumber){
	u64 _returnedReadBytes;
	signed char _returnCode=0;
	if (_passedMusic->_musicType == MUSICTYPE_OGG){
		_returnCode = decoreMoreOGG(_passedMusic->_musicMainStruct,_passedMusic->_musicMusicBuffer[_passedBufferNumber],_passedMusic->_musicShoudLoop,&_returnedReadBytes);
	}else if (_passedMusic->_musicType == MUSICTYPE_WAV){
		_returnCode = decoreMoreWAV(_passedMusic->_musicMainStruct,_passedMusic->_musicMusicBuffer[_passedBufferNumber],_passedMusic->_musicShoudLoop,&_returnedReadBytes);
	}
	// Reset wave buffer
	memset(&(_passedMusic->_musicWaveBuffer[_passedBufferNumber]),0,sizeof(ndspWaveBuf));
	// I don't know why I have to do this. I just copied the example
	DSP_FlushDataCache(_passedMusic->_musicMusicBuffer[_passedBufferNumber],MAXBUFFERSIZE);
	// This points to the actual audio data
	_passedMusic->_musicWaveBuffer[_passedBufferNumber].data_vaddr = _passedMusic->_musicMusicBuffer[_passedBufferNumber];
	// Total number of samples
	_passedMusic->_musicWaveBuffer[_passedBufferNumber].nsamples = (_returnedReadBytes/BYTESPERSAMPLE);
	if (nathanGetMusicNumberOfChannels(_passedMusic)==2){
		// (PCM16=halfwords)
		_passedMusic->_musicWaveBuffer[_passedBufferNumber].nsamples/=2; // I guess we divide the number by 2 because it's stereo
	}
	return _returnCode;
}
long nathanGetMusicRate(NathanMusic* _passedMusic){
	if (_passedMusic->_musicType==MUSICTYPE_OGG){
		vorbis_info* vi=ov_info((_passedMusic->_musicMainStruct),-1);
		return vi->rate;
	}else if (_passedMusic->_musicType==MUSICTYPE_WAV){
		return ((drwav*)(_passedMusic->_musicMainStruct))->sampleRate;
	}else{
		return 0;
	}
}
unsigned char getMusicType(char* _filename){
	if (strcmp(&(_filename[strlen(_filename)-3]),"ogg")==0){
		return MUSICTYPE_OGG;
	}else if (strcmp(&(_filename[strlen(_filename)-3]),"wav")==0){
		return MUSICTYPE_WAV;
	}else{
		return MUSICTYPE_NONE;
	}
}
void _nathanOpenSpecificMusic(NathanMusic* _passedMusic, char* _filename){
	if (_passedMusic->_musicType == MUSICTYPE_OGG){
		FILE* fp = fopen(_filename,"r");
		if(ov_open(fp, (_passedMusic->_musicMainStruct), NULL, 0) != 0){
			fclose(fp);
			printf("open not worked!\n");
			return;
		}
	}else if (_passedMusic->_musicType==MUSICTYPE_WAV){
		if (!drwav_init_file(_passedMusic->_musicMainStruct, _filename)) {
			printf("Error open file!\n");
			return;
		}
	}
}
// Return 1 if problem
char _nathanInitMusic(NathanMusic* _passedMusic, char* _filename){
	_passedMusic->_musicShoudLoop=0;
	_passedMusic->_musicIsTwoBuffers=0;
	_passedMusic->_musicIsDone=0;
	_passedMusic->_musicType = getMusicType(_filename);
	if (_passedMusic->_musicType==MUSICTYPE_OGG){
		_passedMusic->_musicMainStruct = malloc(sizeof(OggVorbis_File));
	}else if (_passedMusic->_musicType==MUSICTYPE_WAV){
		_passedMusic->_musicMainStruct = malloc(sizeof(drwav));
	}else{
		return 1;
	}
	_nathanOpenSpecificMusic(_passedMusic,_filename);
	if (nathanGetMusicNumberOfChannels(_passedMusic)>2){
		return 1;
	}
	return 0;
}
void nathanLoadMusic(NathanMusic* _passedMusic, char* _filename, unsigned char _passedShouldLoop){
	if (_nathanInitMusic(_passedMusic,_filename)==1){
		return;
	}
	_passedMusic->_musicShoudLoop=_passedShouldLoop;
	// Where the actual audio data is held
	// Should be in format NDSP_FORMAT_STEREO_PCM16
	// The buffer size is, I guess, the total number of samples times the sample byte size
	// THIS MUST BE A CHAR POINTER! THIS MUST BE A CHAR POINTER!
	_passedMusic->_musicMusicBuffer[0] = linearAlloc(MAXBUFFERSIZE);
	
	if (nathannathanUpdateAudioBufferNathanMusic(_passedMusic,0)!=1){
		_passedMusic->_musicIsTwoBuffers=1;
		_passedMusic->_musicMusicBuffer[1] = linearAlloc(MAXBUFFERSIZE);
		nathannathanUpdateAudioBufferNathanMusic(_passedMusic,1);
	}else{
		_passedMusic->_musicMusicBuffer[1]=NULL;
	}
}
void nathanLoadSoundEffect(NathanMusic* _passedMusic, char* _filename){
	if (_nathanInitMusic(_passedMusic,_filename)==1){
		return;
	}
	_passedMusic->_musicShoudLoop=0;
	int i;
	for (i=0;i<10;i++){
		_passedMusic->_musicIsTwoBuffers++; // With sound effect, this is number of buffers
		_passedMusic->_musicMusicBuffer[i] = linearAlloc(MAXBUFFERSIZE);
		if (nathannathanUpdateAudioBufferNathanMusic(_passedMusic,i)==1){
			break;
		}
	}
	for (i++;i<10;i++){
		_passedMusic->_musicMusicBuffer[i]=NULL;
	}
}
void nathanQueueMusic3ds(ndspWaveBuf* _musicToPlay, int _channelNumber){
	// Put in queue
	ndspChnWaveBufAdd(_channelNumber, _musicToPlay);
}
char nathanInit3dsSound(){
	// Init sound stupidity
	if (R_SUCCEEDED(ndspInit())!=1){
		return 0;
	}
	ndspSetOutputMode(NDSP_OUTPUT_STEREO);
	return 1;
}
void nathanMakeChannelMono(int _channelNumber){
	ndspChnSetFormat(_channelNumber, NDSP_FORMAT_MONO_PCM16);
}
void nathanMakeChannelStereo(int _channelNumber){
	ndspChnSetFormat(_channelNumber, NDSP_FORMAT_STEREO_PCM16);
}
char nathanSetChannelFormat(int _channelNumber, int _numberOfChannels){
	if (_numberOfChannels==1){
		nathanMakeChannelMono(_channelNumber);
	}else if (_numberOfChannels==2){
		nathanMakeChannelStereo(_channelNumber);
	}else{
		return 1;
	}
	return 0;
}
void nathanInit3dsChannel(int _channelNumber){
	ndspChnWaveBufClear(_channelNumber);
	ndspChnSetInterp(_channelNumber, NDSP_INTERP_LINEAR);
	ndspChnSetFormat(_channelNumber, NDSP_FORMAT_STEREO_PCM16);
	// I think this has to do with left and right ear stereo mixing
	float mix[12];
	memset(mix, 0, sizeof(mix));
	mix[0] = 1.0;
	mix[1] = 1.0;
	ndspChnSetMix(_channelNumber, mix);
}
void nathanSetChannelForMusic(int _channelNumber, NathanMusic* _passedMusic){
	_passedMusic->_musicChannel=_channelNumber;
	_passedMusic->_musicIsDone=0;
	ndspChnSetRate(_passedMusic->_musicChannel, nathanGetMusicRate(_passedMusic));
	nathanSetChannelFormat(_passedMusic->_musicChannel, nathanGetMusicNumberOfChannels(_passedMusic));
}
void stopChannel(int _channelNumber){
	ndspChnReset(_channelNumber);
	ndspChnInitParams(_channelNumber);
	nathanInit3dsChannel(_channelNumber);
}
void nathanPlayMusic(NathanMusic* _passedMusic, unsigned char _channelNumber){
	stopChannel(_channelNumber);
	nathanSetChannelForMusic(_channelNumber,_passedMusic);
	nathanQueueMusic3ds(&(_passedMusic->_musicWaveBuffer[0]),_passedMusic->_musicChannel);
	if (_passedMusic->_musicIsTwoBuffers==1){
		nathanQueueMusic3ds(&(_passedMusic->_musicWaveBuffer[1]),_passedMusic->_musicChannel);
	}
}
void nathanPlaySound(NathanMusic* _passedMusic, unsigned char _channelNumber){
	stopChannel(_channelNumber);
	nathanSetChannelForMusic(_channelNumber,_passedMusic);
	int i;
	for (i=0;i<_passedMusic->_musicIsTwoBuffers;i++){
		nathanQueueMusic3ds(&(_passedMusic->_musicWaveBuffer[i]),_passedMusic->_musicChannel);
	}
}
void nathanUpdateMusicIfNeeded(NathanMusic* _passedMusic){
	if (_passedMusic->_musicIsDone==1){
		return;
	}
	int i;
	for (i=0;i<=(_passedMusic->_musicIsTwoBuffers==1);i++){
		if (_passedMusic->_musicWaveBuffer[i].status == NDSP_WBUF_DONE){
			if (nathannathanUpdateAudioBufferNathanMusic(_passedMusic,i)==1){
				_passedMusic->_musicIsDone=1;
			}
			nathanQueueMusic3ds(&(_passedMusic->_musicWaveBuffer[i]),_passedMusic->_musicChannel);
		}
	}
}
void _nathanFreeSpecificStruct(NathanMusic* _passedMusic){
	if (_passedMusic->_musicType == MUSICTYPE_OGG){
		ov_clear((_passedMusic->_musicMainStruct));
	}else if (_passedMusic->_musicType == MUSICTYPE_WAV){
		drwav_close(_passedMusic->_musicMainStruct);
	}
}
void nathanFreeMusic(NathanMusic* _passedMusic){
	ndspChnWaveBufClear(_passedMusic->_musicChannel);
	DSP_FlushDataCache(_passedMusic->_musicMusicBuffer[0],MAXBUFFERSIZE);
	linearFree(_passedMusic->_musicMusicBuffer[0]);
	if (_passedMusic->_musicIsTwoBuffers==1){
		DSP_FlushDataCache(_passedMusic->_musicMusicBuffer[1],MAXBUFFERSIZE);
		linearFree(_passedMusic->_musicMusicBuffer[1]);
	}
	_nathanFreeSpecificStruct(_passedMusic);
	free(_passedMusic->_musicMainStruct);
}
void nathanFreeSound(NathanMusic* _passedMusic){
	ndspChnWaveBufClear(_passedMusic->_musicChannel);
	int i;
	for (i=0;i<_passedMusic->_musicIsTwoBuffers;i++){
		DSP_FlushDataCache(_passedMusic->_musicMusicBuffer[i],MAXBUFFERSIZE);
		linearFree(_passedMusic->_musicMusicBuffer[i]);
	}
	_nathanFreeSpecificStruct(_passedMusic);
	free(_passedMusic->_musicMainStruct);
}
void nathanSetChannelVolume(int _channelNumber, float _volume){
	float mix[12];
	memset(mix, 0, sizeof(mix));
	mix[0] = _volume;
	mix[1] = _volume;
	ndspChnSetMix(_channelNumber, mix);
}
#endif