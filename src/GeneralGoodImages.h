#ifndef GENERALGOODIMAGESHEADER
#define GENERALGOODIMAGESHEADER
	// Renderer stuff
	#if RENDERER == REND_SDL
		#define CrossTexture SDL_Texture
		#include <SDL2/SDL.h>
		#include <SDL2/SDL_image.h>	
	#endif
	#if RENDERER == REND_VITA2D
		#define CrossTexture vita2d_texture
		#include <vita2d.h>
	#endif
	#if RENDERER == REND_SF2D
		#define CrossTexture sf2d_texture
		#include <3ds.h>
		#include <stdio.h>
		#include <sf2d.h>
		#include <sfil.h>
		#include <3ds/svc.h>
	#endif
	
	/*
	=================================================
	== IMAGES
	=================================================
	*/
	#if RENDERER == REND_SDL
		CrossTexture* surfaceToTexture(SDL_Surface* _tempSurface){
			// Real one we'll return
			SDL_Texture* _returnTexture;
			_returnTexture = SDL_CreateTextureFromSurface( mainWindowRenderer, _tempSurface );
			showErrorIfNull(_returnTexture);
			// Free memori
			SDL_FreeSurface(_tempSurface);
			return _returnTexture;
		}
	#endif
	CrossTexture* loadPNGBuffer(void* _passedBuffer, int _passedBufferSize){
		#if RENDERER==REND_VITA2D
			return vita2d_load_PNG_buffer(_passedBuffer);
		#elif RENDERER==REND_SDL
			SDL_RWops* _passedBufferRW = SDL_RWFromMem(_passedBuffer, _passedBufferSize);
			SDL_Surface* _tempSurface = IMG_LoadPNG_RW(_passedBufferRW);
			showErrorIfNull(_tempSurface);
			SDL_RWclose(_passedBufferRW);
			// Make good
			return surfaceToTexture(_tempSurface);
		#elif RENDERER==REND_SF2D
			return sfil_load_PNG_buffer(_passedBuffer,SF2D_PLACE_RAM);
		#endif
	}
	CrossTexture* loadJPGBuffer(void* _passedBuffer, int _passedBufferSize){
		#if RENDERER==REND_VITA2D
			return vita2d_load_JPEG_buffer(_passedBuffer,_passedBufferSize);
		#elif RENDERER==REND_SDL
			SDL_RWops* _passedBufferRW = SDL_RWFromMem(_passedBuffer, _passedBufferSize);
			SDL_Surface* _tempSurface = IMG_LoadJPG_RW(_passedBufferRW);
			showErrorIfNull(_tempSurface);
			SDL_RWclose(_passedBufferRW);
			// Make good
			return surfaceToTexture(_tempSurface);
		#elif RENDERER==REND_SF2D
			return sfil_load_JPEG_buffer(_passedBuffer,_passedBufferSize,SF2D_PLACE_RAM);
		#endif
	}
	CrossTexture* loadPNG(char* path){
		#if RENDERER==REND_VITA2D
			return vita2d_load_PNG_file(path);
		#elif RENDERER==REND_SDL
			SDL_Surface* _tempSurface = IMG_Load(path);
			showErrorIfNull(_tempSurface);
			// Make good
			return surfaceToTexture(_tempSurface);
		#elif RENDERER==REND_SF2D
			return sfil_load_PNG_file(path,SF2D_PLACE_RAM);
		#endif
	}
	CrossTexture* loadJPG(char* path){
		#if RENDERER==REND_VITA2D
			return vita2d_load_JPEG_file(path);
		#elif RENDERER==REND_SDL
			return loadPNG(path);
		#elif RENDERER==REND_SF2D
			return sfil_load_PNG_file(path,SF2D_PLACE_RAM);
		#endif
	}
	void freeTexture(CrossTexture* passedTexture){
		if (passedTexture==NULL){
			printf("Don't free NULL textures.");
			return;
		}
		#if RENDERER == REND_VITA2D
			vita2d_wait_rendering_done();
			sceDisplayWaitVblankStart();
			vita2d_free_texture(passedTexture);
			passedTexture=NULL;
		#elif RENDERER == REND_SDL
			SDL_DestroyTexture(passedTexture);
			passedTexture=NULL;
		#elif RENDERER == REND_SF2D
			sf2d_free_texture(passedTexture);
			passedTexture=NULL;
		#endif
	}
	/*
	/////////////////////////////////////////////////////
	////// CROSS PLATFORM DRAW FUNCTIONS
	////////////////////////////////////////////////////
	*/

	int getTextureWidth(CrossTexture* passedTexture){
		#if RENDERER == REND_VITA2D
			return vita2d_texture_get_width(passedTexture);
		#elif RENDERER == REND_SDL
			int w, h;
			SDL_QueryTexture(passedTexture, NULL, NULL, &w, &h);
			return w;
		#elif RENDERER == REND_SF2D
			return passedTexture->width;
		#endif
	}
	
	int getTextureHeight(CrossTexture* passedTexture){
		#if RENDERER == REND_VITA2D
			return vita2d_texture_get_height(passedTexture);
		#elif RENDERER == REND_SDL
			int w, h;
			SDL_QueryTexture(passedTexture, NULL, NULL, &w, &h);
			return h;
		#elif RENDERER == REND_SF2D
			return passedTexture->height;
		#endif
	}
	
	void drawTexture(CrossTexture* passedTexture, int _destX, int _destY){
		EASYFIXCOORDS(&_destX,&_destY);
		#if RENDERER == REND_VITA2D
			vita2d_draw_texture(passedTexture,_destX,_destY);
		#elif RENDERER == REND_SDL
			SDL_Rect _srcRect;
			SDL_Rect _destRect;
	
			SDL_QueryTexture(passedTexture, NULL, NULL, &(_srcRect.w), &(_srcRect.h));
	
			_destRect.w=_srcRect.w;
			_destRect.h=_srcRect.h;
	
			_destRect.x=_destX;
			_destRect.y=_destY;
			
			_srcRect.x=0;
			_srcRect.y=0;
		
			SDL_RenderCopy(mainWindowRenderer, passedTexture, &_srcRect, &_destRect );
		#elif RENDERER == REND_SF2D
			sf2d_draw_texture(passedTexture,_destX,_destY);
		#endif
	}

	void drawTexturePartScale(CrossTexture* passedTexture, int destX, int destY, int texX, int texY, int texW, int texH, float texXScale, float texYScale){
		EASYFIXCOORDS(&destX,&destY);
		#if RENDERER == REND_VITA2D
			vita2d_draw_texture_part_scale(passedTexture,destX,destY,texX,texY,texW, texH, texXScale, texYScale);
		#elif RENDERER == REND_SDL
			SDL_Rect _srcRect;
			SDL_Rect _destRect;
			_srcRect.w=texW;
			_srcRect.h=texH;
			
			_srcRect.x=texX;
			_srcRect.y=texY;
		
			_destRect.w=_srcRect.w*texXScale;
			_destRect.h=_srcRect.h*texYScale;
			//printf("Dest dimensionds is %dx%d;%.6f;%.6f\n",_destRect.w,_destRect.h,texXScale,texYScale);
			_destRect.x=destX;
			_destRect.y=destY;
	
			SDL_RenderCopy(mainWindowRenderer, passedTexture, &_srcRect, &_destRect );
		#elif RENDERER==REND_SF2D
			sf2d_draw_texture_part_scale(passedTexture,destX,destY,texX,texY,texW, texH, texXScale, texYScale);
		#endif
	}

	void drawTextureScaleTint(CrossTexture* passedTexture, int destX, int destY, float texXScale, float texYScale, unsigned char r, unsigned char g, unsigned char b){
		EASYFIXCOORDS(&destX,&destY);
		#if RENDERER == REND_VITA2D
			vita2d_draw_texture_tint_scale(passedTexture,destX,destY,texXScale,texYScale,RGBA8(r,g,b,255));
		#elif RENDERER == REND_SDL
			unsigned char oldr;
			unsigned char oldg;
			unsigned char oldb;
			SDL_GetTextureColorMod(passedTexture,&oldr,&oldg,&oldb);
			SDL_SetTextureColorMod(passedTexture, r,g,b);
			SDL_Rect _srcRect;
			SDL_Rect _destRect;
			SDL_QueryTexture(passedTexture, NULL, NULL, &(_srcRect.w), &(_srcRect.h));
			
			_srcRect.x=0;
			_srcRect.y=0;
		
			_destRect.w=(_srcRect.w*texXScale);
			_destRect.h=(_srcRect.h*texYScale);
	
			_destRect.x=destX;
			_destRect.y=destY;
	
			SDL_RenderCopy(mainWindowRenderer, passedTexture, &_srcRect, &_destRect );
			SDL_SetTextureColorMod(passedTexture, oldr, oldg, oldb);
		#elif RENDERER == REND_SF2D
			sf2d_draw_texture_scale_blend(passedTexture,destX,destY,texXScale,texYScale,RGBA8(r,g,b,255));
			
		#endif
	}

	void drawTexturePartScaleTint(CrossTexture* passedTexture, int destX, int destY, int texX, int texY, int texW, int texH, float texXScale, float texYScale, unsigned char r, unsigned char g, unsigned b){
		EASYFIXCOORDS(&destX,&destY);
		#if RENDERER == REND_VITA2D
			vita2d_draw_texture_tint_part_scale(passedTexture,destX,destY,texX,texY,texW, texH, texXScale, texYScale,RGBA8(r,g,b,255));
		#elif RENDERER == REND_SDL
			unsigned char oldr;
			unsigned char oldg;
			unsigned char oldb;
			SDL_GetTextureColorMod(passedTexture,&oldr,&oldg,&oldb);
			SDL_SetTextureColorMod(passedTexture, r,g,b);
			SDL_Rect _srcRect;
			SDL_Rect _destRect;
			_srcRect.w=texW;
			_srcRect.h=texH;
			
			_srcRect.x=texX;
			_srcRect.y=texY;
		
			_destRect.w=_srcRect.w*texXScale;
			_destRect.h=_srcRect.h*texYScale;
	
			_destRect.x=destX;
			_destRect.y=destY;
	
			SDL_RenderCopy(mainWindowRenderer, passedTexture, &_srcRect, &_destRect );
			SDL_SetTextureColorMod(passedTexture, oldr, oldg, oldb);
		#elif RENDERER==REND_SF2D
			sf2d_draw_texture_part_scale_blend(passedTexture,destX,destY,texX,texY,texW, texH, texXScale, texYScale, RGBA8(r,g,b,255));
		#endif
	}
	
	void drawTextureScale(CrossTexture* passedTexture, int destX, int destY, float texXScale, float texYScale){
		EASYFIXCOORDS(&destX,&destY);
		#if RENDERER == REND_VITA2D
			vita2d_draw_texture_scale(passedTexture,destX,destY,texXScale,texYScale);
		#elif RENDERER == REND_SDL
			SDL_Rect _srcRect;
			SDL_Rect _destRect;
			SDL_QueryTexture(passedTexture, NULL, NULL, &(_srcRect.w), &(_srcRect.h));
			
			_srcRect.x=0;
			_srcRect.y=0;
		
			_destRect.w=(_srcRect.w*texXScale);
			_destRect.h=(_srcRect.h*texYScale);
	
			_destRect.x=destX;
			_destRect.y=destY;
	
			SDL_RenderCopy(mainWindowRenderer, passedTexture, &_srcRect, &_destRect );
		#elif RENDERER == REND_SF2D
			sf2d_draw_texture_scale(passedTexture,destX,destY,texXScale,texYScale);
		#endif
	}

	void drawTextureScaleSize(CrossTexture* passedTexture, int destX, int destY, float texXScale, float texYScale){
		EASYFIXCOORDS(&destX,&destY);
		#if RENDERER == REND_VITA2D
			vita2d_draw_texture_scale(passedTexture,destX,destY,texXScale/(double)getTextureWidth(passedTexture),texYScale/(double)getTextureHeight(passedTexture));
		#elif RENDERER == REND_SDL
			SDL_Rect _srcRect;
			SDL_Rect _destRect;
			SDL_QueryTexture(passedTexture, NULL, NULL, &(_srcRect.w), &(_srcRect.h));
			
			_srcRect.x=0;
			_srcRect.y=0;
		
			_destRect.w=(texXScale);
			_destRect.h=(texYScale);
	
			_destRect.x=destX;
			_destRect.y=destY;
	
			SDL_RenderCopy(mainWindowRenderer, passedTexture, &_srcRect, &_destRect );
		#elif RENDERER == REND_SF2D
			sf2d_draw_texture_scale(passedTexture,destX,destY,texXScale,texYScale);
		#endif
	}
	
	// TODO MAKE ROTATE ON WINDOWS
	void drawTexturePartScaleRotate(CrossTexture* texture, int x, int y, float tex_x, float tex_y, float tex_w, float tex_h, float x_scale, float y_scale, float rad){
		EASYFIXCOORDS(&x,&y);
		#if RENDERER == REND_VITA2D
			vita2d_draw_texture_part_scale_rotate(texture,x,y,tex_x,tex_y,tex_w,tex_h,x_scale,y_scale,rad);
		#elif RENDERER == REND_SDL
			drawTexturePartScale(texture,x,y,tex_x,tex_y,tex_w,tex_h,x_scale,y_scale);
		#elif RENDERER == REND_SF2D
			sf2d_draw_texture_part_rotate_scale(texture,x,y,rad,tex_x,tex_y,tex_w,tex_h,x_scale,y_scale);
		#endif
	}
	void drawTextureAlpha(CrossTexture* passedTexture, int _destX, int _destY, unsigned char alpha){
		EASYFIXCOORDS(&_destX,&_destY);
		#if RENDERER == REND_VITA2D
			vita2d_draw_texture_tint(passedTexture,_destX,_destY,RGBA8(255,255,255,alpha));
		#elif RENDERER == REND_SDL
			unsigned char oldAlpha;
			SDL_GetTextureAlphaMod(passedTexture, &oldAlpha);
			SDL_SetTextureAlphaMod(passedTexture, alpha);
			SDL_Rect _srcRect;
			SDL_Rect _destRect;
	
			SDL_QueryTexture(passedTexture, NULL, NULL, &(_srcRect.w), &(_srcRect.h));
	
			_destRect.w=_srcRect.w;
			_destRect.h=_srcRect.h;
	
			_destRect.x=_destX;
			_destRect.y=_destY;
			
			_srcRect.x=0;
			_srcRect.y=0;
		
			SDL_RenderCopy(mainWindowRenderer, passedTexture, &_srcRect, &_destRect );
			SDL_SetTextureAlphaMod(passedTexture, oldAlpha);
		#elif RENDERER == REND_SF2D
			sf2d_draw_texture_blend(passedTexture,_destX,_destY,RGBA8(255,255,255,alpha));
		#endif
	}
	void drawTextureScaleAlpha(CrossTexture* passedTexture, int destX, int destY, float texXScale, float texYScale, unsigned char alpha){
		EASYFIXCOORDS(&destX,&destY);
		#if RENDERER == REND_VITA2D
			vita2d_draw_texture_tint_scale(passedTexture,destX,destY,texXScale,texYScale,RGBA8(255,255,255,alpha));
		#elif RENDERER == REND_SDL
			unsigned char oldAlpha;
			SDL_GetTextureAlphaMod(passedTexture, &oldAlpha);
			SDL_SetTextureAlphaMod(passedTexture, alpha);
			SDL_Rect _srcRect;
			SDL_Rect _destRect;
			SDL_QueryTexture(passedTexture, NULL, NULL, &(_srcRect.w), &(_srcRect.h));
			
			_srcRect.x=0;
			_srcRect.y=0;
		
			_destRect.w=(_srcRect.w*texXScale);
			_destRect.h=(_srcRect.h*texYScale);
	
			_destRect.x=destX;
			_destRect.y=destY;
	
			SDL_RenderCopy(mainWindowRenderer, passedTexture, &_srcRect, &_destRect );
			SDL_SetTextureAlphaMod(passedTexture, oldAlpha);
		#elif RENDERER == REND_SF2D
			sf2d_draw_texture_scale_blend(passedTexture,destX,destY,texXScale,texYScale,RGBA8(255,255,255,alpha));
		#endif
	}

#endif