#ifndef GENERALGOODGRAPHICSHEADER
#define GENERALGOODGRAPHICSHEADER

	#define WINDOWNAME "HappyWindo"

	#if DOFIXCOORDS == 1
		int fixX(int x);
		int fixY(int y);
		void FixCoords(int* _x, int* _y){
			*_x = fixX(*_x);
			*_y = fixY(*_y);
		}
		#define EASYFIXCOORDS(x, y) FixCoords(x,y)
	#else
		#define EASYFIXCOORDS(x,y)
	#endif

	// Renderer stuff
	#if RENDERER == REND_SDL
		#include <SDL2/SDL.h>
		#include <SDL2/SDL_image.h>
		//The window we'll be rendering to
		SDL_Window* mainWindow;
		
		//The window renderer
		SDL_Renderer* mainWindowRenderer;
	#endif
	#if RENDERER == REND_VITA2D
		#include <vita2d.h>
	#endif
	#if RENDERER == REND_SF2D
		#include <3ds.h>
		#include <stdio.h>
		#include <sf2d.h>
		#include <sfil.h>
		#include <3ds/svc.h>
	#endif

	// Used to fix touch coords on Android.
	// Init with dummy values in case used in division
	int _generalGoodRealScreenWidth=1;
	int _generalGoodRealScreenHeight=1;

	void setWindowTitle(char* _newTitle){
		#if RENDERER == REND_SDL
			SDL_SetWindowTitle(mainWindow,_newTitle);
		#else
			printf("Window title is now %s\n",_newTitle);
		#endif
	}

	// _windowWidth and _windowHeight are recommendations for the Window size. Will be ignored on Android, Vita, etc.
	void initGraphics(int _windowWidth, int _windowHeight, int* _storeWindowWidth, int* _storeWindowHeight){
		#if RENDERER == REND_SDL
			SDL_Init(SDL_INIT_VIDEO);
			mainWindowRenderer=NULL;
			// If platform is Android, make the window fullscreen and store the screen size in the arguments.
			#if SUBPLATFORM == SUB_ANDROID
				SDL_DisplayMode displayMode;
				if( SDL_GetCurrentDisplayMode( 0, &displayMode ) == 0 ){
					mainWindow = SDL_CreateWindow( WINDOWNAME, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, displayMode.w, displayMode.h, SDL_WINDOW_SHOWN );
				}else{
					printf("Failed to get display mode....\n");
				}
				*_storeWindowWidth=displayMode.w;
				*_storeWindowHeight=displayMode.h;
			#else
				#if PLATFORM == PLAT_SWITCH
					// For some reason, the other code doesn't work. I have to do it this way.
					SDL_CreateWindowAndRenderer(1280, 720, 0, &mainWindow, &mainWindowRenderer);
					*_storeWindowWidth=1280;
					*_storeWindowHeight=720;
					if (_windowWidth!=*_storeWindowWidth || _windowHeight!=*_storeWindowHeight){
						printf("Switch force window size.\n");
					}
				#else
					//#if PLATFORM == PLAT_COMPUTER
					//	if (*_storeWindowWidth!=0 && *_storeWindowHeight!=0){
					//		printf("Using actual window size %dx%d\n",*_storeWindowWidth,*_storeWindowHeight);
					//		_windowWidth=*_storeWindowWidth;
					//		_windowHeight=*_storeWindowHeight;
					//	}
					//#endif
					mainWindow = SDL_CreateWindow( WINDOWNAME, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, _windowWidth, _windowHeight, SDL_WINDOW_SHOWN );
					*_storeWindowWidth=_windowWidth;
					*_storeWindowHeight=_windowHeight;
					showErrorIfNull(mainWindow);
				#endif
			#endif
			if (mainWindowRenderer==NULL){
				int flags=SDL_RENDERER_ACCELERATED;
				if (USEVSYNC){
					flags|=SDL_RENDERER_PRESENTVSYNC;
				}
				mainWindowRenderer = SDL_CreateRenderer( mainWindow, -1, flags);
				showErrorIfNull(mainWindowRenderer);
			}
			IMG_Init( IMG_INIT_PNG );
			IMG_Init( IMG_INIT_JPG );
			SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
			SDL_SetRenderDrawBlendMode(mainWindowRenderer,SDL_BLENDMODE_BLEND);

			#if SUBPLATFORM == PLAT_COMPUTER
				// Set a solid white icon.
				SDL_Surface* tempIconSurface;
				Uint16* _surfacePixels = malloc(sizeof(Uint16)*16*16);
				char i, j;
				for (i=0;i<16;++i){
					for (j=0;j<16;++j){
						_surfacePixels[i*16+j]=0xffff;
					}
				}
				tempIconSurface = SDL_CreateRGBSurfaceFrom(_surfacePixels,16,16,16,16*2,0x0f00,0x00f0,0x000f,0xf000);
				SDL_SetWindowIcon(mainWindow, tempIconSurface);
				SDL_FreeSurface(tempIconSurface);
				free(_surfacePixels);
			#endif
			_generalGoodRealScreenWidth=*_storeWindowWidth;
			_generalGoodRealScreenHeight=*_storeWindowHeight;
		#elif RENDERER == REND_VITA2D
			vita2d_init();
			*_storeWindowWidth=960;
			*_storeWindowHeight=544;
			if (_windowWidth!=960 || _windowHeight!=544){
				printf("Vita force window size.\n");
			}
		#elif RENDERER == REND_SF2D
			sf2d_init();
			*_storeWindowWidth=400;
			*_storeWindowHeight=240;
			if (_windowWidth!=400 || _windowHeight!=240){
				printf("3ds force window size.\n");
			}
		#else
			#error Hi, Nathan here. I have to make graphics init function for this renderer. RENDERER
		#endif
	}

	void startDrawing(){
		#if RENDERER == REND_VITA2D
			vita2d_start_drawing();
			vita2d_clear_screen();
		#elif RENDERER == REND_SDL
			SDL_RenderClear(mainWindowRenderer);
		#elif RENDERER == REND_SF2D
			sf2d_start_frame(GFX_TOP, GFX_LEFT);
		#endif
	}
	
	void endDrawing(){
		#if RENDERER == REND_VITA2D
			vita2d_end_drawing();
			vita2d_swap_buffers();
			vita2d_wait_rendering_done();
		#elif RENDERER == REND_SDL
			SDL_RenderPresent(mainWindowRenderer);
		#elif RENDERER == REND_SF2D
			sf2d_end_frame();
			sf2d_swapbuffers();
		#endif
	}

	#if RENDERER == REND_SF2D
		void startDrawingBottom(){
			sf2d_start_frame(GFX_BOTTOM, GFX_LEFT);
		}
	#endif

	void quitGraphics(){
		#if RENDERER == REND_SF2D
			sf2d_fini();
		#elif RENDERER == REND_VITA2D
			// TODO - vita2d quit
		#elif RENDERER == REND_SDL
			// TODO - SDL quit
		#endif
	}
	
	/*
	/////////////////////////////////////////////////////
	////// CROSS PLATFORM DRAW FUNCTIONS
	////////////////////////////////////////////////////
	*/
	void setClearColor(int r, int g, int b, int a){
		if (a!=255){
			printf("You're a moron\n");
		}
		#if RENDERER == REND_SDL
			SDL_SetRenderDrawColor( mainWindowRenderer, r, g, b, a );
		#elif RENDERER == REND_VITA2D
			vita2d_set_clear_color(RGBA8(r, g, b, a));
		#elif RENDERER == REND_SF2D
			sf2d_set_clear_color(RGBA8(r, g, b, a));
		#endif
	}

	void drawRectangle(int x, int y, int w, int h, int r, int g, int b, int a){
		EASYFIXCOORDS(&x,&y);
		#if RENDERER == REND_VITA2D
			vita2d_draw_rectangle(x,y,w,h,RGBA8(r,g,b,a));
		#elif RENDERER == REND_SDL
			unsigned char oldr;
			unsigned char oldg;
			unsigned char oldb;
			unsigned char olda;
			SDL_GetRenderDrawColor(mainWindowRenderer,&oldr,&oldg,&oldb,&olda);
			SDL_SetRenderDrawColor(mainWindowRenderer,r,g,b,a);
			SDL_Rect tempRect;
			tempRect.x=x;
			tempRect.y=y;
			tempRect.w=w;
			tempRect.h=h;
			SDL_RenderFillRect(mainWindowRenderer,&tempRect);
			SDL_SetRenderDrawColor(mainWindowRenderer,oldr,oldg,oldb,olda);
		#elif RENDERER == REND_SF2D
			sf2d_draw_rectangle(x,y,w,h,RGBA8(r,g,b,a));
		#endif
	}

#endif
