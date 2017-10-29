// Depending on how you compile this on Windows, you may need DLL files.
#include "../Include/GeneralGoodConfig.h"
#include "../Include/GeneralGoodExtended.h"
#include "../Include/GeneralGood.h"
#include "../Include/GeneralGoodGraphics.h"
#include "../Include/GeneralGoodText.h"
#include "../Include/GeneralGoodImages.h"

// Because we're using SDL on Windows, main definition must be like this.
int main(int argc, char *argv[]){
	int screenWidth;
	int screenHeight;
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

	// Main game loop
	while (1){
		startDrawing();
		drawTexture(myHappyTexture,myImageX,myImageY);
		endDrawing();
		controlsStart();
		if (wasJustPressed(SCE_CTRL_START)){
			break;
		}
		if (isDown(SCE_CTRL_RIGHT)){
			myImageY++;
		}
		if (isDown(SCE_CTRL_LEFT)){
			myImageX--;
		}
		if (wasJustPressed(SCE_CTRL_UP)){
			myImageY--;
		}
		if (wasJustPressed(SCE_CTRL_DOWN)){
			myImageY++;
		}
		controlsEnd();

		wait(16); // Just to keep this from eating all your CPU.
	}

	/* code */
	return 0;
}