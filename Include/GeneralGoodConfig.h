#ifndef NATHANHAPPYCONFIG
#define NATHANHAPPYCONFIG
	#ifdef ISCOMPILINGLIBRARY
		extern char* ANDROIDPACKAGENAME;
		extern char* VITAAPPID;
	#else
		char* ANDROIDPACKAGENAME = "good.package.name";
		// 9 characters
		char* VITAAPPID = "123456789";
	#endif

	#define PLAT_VITA 1
	#define PLAT_COMPUTER 2
	#define PLAT_3DS 3
	#define PLAT_SWITCH 4
	
	#define SUB_NONE 0
	#define SUB_ANDROID 1
	#define SUB_UNIX 2
	#define SUB_WINDOWS 3
	
	#define SND_NONE 0
	#define SND_SDL 1
	#define SND_SOLOUD 2
	#define SND_3DS 3 // Only OGG supported
	#define SND_VITA 4 // OGG and MP3

	#define REND_UNDEFINED 0
	#define REND_SDL 1
	#define REND_VITA2D 2
	#define REND_SF2D 3
	
	#define TEXT_UNDEFINED 0
	#define TEXT_DEBUG 1
	#define TEXT_VITA2D 2
	#define TEXT_FONTCACHE 3

	// Avalible presets
	#define PRE_NONE 0
	#define PRE_COMPUTER 1
	#define PRE_VITA 2
	#define PRE_3DS 3
	#define PRE_ANDROID 4
	#define PRE_SWITCH 5

	//===============================
	// Auto platform
	//===============================
	#if _WIN32
		#define PRESET PRE_COMPUTER
		#define SUBPLATFORM SUB_WINDOWS
	#elif __ANDROID__
		#define PRESET PRE_ANDROID
		#define SUBPLATFORM SUB_ANDROID
	#elif __unix__
		#define PRESET PRE_COMPUTER
		#define SUBPLATFORM SUB_UNIX
	#elif __vita__
		#define PRESET PRE_VITA
	#elif _3DS
		#define PRESET PRE_3DS
	#elif __SWITCH__
		#define PRESET PRE_SWITCH
	#endif
	#ifndef PRESET
		#warning No preset defined. Will use manual settings.
	#endif

	//===============================
	// CHANGE THIS CODE TO CONFIGURE
	//===============================
	
	// These must be changed BEFORE compiling the library.
		// If the program will use uma0 for the data directory instead of ux0.
		#define USEUMA0 1
		// For some reason, I can't remember what exactly this does. Something for Android.
		#define DOFIXCOORDS 0
	// Support depends on renderer.
	#define USEVSYNC 1

	#ifdef FORCESDL
		#define RENDERER REND_SDL
		#define TEXTRENDERER TEXT_FONTCACHE
	#endif

	// Here, you can change the defaults for each platform.
	#if PRESET == PRE_COMPUTER
		#define PLATFORM PLAT_COMPUTER
		#define SOUNDPLAYER SND_SDL
		#ifndef RENDERER
			#define RENDERER REND_SDL
			#define TEXTRENDERER TEXT_FONTCACHE
		#endif
	#elif PRESET == PRE_VITA
		#define PLATFORM PLAT_VITA
		#define SOUNDPLAYER SND_VITA
		#ifndef RENDERER
			#define RENDERER REND_VITA2D
			#define TEXTRENDERER TEXT_VITA2D
		#endif
	#elif PRESET == PRE_3DS
		#define PLATFORM PLAT_3DS
		#define SOUNDPLAYER SND_3DS
		#define RENDERER REND_SF2D
		#define TEXTRENDERER TEXT_DEBUG
	#elif PRESET == PRE_ANDROID
		#define PLATFORM PLAT_COMPUTER
		#define SOUNDPLAYER SND_SDL
		#define RENDERER REND_SDL
		#define TEXTRENDERER TEXT_FONTCACHE
	#elif PRESET == PRE_SWITCH
		#define PLATFORM PLAT_SWITCH
		#define SOUNDPLAYER SND_SDL
		#define RENDERER REND_SDL
		#define TEXTRENDERER TEXT_FONTCACHE
	#else
		#warning Put custom stuff here
		// #define PLATFORM a
		// #define SUBPLATFORM a
		// #define SOUNDPLAYER a
		// #define RENDERER a
		// #define TEXTRENDERER a
	#endif

	#ifndef SUBPLATFORM
		#define SUBPLATFORM SUB_NONE
	#endif
#endif
