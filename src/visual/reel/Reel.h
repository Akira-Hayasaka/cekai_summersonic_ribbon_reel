#pragma once

#include "ofMain.h"
#include "ofxEasingFunc.h"
#include "Constants.h"
#include "Poster.h"
#include "YearReel.h"
#include "PosterReel.h"
//#include "HeadlinerReel.h"
#include "Transition.h"

class Reel {
public:
	Reel(
		std::vector<std::shared_ptr<Poster>> posters,
		std::vector<std::shared_ptr<Headliner>> headliners) {

		poster_reel = std::make_unique<PosterReel>(std::move(posters));
		//headliner_reel = std::make_unique<HeadlinerReel>(std::move(headliners));

		state = State::idle;

		center = glm::vec3(Constants::APP_W * 0.5f, Constants::APP_H * 0.5f, 0.0f);
		cam.enableOrtho();
		cam.setPosition(center.x, center.y, 1000.0f);
		cam.lookAt(center);
		cam.setVFlip(true);
		cam.setFarClip(20000.0f);

		zoom_pt = glm::vec2(270, 675);
		zoom_scale = 0.5f;
		zoom_duration_sec = 1.9;
		zoom_anim_duration_sec = zoom_duration_sec;

				const float s = zoom_scale;
		const float px = zoom_pt.x - (zoom_pt.x - center.x) / s;
		const float py = zoom_pt.y - (zoom_pt.y - center.y) / s;
		cam.setScale(zoom_scale, zoom_scale, 1.0f);
		cam.setPosition(px, py, 1000.0f);

		zoom_restore_pos = glm::vec3(center.x, center.y, 1000.0f); // Z を明示的に 1000.0f に
		zoom_restore_scale = glm::vec3(1.0f, 1.0f, 1.0f);

		ofAddListener(Globals::sequencer->keyframeEvent, this, &Reel::on_SequencerKeyframeEvent);
		
		// bind key events to control the reel state
		ofAddListener(ofEvents().keyPressed, this, &Reel::onKeyPressed);
	};

	void on_SequencerKeyframeEvent(SequencerKeyframeEvent & e) {

	}

	void update() {

		//zoom_pt = glm::vec2(Globals::proto->v[0], Globals::proto->v[1]);

		updateZoom();
		updateShowAllPostersDelay();
		poster_reel->update();
		//headliner_reel->update();
	}

	void draw() {
		if (!b_visible) return;

		cam.begin();
		poster_reel->draw();
		cam.end();

		if (posterOnlyDrawEnabled_) return;
	}

	void enablePosterOnlyDraw() { posterOnlyDrawEnabled_ = true; }
	void disablePosterOnlyDraw() { posterOnlyDrawEnabled_ = false; }

	void in() {
		state = State::in;
		//year_reel->in();
		poster_reel->in();
		//headliner_reel->in();
	}

	void spin() {
		state = State::spinning;
		//year_reel->spin();
		poster_reel->spin();
		//headliner_reel->spin();
	}

	void stop() {
		state = State::stop;
		//year_reel->stop();
		poster_reel->stop();
		//headliner_reel->stop();
	}

	void zoom_in() {
		has_zoom_restore = true;

		zoom_from_pos = cam.getPosition();
		zoom_from_scale = cam.getScale();

		zoom_to_scale = glm::vec3(zoom_scale, zoom_scale, 1.0f);
		const float s = zoom_scale;
		const float px = zoom_pt.x - (zoom_pt.x - center.x) / s;
		const float py = zoom_pt.y - (zoom_pt.y - center.y) / s;
		zoom_to_pos = glm::vec3(px, py, 1000.0f);
		zoom_anim_duration_sec = zoom_duration_sec;
		zoom_scale_ease_gain = 2.0f;

		zoom_state = ZoomState::in;
		zoom_start_sec = Globals::timeline->getCurrentTimeSec();
	}

	void zoom_in(const float zoom_from_scale_value, const float zoom_duration_sec_value) {
		has_zoom_restore = true;

		zoom_from_pos = cam.getPosition();
		zoom_from_scale = glm::vec3(zoom_from_scale_value, zoom_from_scale_value, 1.0f);
		cam.setScale(zoom_from_scale.x, zoom_from_scale.y, zoom_from_scale.z);

		zoom_to_scale = glm::vec3(zoom_scale, zoom_scale, 1.0f);
		const float s = zoom_scale;
		const float px = zoom_pt.x - (zoom_pt.x - center.x) / s;
		const float py = zoom_pt.y - (zoom_pt.y - center.y) / s;
		zoom_to_pos = glm::vec3(px, py, 1000.0f);
		zoom_anim_duration_sec = std::max(zoom_duration_sec_value, 0.0001f);
		zoom_scale_ease_gain = 1.0f;

		zoom_state = ZoomState::in;
		zoom_start_sec = Globals::timeline->getCurrentTimeSec();
	}

