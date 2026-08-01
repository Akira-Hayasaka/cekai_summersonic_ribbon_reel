#include "ofMain.h"
#include "ofApp.h"
#include "Globals.h"
#include "Constants.h"

//========================================================================
int main( )
{
	Constants::init();

	ofFile f("package.json");
	f >> Globals::package;

	ofFile ff("asset.json");
	ff >> Globals::asset;

	ofGLWindowSettings s;
	auto debug_scale = 0.0;

	if (Globals::package["full_scrn"].get<bool>()) {
		s.windowMode = OF_GAME_MODE;
	} else {
		debug_scale = 0.25;
		s.windowMode = OF_WINDOW;
	}

	s.setSize(Constants::APP_W, Constants::APP_H);
	s.setGLVersion(3, 2);
	ofCreateWindow(s);
	ofRunApp(new ofApp());
}
