#pragma once

#include "ofMain.h"
#include <limits>

#include "Constants.h"
#include "Globals.h"
#include "Poster.h"
#include "ofxEasingFunc.h"

class PosterImg {
public:

	PosterImg(std::shared_ptr<Poster> poster, const int year, float base_y) {
		this->poster = std::move(poster);
		value = year;
		baseY = base_y;
		float x = 575.0f;
		position = glm::vec3(x, baseY, 0.0f);
	};

	void update() { }

	void draw() {
		if (!visible) return;
		poster->draw(position.x, position.y, width, height);
	}

	int getValue() const { return value; }
	float getBaseY() const { return baseY; }
	void setBaseY(float y) { baseY = y; }
	void setY(float y) {
		position.y = y;
	}
	float getY() const { return position.y; }
	const float getWidth() const { return width; }
	const float getHeight() const { return height; }
	void setVisible(bool v) { visible = v; }
	bool isVisible() const { return visible; }

private:
	std::shared_ptr<Poster> poster;
	bool visible = true;
	int value = 0;
	float baseY = 0.0f;
	glm::vec3 position = { 0.0f, 0.0f, 0.0f };
	const float width = 468.0f;
	const float height = 661.47f;
};

class PosterReel {
public:

	PosterReel(std::vector<std::shared_ptr<Poster>> posters)
		: posters(std::move(posters))
	{
		// ポスターを昇順でソート
		std::sort(this->posters.begin(), this->posters.end(),
			[](const std::shared_ptr<Poster> & a, const std::shared_ptr<Poster> & b) {
				return a->getYear() < b->getYear();
			});
		for (auto i = 0; i < this->posters.size(); ++i) {
			int year = std::stoi(this->posters[i]->getYear());
			float base_y = i * spacing;
			poster_imgs.emplace_back(this->posters[i], year, base_y);
		}

		if (!poster_imgs.empty()) {
			targetCenterY = Constants::APP_H / 2.0f - poster_imgs[0].getHeight() / 2.0f;
		}
		idle_pos = glm::vec2(0, 2200);
		active_pos = glm::vec2(0, 0);
		out_pos = glm::vec2(0, -18000);
		cur_pos = idle_pos;
		lastUpdateTimeSec = Globals::timeline->getCurrentTimeSec();

		ofAddListener(Globals::sequencer->keyframeEvent, this, &PosterReel::on_SequencerKeyframeEvent);
	}

