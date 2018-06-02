// Depending on how you compile this on Windows, you may need DLL files.
#include "../Include/GeneralGoodConfig.h"
#include "../Include/GeneralGoodExtended.h"
#include "../Include/GeneralGood.h"
#include "../Include/GeneralGoodGraphics.h"
#include "../Include/GeneralGoodText.h"
#include "../Include/GeneralGoodImages.h"

// Because we're using SDL on Windows, main definition must be like this.
int main(int argc, char *argv[]){
	// libGeneralGood will function without calling this, but some things may not work.
	generalGoodInit();
	// May or may not need to initialize these to 0.
	int screenWidth=0;
	int screenHeight=0;
	// Make 640x480 window. If not on Windows, will use the real screen resolution and then store it in screenWidth and screenHeight. For example, if you compile this for PS Vita, screenWidth will be 960 and screenHeight will be 544.
	initGraphics(640,480, &screenWidth, &screenHeight);
	// Generate the path to the file "a.png" in the data folder.
	// On PS Vita, this will be ux0:data/VITAAPPID/a.png
	// On Windows, this will be ./a.png
	fixPath("a.png",tempPathFixBuffer,TYPE_DATA);
	// Load a new image using the file path we just generated
	CrossTexture* myHappyTexture = loadPNG(tempPathFixBuffer);
	int myImageX=0;
	int myImageY=0;
	char redRectangleEnabled=0;
	char pixelsPerFrame=4;
	// Main game loop
	while (appletMainLoop()){
		startDrawing();
		if (redRectangleEnabled){
			drawRectangle(32,32,100,100,255,0,0,255);
		}
		drawTexture(myHappyTexture,myImageX,myImageY);
		endDrawing();
		controlsStart();
		if (wasJustPressed(SCE_CTRL_START)){
			break;
		}
		if (wasJustPressed(SCE_CTRL_CROSS)){
			redRectangleEnabled=!redRectangleEnabled;
		}
		// You can hold down direction buttons to move
		if (isDown(SCE_CTRL_RIGHT)){
			myImageX+=pixelsPerFrame;
			if (myImageX+getTextureWidth(myHappyTexture)>screenWidth){
				myImageX=0;
			}
		}
		if (isDown(SCE_CTRL_LEFT)){
			myImageX-=pixelsPerFrame;
			if (myImageX<0){
				myImageX=screenWidth-getTextureWidth(myHappyTexture);
			}
		}
		if (isDown(SCE_CTRL_UP)){
			myImageY-=pixelsPerFrame;
			if (myImageY<0){
				myImageY=screenHeight-getTextureHeight(myHappyTexture);
			}
		}
		if (isDown(SCE_CTRL_DOWN)){
			myImageY+=pixelsPerFrame;
			if (myImageY+getTextureHeight(myHappyTexture)>screenHeight){
				myImageY=0;
			}
		}
		controlsEnd();
		wait(16); // Just to keep this from eating all your CPU.
	}

	// May need proper exit function here
	return 0;
}