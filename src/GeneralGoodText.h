#ifndef GENERALGOODTEXTHEADER
#define GENERALGOODTEXTHEADER
	// Text Stuff
	#if TEXTRENDERER == TEXT_FONTCACHE
		#include <SDL_FontCache.h>
		#define CrossFont FC_Font
	#elif TEXTRENDERER == TEXT_DEBUG
		#define CrossFont CrossTexture
	#elif TEXTRENDERER == TEXT_VITA2D
		#define CrossFont vita2d_font
	#endif

	CrossFont* fontImage=NULL;
	CrossFont* fontImage2=NULL;
	CrossFont* fontImage3=NULL;

	char bitmapFontWidth=14;
	char bitmapFontHeight=15;
	short bitmapFontLettersPerImage=73;

	#if TEXTRENDERER == TEXT_DEBUG
		float fontSize = 1;
	#endif
	#if TEXTRENDERER == TEXT_FONTCACHE
		//int fontSize = 20;
		int fontSize=50;
	#endif
	#if TEXTRENDERER == TEXT_VITA2D
		int fontSize=32;
	#endif

	void loadFont(char* filename){
		#if TEXTRENDERER == TEXT_DEBUG
			char _fixedPathBuffer[256];
			fixPath(filename,_fixedPathBuffer,TYPE_EMBEDDED);
			char _specificFontImageBuffer[strlen(_fixedPathBuffer)+1+1];
			sprintf(_specificFontImageBuffer, "%s%d", _fixedPathBuffer, 1);
			fontImage=loadPNG(_specificFontImageBuffer);
			sprintf(_specificFontImageBuffer, "%s%d", _fixedPathBuffer, 2);
			fontImage2=loadPNG(_specificFontImageBuffer);
			sprintf(_specificFontImageBuffer, "%s%d", _fixedPathBuffer, 3);
			fontImage3=loadPNG(_specificFontImageBuffer);
		#elif TEXTRENDERER == TEXT_FONTCACHE
			//fontSize = (SCREENHEIGHT-TEXTBOXY)/3.5;
			FC_FreeFont(fontImage);
			fontImage = NULL;
			fontImage = FC_CreateFont();
			FC_LoadFont(fontImage, mainWindowRenderer, filename, fontSize, FC_MakeColor(0,0,0,255), TTF_STYLE_NORMAL);
		#elif TEXTRENDERER == TEXT_VITA2D
			if (fontImage!=NULL){
				vita2d_free_font(fontImage);
				fontImage=NULL;
			}
			fontImage = vita2d_load_font_file(filename);
		#endif
	}

	int textHeight(float scale){
		#if TEXTRENDERER == TEXT_DEBUG
			return (bitmapFontHeight*scale);
		#elif TEXTRENDERER == TEXT_VITA2D
			return vita2d_font_text_height(fontImage,scale,"a");
		#elif TEXTRENDERER == TEXT_FONTCACHE
			return floor(FC_GetRealHeight(fontImage));
		#endif
	}

	// Please always use the same font size
	int textWidth(float scale, const char* message){
		#if TEXTRENDERER == TEXT_DEBUG
			return floor((bitmapFontWidth*scale)*strlen(message));
		#elif TEXTRENDERER == TEXT_VITA2D
			return vita2d_font_text_width(fontImage,scale,message);
		#elif TEXTRENDERER == TEXT_FONTCACHE
			return FC_GetWidth(fontImage,"%s",message);
		#endif
	}
	
	#if TEXTRENDERER == TEXT_DEBUG
		void drawLetter(int letterId, int _x, int _y, float size){
			letterId-=32;
			CrossFont* _tempFontToUse;
			if (letterId<bitmapFontLettersPerImage){
				_tempFontToUse = fontImage;
			}else if (letterId<bitmapFontLettersPerImage*2){
				_tempFontToUse = fontImage2;
				letterId-=bitmapFontLettersPerImage;
			}else{
				_tempFontToUse = fontImage3;
				letterId-=bitmapFontLettersPerImage*2;
			}
			drawTexturePartScale(_tempFontToUse,_x,_y,(letterId)*(bitmapFontWidth),0,bitmapFontWidth,bitmapFontHeight,size,size);
		}
		void drawLetterColor(int letterId, int _x, int _y, float size, unsigned char r, unsigned char g, unsigned char b){
			letterId-=32;
			CrossFont* _tempFontToUse;
			if (letterId<bitmapFontLettersPerImage){
				_tempFontToUse = fontImage;
			}else if (letterId<bitmapFontLettersPerImage*2){
				_tempFontToUse = fontImage2;
				letterId-=bitmapFontLettersPerImage;
			}else{
				_tempFontToUse = fontImage3;
				letterId-=bitmapFontLettersPerImage*2;
			}
			drawTexturePartScaleTint(_tempFontToUse,_x,_y,(letterId)*(bitmapFontWidth),0,bitmapFontWidth,bitmapFontHeight,size,size,r,g,b);
		}
	#endif	
	void goodDrawTextColored(int x, int y, const char* text, float size, unsigned char r, unsigned char g, unsigned char b){
		#if TEXTRENDERER == TEXT_VITA2D
			EASYFIXCOORDS(&x,&y);
			vita2d_font_draw_text(fontImage,x,y+textHeight(size), RGBA8(r,g,b,255),floor(size),text);
		#elif TEXTRENDERER == TEXT_DEBUG
			int i=0;
			int notICounter=0;
			for (i = 0; i < strlen(text); i++){
				drawLetterColor(text[i],(x+(notICounter*(bitmapFontWidth*size))),(y),size,r,g,b);
				notICounter++;
			}
		#elif TEXTRENDERER == TEXT_FONTCACHE
			EASYFIXCOORDS(&x,&y);
			SDL_Color _tempcolor;
			_tempcolor.r = r;
			_tempcolor.g = g;
			_tempcolor.b = b;
			_tempcolor.a = 255;
			FC_DrawColor(fontImage, mainWindowRenderer, x, y, _tempcolor ,"%s", text);
		#endif
	}
	void goodDrawText(int x, int y, const char* text, float size){
		#if TEXTRENDERER == TEXT_VITA2D
			EASYFIXCOORDS(&x,&y);
			vita2d_font_draw_text(fontImage,x,y+textHeight(size), RGBA8(255,255,255,255),floor(size),text);
		#elif TEXTRENDERER == TEXT_DEBUG
			int i=0;
			int notICounter=0;
			for (i = 0; i < strlen(text); i++){
				drawLetter(text[i],(x+(notICounter*(bitmapFontWidth*size))),(y),size);
				notICounter++;
			}
		#elif TEXTRENDERER == TEXT_FONTCACHE
			goodDrawTextColored(x,y,text,size,255,255,255);
		#endif
	}

#endif