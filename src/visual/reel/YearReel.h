#pragma once

#include "ofMain.h"
#include "ofxEasingFunc.h"
#include "Constants.h"
#include "FontManager.h"
#include "TextObject.h"

class Year {
public:
	Year(std::shared_ptr<FontFace> year_font, const int year, float base_y) {
		value = year;
		baseY = base_y;
		float x = 550.0f;
		year_text.setFont(year_font);
		// prepend "0" if year is less than 10
		year_text.setText(year < 10 ? "0" + ofToString(year) : ofToString(year));
		year_text.setFontSize(350.09);
		year_text.setLineHeight(291.7);
		year_text.setLetterSpacing(-4.0);
		year_text.setAlign(TextAlign::Right);
		year_text.setColor(ofColor::white);
		year_text.rebuild();
		position = glm::vec3(x, baseY, 0.0f);
		year_text.setPosition(position);
	};

	void update() { }

	void draw() {
		year_text.draw();
	}

	int getValue() const { return value; }
	float getBaseY() const { return baseY; }
	void setBaseY(float y) { baseY = y; }
	void setY(float y) {
		position.y = y;
		year_text.setPosition(position);
	}
	float getY() const { return position.y; }
	void setAlpha(float a) {
		year_text.setColor(ofColor(255, 255, 255, static_cast<int>(ofClamp(a, 0.0f, 1.0f) * 255.0f)));
	}

private:
	int value = 0;
	float baseY = 0.0f;
	glm::vec3 position = { 0.0f, 0.0f, 0.0f };
	TextObject year_text;
};

class YearReel {
public:

	YearReel() {
		font_manager.setup();
		year_font = font_manager.loadFont(
			"year_font",
			ofToDataPath("fonts/Antonio-Regular.ttf", true), 0);
		twenty_text.setFont(year_font);
		twenty_text.setText("20");
		twenty_text.setFontSize(350.09);
		twenty_text.setLineHeight(291.7);
		twenty_text.setLetterSpacing(-4.0);
		twenty_text.setAlign(TextAlign::Right);
		twenty_text.setColor(ofColor::white);
		twenty_text.rebuild();
		// position to Y axis center, and a bit to the right of the left edge
		auto bbox = twenty_text.getBBox();
		float ty = Constants::APP_H / 2 + bbox.getHeight() / 2;
		targetCenterY = ty;
		twenty_text.setPosition(glm::vec3(290, ty, 0));

		int year_from = 00;
		int year_to = 25;
		for (int i = year_from; i <= year_to; i++) {
			years.push_back(std::make_unique<Year>(year_font, i, static_cast<float>(i) * yearSpacing));
		}

		idle_pos = glm::vec2(0, 2000);
		active_pos = glm::vec2(0, 0);
		out_pos = glm::vec2(0, -9000);
		cur_pos = idle_pos;

		state = State::idle;
		lastUpdateTimeSec = Globals::timeline->getCurrentTimeSec();

		ofAddListener(Globals::sequencer->keyframeEvent, this, &YearReel::on_SequencerKeyframeEvent);
	};

	void on_SequencerKeyframeEvent(SequencerKeyframeEvent& e)
	{
		if (e.trackName == "YearReel") {
			if (e.keyframeName == "in") {
				in();
			} else if (e.keyframeName == "spin") {
				spin();
			} else if (e.keyframeName == "stop") {
				int yearInt = Globals::package["target_year"][0].get<int>();
				// 例: 2005 -> 5, 2025 -> 25
				int yearTwoDigit = yearInt % 100;
				stop(yearTwoDigit);
			} else if (e.keyframeName == "stoplast") {
				auto lastidx = Globals::package["target_year"].size() - 1;
				int yearInt = Globals::package["target_year"][lastidx].get<int>();
				// 例: 2005 -> 5, 2025 -> 25
				int yearTwoDigit = yearInt % 100;
				stop(yearTwoDigit);
			} else if (e.keyframeName == "out") {
				out();
			} else if (e.keyframeName == "fadeout") {
				fadeout();
			} else if (e.keyframeName == "fadein") {
				fadein();
			}
		}
	}

