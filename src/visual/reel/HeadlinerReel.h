#pragma once

#include "ofMain.h"
#include <limits>

#include "Constants.h"
#include "Globals.h"
#include "Headliner.h"
#include "ofxEasingFunc.h"

class Container : public ofRectangle {
public:

	Container(float x, float y, float w, float h)
		: ofRectangle(x, y, w, h)
		, baseY_(y) {
	}

	void update() { }

	void draw() {
		ofDrawRectangle(*this);
	}

	float getBaseY() const { return baseY_; }
	void  setY(float newY) { y = newY; }

private:
	float baseY_ = 0.0f;
};

class HeadlinerReel {
public:

	HeadlinerReel(std::vector<std::shared_ptr<Headliner>> headliners)
		: headliners(std::move(headliners)) {

		const float spacing     = static_cast<float>(Constants::APP_H) / 3.0f;
		const float container_w = static_cast<float>(Constants::APP_W);
		for (int i = 0; i < n_containers_; ++i) {
			containers.emplace_back(
				0.0f, spacing * static_cast<float>(i), container_w, spacing);
		}

		containerHeadliners_.resize(n_containers_);
		if (!this->headliners.empty()) {
			for (int i = 0; i < n_containers_; ++i) {
				const int idx = i % static_cast<int>(this->headliners.size());
				containerHeadliners_[i] = this->headliners[idx];
			}
		}

		targetCenterY_     = static_cast<float>(Constants::APP_H) / 2.0f - spacing / 2.0f;
		idle_pos_          = glm::vec2(0.0f, 2000.0f);
		active_pos_        = glm::vec2(0.0f, 0.0f);
		out_pos_           = glm::vec2(0.0f, -5000.0f);
		cur_pos_           = idle_pos_;
		lastUpdateTimeSec_ = Globals::timeline->getCurrentTimeSec();

		ofAddListener(Globals::sequencer->keyframeEvent, this, &HeadlinerReel::on_SequencerKeyframeEvent);
	}

	void on_SequencerKeyframeEvent(SequencerKeyframeEvent & e) {
		if (e.trackName == "HeadlinerReel") {
			if (e.keyframeName == "in") {

				for (auto & headliner : this->headliners) {
					headliner->setActivationWindow(
						e.currentFrame / Globals::sequencer->get_fps(),
						headliner->getActivationDurationSec() * 2.0);
				}

				in();
			} else if (e.keyframeName == "spin") {
				spin();
			} else if (e.keyframeName == "out") {
				out();
			}
		}
	}


	void update() {
		float now = Globals::timeline->getCurrentTimeSec();
		float dt  = now - lastUpdateTimeSec_;
		lastUpdateTimeSec_ = now;

		dt = ofClamp(dt, 0.0f, 1.0f / 15.0f);
		lastFrameDtSec_ = dt;

		if (inAnimating_) {
			updateIn(now);
		}
		if (outAnimating_) {
			updateOut(now);
		}

		if (state_ == State::spinning) {
			updateSpinning(now, dt);
		} else if (state_ == State::stop) {
			updateStopping(now);
		}

		layoutContainersFromOffset();

		if (inAnimating_ && state_ == State::spinning) {
			const float dOffset = std::isfinite(debugPrevEffectiveOffset_)
				? std::abs(debugEffectiveOffset_ - debugPrevEffectiveOffset_)
				: 0.0f;
			const float dCurY = debugHasPrevCurPos_ ? std::abs(cur_pos_.y - debugPrevCurPos_.y) : 0.0f;
			const bool shouldLog =
				(debugLastLogTimeSec_ < 0.0f) ||
				((now - debugLastLogTimeSec_) >= 0.05f) ||
				(dOffset > 40.0f) ||
				(dCurY > 20.0f);
			if (shouldLog) {
				ofLogWarning("HeadlinerReelDBG")
					<< "time=" << now
					<< " dt=" << dt
					<< " inBlend=" << inSpinBlend_
					<< " reelOffset=" << reelOffsetPx_
					<< " baseOffset=" << spinBlendBaseOffset_
					<< " effectiveOffset=" << debugEffectiveOffset_
					<< " dEffectiveOffset=" << dOffset
					<< " curPosY=" << cur_pos_.y
					<< " dCurPosY=" << dCurY;
				debugLastLogTimeSec_ = now;
			}
			debugPrevEffectiveOffset_ = debugEffectiveOffset_;
			debugPrevCurPos_ = cur_pos_;
			debugHasPrevCurPos_ = true;
		}

		for (auto & c : containers) {
			c.update();
		}
	}

