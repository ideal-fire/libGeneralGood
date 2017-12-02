Makefile.vita is for PS Vita.
build.bat is for Windows.
Makefile.3ds is for 3ds
Makefile.3ds.soft is for 3ds if you want sound.
buildlinux is for Linux

`make -f makefile_name_here`

To compile with sound support for 3ds, everything must be compiled with -mfloat-abi=softfp. If not, libogg breaks. Find compiled 3ds library in 3dsBuild folder.

Sometimes I break support for some platforms and just forget because I don't use them. Sorry.

---

You may want to edit Include/GeneralGoodConfig.h before compiling.
Find this section:
```
#if PRESET == PRE_COMPUTER // THIS IS THE CONFIGURATION FOR WINDOWS
	#define PLATFORM PLAT_COMPUTER
	#define SOUNDPLAYER SND_NONE
	#define RENDERER REND_SDL
	#define TEXTRENDERER TEXT_FONTCACHE
#elif PRESET == PRE_VITA // THIS IS THE CONFIGURATION FOR PS VITA
	#define PLATFORM PLAT_VITA
	#define SOUNDPLAYER SND_NONE
	#define RENDERER REND_VITA2D
	#define TEXTRENDERER TEXT_VITA2D
#endif
```

---

Things I've tested on 3ds:

* If RENDERER is REND_SF2D, compile libsf2d and libsfil
* If SOUNDPLAYER is SND_3DS, compile with libvorbis, libvorbisfile, and libogg
* If TEXTRENDERER is TEXT_DEBUG, you don't need any special libraries. Just make sure you know you're using a bitmap font.

Things I've only tested on PS Vita:

* If RENDERER is REND_VITA2D, compile with libvita2d.
* If SOUNDPLAYER is SND_SOLOUD, compile with SoLoud. If SOUNDPLAYER is SND_SDL, compile with SDL_mixer and SDL.
* If TEXTRENDERER is TEXT_VITA2D, compile with libvita2d.

Things I've only tested on Windows:

* If RENDERER is REND_SDL, compile with SDL2.
* If TEXTRENDERER is TEXT_FONTCACHE, compile with SDL_FontCache and SDL_ttf.
* If SOUNDPLAYER is SND_SDL, compile with SDL_mixer.

---