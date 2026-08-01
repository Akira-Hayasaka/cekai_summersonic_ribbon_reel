#pragma once

#include "ofMain.h"
#include <cmath>
#include "Constants.h"
#include "Globals.h"
#include "ofxEasingFunc.h"
#include "ofxAutoReloadedShader.h"
#include "ofxGui.h"

class BG {
public:
	BG() {
		quad.getVertices().resize(4);
		quad.getTexCoords().resize(4);
		quad.setMode(OF_PRIMITIVE_TRIANGLE_FAN);
		shader.load("shaders/bg");

		color0 = ofColor::fromHex(0x0199E2);
		color1 = ofColor::fromHex(0x28287E);
		color2 = ofColor::fromHex(0xFDAB89);
		color3 = ofColor::fromHex(0xDB0C36);
		orig_color0 = color0;
		orig_color1 = color1;
		orig_color2 = color2;
		orig_color3 = color3;
		inv_color0 = color0.getInverted();
		inv_color1 = color1.getInverted();
		inv_color2 = color2.getInverted();
		inv_color3 = color3.getInverted();

		guiParams.setName("BG Shader");
		guiColor0.set("color0", ofFloatColor(color0), ofFloatColor(0.0f, 0.0f, 0.0f, 1.0f), ofFloatColor(1.0f, 1.0f, 1.0f, 1.0f));
		guiColor1.set("color1", ofFloatColor(color1), ofFloatColor(0.0f, 0.0f, 0.0f, 1.0f), ofFloatColor(1.0f, 1.0f, 1.0f, 1.0f));
		guiColor2.set("color2", ofFloatColor(color2), ofFloatColor(0.0f, 0.0f, 0.0f, 1.0f), ofFloatColor(1.0f, 1.0f, 1.0f, 1.0f));
		guiColor3.set("color3", ofFloatColor(color3), ofFloatColor(0.0f, 0.0f, 0.0f, 1.0f), ofFloatColor(1.0f, 1.0f, 1.0f, 1.0f));
		guiGradientRadius.set("gradientRadius", 0.25f, 0.01f, 1.00f);
		guiColorMass0.set("colorMass0", 1.0f, 0.0f, 4.0f);
		guiColorMass1.set("colorMass1", 1.0f, 0.0f, 4.0f);
		guiColorMass2.set("colorMass2", 1.0f, 0.0f, 4.0f);
		guiColorMass3.set("colorMass3", 1.0f, 0.0f, 4.0f);
		guiParams.add(guiColor0);
		guiParams.add(guiColor1);
		guiParams.add(guiColor2);
		guiParams.add(guiColor3);
		guiParams.add(guiGradientRadius);
		guiParams.add(guiColorMass0);
		guiParams.add(guiColorMass1);
		guiParams.add(guiColorMass2);
		guiParams.add(guiColorMass3);
		gui.setup(guiParams);
		gui.setPosition(20.0f, 20.0f);
		gui.loadFromFile("settings.xml");

		// Initial positions: place each control point at the centre of its zone
		// (vertical split at 65 % so color0/color1 occupy the upper portion).
		const float initSplitY = Constants::APP_H * 0.65f;
		p0 = glm::vec2(Constants::APP_W * 0.25f, initSplitY * 0.5f);
		p1 = glm::vec2(Constants::APP_W * 0.75f, initSplitY * 0.5f);
		p2 = glm::vec2(Constants::APP_W * 0.75f, initSplitY + (Constants::APP_H - initSplitY) * 0.5f);
		p3 = glm::vec2(Constants::APP_W * 0.25f, initSplitY + (Constants::APP_H - initSplitY) * 0.5f);

		state = State::idle;
		lastUpdateTimeSec = Globals::timeline->getCurrentTimeSec();
	}

	void spin() {
		beginStateTransition(State::spin);
	}

	void idle() {
		beginStateTransition(State::idle);
	}

	void setSpinSpeedPxPerSec(float speed) {
		spinSpeedPxPerSec = speed;
	}

	void setSpinWrapBounds(float topY, float bottomY) {
		spinWrapTopY = topY;
		spinWrapBottomY = bottomY;
	}

	void setSpinParams(float speed, float topY, float bottomY) {
		setSpinSpeedPxPerSec(speed);
		setSpinWrapBounds(topY, bottomY);
	}