	void zoom_out() {
		//if (!has_zoom_restore) {
		//	return;
		//}

		zoom_from_pos = cam.getPosition();
		zoom_from_scale = cam.getScale();
		zoom_to_pos = zoom_restore_pos;
		zoom_to_scale = zoom_restore_scale;
		zoom_anim_duration_sec = 0.417; //zoom_duration_sec;
		zoom_scale_ease_gain = 1.0f;

		zoom_state = ZoomState::out;
		zoom_start_sec = Globals::timeline->getCurrentTimeSec();
	}

	void setHeadlinerDrawEnabled(bool enabled) {
		//headliner_reel->setHeadlinerDrawEnabled(enabled);
	}

	void reset_reels() {
		state = State::idle;
		poster_reel->reset();
		//headliner_reel->reset();
		zoom_state = ZoomState::idle;
		zoom_start_sec = 0.0f;
		has_zoom_restore = false;
		cam.setScale(1.0f, 1.0f, 1.0f);
		cam.setPosition(center.x, center.y, 1000.0f);
	}

	void onKeyPressed(ofKeyEventArgs & args) {
		switch (args.key) {
		case 'i':
			in();
			break;
		case 's':
			spin();
			break;
		case 'x':
			stop();
			break;
		case 'o':
			//headliner_reel->out();
			break;
		case 'z':
			zoom_in();
			break;
		case 'u':
			zoom_out();
			break;
		case 'l':
			reset_reels();
			break;
		default:
			break;
		}
	}

private:

	void show() { b_visible = true; }
	void hide() { b_visible = false; }

	enum struct State { idle, in, spinning, stop };
	State state;

	bool b_visible = true;
	bool posterOnlyDrawEnabled_ = false;

	ofCamera cam;
	glm::vec3 center;

	std::unique_ptr<PosterReel> poster_reel;
	//std::unique_ptr<HeadlinerReel> headliner_reel;

	glm::vec2 zoom_pt;
	float zoom_scale;
	float zoom_duration_sec;
	float zoom_anim_duration_sec;
	float zoom_scale_ease_gain = 2.0f;

	enum struct ZoomState { idle, in, out };
	ZoomState zoom_state = ZoomState::idle;
	float zoom_start_sec = 0.0f;

	glm::vec3 zoom_from_pos = glm::vec3(0.0f);
	glm::vec3 zoom_to_pos = glm::vec3(0.0f);
	glm::vec3 zoom_from_scale = glm::vec3(1.0f, 1.0f, 1.0f);
	glm::vec3 zoom_to_scale = glm::vec3(1.0f, 1.0f, 1.0f);

	glm::vec3 zoom_restore_pos = glm::vec3(0.0f);
	glm::vec3 zoom_restore_scale = glm::vec3(1.0f, 1.0f, 1.0f);
	bool has_zoom_restore = false;

	bool showAllPostersPending = false;
	float showAllPostersDelaySec = 0.0f;
	float showAllPostersStartSec = 0.0f;

	void updateShowAllPostersDelay() {
		if (!showAllPostersPending) return;
		const float elapsed = Globals::timeline->getCurrentTimeSec() - showAllPostersStartSec;
		if (elapsed >= showAllPostersDelaySec) {
			poster_reel->showAllPosters();
			showAllPostersPending = false;
		}
	}

	void updateZoom() {
		if (zoom_state == ZoomState::idle) return;
		float t     = ofClamp((Globals::timeline->getCurrentTimeSec() - zoom_start_sec) / zoom_anim_duration_sec, 0.0f, 1.0f);
		float eased = ofxEasingFunc::Expo::easeOut(t);
		const float eased_scale = ofClamp(eased * zoom_scale_ease_gain, 0.0f, 1.0f);
		const float eased_y = ofClamp(eased * 1.25f, 0.0f, 1.0f);

		const glm::vec3 scale = glm::mix(zoom_from_scale, zoom_to_scale, eased_scale);
		const glm::vec3 pos = glm::vec3(
			glm::mix(zoom_from_pos.x, zoom_to_pos.x, eased),
			glm::mix(zoom_from_pos.y, zoom_to_pos.y, eased_y),
			glm::mix(zoom_from_pos.z, zoom_to_pos.z, eased));
		cam.setScale(scale.x, scale.y, scale.z);
		cam.setPosition(pos);

		if (t >= 1.0f) zoom_state = ZoomState::idle;
	}
};
