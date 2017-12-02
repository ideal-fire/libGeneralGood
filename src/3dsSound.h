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

#define BYTESPERSAMPLE 2

// Total samples in the OGG file
long storedTotalSamples;

#define MAXBUFFERSIZE 2000000
#define SINGLEOGGREAD 4096

typedef struct{
	OggVorbis_File _musicOggFile;
	char* _musicMusicBuffer[10]; // Only 2 used normally. Up 10 for single buffer sound effects
	ndspWaveBuf _musicWaveBuffer[10];
	int _musicOggCurrentSection; // Used by libvorbis
	char _musicIsTwoBuffers;
	unsigned char _musicChannel;
	unsigned char _musicShoudLoop;
	char _musicIsDone;
}NathanMusic;

// Returns -1 if error
// Returns 1 if EOF
// Returns 0 otherwise
signed char nathanUpdateAudioBuffer(OggVorbis_File* _passedOggFile, char* _passedAudioBuffer, ndspWaveBuf* _passedWaveBuffer, int* _passedCurrentSection, char _passedShouldLoop){
	// Load the actual OGG sound data
	u64 _soundBufferWriteOffset=0;
	signed char _returnCode=0;
	while(1){
		// Read from my OGG file, at the correct offset in my buffer, I can read 4096 bytes at most, big endian, 16-bit samples, and signed samples
		long ret=ov_read(_passedOggFile,&(_passedAudioBuffer[_soundBufferWriteOffset]),SINGLEOGGREAD,0,2,1,_passedCurrentSection);
		if (ret == 0) {
			printf("eof reached: %lld, %d\n",_soundBufferWriteOffset,_passedShouldLoop);
			// EOF
			if (_soundBufferWriteOffset==0){
				if (_passedShouldLoop==1){
					ov_raw_seek(_passedOggFile,0);
					_soundBufferWriteOffset=0;
					continue;
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
			_soundBufferWriteOffset+=ret;
			if (_soundBufferWriteOffset+SINGLEOGGREAD>MAXBUFFERSIZE){
				break;
			}
		}
	}
	// Reset wave buffer
	memset(_passedWaveBuffer,0,sizeof(ndspWaveBuf));
	// I don't know why I have to do this. I just copied the example
	DSP_FlushDataCache(_passedAudioBuffer,MAXBUFFERSIZE);
	// This points to the actual audio data
	_passedWaveBuffer->data_vaddr = _passedAudioBuffer;
	// Total number of samples (PCM16=halfwords)
	_passedWaveBuffer->nsamples = (_soundBufferWriteOffset/BYTESPERSAMPLE)/2; // I guess we divide the number by 2 because it's stereo
	return _returnCode;
}
signed char nathannathanUpdateAudioBufferNathanMusic(NathanMusic* _passedMusic, int _passedBufferNumber){
	return nathanUpdateAudioBuffer(&(_passedMusic->_musicOggFile),_passedMusic->_musicMusicBuffer[_passedBufferNumber], &(_passedMusic->_musicWaveBuffer[_passedBufferNumber]),&(_passedMusic->_musicOggCurrentSection),_passedMusic->_musicShoudLoop);
}
int nathanGetMusicNumberOfChannels(NathanMusic* _passedMusic){
	vorbis_info* vi=ov_info(&(_passedMusic->_musicOggFile),-1);
	return vi->channels;
}
long nathanGetMusicRate(NathanMusic* _passedMusic){
	vorbis_info* vi=ov_info(&(_passedMusic->_musicOggFile),-1);
	return vi->rate;
}
void nathanLoadMusic(NathanMusic* _passedMusic, char* _filename, unsigned char _passedShouldLoop){
	_passedMusic->_musicShoudLoop=_passedShouldLoop;
	_passedMusic->_musicIsTwoBuffers=0;
	_passedMusic->_musicIsDone=0;

	// Load two sections of music first
	// Load OGG file and some stupid info from it.
	// By stupid info, I mean like the vendor and stuff
	// This is also where we get the total number of samples
	FILE* fp = fopen(_filename,"r");
	if(ov_open(fp, &(_passedMusic->_musicOggFile), NULL, 0) != 0){
		fclose(fp);
		printf("open not worked!\n");
		return;
	}
	// Only programmed to work with stereo at the moment
	if (nathanGetMusicNumberOfChannels(_passedMusic)!=2){
		return;
	}
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
	_passedMusic->_musicShoudLoop=0;
	_passedMusic->_musicIsTwoBuffers=0;
	_passedMusic->_musicIsDone=0;
	FILE* fp = fopen(_filename,"r");
	if(ov_open(fp, &(_passedMusic->_musicOggFile), NULL, 0) != 0){
		fclose(fp);
		printf("open not worked!\n");
		return;
	}
	if (nathanGetMusicNumberOfChannels(_passedMusic)!=2){
		return;
	}
	int i;
	for (i=0;i<10;i++){
		_passedMusic->_musicIsTwoBuffers++; // With sound effect, this is number of buffers
		_passedMusic->_musicMusicBuffer[i] = linearAlloc(MAXBUFFERSIZE);
		if (nathannathanUpdateAudioBufferNathanMusic(_passedMusic,i)==1){
			break;
		}
	}
	for (;i<10;i++){
		_passedMusic->_musicMusicBuffer[i]=NULL;
	}
}
void nathanQueueMusic3ds(ndspWaveBuf* _musicToPlay, int _channelNumber){
	// Put in queue
	ndspChnWaveBufAdd(_channelNumber, _musicToPlay);
}
void nathanInit3dsSound(){
	// Init sound stupidity
	ndspInit();
	ndspSetOutputMode(NDSP_OUTPUT_STEREO);
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
void nathanPlayMusic(NathanMusic* _passedMusic, unsigned char _channelNumber){
	_passedMusic->_musicChannel=_channelNumber;
	_passedMusic->_musicIsDone=0;
	ndspChnSetRate(_passedMusic->_musicChannel, nathanGetMusicRate(_passedMusic));
	nathanQueueMusic3ds(&(_passedMusic->_musicWaveBuffer[0]),_passedMusic->_musicChannel);
	if (_passedMusic->_musicIsTwoBuffers==1){
		nathanQueueMusic3ds(&(_passedMusic->_musicWaveBuffer[1]),_passedMusic->_musicChannel);
	}
}
void nathanPlaySound(NathanMusic* _passedMusic, unsigned char _channelNumber){
	_passedMusic->_musicChannel=_channelNumber;
	_passedMusic->_musicIsDone=0;
	ndspChnSetRate(_passedMusic->_musicChannel, nathanGetMusicRate(_passedMusic));
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
void nathanFreeMusic(NathanMusic* _passedMusic){
	ndspChnWaveBufClear(_passedMusic->_musicChannel);
	linearFree(_passedMusic->_musicMusicBuffer[0]);
	if (_passedMusic->_musicIsTwoBuffers==1){
		linearFree(_passedMusic->_musicMusicBuffer[1]);
	}
	ov_clear(&(_passedMusic->_musicOggFile));
}
void nathanFreeSound(NathanMusic* _passedMusic){
	ndspChnWaveBufClear(_passedMusic->_musicChannel);
	int i;
	for (i=0;i<_passedMusic->_musicIsTwoBuffers;i++){
		linearFree(_passedMusic->_musicMusicBuffer[i]);
	}
	ov_clear(&(_passedMusic->_musicOggFile));
}
void nathanSetChannelVolume(int _channelNumber, float _volume){
	float mix[12];
	memset(mix, 0, sizeof(mix));
	mix[0] = _volume;
	mix[1] = _volume;
	ndspChnSetMix(_channelNumber, mix);
}

#endif