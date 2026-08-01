#pragma once

#include "ofMain.h"
#include "ofxAutoReloadedShader.h"
#include "ofxEasingFunc.h"
#include "ofxGui.h"
#include "Constants.h"
#include "Globals.h"

class GradMix {
public:

	GradMix() {
		shader.load("shaders/gradmix");
		quad.getVertices().resize(4);
		quad.getTexCoords().resize(4);
		quad.setMode(OF_PRIMITIVE_TRIANGLE_FAN);

		// GUI initialization
		// gui.setup();
		// gui.add(uProgress.set("uProgress", 0.0f, 0.0f, 1.0f));
		// gui.add(uMaxBlur.set("uMaxBlur", 80.0f, 0.0f, 200.0f));
		// gui.add(uNoiseAmount.set("uNoiseAmount", 30.0f, 0.0f, 100.0f));

        uProgress = 0.0f;
        uMaxBlur = 200.0f;
        uNoiseAmount = 100.0f;

        ofFboSettings fboSettings;
        fboSettings.width = Constants::APP_W;
        fboSettings.height = Constants::APP_H;
        fboSettings.internalformat = GL_RGBA;
        fboSettings.numSamples = 4; // 4x MSAA
        lastScreenFbo.allocate(fboSettings);

        ofAddListener(Transition::first_movie_play_start_event, this, &GradMix::on_first_movie_play_start_event);
	};

	void update() {
		if (!isAnimating_) return;
		const float elapsed = Globals::timeline->getCurrentTimeSec() - animStartTime_;
		const float t = ofClamp(elapsed / animDuration_, 0.0f, 1.0f);
		uProgress = ofxEasingFunc::Expo::easeOut(t);
		if (t >= 1.0f) isAnimating_ = false;
	};

	bool isRunning() const { return isAnimating_; }

	void draw(ofTexture& b) {
		shader.begin();
		shader.setUniformTexture("uTexA", lastScreenFbo.getTexture(), 0);
		shader.setUniformTexture("uTexB", b, 1);
		shader.setUniform2f("uResolution", (float)ofGetWidth(), (float)ofGetHeight());
		shader.setUniform1f("uProgress", uProgress);
		shader.setUniform1f("uMaxBlur", uMaxBlur);
		shader.setUniform1f("uNoiseAmount", uNoiseAmount);
        shader.setUniform1f("uTime", Globals::timeline->getCurrentTimeSec() * 10.0f);
		draw_quad(ofGetWidth(), ofGetHeight());
		shader.end();

		// gui.draw();
	};

private:

	void on_first_movie_play_start_event(float& delay) {
		lastScreenFbo.begin();
		ofClear(0, 0, 0, 0);
		Globals::renderer->draw();
		lastScreenFbo.end();
		uProgress = 0.0f;
		animStartTime_ = Globals::timeline->getCurrentTimeSec();
		isAnimating_ = true;
	}

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
	ofxPanel gui;
	ofParameter<float> uProgress;
	ofParameter<float> uMaxBlur;
	ofParameter<float> uNoiseAmount;

    ofFbo lastScreenFbo;

	bool isAnimating_ = false;
	float animStartTime_ = 0.0f;
	static constexpr float animDuration_ = 1.35f;
};