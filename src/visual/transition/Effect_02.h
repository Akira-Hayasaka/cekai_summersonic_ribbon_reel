#pragma once

#include "Globals.h"
#include "Poster.h"
#include "ofMain.h"
#include "ofxAutoReloadedShader.h"
#include "ofxEasingFunc.h"
#include "ofxGui.h"

class EF02Container {
public:
	static constexpr float renderScale = 1.5f;

	EF02Container(std::shared_ptr<Poster> poster_, int initialSlotIndex)
		: slotIndex(initialSlotIndex) {
		ofDisableArbTex();
		tex = poster_->getTexture();
		ofEnableArbTex();
		position = glm::vec2(getCenterX(), slotToY(slotIndex));
		startPosition = position;
		targetPosition = position;
	}

	void beginSlide(const int rightMostSlotIndex) {
		jumpSlotIndex = rightMostSlotIndex;

		// Delay jump by one cycle: after reaching the top slot (0),
		// this cycle moves further up (to -1), then jumps at commit.
		if (slotIndex == 0) {
			nextSlotIndex = -1;
			shouldJumpOnCommit = true;
		} else {
			nextSlotIndex = slotIndex - 1;
			shouldJumpOnCommit = false;
		}

		startPosition = glm::vec2(getCenterX(), slotToY(slotIndex));
		targetPosition = glm::vec2(getCenterX(), slotToY(nextSlotIndex));
		position = startPosition;
	}

	void evaluateSlide(float easedProgress) {
		position = glm::mix(startPosition, targetPosition, easedProgress);
		position.x = getCenterX();
	}

	void commitSlide() {
		slotIndex = nextSlotIndex;
		position = targetPosition;
		position.x = getCenterX();

		if (shouldJumpOnCommit) {
			slotIndex = jumpSlotIndex;
			position = glm::vec2(getCenterX(), slotToY(slotIndex));
			startPosition = position;
			targetPosition = position;
			shouldJumpOnCommit = false;
		}
	}

	void draw() {
		if (!visible) return;
		tex->draw(position, drawWidth, drawHeight);
	}

	void setVisible(bool v) { visible = v; }
	int getSlotIndex() const { return slotIndex; }

private:
	bool visible = true;
	std::shared_ptr<ofTexture> tex;
	glm::vec2 position;
	glm::vec2 startPosition;
	glm::vec2 targetPosition;
	int slotIndex = 0;
	int nextSlotIndex = 0;
	int jumpSlotIndex = 0;
	bool shouldJumpOnCommit = false;
	static constexpr float drawWidth = 450.0f * renderScale;
	static constexpr float drawHeight = 631.5f * renderScale;
	static constexpr float containerGap = 10.0f * renderScale;
	static constexpr int centerSlot = 1; // Matches Effect_02::visibleSlots / 2; this slot is anchored to screen center

	float slotToY(const int slot) const {
		const float slotStep = drawHeight + containerGap;
		const float centerY = static_cast<float>(ofGetHeight()) * renderScale * 0.5f;
		return centerY - drawHeight * 0.5f + static_cast<float>(slot - centerSlot) * slotStep;
	}

	float getCenterX() const {
		return (static_cast<float>(ofGetWidth()) * renderScale - drawWidth) * 0.5f;
	}
};

