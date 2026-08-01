#pragma once

#include "ofMain.h"
#include "ofxAutoReloadedShader.h"
#include "ofxEasingFunc.h"
#include "Constants.h"
#include "Globals.h"
#include "Transition.h"

// 歪ませたいテクスチャを受け取って、それを歪ませて、描画するだけのクラス
class Warper {
public:

	Warper() {
		ofDisableArbTex();
		shader.load("shaders/warper");
		quad.getVertices().resize(4);
		quad.getTexCoords().resize(4);
		quad.setMode(OF_PRIMITIVE_TRIANGLE_FAN);
		ofFboSettings fboSettings;
		fboSettings.width = Constants::APP_W;
		fboSettings.height = Constants::APP_H;
		fboSettings.internalformat = GL_RGBA;
		fboSettings.numSamples = 4; // 4x MSAA
		fbo.allocate(fboSettings);
		ofEnableArbTex();

		ofAddListener(Transition::first_movie_play_start_event, this, &Warper::on_first_movie_play_start_event);
	}

	void update() {
		if (distortion_anim_start_sec >= 0.0f) {
			const float elapsed = static_cast<float>(Globals::timeline->getCurrentTimeSec()) - distortion_anim_start_sec;
			if (elapsed < distortion_rise_duration) {
				// rise phase: 0.0 -> 1.0 over distortion_rise_duration (easeInSine)
				const float t = ofClamp(elapsed / distortion_rise_duration, 0.0f, 1.0f);
				distortionAmount = ofxEasingFunc::Sine::easeIn(t);
			} else {
				const float fade_elapsed = elapsed - distortion_anim_delay;
				if (fade_elapsed < 0.0f) {
					distortionAmount = 1.0f;
				} else {
					const float t = ofClamp(fade_elapsed / distortion_anim_duration, 0.0f, 1.0f);
					distortionAmount = 1.0f - ofxEasingFunc::Sine::easeOut(t);
					if (t >= 1.0f) distortion_anim_start_sec = -1.0f;
				}
			}
		}
	};

	void draw() {
		shader.begin();
		shader.setUniformTexture("uTex", fbo.getTexture(), 0);
		shader.setUniform2f("uResolution", Constants::APP_W, Constants::APP_H);
		shader.setUniform1f("uTime", Globals::timeline->getCurrentTimeSec());
		shader.setUniform1f("uDistortionAmount", distortionAmount);
		draw_quad(Constants::APP_W, Constants::APP_H);
		shader.end();
	}

	void begin() {
		fbo.begin();
		ofClear(0, 0, 0, 0);
	}

	void end() {
		fbo.end();
	}

private:

	void on_first_movie_play_start_event(float& delay) {
		distortion_anim_delay = delay;
		distortionAmount = 0.0f;
		distortion_anim_start_sec = static_cast<float>(Globals::timeline->getCurrentTimeSec());
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
	float distortionAmount = 1.0f;
	float distortion_anim_start_sec = -1.0f;
	float distortion_anim_delay = 0.5f;
	static constexpr float distortion_anim_duration = 0.25f;
	static constexpr float distortion_rise_duration = 0.07f;

	ofVboMesh quad;
	ofFbo fbo;
};