	void update() {
		float now = Globals::timeline->getCurrentTimeSec();
		float dt = now - lastUpdateTimeSec;
		lastUpdateTimeSec = now;

		dt = ofClamp(dt, 0.0f, 1.0f / 15.0f);

		if (inAnimating) {
			updateIn(now);
		}
		if (outAnimating) {
			updateOut(now);
		}

		if (isSpinning) {
			updateSpinning(now, dt);
		}
		if (state == State::stop) {
			updateStopping(now);
		}

		layoutYearsFromOffset();

		updateFade(now);
		for (auto& year : years) {
			year->update();
		}
	}

	void draw() {
		ofPushMatrix();
		ofTranslate(cur_pos);
		for (auto & year : years) {
			year->draw();
		}
		twenty_text.draw();
		ofPopMatrix();
	}

	void in() {
		state = State::in;
		inSec = Globals::timeline->getCurrentTimeSec();
		inAnimating = true;
	}

	void out() {
		state = State::out;
		outSec = Globals::timeline->getCurrentTimeSec();
		outAnimating = true;
	}

	void spin() {
		spinStartTimeSec = Globals::timeline->getCurrentTimeSec();
		isSpinning = true;
	}

	void reset() {
		state                = State::idle;
		inAnimating          = false;
		outAnimating         = false;
		isSpinning           = false;
		reelOffsetPx         = 0.0f;
		currentSpeedPxPerSec = 0.0f;
		spinStartTimeSec     = 0.0f;
		stopStartOffsetPx    = 0.0f;
		stopEndOffsetPx      = 0.0f;
		cur_pos              = idle_pos;
		lastUpdateTimeSec    = Globals::timeline->getCurrentTimeSec();
		layoutYearsFromOffset();
	}

	void stop(const int tgt_year = 25) {
		int normalizedYear = tgt_year;
		if (normalizedYear >= 100) {
			normalizedYear = normalizedYear % 100;
		}

		Year* targetYear = nullptr;
		for (auto& y : years) {
			if (y->getValue() == normalizedYear) {
				targetYear = y.get();
				break;
			}
		}
		if (!targetYear) return;

		const float period = yearSpacing * static_cast<float>(years.size());
		const float minStopTravelPx = period * 0.25f;

		stopStartOffsetPx = reelOffsetPx;
		stopStartTimeSec = Globals::timeline->getCurrentTimeSec();

		float targetBaseY = targetYear->getBaseY();
		float targetAlignmentOffset = targetCenterY - targetBaseY;
		float delta = targetAlignmentOffset - reelOffsetPx;

		while (delta >= -minStopTravelPx) {
			delta -= period;
		}
		delta -= period * static_cast<float>(extraStopLoops);

		stopEndOffsetPx = reelOffsetPx + delta;

		isSpinning = false;
		state = State::stop;
	}

	void fadeout() {
		fadeStartSec = Globals::timeline->getCurrentTimeSec();
		fadeFromAlpha = fadeAlpha;
		isFadingOut = true;
		isFadingIn = false;
	}

	void fadein() {
		fadeStartSec = Globals::timeline->getCurrentTimeSec();
		fadeFromAlpha = fadeAlpha;
		isFadingIn = true;
		isFadingOut = false;
	}

private:

	enum struct State { idle, in, stop, out };
	State state = State::idle;
	bool isSpinning = false;

	std::vector<std::unique_ptr<Year>> years;

	FontManager font_manager;
	TextObject twenty_text;
	std::shared_ptr<FontFace> year_font;

	static constexpr float yearSpacing = 340.0f;

	float inSec = 0.0f;
	float inDurationSec = 0.6f;
	bool inAnimating = false;

	float outSec = 0.0f;
	float outDurationSec = 1.0f;
	bool outAnimating = false;

	float reelOffsetPx = 0.0f;
	float lastUpdateTimeSec = 0.0f;
	float targetCenterY = 0.0f;
	float currentSpeedPxPerSec = 0.0f;

