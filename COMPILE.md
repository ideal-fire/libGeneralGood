Makefile is for PS Vita.
build.bat is for Windows.

---

Please edit Include/GeneralGoodConfig.h before compiling.
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

Things I've only tested on PS Vita:

* If RENDERER is REND_VITA2D, compile with libvita2d.
* If SOUNDPLAYER is SND_SOLOUD, compile with SoLoud.
* If TEXTRENDERER is TEXT_VITA2D, compile with libvita2d.

Things I've only tested on Windows:

* If RENDERER is REND_SDL, compile with SDL2.
* If TEXTRENDERER is TEXT_FONTCACHE, compile with SDL_FontCache and SDL_ttf.
* If SOUNDPLAYER is SND_SDL, compile with SDL_mixer.

---