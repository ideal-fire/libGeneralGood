#ifndef GENERALGOODSOUNDHEADER
#define GENERALGOODSOUNDHEADER
	/*
	================================================
	== SOUND
	=================================================
	*/
	#if SOUNDPLAYER == SND_SDL
		#ifndef _3DS
			#include <SDL2/SDL.h>
			#include <SDL2/SDL_mixer.h>
		#else
			#include <SDL/SDL.h>
			#include <SDL/SDL_mixer.h>
		#endif
		#define CROSSSFX Mix_Chunk
		#define CROSSMUSIC Mix_Music
		#define CROSSPLAYHANDLE void*
	#endif
	#if SOUNDPLAYER == SND_NONE
		#define CROSSSFX int
		#define CROSSMUSIC int
		#define CROSSPLAYHANDLE char
	#endif
	#if SOUNDPLAYER == SND_SOLOUD
		#include <soloud_c.h>
		#define CROSSMUSIC WavStream
		#define CROSSSFX Wav
		#define CROSSPLAYHANDLE unsigned int
		Soloud* mySoLoudEngine;
	#endif
	#if SOUNDPLAYER == SND_3DS
		#include "3dsSound.h"
		#define CROSSMUSIC NathanMusic
		#define CROSSSFX NathanMusic
		#define CROSSPLAYHANDLE unsigned char
	#endif

	char initAudio(){
		#if SOUNDPLAYER == SND_SDL
			SDL_Init( SDL_INIT_AUDIO );
			Mix_Init(MIX_INIT_OGG);
			#if PLATFORM != PLAT_3DS
				Mix_OpenAudio( 44100, MIX_DEFAULT_FORMAT, 2, 2048 );
			#else
				Mix_OpenAudio( 22050, MIX_DEFAULT_FORMAT, 2, 2048 );
			#endif
			return 1;
		#elif SOUNDPLAYER == SND_SOLOUD
			mySoLoudEngine = Soloud_create();
			Soloud_init(mySoLoudEngine);
			return 1;
		#elif SOUNDPLAYER == SND_3DS
			if (nathanInit3dsSound()==0){
				return 0;
			}
			int i;
			for (i=0;i<=20;i++){
				nathanInit3dsChannel(i);
			}
			return 1;
		#endif
	}
	void quitAudio(){
		#if SOUNDPLAYER == SND_SDL
			// TODO
		#elif SOUNDPLAYER == SND_SOLOUD
			// TODO
		#elif SOUNDPLAYER == SND_3DS
			ndspExit();
		#endif
	}
	int getMusicVolume(CROSSPLAYHANDLE _passedMusicHandle){
		#if SOUNDPLAYER == SND_SDL
			return Mix_VolumeMusic(-1);
		#elif SOUNDPLAYER == SND_SOLOUD
			return Soloud_getVolume(mySoLoudEngine,_passedMusicHandle)*128;
		#elif SOUNDPLAYER == SND_3DS
			// TODO
			return 128;
		#elif SOUNDPLAYER == SND_NONE
			return 0;
		#endif
	}
	void setMusicVolume(CROSSPLAYHANDLE _passedMusic,int vol){
		#if SOUNDPLAYER == SND_SDL
			Mix_VolumeMusic(vol);
		#elif SOUNDPLAYER == SND_SOLOUD
			Soloud_setVolume(mySoLoudEngine,_passedMusic,(float)((float)vol/(float)128));
		#elif SOUNDPLAYER == SND_3DS
			nathanSetChannelVolume(_passedMusic,(float)(((double)vol)/128));
		#endif
	}
	void setMusicVolumeBefore(CROSSMUSIC* _passedMusic,int vol){
		#if SOUNDPLAYER == SND_SDL
			Mix_VolumeMusic(vol);
		#elif SOUNDPLAYER == SND_SOLOUD
			WavStream_setVolume(_passedMusic,(float)((float)vol/(float)128));
		#elif SOUNDPLAYER == SND_3DS
			setMusicVolume(_passedMusic->_musicChannel, vol);
		#endif
	}
	void setSFXVolumeBefore(CROSSSFX* tochange, int toval){
		#if SOUNDPLAYER == SND_SDL
			Mix_VolumeChunk(tochange,toval);
		#elif SOUNDPLAYER == SND_SOLOUD
			Wav_setVolume(tochange,(float)((float)toval/(float)128));
		#elif SOUNDPLAYER == SND_3DS
			setMusicVolumeBefore(tochange,toval);
		#endif
	}
	void setSFXVolume(CROSSPLAYHANDLE tochange, int toval){
		#if SOUNDPLAYER == SND_SDL
			setSFXVolumeBefore(tochange,toval);
		#elif SOUNDPLAYER == SND_SOLOUD
			setMusicVolume(tochange,toval);
		#elif SOUNDPLAYER == SND_3DS
			setMusicVolume(tochange,toval);
		#endif
	}
	void fadeoutMusic(CROSSPLAYHANDLE _passedHandle,int time){
		#if SOUNDPLAYER == SND_SDL
			Mix_FadeOutMusic(time);
		#elif SOUNDPLAYER == SND_SOLOUD
			Soloud_fadeVolume(mySoLoudEngine,_passedHandle,0,(double)((double)time/(double)1000));
			Soloud_scheduleStop(mySoLoudEngine,_passedHandle,(double)((double)time/(double)1000));
		#elif SOUNDPLAYER == SND_3DS
			float _perTenthSecond=(float)((float)1/((double)time/(double)100));
			if (_perTenthSecond==0){
				_perTenthSecond=.00001;
			}
			float _currentFadeoutVolume=1;
			while (_currentFadeoutVolume>0){
				if (_currentFadeoutVolume<_perTenthSecond){
					_currentFadeoutVolume=0;
				}else{
					_currentFadeoutVolume-=_perTenthSecond;
				}
				nathanSetChannelVolume(_passedHandle,_currentFadeoutVolume);
				svcSleepThread(100000000); // Wait for one tenth of a second
			}
			ndspChnWaveBufClear(_passedHandle);
		#endif
	}
	CROSSSFX* loadSound(char* filepath){
		#if SOUNDPLAYER == SND_SDL
			return Mix_LoadWAV(filepath);
		#elif SOUNDPLAYER == SND_SOLOUD
			CROSSSFX* _myLoadedSoundEffect = Wav_create();
			Wav_load(_myLoadedSoundEffect,filepath);
			return _myLoadedSoundEffect;
		#elif SOUNDPLAYER == SND_3DS
			NathanMusic* _tempReturn = malloc(sizeof(NathanMusic));
			nathanLoadSoundEffect(_tempReturn,filepath);
			return _tempReturn;
		#elif SOUNDPLAYER == SND_NONE
			return NULL;
		#endif
	}
	CROSSMUSIC* loadMusic(char* filepath){
		#if SOUNDPLAYER == SND_SDL
			return Mix_LoadMUS(filepath);
		#elif SOUNDPLAYER == SND_SOLOUD
			CROSSMUSIC* _myLoadedMusic = WavStream_create();
			WavStream_load(_myLoadedMusic,filepath);
			return _myLoadedMusic;
		#elif SOUNDPLAYER == SND_3DS
			NathanMusic* _tempReturn = malloc(sizeof(NathanMusic));
			nathanLoadMusic(_tempReturn,filepath,1);
			return _tempReturn;
		#elif SOUNDPLAYER == SND_NONE
			return NULL;
		#endif
	}
	void pauseMusic(CROSSPLAYHANDLE _passedHandle){
		#if SOUNDPLAYER == SND_SDL
			Mix_PauseMusic();
		#elif SOUNDPLAYER == SND_SOLOUD
			Soloud_setPause(mySoLoudEngine,_passedHandle, 1);
		#elif SOUNDPLAYER == SND_3DS
			ndspChnSetPaused(_passedHandle,1);
		#endif
	}
	void resumeMusic(CROSSPLAYHANDLE _passedHandle){
		#if SOUNDPLAYER == SND_SDL
			Mix_ResumeMusic();
		#elif SOUNDPLAYER == SND_SOLOUD
			Soloud_setPause(mySoLoudEngine,_passedHandle, 0);
		#elif SOUNDPLAYER == SND_3DS
			ndspChnSetPaused(_passedHandle,0);
		#endif
	}
	void stopMusic(CROSSPLAYHANDLE toStop){
		#if SOUNDPLAYER == SND_SDL
			Mix_HaltMusic();
		#elif SOUNDPLAYER == SND_SOLOUD
			Soloud_stop(mySoLoudEngine,toStop);
		#elif SOUNDPLAYER == SND_3DS
			ndspChnReset(toStop);
			ndspChnInitParams(toStop);
			nathanInit3dsChannel(toStop);
			ndspChnWaveBufClear(toStop);
		#endif
	}
	void stopSound(CROSSSFX* toStop){
		#if SOUNDPLAYER == SND_SDL
			//#warning CANT STOP SOUND WITH SDL_MIXER
		#elif SOUNDPLAYER == SND_SOLOUD
			Wav_stop(toStop);
		#elif SOUNDPLAYER == SND_3DS
			stopMusic(toStop->_musicChannel);
		#endif
	}
	CROSSPLAYHANDLE playSound(CROSSSFX* toPlay, int timesToPlay, unsigned char _passedChannel){
		#if SOUNDPLAYER == SND_SDL
			Mix_PlayChannel( -1, toPlay, timesToPlay-1 );
			return toPlay;
		#elif SOUNDPLAYER == SND_SOLOUD
			if (timesToPlay!=1){
				printf("SoLoud can only play a sound once!");
			}
			return Soloud_play(mySoLoudEngine,toPlay);
		#elif SOUNDPLAYER == SND_3DS
			ndspChnWaveBufClear(_passedChannel);
			nathanPlaySound(toPlay, _passedChannel);
			return toPlay->_musicChannel;
		#elif SOUNDPLAYER == SND_NONE
			return 0;
		#endif
	}
	CROSSPLAYHANDLE playMusic(CROSSMUSIC* toPlay, unsigned char _passedChannel){
		#if SOUNDPLAYER == SND_SDL
			Mix_PlayMusic(toPlay,-1);
			return toPlay;
		#elif SOUNDPLAYER == SND_SOLOUD
			WavStream_setLooping(toPlay,1);
			return Soloud_play(mySoLoudEngine,toPlay);
		#elif SOUNDPLAYER == SND_3DS
			nathanPlayMusic(toPlay,_passedChannel);
			return toPlay->_musicChannel;
		#elif SOUNDPLAYER == SND_NONE
			return 1;
		#endif
	}
	void freeSound(CROSSSFX* toFree){
		#if SOUNDPLAYER == SND_SDL
			Mix_FreeChunk(toFree);
		#elif SOUNDPLAYER == SND_SOLOUD
			Wav_destroy(toFree);
		#elif SOUNDPLAYER == SND_3DS
			nathanFreeSound(toFree);
			free(toFree);
		#endif
	}
	void freeMusic(CROSSMUSIC* toFree){
		#if SOUNDPLAYER == SND_SDL
			Mix_FreeMusic(toFree);
		#elif SOUNDPLAYER == SND_SOLOUD
			WavStream_destroy(toFree);
		#elif SOUNDPLAYER == SND_3DS
			nathanFreeMusic(toFree);
			free(toFree);
		#endif
	}
#endif