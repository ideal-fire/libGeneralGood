#if TEXTRENDERER != TEXT_UNDEFINED
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

	#if TEXTRENDERER == TEXT_DEBUG
		typedef struct gfreughrue{
			int x;
			int y;
			int imageWidth;
			int imageHeight;
			int imageDisplayWidth;
		}BitmapFontLetter;
		
		BitmapFontLetter bitmapFontLetterInfo[95];
		unsigned short maxBitmapCharacterHeight = 12;

		void drawLetter(int letterId, int _x, int _y, float size){
			letterId-=32;
			drawTexturePartScale(fontImage,_x,_y+bitmapFontLetterInfo[letterId].y,bitmapFontLetterInfo[letterId].x,bitmapFontLetterInfo[letterId].y,bitmapFontLetterInfo[letterId].imageWidth,bitmapFontLetterInfo[letterId].imageHeight,size,size);
		}
		void drawLetterColor(int letterId, int _x, int _y, float size, unsigned char r, unsigned char g, unsigned char b){
			letterId-=32;
			drawTexturePartScaleTint(fontImage,_x,_y+bitmapFontLetterInfo[letterId].y,bitmapFontLetterInfo[letterId].x,bitmapFontLetterInfo[letterId].y,bitmapFontLetterInfo[letterId].imageWidth,bitmapFontLetterInfo[letterId].imageHeight,size,size,r,g,b);
		}
	#endif

	void loadFont(char* filename){
		#if TEXTRENDERER == TEXT_DEBUG
			// Load bitmap font image
			char _specificFontImageBuffer[strlen(filename)+5+1]; // filepath + extention + null
			sprintf(_specificFontImageBuffer, "%s%s", filename, ".png");
			fontImage=loadPNG(_specificFontImageBuffer);
			
			// Load font info
			sprintf(_specificFontImageBuffer, "%s%s", filename, ".info");
			FILE* fp = fopen(_specificFontImageBuffer,"r");
			int i;
			for (i=0;i<95;i++){
				fscanf(fp,"%d %d %d %d %d\n",&(bitmapFontLetterInfo[i].x),&(bitmapFontLetterInfo[i].y),&(bitmapFontLetterInfo[i].imageWidth),&(bitmapFontLetterInfo[i].imageHeight),&(bitmapFontLetterInfo[i].imageDisplayWidth));
				if (bitmapFontLetterInfo[i].imageHeight>maxBitmapCharacterHeight){
					maxBitmapCharacterHeight=bitmapFontLetterInfo[i].imageHeight;
				}
			}
			fclose(fp);
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
			return (maxBitmapCharacterHeight*scale);
		#elif TEXTRENDERER == TEXT_VITA2D
			return vita2d_font_text_height(fontImage,scale,"a");
		#elif TEXTRENDERER == TEXT_FONTCACHE
			return floor(FC_GetRealHeight(fontImage));
		#endif
	}

	// Please always use the same font size
	int textWidth(float scale, const char* message){
		#if TEXTRENDERER == TEXT_DEBUG
			int _currentWidth=0;
			int i;
			for (i=0;i<strlen(message);i++){
				_currentWidth+=(bitmapFontLetterInfo[message[i]-32].imageDisplayWidth)*scale;
			}
			return _currentWidth;
		#elif TEXTRENDERER == TEXT_VITA2D
			return vita2d_font_text_width(fontImage,scale,message);
		#elif TEXTRENDERER == TEXT_FONTCACHE
			return FC_GetWidth(fontImage,"%s",message);
		#endif
	}
	void goodDrawTextColored(int x, int y, const char* text, float size, unsigned char r, unsigned char g, unsigned char b){
		#if TEXTRENDERER == TEXT_VITA2D
			EASYFIXCOORDS(&x,&y);
			vita2d_font_draw_text(fontImage,x,y+textHeight(size), RGBA8(r,g,b,255),floor(size),text);
		#elif TEXTRENDERER == TEXT_DEBUG
			int i=0;
			int _currentDrawTextX=x;
			for (i = 0; i < strlen(text); i++){
				drawLetterColor(text[i],_currentDrawTextX,y,size,r,g,b);
				_currentDrawTextX+=bitmapFontLetterInfo[text[i]-32].imageDisplayWidth;
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
			int _currentDrawTextX=x;
			for (i = 0; i < strlen(text); i++){
				drawLetter(text[i],_currentDrawTextX,y,size);
				_currentDrawTextX+=bitmapFontLetterInfo[text[i]-32].imageDisplayWidth;
			}
		#elif TEXTRENDERER == TEXT_FONTCACHE
			goodDrawTextColored(x,y,text,size,255,255,255);
		#endif
	}

#endif
#endif