	void update() {
		const float now = Globals::timeline->getCurrentTimeSec();
		float dt = now - lastUpdateTimeSec;
		lastUpdateTimeSec = now;
		dt = ofClamp(dt, 0.0f, 1.0f / 15.0f);

		const float t = now;
		const float freq = 0.9f;
		float centerX = Constants::APP_W / 2.0f;
		float centerY = Constants::APP_H / 2.0f;
		// color0/color1 (top blues) occupy the upper 65% of the screen;
		// color2/color3 (warm tones) are concentrated in the bottom 35%.
		float splitY = Constants::APP_H * 0.65f;
		glm::vec2 base0, base1, base2, base3;

		// p0: top-left quadrant
		base0.x = ofMap(ofNoise(t * freq, 0, PI), 0, 1, 0, centerX - padding);
		base0.y = ofMap(ofNoise(t * freq, 0, TWO_PI), 0, 1, 0, splitY);

		// p1: top-right quadrant
		base1.x = ofMap(ofNoise(t * freq, 1, PI), 0, 1, centerX + padding, Constants::APP_W);
		base1.y = ofMap(ofNoise(t * freq, 1, TWO_PI), 0, 1, 0, splitY);

		// p2: bottom-right quadrant
		base2.x = ofMap(ofNoise(t * freq, 2, PI), 0, 1, centerX + padding, Constants::APP_W);
		base2.y = ofMap(ofNoise(t * freq, 2, TWO_PI), 0, 1, splitY + padding, Constants::APP_H);

		// p3: bottom-left quadrant
		base3.x = ofMap(ofNoise(t * freq, 3, PI), 0, 1, 0, centerX - padding);
		base3.y = ofMap(ofNoise(t * freq, 3, TWO_PI), 0, 1, splitY + padding, Constants::APP_H);

		//// color0, color1, color2, color3を、それぞれのインバートカラーとの間でnoiseを使って変化させる
		//auto color_freq = 0.5f;
		//color0 = orig_color0.getLerped(inv_color0, ofNoise(t * color_freq, 4));
		//color1 = orig_color1.getLerped(inv_color1, ofNoise(t * color_freq, 5));
		//color2 = orig_color2.getLerped(inv_color2, ofNoise(t * color_freq, 6));
		//color3 = orig_color3.getLerped(inv_color3, ofNoise(t * color_freq, 7));

		//// rotate p0, p1, p2, p3 around the center of the screen
		//float angle = t * 20; // 回転速度
		//glm::mat2 rotationMatrix = glm::mat2(
		//	cos(glm::radians(angle)), -sin(glm::radians(angle)),
		//	sin(glm::radians(angle)), cos(glm::radians(angle)));
		//
		//const glm::vec2 center(Constants::APP_W / 2.0f, Constants::APP_H / 2.0f);
		//base0 = rotationMatrix * (base0 - center) + center;
		//base1 = rotationMatrix * (base1 - center) + center;
		//base2 = rotationMatrix * (base2 - center) + center;
		//base3 = rotationMatrix * (base3 - center) + center;

		//updateSpinBlend(now);
		//if (spinBlend > 0.0f) {
		//	spinYOffsetAccum -= spinSpeedPxPerSec * dt;
		//}

		p0 = applySpinY(base0);
		p1 = applySpinY(base1);
		p2 = applySpinY(base2);
		p3 = applySpinY(base3);
	}

	void draw() {
		ofFloatColor c0 = guiColor0.get();
		ofFloatColor c1 = guiColor1.get();
		ofFloatColor c2 = guiColor2.get();
		ofFloatColor c3 = guiColor3.get();
		color0 = ofColor(c0);
		color1 = ofColor(c1);
		color2 = ofColor(c2);
		color3 = ofColor(c3);

		shader.begin();
		shader.setUniform4f("color0", c0.r, c0.g, c0.b, c0.a);
		shader.setUniform4f("color1", c1.r, c1.g, c1.b, c1.a);
		shader.setUniform4f("color2", c2.r, c2.g, c2.b, c2.a);
		shader.setUniform4f("color3", c3.r, c3.g, c3.b, c3.a);
		shader.setUniform2f("p0", p0.x, p0.y);
		shader.setUniform2f("p1", p1.x, p1.y);
		shader.setUniform2f("p2", p2.x, p2.y);
		shader.setUniform2f("p3", p3.x, p3.y);
		shader.setUniform2f("resolution", Constants::APP_W, Constants::APP_H);
		shader.setUniform1f("gradientRadius", guiGradientRadius.get());
		shader.setUniform4f("colorMass", guiColorMass0.get(), guiColorMass1.get(), guiColorMass2.get(), guiColorMass3.get());
		draw_quad(Constants::APP_W, Constants::APP_H);
		shader.end();

		//gui.draw();

		////// debug draw p
		//ofPushStyle();
		//ofSetColor(color0);
		//ofDrawCircle(p0, 10);
		//ofNoFill();
		//ofSetColor(ofColor::black);
		//ofDrawCircle(p0, 10);
		//ofFill();
		//ofSetColor(color1);
		//ofDrawCircle(p1, 10);
		//ofNoFill();
		//ofSetColor(ofColor::black);
		//ofDrawCircle(p1, 10);
		//ofFill();
		//ofSetColor(color2);
		//ofDrawCircle(p2, 10);
		//ofNoFill();
		//ofSetColor(ofColor::black);
		//ofDrawCircle(p2, 10);
		//ofFill();
		//ofSetColor(color3);
		//ofDrawCircle(p3, 10);
		//ofNoFill();
		//ofSetColor(ofColor::black);
		//ofDrawCircle(p3, 10);
		//ofFill();
		//ofPopStyle();

		//// debug draw 四象限
		//float centerX = Constants::APP_W / 2.0f;
		//float splitY   = Constants::APP_H * 0.65f;
		//ofNoFill();
		//ofPushStyle();
		//ofSetColor(ofColor::black);
		//// 第一象限
		//ofDrawRectangle(0, 0, centerX - padding, splitY - padding);
		//// 第二象限
		//ofDrawRectangle(centerX + padding, 0, centerX - padding, splitY - padding);
		//// 第三象限
		//ofDrawRectangle(centerX + padding, splitY + padding, centerX - padding, Constants::APP_H - splitY - padding);
		//// 第四象限
		//ofDrawRectangle(0, splitY + padding, centerX - padding, Constants::APP_H - splitY - padding);
		//ofPopStyle();
		//ofFill();
	}

private:
	enum struct State { idle, spin };