	void draw() {
		ofPushMatrix();
		ofTranslate(cur_pos_);
		for (int i = 0; i < static_cast<int>(containers.size()); ++i) {
			const auto & container = containers[i];
			const auto & headliner = containerHeadliners_[i];
			const auto topMargin = 50;
			if (headlinerDrawEnabled_ && headliner && headliner->isActive()) {
				headliner->draw(
					container.x, container.y, container.width, container.height,
					528, 0 + topMargin, 864, container.height);
			}
		}

		//ofPushStyle();
		//ofNoFill();
		//ofSetColor(ofColor::magenta);
		//for (const auto & container : containers) {
		//	ofDrawRectangle(container);
		//}
		//ofFill();
		//ofPopStyle();

		ofPopMatrix();
	}

	void setHeadlinerDrawEnabled(bool enabled) { headlinerDrawEnabled_ = enabled; }
	bool isHeadlinerDrawEnabled() const { return headlinerDrawEnabled_; }

	void in() {
		state_       = State::in;
		inSec_       = Globals::timeline->getCurrentTimeSec();
		inAnimating_ = true;
		inSpinBlend_ = 0.0f;
	}

	void out() {
		if (state_ != State::spinning) {
			state_ = State::out;
		}
		outSec_       = Globals::timeline->getCurrentTimeSec();
		outAnimating_ = true;
	}

	void spin() {
		spinBlendBaseOffset_ = reelOffsetPx_;
		spinStartTimeSec_ = Globals::timeline->getCurrentTimeSec();
		state_            = State::spinning;
	}

	void reset() {
		state_               = State::idle;
		inAnimating_         = false;
		outAnimating_        = false;
		reelOffsetPx_        = 0.0f;
		inSpinBlend_         = 1.0f;
		spinBlendBaseOffset_ = 0.0f;
		currentSpeedPxPerSec_ = 0.0f;
		spinStartTimeSec_    = 0.0f;
		debugEffectiveOffset_ = 0.0f;
		visualEffectiveOffset_ = 0.0f;
		debugPrevEffectiveOffset_ = std::numeric_limits<float>::quiet_NaN();
		debugLastLogTimeSec_ = -1.0f;
		debugHasPrevCurPos_ = false;
		stopStartOffsetPx_   = 0.0f;
		stopEndOffsetPx_     = 0.0f;
		cur_pos_             = idle_pos_;
		lastUpdateTimeSec_   = Globals::timeline->getCurrentTimeSec();
		layoutContainersFromOffset();
	}

	void stop() {
		if (containers.empty()) return;

		const float spacing = containers[0].height;
		const float period  = spacing * static_cast<float>(containers.size());
		const float minStopTravelPx = period * 0.25f;

		const int rawIdx    = static_cast<int>(ofRandom(0.0f, static_cast<float>(containers.size())));
		const int targetIdx = std::max(0, std::min(rawIdx, static_cast<int>(containers.size()) - 1));

		stopStartOffsetPx_ = reelOffsetPx_;
		stopStartTimeSec_ = Globals::timeline->getCurrentTimeSec();

		float delta = (targetCenterY_ - containers[targetIdx].getBaseY()) - reelOffsetPx_;
		while (delta >= -minStopTravelPx) {
			delta -= period;
		}
		delta -= period * static_cast<float>(extraStopLoops_);

		stopEndOffsetPx_ = reelOffsetPx_ + delta;
		state_           = State::stop;
	}

private:

	enum struct State { idle, in, out, spinning, stop };
	State state_ = State::idle;

	std::vector<std::shared_ptr<Headliner>> headliners;
	std::vector<Container> containers;
	std::vector<std::shared_ptr<Headliner>> containerHeadliners_;

	static constexpr int n_containers_ = 10;

	float inSec_         = 0.0f;
	float inDurationSec_ = 0.1f;
	bool  inAnimating_   = false;
	float inSpinBlend_   = 1.0f;

	float outSec_         = 0.0f;
	float outDurationSec_ = 1.25f;
	bool  outAnimating_   = false;

	float reelOffsetPx_         = 0.0f;
	float spinBlendBaseOffset_  = 0.0f;
	float lastUpdateTimeSec_    = 0.0f;
	float targetCenterY_        = 0.0f;
	float currentSpeedPxPerSec_ = 0.0f;

