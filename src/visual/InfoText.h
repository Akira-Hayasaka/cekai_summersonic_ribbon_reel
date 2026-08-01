#pragma once

#include "ofMain.h"
#include "ofxEasingFunc.h"
#include "Constants.h"
#include "Globals.h"

class InfoText {
public:
	InfoText() {
		// Use straight-alpha texture data and standard alpha blending.
		// This keeps fade transitions visually cleaner on most PNG assets.
		ofLoadImage(tex, "imgs/info.png");

		upperPos = glm::vec2(Constants::APP_W / 2, (tex.getHeight() / 2) * -1.20);
		lowerPos = glm::vec2(Constants::APP_W / 2, Constants::APP_H + tex.getHeight() / 2);
		centerPos = glm::vec2(Constants::APP_W / 2, Constants::APP_H / 2);
		currentPos = centerPos;
		currentPos.y += 40.0f;  // slight upward offset to avoid overlapping with headliner
    }

	void update() {
		if (state == State::in) {
			float elapsed = Globals::timeline->getCurrentTimeSec() - motionBeginTimeSec;
			float pct = ofClamp(elapsed / 1.0, 0.0f, 1.0f);
			float easing = ofxEasingFunc::Expo::easeOut(pct);
			currentPos = glm::mix(lowerPos, centerPos, easing);
			if (pct >= 1.0f) {
				state = State::idle;
			}
		} else if (state == State::out) {
			float elapsed = Globals::timeline->getCurrentTimeSec() - motionBeginTimeSec;
			float pct = ofClamp(elapsed / motionDurationSec, 0.0f, 1.0f);
			float easing = ofxEasingFunc::Cubic::easeOut(pct);
			currentPos = glm::mix(centerPos, upperPos, easing);
			if (pct >= 1.0f) {
				state = State::idle;
			}
		} else if (state == State::fade_in) {
			float elapsed = Globals::timeline->getCurrentTimeSec() - fadeBeginTimeSec;
			float pct = ofClamp(elapsed / fadeDurationSec, 0.0f, 1.0f);
			float easing = ofxEasingFunc::Cubic::easeOut(pct);
			alpha = ofLerp(0.0f, 255.0f, easing);
			currentPos = glm::mix(fadeBeginPos, centerPos, easing);
			if (pct >= 1.0f) {
				alpha = 255.0f;
				currentPos = centerPos;
				state = State::idle;
			}
		} else if (state == State::fade_out) {
			float elapsed = Globals::timeline->getCurrentTimeSec() - fadeBeginTimeSec;
			float pct = ofClamp(elapsed / 0.5, 0.0f, 1.0f);
			float easing = ofxEasingFunc::Cubic::easeOut(pct);
			alpha = ofLerp(255.0f, 0.0f, easing);
			const glm::vec2 fadeOutTargetPos = glm::vec2(centerPos.x, centerPos.y - 40.0f);
			currentPos = glm::mix(fadeBeginPos, fadeOutTargetPos, easing);
			if (pct >= 1.0f) {
				alpha = 0.0f;
				currentPos = fadeOutTargetPos;
				state = State::idle;
			}
		}
	}

	void draw() {
		ofPushStyle();

		ofSetColor(255, 255, 255, alpha);
		ofEnableBlendMode(OF_BLENDMODE_ALPHA);
		ofSetRectMode(OF_RECTMODE_CENTER);
		tex.draw(currentPos.x, currentPos.y);
		ofSetRectMode(OF_RECTMODE_CORNER);
		ofPopStyle();
	}

	void in() {
		currentPos = lowerPos;
		motionBeginTimeSec = Globals::timeline->getCurrentTimeSec();
		state = State::in;
	}

	void out() {
		currentPos = centerPos;
		motionBeginTimeSec = Globals::timeline->getCurrentTimeSec();
		state = State::out;
	}

	void fadeIn() {
		alpha = 0.0f;
		fadeBeginTimeSec = Globals::timeline->getCurrentTimeSec();
		fadeBeginPos = currentPos;
		state = State::fade_in;
	}

	void fadeOut() {
		alpha = 255.0f;
		fadeBeginTimeSec = Globals::timeline->getCurrentTimeSec();
		fadeBeginPos = currentPos;
		state = State::fade_out;
	}

private:

	enum struct State { idle, in, out, fade_in, fade_out };
	State state = State::idle;

    ofTexture tex;
	float motionBeginTimeSec = 0.0f;
	const float motionDurationSec = 0.25f;
	float fadeBeginTimeSec = 0.0f;
	const float fadeDurationSec = 0.6f;
    glm::vec2 fadeBeginPos = glm::vec2(0.0f, 0.0f);

	float alpha = 0.0f;

	glm::vec2 upperPos, lowerPos, centerPos, currentPos;
};