	void beginStateTransition(State target) {
		if (targetState == target && isStateBlending) {
			return;
		}
		targetState = target;
		isStateBlending = true;
		stateBlendBeginSec = Globals::timeline->getCurrentTimeSec();
		stateBlendFrom = spinBlend;
		stateBlendTo = (target == State::spin) ? 1.0f : 0.0f;
		if (state == target && std::abs(spinBlend - stateBlendTo) < 1e-6f) {
			isStateBlending = false;
		}
	}

	void updateSpinBlend(float now) {
		if (!isStateBlending) {
			return;
		}
		float t = ofClamp((now - stateBlendBeginSec) / stateTransitionDurationSec, 0.0f, 1.0f);
		float eased = ofxEasingFunc::Cubic::easeOut(t);
		spinBlend = ofLerp(stateBlendFrom, stateBlendTo, eased);
		if (t >= 1.0f) {
			spinBlend = stateBlendTo;
			state = targetState;
			isStateBlending = false;
		}
	}

	glm::vec2 applySpinY(const glm::vec2& basePoint) const {
		if (spinBlend <= 0.0f) {
			return basePoint;
		}

		const float wrapSpan = spinWrapBottomY - spinWrapTopY;
		float spunY = basePoint.y + spinYOffsetAccum;

		if (wrapSpan > 0.0f) {
			// Keep cycling from bottom(C) to top(B) while moving upward.
			while (spunY <= spinWrapTopY) {
				spunY += wrapSpan;
			}
			while (spunY > spinWrapBottomY) {
				spunY -= wrapSpan;
			}
		}

		glm::vec2 outPoint = basePoint;
		outPoint.y = ofLerp(basePoint.y, spunY, spinBlend);
		return outPoint;
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

	ofColor color0, color1, color2, color3;
	ofColor orig_color0, orig_color1, orig_color2, orig_color3;
	ofColor inv_color0, inv_color1, inv_color2, inv_color3;

	ofxPanel gui;
	ofParameterGroup guiParams;
	ofParameter<ofFloatColor> guiColor0;
	ofParameter<ofFloatColor> guiColor1;
	ofParameter<ofFloatColor> guiColor2;
	ofParameter<ofFloatColor> guiColor3;
	ofParameter<float> guiGradientRadius;
	ofParameter<float> guiColorMass0;
	ofParameter<float> guiColorMass1;
	ofParameter<float> guiColorMass2;
	ofParameter<float> guiColorMass3;

	glm::vec2 p0, p1, p2, p3;

	State state = State::idle;
	State targetState = State::idle;
	bool isStateBlending = false;
	float stateBlendBeginSec = 0.0f;
	float stateBlendFrom = 0.0f;
	float stateBlendTo = 0.0f;
	float spinBlend = 0.0f;
	float lastUpdateTimeSec = 0.0f;
	float spinYOffsetAccum = 0.0f;
	float stateTransitionDurationSec = 0.25f;

	// Tunable spin parameters: A (speed), B (top wrap), C (bottom wrap).
	float spinSpeedPxPerSec = 1200.0f; // A
	float spinWrapTopY = -500.0f;     // B
	float spinWrapBottomY = static_cast<float>(Constants::APP_H) + 500.0f; // C

	float padding = 100;
};
