#pragma once

#include "ofMain.h"
#include "Globals.h"
#include "UI.h"
#include "Visual.h"
#include "Sequencer.h"

class ofApp : public ofBaseApp
{
public:

	void setup();
	void update();
	void draw();

	void keyPressed(int key);
	void keyReleased(int key);
	void mouseMoved(int x, int y );
	void mouseDragged(int x, int y, int button);
	void mousePressed(int x, int y, int button);
	void mouseReleased(int x, int y, int button);
	void mouseEntered(int x, int y);
	void mouseExited(int x, int y);
	void windowResized(int w, int h);
	void dragEvent(ofDragInfo dragInfo);
	void gotMessage(ofMessage msg);

private:
	static constexpr int RENDER_AUDIO_SAMPLE_RATE = 48000;
	static constexpr int RENDER_AUDIO_CHANNELS = 2;

	void startRendering();
	void stopRendering();
	void onSequencerEndReached(SequencerEndReachedEvent& e);

	std::unique_ptr<UI> ui;
	std::unique_ptr<Visual> visual;
	std::vector<float> render_audio_buffer;
	bool is_rendering = false;
};