class Effect_02 {
public:
	Effect_02(std::vector<std::shared_ptr<Poster>> posters_, float scaleCycleSeconds_) {
		poster_textures = std::move(posters_);

		for (const auto & poster : poster_textures) {
			ofLog() << "Poster year: " << poster->getYear();
		}

		ofDisableArbTex();
		shader.load("shaders/edgeWarp");
		quad.getVertices().resize(4);
		quad.getTexCoords().resize(4);
		quad.setMode(OF_PRIMITIVE_TRIANGLE_FAN);
		ofEnableArbTex();

		ofFbo::Settings fboSettings;
		fboSettings.width = static_cast<int>(std::round(static_cast<float>(ofGetWidth()) * EF02Container::renderScale));
		fboSettings.height = static_cast<int>(std::round(static_cast<float>(ofGetHeight()) * EF02Container::renderScale));
		fboSettings.internalformat = GL_RGBA;
		fboSettings.textureTarget = GL_TEXTURE_RECTANGLE_ARB;
		fboSettings.useDepth = false;
		fboSettings.useStencil = false;
		fboSettings.numSamples = 4; // 4x MSAA でポスターのエッジを滑らかに
		fbo.allocate(fboSettings);

		const int n = static_cast<int>(poster_textures.size());
		for (size_t i = 0; i < poster_textures.size(); ++i) {
			auto & tex = poster_textures[i];
			const int initialSlot = (static_cast<int>(i) + 1) % n;
			posters.push_back(std::make_shared<EF02Container>(tex, initialSlot));
		}

		cycleBeginSeconds = Globals::timeline->getCurrentTimeSec();

		params.setName("Transition 3");
		params.add(margin.set("Margin", 200.0f, 0.0f, 1000.0f));
		params.add(strengthLeft.set("Strength Left", 0.5f, -1.0f, 1.0f));
		params.add(strengthRight.set("Strength Right", 0.5f, -1.0f, 1.0f));
		params.add(edgeWidth.set("Edge Width", 0.2f, 0.01f, 0.5f));
		params.add(edgePower.set("Edge Power", 2.0f, 0.1f, 8.0f));
		params.add(centerY.set("Center Y", 0.5f, 0.0f, 1.0f));
		gui.setup(params);
		gui.loadFromFile("edgeWarp_settings.xml");
	}

	void update() {
		updateOutputScale();

		if (isSliding) {
			const float now = Globals::timeline->getCurrentTimeSec();
			float slideProgress = (now - cycleBeginSeconds) / slideDurationSeconds;
			slideProgress = ofClamp(slideProgress, 0.0f, 1.0f);
			const float eased = ofxEasingFunc::Expo::easeInOut(slideProgress);

			for (const auto & poster : posters) {
				poster->evaluateSlide(eased);
			}

			if (slideProgress >= 1.0f) {
				for (const auto & poster : posters) {
					poster->commitSlide();
				}
				isSliding = false;
			}
		}

		fbo.begin();
		ofClear(0, 0, 0, 0); // Transparent background (RGBA 0,0,0,0)
		for (const auto & poster : posters) {
			poster->draw();
		}
		fbo.end();
	}

	void next() {
		if (isSliding) return;
		hasStarted_ = true;
		beginCycle();
		cycleBeginSeconds = Globals::timeline->getCurrentTimeSec();
		isSliding = true;
	}

	void zoomin(const float durationSeconds = 0.2f) {
		scaleAnimStartSeconds = Globals::timeline->getCurrentTimeSec();
		scaleAnimFrom = minOutputScale;
		scaleAnimTo = maxOutputScale;
		scaleAnimDurationSeconds = std::max(durationSeconds, 0.0f);
		scaleAnimEaseIn = true;
		isScaleAnimating = true;
		outputScale = scaleAnimFrom;
	}

	void zoomout(const float durationSeconds = 0.2f) {
		scaleAnimStartSeconds = Globals::timeline->getCurrentTimeSec();
		scaleAnimFrom = maxOutputScale;
		scaleAnimTo = minOutputScale;
		scaleAnimDurationSeconds = std::max(durationSeconds, 0.0f);
		scaleAnimEaseIn = false;
		isScaleAnimating = true;
		outputScale = scaleAnimFrom;
	}

	void toOutputScale(const float durationSeconds = 0.25f) {
		hasStarted_ = true;
		scaleAnimStartSeconds = Globals::timeline->getCurrentTimeSec();
		scaleAnimFrom = 0.0f;
		scaleAnimTo = maxOutputScale;
		scaleAnimDurationSeconds = std::max(durationSeconds, 0.0f);
		scaleAnimEaseIn = true;
		isScaleAnimating = true;
		outputScale = scaleAnimFrom;
	}

	void hideNonCenter() {
		const int centerSlot = visibleSlots / 2;
		for (const auto & poster : posters) {
			poster->setVisible(poster->getSlotIndex() == centerSlot);
		}
	}

	void showAll() {
		for (const auto & poster : posters) {
			poster->setVisible(true);
		}
	}

