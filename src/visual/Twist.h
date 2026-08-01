#pragma once

#include "ofMain.h"
#include "ofxAutoReloadedShader.h"
#include "ofxEasingFunc.h"
#include "ofxGui.h"
#include "Constants.h"
#include "Globals.h"

class Twist {
public:

	Twist() {
		shader.load("shaders/twist");
		quad.getVertices().resize(4);
		quad.getTexCoords().resize(4);
		quad.setMode(OF_PRIMITIVE_TRIANGLE_FAN);
		ofFboSettings fboSettings;
		fboSettings.width = Constants::APP_W;
		fboSettings.height = Constants::APP_H;
		fboSettings.internalformat = GL_RGBA;
		fboSettings.numSamples = 4; // 4x MSAA
		fbo.allocate(fboSettings);

		ofAddListener(Globals::sequencer->keyframeEvent, this, &Twist::on_SequencerKeyframeEvent);
		val2 = 0.0f;

		// // GUI initialization
		// gui.setup();
		// gui.add(val2.set("val2", 0.5, 0.0, 100.0));
	};

	void on_SequencerKeyframeEvent(SequencerKeyframeEvent & e) {
		if (e.trackName == "Twist") {
			if (e.keyframeName == "do") {
				animStartTime = Globals::timeline->getCurrentTimeSec();
				animActive = true;
			}
		}
	}

	void update() {
		if (!animActive) return;

		float elapsed = Globals::timeline->getCurrentTimeSec() - animStartTime;
		const float phase1Duration = 0.15f;
		const float phase2Duration = 2.0f;
		const float totalDuration = phase1Duration + phase2Duration;
		const float toVal = 20.0f;

		if (elapsed < phase1Duration) {
			float t = elapsed / phase1Duration;
			val2 = ofxEasingFunc::Sine::easeIn(t) * toVal;
		} else if (elapsed < totalDuration) {
			float t = (elapsed - phase1Duration) / phase2Duration;
			val2 = (1.0f - ofxEasingFunc::Sine::easeOut(t)) * toVal;
		} else {
			val2 = 0.0f;
			animActive = false;
		}
	};

	void draw() {
		shader.begin();
		shader.setUniformTexture("uTex", fbo.getTexture(), 0);
		shader.setUniform1f("timer", Globals::timeline->getCurrentTimeSec() * 50.0);
		shader.setUniform1f("intensity", val2);
		draw_quad(ofGetWidth(), ofGetHeight());
		shader.end();

		// gui.draw();
	};

	void begin() {
		fbo.begin();
		ofClear(0, 0, 0, 0);
	}

	void end() {
		fbo.end();
	}

	const bool isAnimating() const {
		return animActive;
	}

private:

	void draw_quad(const float _width, const float _height) {
		quad.setVertex(0, ofVec3f(0, 0, 0));
		quad.setVertex(1, ofVec3f(_width, 0, 0));
		quad.setVertex(2, ofVec3f(_width, _height, 0));
		quad.setVertex(3, ofVec3f(0, _height, 0));
		quad.setTexCoord(0, ofVec2f(0, 0));
		quad.setTexCoord(1, ofVec2f(_width, 0));
		quad.setTexCoord(2, ofVec2f(_width, _height));
		quad.setTexCoord(3, ofVec2f(0, _height));
		quad.draw();
	}

	ofxAutoReloadedShader shader;
	ofVboMesh quad;
	ofFbo fbo;
	ofxPanel gui;
	ofParameter<float> val1, val2, val3, val4;

	bool animActive = false;
	float animStartTime = 0.0f;

};