	void on_SequencerKeyframeEvent(SequencerKeyframeEvent & e) {
		if (e.trackName == "PosterReel") {
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
			} 
		}
	}

	void update() {
		float now = Globals::timeline->getCurrentTimeSec();
		float dt = now - lastUpdateTimeSec;
		lastUpdateTimeSec = now;

		dt = ofClamp(dt, 0.0f, 1.0f / 15.0f);
		lastFrameDtSec = dt;

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

		if (inAnimating && isSpinning) {
			const float dOffset = std::isfinite(debugPrevEffectiveOffset)
				? std::abs(debugEffectiveOffset - debugPrevEffectiveOffset)
				: 0.0f;
			const float dCurY = debugHasPrevCurPos ? std::abs(cur_pos.y - debugPrevCurPos.y) : 0.0f;
			const bool shouldLog =
				(debugLastLogTimeSec < 0.0f) ||
				((now - debugLastLogTimeSec) >= 0.05f) ||
				(dOffset > 40.0f) ||
				(dCurY > 20.0f);
			if (shouldLog) {
				ofLogWarning("PosterReelDBG")
					<< "time=" << now
					<< " dt=" << dt
					<< " inBlend=" << inSpinBlend
					<< " reelOffset=" << reelOffsetPx
					<< " baseOffset=" << spinBlendBaseOffset
					<< " effectiveOffset=" << debugEffectiveOffset
					<< " dEffectiveOffset=" << dOffset
					<< " curPosY=" << cur_pos.y
					<< " dCurPosY=" << dCurY;
				debugLastLogTimeSec = now;
			}
			debugPrevEffectiveOffset = debugEffectiveOffset;
			debugPrevCurPos = cur_pos;
			debugHasPrevCurPos = true;
		}

		for (auto& img : poster_imgs) {
			img.update();
		}
	}

	void draw() {
		ofPushMatrix();
		ofTranslate(cur_pos);
		for (auto& img : poster_imgs) {
			img.draw();
		}
		ofPopMatrix();
	}

	void in() {
		state = State::in;
		inSec = Globals::timeline->getCurrentTimeSec();
		inAnimating = true;
		inSpinBlend = 0.0f;
	}

	void out() {
		state = State::out;
		outSec = Globals::timeline->getCurrentTimeSec();
		outAnimating = true;
	}

	void spin() {
		spinBlendBaseOffset = reelOffsetPx;
		spinStartTimeSec = Globals::timeline->getCurrentTimeSec();
		isSpinning = true;
	}

	void reset() {
		state                = State::idle;
		inAnimating          = false;
		outAnimating         = false;
		isSpinning           = false;
		reelOffsetPx         = 0.0f;
		inSpinBlend          = 1.0f;
		spinBlendBaseOffset  = 0.0f;
		debugEffectiveOffset = 0.0f;
		visualEffectiveOffset = 0.0f;
		debugPrevEffectiveOffset = std::numeric_limits<float>::quiet_NaN();
		debugLastLogTimeSec = -1.0f;
		debugHasPrevCurPos = false;
		currentSpeedPxPerSec = 0.0f;
		spinStartTimeSec     = 0.0f;
		stopStartOffsetPx    = 0.0f;
		stopEndOffsetPx      = 0.0f;
		cur_pos              = idle_pos;
		lastUpdateTimeSec    = Globals::timeline->getCurrentTimeSec();
		showAllPosters();
		layoutYearsFromOffset();
	}

	void stop(const int tgt_year = 25) {
		const int searchYear = normalizeTargetYear(tgt_year);

		PosterImg* target = nullptr;
		for (auto& img : poster_imgs) {
			if (img.getValue() == searchYear) {
				target = &img;
				break;
			}
		}
		if (!target) return;

		const float period = spacing * static_cast<float>(poster_imgs.size());
		const float minStopTravelPx = period * 0.25f;

		stopStartOffsetPx = reelOffsetPx;
		stopStartTimeSec = Globals::timeline->getCurrentTimeSec();

		float delta = (targetCenterY - target->getBaseY()) - reelOffsetPx;
		while (delta >= -minStopTravelPx) {
			delta -= period;
		}
		delta -= period * static_cast<float>(extraStopLoops);

		stopEndOffsetPx = reelOffsetPx + delta;

		isSpinning = false;
		state = State::stop;
	}

	void showOnlyTargetPoster(const int tgt_year = 0) {
		const int searchYear = normalizeTargetYear(tgt_year);
		for (auto & img : poster_imgs) {
			img.setVisible(img.getValue() == searchYear);
		}
	}

	void showAllPosters() {
		for (auto & img : poster_imgs) {
			img.setVisible(true);
		}
	}

