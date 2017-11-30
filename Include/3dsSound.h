#ifndef THREEDSSOUNDHEADERGUARD
#define THREEDSSOUNDHEADERGUARD
	#include <3ds.h>
	#include <ogg/ogg.h>
	#include <vorbis/codec.h>
	#include <vorbis/vorbisfile.h>
	
	typedef struct{
		OggVorbis_File _musicOggFile;
		char* _musicMusicBuffer[2];
		ndspWaveBuf _musicWaveBuffer[2];
		int _musicOggCurrentSection; // Use by libvorbis
		char _musicIsTwoBuffers;
		unsigned char _musicChannel;
		unsigned char _musicShoudLoop;
		char _musicIsDone;
	}NathanMusic;
#endif