	float spinStartTimeSec_     = 0.0f;
	float spinAccelDurationSec_ = 0.0f;
	float spinMaxSpeedPxPerSec_ = 8000.0f;
	float maxInSpinBlendSpeedPxPerSec_ = 2400.0f;
	float lastFrameDtSec_ = 1.0f / 60.0f;

	float debugEffectiveOffset_ = 0.0f;
	float visualEffectiveOffset_ = 0.0f;
	float debugPrevEffectiveOffset_ = std::numeric_limits<float>::quiet_NaN();
	float debugLastLogTimeSec_ = -1.0f;
	glm::vec2 debugPrevCurPos_ = glm::vec2(0.0f, 0.0f);
	bool debugHasPrevCurPos_ = false;

	float stopStartTimeSec_  = 0.0f;
	float stopDurationSec_   = 2.0f;
	float stopStartOffsetPx_ = 0.0f;
	float stopEndOffsetPx_   = 0.0f;
	int   extraStopLoops_    = 1;

	glm::vec2 idle_pos_, active_pos_, out_pos_, cur_pos_;

	bool headlinerDrawEnabled_ = false;

	void updateIn(float now) {
		float t     = ofClamp((now - inSec_) / inDurationSec_, 0.0f, 1.0f);
		float eased = ofxEasingFunc::Cubic::easeOut(t);
		cur_pos_    = idle_pos_ + (active_pos_ - idle_pos_) * eased;
		inSpinBlend_ = eased;
		if (t >= 1.0f) {
			cur_pos_     = active_pos_;
			inAnimating_ = false;
			inSpinBlend_ = 1.0f;
			if (state_ == State::in) {
				state_ = State::idle;
			}
		}
	}

	void updateOut(float now) {
		float t     = ofClamp((now - outSec_) / outDurationSec_, 0.0f, 1.0f);
		float eased = ofxEasingFunc::Cubic::easeIn(t);
		cur_pos_    = active_pos_ + (out_pos_ - active_pos_) * eased;
		if (t >= 1.0f) {
			cur_pos_      = out_pos_;
			outAnimating_ = false;
			state_ = State::idle;
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

			const float t = ofClamp((simNow - spinStartTimeSec_) / spinAccelDurationSec_, 0.0f, 1.0f);
			const float eased = ofxEasingFunc::Cubic::easeInOut(t);
			const float speed = spinMaxSpeedPxPerSec_ * eased;
			reelOffsetPx_ -= speed * step;

			remaining -= step;
		}

		const float tNow = ofClamp((now - spinStartTimeSec_) / spinAccelDurationSec_, 0.0f, 1.0f);
		const float easedNow = ofxEasingFunc::Cubic::easeInOut(tNow);
		currentSpeedPxPerSec_ = spinMaxSpeedPxPerSec_ * easedNow;
	}

	void updateStopping(float now) {
		float t     = ofClamp((now - stopStartTimeSec_) / stopDurationSec_, 0.0f, 1.0f);
		float eased = ofxEasingFunc::Cubic::easeOut(t);
		reelOffsetPx_ = stopStartOffsetPx_ + (stopEndOffsetPx_ - stopStartOffsetPx_) * eased;
		if (t >= 1.0f) {
			reelOffsetPx_ = stopEndOffsetPx_;
			layoutContainersFromOffset();
			state_ = State::idle;
		}
	}

	void layoutContainersFromOffset() {
		if (containers.empty()) return;
		const float spacing  = containers[0].height;
		const float period   = spacing * static_cast<float>(containers.size());
		const float wrapTopY = -spacing;
		float targetEffectiveOffset = reelOffsetPx_;
		if (inAnimating_ && state_ == State::spinning) {
			targetEffectiveOffset = spinBlendBaseOffset_ + (reelOffsetPx_ - spinBlendBaseOffset_) * inSpinBlend_;
		}

		float effectiveOffset = targetEffectiveOffset;
		if (inAnimating_ && state_ == State::spinning) {
			const float maxStep = maxInSpinBlendSpeedPxPerSec_ * lastFrameDtSec_;
			const float delta = targetEffectiveOffset - visualEffectiveOffset_;
			effectiveOffset = visualEffectiveOffset_ + ofClamp(delta, -maxStep, maxStep);
		}
		visualEffectiveOffset_ = effectiveOffset;

		debugEffectiveOffset_ = effectiveOffset;
		for (auto & c : containers) {
			float raw     = c.getBaseY() + effectiveOffset - wrapTopY;
			float wrapped = std::fmod(std::fmod(raw, period) + period, period);
			c.setY(wrapTopY + wrapped);
		}
	}
};