private:
	int normalizeTargetYear(const int tgt_year) const {
		// Accept 2-digit (00-99) or 4-digit (2000-2099) years.
		return tgt_year < 100 ? 2000 + tgt_year : tgt_year;
	}

	enum struct State { idle, in, stop, out };
	State state = State::idle;
	bool isSpinning = false;
	std::vector<std::shared_ptr<Poster>> posters;
	std::vector<PosterImg> poster_imgs;

	static constexpr float spacing = 680.47f;

	float inSec = 0.0f;
	float inDurationSec = 0.6f;
	bool inAnimating = false;
	float inSpinBlend = 1.0f;

	float outSec = 0.0f;
	float outDurationSec = 1.0f;
	bool outAnimating = false;

	float reelOffsetPx = 0.0f;
	float spinBlendBaseOffset = 0.0f;
	float lastUpdateTimeSec = 0.0f;
	float targetCenterY = 0.0f;
	float currentSpeedPxPerSec = 0.0f;

	float spinStartTimeSec = 0.0f;
	float spinAccelDurationSec = 0.5f;
	float spinMaxSpeedPxPerSec = 9000.0f;
	float maxInSpinBlendSpeedPxPerSec = 2400.0f;
	float lastFrameDtSec = 1.0f / 60.0f;

	float debugEffectiveOffset = 0.0f;
	float visualEffectiveOffset = 0.0f;
	float debugPrevEffectiveOffset = std::numeric_limits<float>::quiet_NaN();
	float debugLastLogTimeSec = -1.0f;
	glm::vec2 debugPrevCurPos = glm::vec2(0.0f, 0.0f);
	bool debugHasPrevCurPos = false;

	float stopStartTimeSec = 0.0f;
	float stopDurationSec = 1.9f;
	float stopStartOffsetPx = 0.0f;
	float stopEndOffsetPx = 0.0f;
	int extraStopLoops = 1;

	glm::vec2 idle_pos, active_pos, out_pos, cur_pos;

	void updateIn(float now) {
		float t = ofClamp((now - inSec) / inDurationSec, 0.0f, 1.0f);
		float eased = ofxEasingFunc::Cubic::easeOut(t);
		cur_pos = idle_pos + (active_pos - idle_pos) * eased;
		inSpinBlend = eased;
		if (t >= 1.0f) {
			cur_pos = active_pos;
			inAnimating = false;
			inSpinBlend = 1.0f;
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
		const float fixedStepSec = 1.0f / 240.0f;
		const float clampedDt = ofClamp(dt, 0.0f, 1.0f / 15.0f);
		float remaining = clampedDt;
		float simNow = now - clampedDt;

		while (remaining > 0.0f) {
			const float step = std::min(remaining, fixedStepSec);
			simNow += step;

			const float t = ofClamp((simNow - spinStartTimeSec) / spinAccelDurationSec, 0.0f, 1.0f);
			const float eased = ofxEasingFunc::Cubic::easeInOut(t);
			const float speed = spinMaxSpeedPxPerSec * eased;
			reelOffsetPx -= speed * step;

			remaining -= step;
		}

		const float tNow = ofClamp((now - spinStartTimeSec) / spinAccelDurationSec, 0.0f, 1.0f);
		const float easedNow = ofxEasingFunc::Cubic::easeInOut(tNow);
		currentSpeedPxPerSec = spinMaxSpeedPxPerSec * easedNow;
	}

	void updateStopping(float now) {
		float t = ofClamp((now - stopStartTimeSec) / stopDurationSec, 0.0f, 1.0f);
		float eased = ofxEasingFunc::Expo::easeOut(t);
		reelOffsetPx = stopStartOffsetPx + (stopEndOffsetPx - stopStartOffsetPx) * eased;
		if (t >= 1.0f) {
			reelOffsetPx = stopEndOffsetPx;
			layoutYearsFromOffset();
			state = State::idle;
		}
	}

	void layoutYearsFromOffset() {
		const float period = spacing * static_cast<float>(poster_imgs.size());
		const float wrapTopY = -spacing;
		float targetEffectiveOffset = reelOffsetPx;
		if (inAnimating && isSpinning) {
			targetEffectiveOffset = spinBlendBaseOffset + (reelOffsetPx - spinBlendBaseOffset) * inSpinBlend;
		}

		float effectiveOffset = targetEffectiveOffset;
		if (inAnimating && isSpinning) {
			const float maxStep = maxInSpinBlendSpeedPxPerSec * lastFrameDtSec;
			const float delta = targetEffectiveOffset - visualEffectiveOffset;
			effectiveOffset = visualEffectiveOffset + ofClamp(delta, -maxStep, maxStep);
		}
		visualEffectiveOffset = effectiveOffset;

		debugEffectiveOffset = effectiveOffset;
		for (auto & poster_img : poster_imgs) {
			float raw = poster_img.getBaseY() + effectiveOffset - wrapTopY;
			float wrapped = std::fmod(std::fmod(raw, period) + period, period);
			poster_img.setY(wrapTopY + wrapped);
		}
	}
};