	void reset() {
		hasStarted_ = false;
		isSliding = false;
		cycleBeginSeconds = Globals::timeline->getCurrentTimeSec();
		posters.clear();
		const int n = static_cast<int>(poster_textures.size());
		for (size_t i = 0; i < poster_textures.size(); ++i) {
			const int initialSlot = (static_cast<int>(i) + 1) % n;
			posters.push_back(std::make_shared<EF02Container>(poster_textures[i], initialSlot));
		}
	}

	void draw() {
		if (!hasStarted_) return;

		shader.begin();

		shader.setUniformTexture("uTex", fbo.getTexture(), 0);
		shader.setUniform2f("uTexSize", fbo.getWidth(), fbo.getHeight());
		shader.setUniform2f("uStrengthLR", strengthLeft, strengthRight);
		shader.setUniform1f("uEdgeWidth", edgeWidth);
		shader.setUniform1f("uEdgePower", edgePower);
		shader.setUniform1f("uCenterY", centerY);

		draw_quad(
			fbo.getWidth() * outputScale,
			fbo.getHeight() * outputScale);

		shader.end();
	}

private:
	void updateOutputScale() {
		if (!isScaleAnimating) return;

		if (scaleAnimDurationSeconds <= 0.0f) {
			outputScale = scaleAnimTo;
			isScaleAnimating = false;
			return;
		}

		const float elapsed = static_cast<float>(Globals::timeline->getCurrentTimeSec()) - scaleAnimStartSeconds;
		const float progress = ofClamp(elapsed / scaleAnimDurationSeconds, 0.0f, 1.0f);
		const float eased = scaleAnimEaseIn
			? ofxEasingFunc::Sine::easeIn(progress)
			: ofxEasingFunc::Sine::easeOut(progress);

		outputScale = ofLerp(scaleAnimFrom, scaleAnimTo, eased);

		if (progress >= 1.0f) {
			outputScale = scaleAnimTo;
			isScaleAnimating = false;
		}
	}

	void beginCycle() {
		const int rightMostSlotIndex = std::max(visibleSlots - 1, static_cast<int>(posters.size()) - 1);
		for (const auto & poster : posters) {
			poster->beginSlide(rightMostSlotIndex);
			poster->evaluateSlide(0.0f);
		}
	}

	void draw_quad(const float _width, const float _height) {
		const float destX = (static_cast<float>(ofGetWidth()) - _width) * 0.5f;
		const float destY = (static_cast<float>(ofGetHeight()) - _height) * 0.5f;

		quad.setVertex(0, ofVec3f(destX, destY, 0));
		quad.setVertex(1, ofVec3f(destX + _width, destY, 0));
		quad.setVertex(2, ofVec3f(destX + _width, destY + _height, 0));
		quad.setVertex(3, ofVec3f(destX, destY + _height, 0));
		quad.setTexCoord(0, ofVec2f(0, 0));
		quad.setTexCoord(1, ofVec2f(fbo.getWidth(), 0));
		quad.setTexCoord(2, ofVec2f(fbo.getWidth(), fbo.getHeight()));
		quad.setTexCoord(3, ofVec2f(0, fbo.getHeight()));
		quad.draw();
	}

	std::vector<std::shared_ptr<Poster>> poster_textures;
	std::vector<std::shared_ptr<EF02Container>> posters;
	const int visibleSlots = 3;
	const float slideDurationSeconds = 1.8f;
	const float minOutputScale = 0.75f;
	const float maxOutputScale = 1.35f;
	float outputScale = 1.0f;
	float scaleAnimDurationSeconds = 0.2f;
	float scaleAnimStartSeconds = 0.0f;
	float scaleAnimFrom = 1.0f;
	float scaleAnimTo = 1.0f;
	float cycleBeginSeconds = 0.0f;
	bool isSliding = false;
	bool isScaleAnimating = false;
	bool scaleAnimEaseIn = true;
	bool hasStarted_ = false;

	ofFbo fbo;
	ofxAutoReloadedShader shader;
	ofVboMesh quad;

	ofParameterGroup params;
	ofxPanel gui;

	ofParameter<float> margin;
	ofParameter<float> strengthLeft;
	ofParameter<float> strengthRight;
	ofParameter<float> edgeWidth;
	ofParameter<float> edgePower;
	ofParameter<float> centerY;
};
