#pragma once

#include "ofMain.h"
#include "ofxAutoReloadedShader.h"
#include "Constants.h"
#include "Globals.h"
#include "Poster.h"

class EFFContainer {
public:
	EFFContainer(std::shared_ptr<Poster> poster_) {
		ofDisableArbTex();
		tex = poster_->getTexture();
		ofFboSettings fboSettings;
		fboSettings.width = Constants::APP_W;
		fboSettings.height = Constants::APP_H;
		fboSettings.internalformat = GL_RGBA;
		fboSettings.numSamples = 4; // 4x MSAA でポスター矩形のエッジを滑らかに
		fbo.allocate(fboSettings);
		shader.load("shaders/transition_2");
		quad.getVertices().resize(4);
		quad.getTexCoords().resize(4);
		quad.setMode(OF_PRIMITIVE_TRIANGLE_FAN);
		ofEnableArbTex();

		borntime = Globals::timeline->getCurrentTimeSec();
	}

	~EFFContainer() {
		tex->clear();
		fbo.clear();
		shader.unload();
	}

	void update() {
		auto diff = Globals::timeline->getCurrentTimeSec() - borntime;
		float scale = ofClamp(diff, 0.0f, scaleCycleSeconds) / scaleCycleSeconds;
		scale = ofxEasingFunc::Cubic::easeOut(scale);
		if (scale >= 1.0f) {
			scale = 1.0f;
			b_done = true;
		}

		fbo.begin();
		ofClear(0);
		ofSetRectMode(OF_RECTMODE_CENTER);
		tex->draw(Constants::APP_W / 2, Constants::APP_H / 2, Constants::APP_W * scale, Constants::APP_H * scale);
		ofSetRectMode(OF_RECTMODE_CORNER);
		fbo.end();
	}

	void draw() {
		//fbo.draw(0, 0);

		shader.begin();

		shader.setUniformTexture("uTex", fbo.getTexture(), 0);
		shader.setUniform2f("uResolution", Constants::APP_W, Constants::APP_H);
		shader.setUniform1f("uTime", Globals::timeline->getCurrentTimeSec());

		draw_quad(Constants::APP_W, Constants::APP_H);

		shader.end();
	}

	const bool isDone() const {
		return b_done;
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

	std::shared_ptr<ofTexture> tex;
	ofFbo fbo;
	ofVboMesh quad;
	ofxAutoReloadedShader shader;

	float scaleCycleSeconds = 4.0f;
	float borntime = 0.0f;
	bool b_done = false;
};

class Effect_01 {

public:
	Effect_01(std::vector<std::shared_ptr<Poster>> posters_, float scaleCycleSeconds_) {
		poster_textures = posters_;
	}

	void update() {
		for (auto & poster : posters) {
			poster->update();
		}
	}

	void draw() {
		// ポスターは古いものから順に描画
		for (const auto & poster : posters) {
			poster->draw();
		}
	}

	void next() {
		posters.push_back(std::make_shared<EFFContainer>(
			poster_textures[nextTextureIndex % poster_textures.size()]));
		++nextTextureIndex;
	}

	void reset() {
		posters.clear();
		nextTextureIndex = 0;
	}

private:
	std::vector<std::shared_ptr<Poster>> poster_textures;
	std::deque<std::shared_ptr<EFFContainer>> posters;
	const float spawnPosterSeconds = 1.0f;
	float lastSpawnPosterSeconds = 0.0f;
	int nextTextureIndex = 0; 
};