	float spinStartTimeSec = 0.0f;
	float spinAccelDurationSec = 0.5f;
	float spinMaxSpeedPxPerSec = 9000.0f;

	float stopStartTimeSec = 0.0f;
	float stopDurationSec = 0.9f;
	float stopStartOffsetPx = 0.0f;
	float stopEndOffsetPx = 0.0f;
	int extraStopLoops = 1;

	glm::vec2 idle_pos, active_pos, out_pos, cur_pos;

	float fadeAlpha = 1.0f;
	float fadeFromAlpha = 1.0f;
	float fadeStartSec = 0.0f;
	bool isFadingOut = false;
	bool isFadingIn = false;
	static constexpr float fadeDurationSec = 0.25f;

	void updateFade(float now) {
		if (isFadingOut) {
			const float t = ofClamp((now - fadeStartSec) / fadeDurationSec, 0.0f, 1.0f);
			fadeAlpha = ofLerp(fadeFromAlpha, 0.0f, ofxEasingFunc::Sine::easeOut(t));
			if (t >= 1.0f) { fadeAlpha = 0.0f; isFadingOut = false; }
		} else if (isFadingIn) {
			const float t = ofClamp((now - fadeStartSec) / fadeDurationSec, 0.0f, 1.0f);
			fadeAlpha = ofLerp(fadeFromAlpha, 1.0f, ofxEasingFunc::Sine::easeIn(t));
			if (t >= 1.0f) { fadeAlpha = 1.0f; isFadingIn = false; }
		}
		const unsigned char a = static_cast<unsigned char>(ofClamp(fadeAlpha, 0.0f, 1.0f) * 255.0f);
		for (auto& year : years) {
			year->setAlpha(fadeAlpha);
		}
		twenty_text.setColor(ofColor(255, 255, 255, a));
	}

	void updateIn(float now) {
		float t = ofClamp((now - inSec) / inDurationSec, 0.0f, 1.0f);
		float eased = ofxEasingFunc::Cubic::easeOut(t);
		cur_pos = idle_pos + (active_pos - idle_pos) * eased;
		if (t >= 1.0f) {
			cur_pos = active_pos;
			inAnimating = false;
			if (state == State::in) {
				state = State::idle;
			}
		}
	}

	void updateOut(float now) {
		float t = ofClamp((now - outSec) / outDurationSec, 0.0f, 1.0f);
		float eased = ofxEasingFunc::Cubic::easeIn(t);
		cur_pos = active_pos + (out_pos - active_pos) * eased;
		if (t >= 1.0f) {
			cur_pos = out_pos;
			outAnimating = false;
			if (state == State::out) {
				state = State::idle;
			}
		}
	}

	void updateSpinning(float now, float dt) {
		float t = ofClamp((now - spinStartTimeSec) / spinAccelDurationSec, 0.0f, 1.0f);
		float eased = ofxEasingFunc::Cubic::easeInOut(t);
		currentSpeedPxPerSec = spinMaxSpeedPxPerSec * eased;
		reelOffsetPx -= currentSpeedPxPerSec * dt;
	}

	void updateStopping(float now) {
		float t = ofClamp((now - stopStartTimeSec) / stopDurationSec, 0.0f, 1.0f);
		float eased = ofxEasingFunc::Cubic::easeOut(t);
		reelOffsetPx = stopStartOffsetPx + (stopEndOffsetPx - stopStartOffsetPx) * eased;
		if (t >= 1.0f) {
			reelOffsetPx = stopEndOffsetPx;
			layoutYearsFromOffset();
			state = State::idle;
		}
	}

	void layoutYearsFromOffset() {
		const float period = yearSpacing * static_cast<float>(years.size());
		const float wrapTopY = -yearSpacing;
		for (auto& year : years) {
			float raw = year->getBaseY() + reelOffsetPx - wrapTopY;
			float wrapped = std::fmod(std::fmod(raw, period) + period, period);
			year->setY(wrapTopY + wrapped);
		}
	}
};
