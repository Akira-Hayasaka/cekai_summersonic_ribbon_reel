#pragma once

#include <algorithm>
#include <array>

#include "ofMain.h"
#include "ofxEasingFunc.h"
#include "Constants.h"
#include "Globals.h"
#include "Poster.h"
#include "Headliner.h"
#include "Effect_01.h"
#include "Effect_02.h"
#include "ArtistName.h"

class Transition {
public:

	inline static ofEvent<float> first_movie_play_start_event;

	Transition(
		std::vector<int> target_years_,
		std::vector<std::shared_ptr<Poster>> posters_,
		std::vector<std::shared_ptr<Headliner>> headliners_) {

		headliner_duration_ = Globals::asset.value("headlinerDuration", 2.5);

		// 連続しない年代も正しく扱えるように、ソート済み・重複なしの対象年リストとして保持する。
		target_years = std::move(target_years_);
		std::sort(target_years.begin(), target_years.end());
		target_years.erase(std::unique(target_years.begin(), target_years.end()), target_years.end());

		current_year_idx = 0;
		current_year = target_years.empty() ? 0 : target_years.front();
		posters = std::move(posters_);
		headliners = std::move(headliners_);

		// headlinersの名前をもとに、ArtistNameオブジェクトを作成して保持
		for (const auto & headliner : headliners) {
			ofLog() << "Creating ArtistName for headliner: " << headliner->getArtistName();
			const ArtistName::ViewPadding viewPadding{
				headliner->getArtistNameViewPaddingLeft(),
				headliner->getArtistNameViewPaddingBottom() };
			artistNames.emplace_back(std::make_unique<ArtistName>(
				headliner->getYear(), headliner->getArtistName(), viewPadding));
		}
		popTriggerStates.resize(headliners.size());

		// 年ごとのヘッドライナー数を target_years と整列した配列として保持する。
		// (年代ごとにヘッドライナー数が可変になったため、サイクル長を年ごとに算出できるようにする)
		headliner_counts_.assign(target_years.size(), 0);
		for (size_t idx = 0; idx < target_years.size(); ++idx) {
			for (const auto & h : headliners) {
				if (h && h->getYear() == target_years[idx]) ++headliner_counts_[idx];
			}
		}

		// 各年の最後のヘッドライナーについて、ArtistNameを現状より0.1秒速くフェードアウトさせるための
		// フェード時間(秒)を求めておく。
		// - 最終年の最後のヘッドライナー: 従来0.2秒だったため 0.2+0.1=0.3秒 に。
		// - それ以外の年の最後のヘッドライナー: 遷移オーバーラップ(0.25秒)による自然なフェードより
		//   0.1秒速い 0.25+0.1=0.35秒 に(オーバーラップのフェードと併用し、より速い方を採用する)。
		last_headliner_fade_duration_.assign(headliners.size(), -1.0f);
		for (size_t idx = 0; idx < target_years.size(); ++idx) {
			const int y = target_years[idx];
			int last_idx = -1;
			for (size_t i = 0; i < headliners.size(); ++i) {
				if (headliners[i] && headliners[i]->getYear() == y) last_idx = static_cast<int>(i);
			}
			if (last_idx >= 0) {
				const bool is_last_year = (idx == target_years.size() - 1);
				last_headliner_fade_duration_[last_idx] = is_last_year ? artistNameLastYearFadeSec : artistNameOtherYearFadeSec;
			}
		}

		// 各年の最初のヘッドライナーについて、ArtistNameをフェードインさせる対象として記録しておく。
		first_headliner_of_year_.assign(headliners.size(), false);
		for (size_t idx = 0; idx < target_years.size(); ++idx) {
			const int y = target_years[idx];
			for (size_t i = 0; i < headliners.size(); ++i) {
				if (headliners[i] && headliners[i]->getYear() == y) {
					first_headliner_of_year_[i] = true;
					break;
				}
			}
		}

		fbo = std::make_shared<ofFbo>();
		ofDisableArbTex();
		ofFboSettings fboSettings;
		fboSettings.width = Constants::APP_W;
		fboSettings.height = Constants::APP_H;
		fboSettings.internalformat = GL_RGBA;
		fboSettings.numSamples = 4; // 4x MSAA
		fbo->allocate(fboSettings);
		ofEnableArbTex();

		// 対象年(target_years)に含まれるポスターのみをEffectにわたす（連続しない年代も正しく除外される）
		std::vector<std::shared_ptr<Poster>> poster_filter;
		for (auto& p : posters) {
			if (std::binary_search(target_years.begin(), target_years.end(), ofToInt(p->getYear()))) {
                poster_filter.push_back(p);
				ofLog() << "Poster for year " << p->getYear() << " included in transition effect.";
            }
		}
		effect = std::make_unique<Effect_02>(poster_filter, 4.0f);

		state = State::idle;
	}

	void update() {
		if (state == State::idle) return;

		double now     = Globals::timeline->getCurrentTimeSec();
		double elapsed = now - cycle_start_sec;

		// advance through any completed cycles
		// 各年のサイクル長はヘッドライナー数に応じて可変。
		while (true) {
			double total = yearCycleTotal(current_year_idx);
			if (elapsed < total) break;
			elapsed         -= total;
			cycle_start_sec += total;
			current_year_idx++;
			if (current_year_idx >= target_years.size()) {
				ofLog() << "current_year: " << current_year << ", current_year_idx: " << current_year_idx << ", target_years.size(): " << target_years.size() << ", all cycles completed. Transition back to idle.";
				state = State::idle;
				headlinerDrawEnabled_ = false;
				return;
			}
			current_year = target_years[current_year_idx];
		}

		auto last_state = state;
		auto last_in_transition_overlap = in_transition_overlap;

		// determine state from elapsed time
		// 年ごとのヘッドライナー数(n)に応じて、各ヘッドライナー区間を一般化して判定する。
		const int    n            = headliner_counts_[current_year_idx];
		const double movies_total = static_cast<double>(n) * headliner_duration_;
		const bool   is_last_year = (current_year_idx == target_years.size() - 1);

		if (elapsed < movies_total) {
			// 現在再生中のヘッドライナーのインデックス(0始まり)
			int h_idx = static_cast<int>(elapsed / headliner_duration_);
			if (h_idx > n - 1) h_idx = n - 1; // 浮動小数点誤差の保護
			current_headliner_in_cycle_ = h_idx;

			// 最終年でなければ、最後のヘッドライナーの末尾 transition_overlap_sec で遷移を開始する
			const bool   in_last_headliner        = (h_idx == n - 1);
			const double time_into_last_headliner = elapsed - static_cast<double>(n - 1) * headliner_duration_;
			if (!is_last_year && in_last_headliner &&
				time_into_last_headliner >= headliner_duration_ - transition_overlap_sec) {
				// overlap period: transition starts while the last headliner is still playing
				state = State::transition_to_next_poster;
				in_transition_overlap = true;
			}
			else {
				state = State::playing_headliner_movie;
				in_transition_overlap = false;
			}
		}
		else {
			// 全ヘッドライナーの再生が終わった後
			current_headliner_in_cycle_ = n - 1;
			if (is_last_year) {
				// last year: no overlap; go idle right after the last headliner ends
				state = State::idle;
				in_transition_overlap = false;
			}
			else {
				state = State::transition_to_next_poster;
				in_transition_overlap = false;
			}
		}

		// track when overlap period starts for alpha fadeout
		if (!last_in_transition_overlap && in_transition_overlap) {
			headliner_alpha_fadeout_start_sec = static_cast<float>(now);
		}

		if (state != last_state && state == State::transition_to_next_poster) {
			ofLog() << "Transition to next poster triggered for year " << current_year << " at elapsed time " << elapsed << "s";
			effect->toOutputScale(static_cast<float>(to_output_scale_duration_ms) / 1000.0f);
			//effect->hideNonCenter();
			zoom_cs.unsubscribe();
			zoom_cs = rxcpp::composite_subscription();
			rxcpp::observable<>::timer(
				std::chrono::milliseconds(zoom_out_delay_ms),
				rxcpp::observe_on_run_loop(rl))
				.subscribe(zoom_cs, [this](long) { 
					effect->showAll();
					//effect->zoomout(); 
					effect->next();
					next_cs.unsubscribe();
					next_cs = rxcpp::composite_subscription();
					rxcpp::observable<>::timer(
						std::chrono::milliseconds(zoom_in_after_next_delay_ms),
						rxcpp::observe_on_run_loop(rl))
						.subscribe(next_cs, [this](long) {
						//effect->zoomin();
							});
				});
			float delayA = 0.35f;
			ofNotifyEvent(first_movie_play_start_event, delayA);
		}
		while (!rl.empty() && rl.peek().when <= rl.now()) {
			rl.dispatch();
		}

		// サイクル先頭(ヘッドライナーidx==0)に新規突入したフレームで、1本目のスケールインを開始する。
		const bool entered_new_first_headliner =
			(state == State::playing_headliner_movie) &&
			(current_headliner_in_cycle_ == 0) &&
			!(prev_year_idx_ == current_year_idx &&
			  prev_headliner_in_cycle_ == 0 &&
			  last_state == State::playing_headliner_movie);
		if (entered_new_first_headliner) {
			first_movie_scale_start_sec = static_cast<float>(now);
			first_movie_scale = 0.0f;
			float delayB = 0.5f;
			ofNotifyEvent(first_movie_play_start_event, delayB);
		}
		if (first_movie_scale_start_sec >= 0.0f) {
			const float t = ofClamp((static_cast<float>(now) - first_movie_scale_start_sec) / first_movie_scale_anim_duration, 0.0f, 1.0f);
			first_movie_scale = ofxEasingFunc::Quint::easeOut(t);
		}

		effect->update();

		const float nowSec = static_cast<float>(Globals::timeline->getCurrentTimeSec());
		for (size_t i = 0; i < headliners.size(); ++i) {
			auto & trigger = popTriggerStates[i];
			const bool isActive = headliners[i] && headliners[i]->isActive();

			if (isActive && !trigger.wasActive) {
				trigger.activeStartSec = nowSec;
				trigger.popCalled = false;
			}

			// 各年最初のヘッドライナーも、Warperの歪み開始(entered_new_first_headliner)と同じフレームで
			// pop()させるため、遅延を挟まず isActive() になった瞬間に即座に発火させる。
			if (isActive && !trigger.popCalled) {
				if (i < artistNames.size() && artistNames[i]) {
					artistNames[i]->pop();
				}
				trigger.popCalled = true;
				trigger.popTimeSec = nowSec;
			}

			if (!isActive) {
				trigger.activeStartSec = 0.0f;
				trigger.popCalled = false;
				trigger.popTimeSec = -1.0f;
			}

			trigger.wasActive = isActive;
		}

		prev_year_idx_ = current_year_idx;
		prev_headliner_in_cycle_ = current_headliner_in_cycle_;
	}

	void draw() {

		if (state == State::idle) return;

		//fbo->draw(0, 0);
		if (!in_transition_overlap)
			effect->draw();

		// draw headliner during its play state OR during the overlap period
		bool should_draw_headliner = headlinerDrawEnabled_ && (
			state == State::playing_headliner_movie ||
			in_transition_overlap
		);

		if (should_draw_headliner) {
			// calculate alpha for headliner fadeout during overlap period
			float headliner_alpha = 255.0f;
			if (in_transition_overlap && headliner_alpha_fadeout_start_sec >= 0.0f) {
				const float now = static_cast<float>(Globals::timeline->getCurrentTimeSec());
				const float elapsed_fadeout = now - headliner_alpha_fadeout_start_sec;
				const float t = ofClamp(elapsed_fadeout / static_cast<float>(transition_overlap_sec), 0.0f, 1.0f);
				// easeOutSine: 255.0 -> 0.0
				headliner_alpha = 255.0f * (1.0f - ofxEasingFunc::Expo::easeIn(t));
			}

			const float nowSec = static_cast<float>(Globals::timeline->getCurrentTimeSec());
			for (size_t i = 0; i < headliners.size(); ++i) {
				auto & headliner = headliners[i];
				if (headliner->isActive()) {

					// imgタイプのheadlinerは、表示開始から5秒かけて画面中央センター合わせでスケールを1.0から1.2までLinearに拡大する
					float imgScale = 1.0f;
					if (headliner->isImageType() && i < popTriggerStates.size()) {
						const float elapsedSinceActive = nowSec - popTriggerStates[i].activeStartSec;
						const float t = ofClamp(elapsedSinceActive / imgScaleAnimDurationSec, 0.0f, 1.0f);
						imgScale = ofLerp(imgScaleStart, imgScaleEnd, t);
					}

					ofPushStyle();
					ofSetColor(255, 255, 255, headliner_alpha);

					ofPushMatrix();
					ofSetRectMode(OF_RECTMODE_CENTER);
					ofTranslate(Constants::APP_W / 2, Constants::APP_H / 2);
					ofScale(first_movie_scale, first_movie_scale);
					ofScale(imgScale, imgScale);

					auto ss = headliner->getCropSubsection();
					headliner->draw(0, 0, Constants::APP_W, Constants::APP_H, ss.x, ss.y, ss.z, ss.w);

					ofSetRectMode(OF_RECTMODE_CORNER);
					ofPopMatrix();

					ofPopStyle();
				}
			}
		}

		if (in_transition_overlap) {
			effect->draw();
		}
	}

	void drawArtistNames() {
		bool should_draw_headliner = headlinerDrawEnabled_ && (state == State::playing_headliner_movie || in_transition_overlap);

		if (should_draw_headliner) {
			// calculate alpha for headliner fadeout during overlap period
			float headliner_alpha = 255.0f;
			if (in_transition_overlap && headliner_alpha_fadeout_start_sec >= 0.0f) {
				const float now = static_cast<float>(Globals::timeline->getCurrentTimeSec());
				const float elapsed_fadeout = now - headliner_alpha_fadeout_start_sec;
				const float t = ofClamp(elapsed_fadeout / static_cast<float>(transition_overlap_sec), 0.0f, 1.0f);
				// easeOutSine: 255.0 -> 0.0
				headliner_alpha = 255.0f * (1.0f - ofxEasingFunc::Expo::easeIn(t));
			}

			// headlinersとartistNamesは同じ順序・要素数で構築されているため、インデックスで対応付ける
			const float now = static_cast<float>(Globals::timeline->getCurrentTimeSec());
			for (size_t i = 0; i < headliners.size(); ++i) {
				if (headliners[i]->isActive()) {
					if (i < artistNames.size() && artistNames[i]) {
						float alpha = headliner_alpha;

						// 各年の最後のヘッドライナー区間では、activation終了に向けて
						// 現状(最終年0.2秒/それ以外0.25秒)より0.1秒速くArtistNameをフェードアウトさせ、
						// ムービーの非表示に遅れないようにする。
						if (i < last_headliner_fade_duration_.size() && last_headliner_fade_duration_[i] > 0.0f) {
							const float fadeDur = last_headliner_fade_duration_[i];
							const float activation_end = headliners[i]->getActivationStartSec() + headliners[i]->getActivationDurationSec();
							const float remaining = activation_end - now;
							if (remaining < fadeDur) {
								const float t = ofClamp(remaining / fadeDur, 0.0f, 1.0f);
								alpha = std::min(alpha, 255.0f * t);
							}
						}

						// 各年の最初のヘッドライナーは、pop()時刻から0.25秒かけてQuint::easeInでフェードインさせる
						if (i < first_headliner_of_year_.size() && first_headliner_of_year_[i] &&
							i < popTriggerStates.size() && popTriggerStates[i].popTimeSec >= 0.0f) {
							const float elapsed_fadein = now - popTriggerStates[i].popTimeSec;
							const float t = ofClamp(elapsed_fadein / artistNameFirstYearFadeInSec, 0.0f, 1.0f);
							alpha *= ofxEasingFunc::Quint::easeIn(t);
						}

						artistNames[i]->draw(alpha);
					}
				}
			}
		}
	}

	void setHeadlinerDrawEnabled(bool enabled) { headlinerDrawEnabled_ = enabled; }
	bool isHeadlinerDrawEnabled() const { return headlinerDrawEnabled_; }

	void start() {
		current_year_idx = 0;
		current_year    = target_years.empty() ? 0 : target_years.front();
		cycle_start_sec = Globals::timeline->getCurrentTimeSec();
		in_transition_overlap = false;
		headliner_alpha_fadeout_start_sec = -1.0f;
		current_headliner_in_cycle_ = -1;
		prev_year_idx_ = static_cast<size_t>(-1);
		prev_headliner_in_cycle_ = -1;

		for (auto & trigger : popTriggerStates) {
			trigger.wasActive = false;
			trigger.activeStartSec = 0.0f;
			trigger.popCalled = false;
			trigger.popTimeSec = -1.0f;
		}

		// deactivate all headliners first
		for (auto & h : headliners)
			h->setActivationWindow(0.0f, 0.0f);

		// pre-assign activation windows for every year cycle.
		// 年ごとのサイクル開始時刻は、それ以前の年の(可変長)サイクル合計の累積で決まる。
		double cs = cycle_start_sec;
		for (size_t idx = 0; idx < target_years.size(); idx++) {
			const int y = target_years[idx];

			std::vector<Headliner*> hy;
			for (auto & h : headliners)
				if (h->getYear() == y) hy.push_back(h.get());

			// その年のヘッドライナーを headliner_duration_ ずつ順番に有効化する
			for (size_t k = 0; k < hy.size(); ++k) {
				const double h_start = cs + static_cast<double>(k) * headliner_duration_;
				hy[k]->setActivationWindow(
					static_cast<float>(h_start),
					static_cast<float>(headliner_duration_));
				ofLogNotice("Transition") << hy[k]->get_videoPath()
					<< " activation window: " << h_start << "s + " << headliner_duration_ << "s";
			}

			cs += yearCycleTotal(idx);
		}

		effect->reset();

		for (auto & artistName : artistNames) {
			artistName->reset_pop();
		}

		state = State::playing_headliner_movie;
	}

	void begin_feed_first_texture() {
		fbo->begin();
		ofClear(0);
	}

	void end_feed_first_texture() {
		fbo->end();
	}

private:

	// 指定した年(インデックス)のサイクル全体の長さ(秒)。
	// = ヘッドライナー数 * headliner_duration_ + transition_duration_
	double yearCycleTotal(size_t idx) const {
		const int n = (idx < headliner_counts_.size()) ? headliner_counts_[idx] : 0;
		return static_cast<double>(n) * headliner_duration_ + transition_duration_;
	}

	std::shared_ptr<ofFbo> fbo;

	std::vector<int> target_years;
	size_t current_year_idx = 0;
	int current_year = 0;

	bool headlinerDrawEnabled_ = false;

	enum struct State {
		idle,
		playing_headliner_movie,
		transition_to_next_poster,
	};
	State state;

	std::vector<std::shared_ptr<Poster>> posters;
	std::vector<std::shared_ptr<Headliner>> headliners;
	std::vector<std::shared_ptr<ArtistName>> artistNames;

	struct ArtistNamePopTriggerState {
		bool wasActive = false;
		float activeStartSec = 0.0f;
		bool popCalled = false;
		float popTimeSec = -1.0f;
	};
	std::vector<ArtistNamePopTriggerState> popTriggerStates;

	// 最後のヘッドライナー区間で、ArtistNameを現状より0.1秒速くフェードアウトさせるための秒数。
	// (最終年: 0.2+0.1=0.3秒 / それ以外の年: 0.25(オーバーラップ)+0.1=0.35秒)
	static constexpr float artistNameLastYearFadeSec = 0.3f;
	static constexpr float artistNameOtherYearFadeSec = 0.35f;
	// headliners と同じ並びで、各要素が「自分の年の最後のヘッドライナー」であればフェード秒数、
	// そうでなければ -1.0f を保持する。
	std::vector<float> last_headliner_fade_duration_;

	// 各年の最初のヘッドライナーをpop()時刻からフェードインさせるための秒数と対象フラグ。
	static constexpr float artistNameFirstYearFadeInSec = 0.25f;
	// headliners と同じ並びで、各要素が「自分の年の最初のヘッドライナー」であれば true。
	std::vector<bool> first_headliner_of_year_;

	// サイクル内で現在再生中のヘッドライナーのインデックス(0始まり)。
	int current_headliner_in_cycle_ = -1;
	// 直前フレームの年/ヘッドライナーインデックス(サイクル先頭検出用)。
	size_t prev_year_idx_ = static_cast<size_t>(-1);
	int prev_headliner_in_cycle_ = -1;

	// imgタイプのheadlinerが表示開始してから拡大するアニメーション設定
	static constexpr float imgScaleAnimDurationSec = 2.5f;
	static constexpr float imgScaleStart = 1.0f;
	static constexpr float imgScaleEnd = 1.1f;

	std::unique_ptr<Effect_02> effect;

	double cycle_start_sec = 0.0;

	// 1ヘッドライナーあたりの表示時間(asset.jsonのheadlinerDuration)。
	double headliner_duration_ = 3.0;
	// 各サイクル末尾の遷移(次ポスターへの切替)にかける時間。
	double transition_duration_ = 2.0;
	// target_years と整列した、年ごとのヘッドライナー数。
	std::vector<int> headliner_counts_;

	// transition starts 0.25s before the last headliner ends, creating an overlay period
	static constexpr double transition_overlap_sec = 0.25;
	bool in_transition_overlap = false;
	float headliner_alpha_fadeout_start_sec = -1.0f;

	float first_movie_scale = 0.0f;
	float first_movie_scale_start_sec = -1.0f;
	static constexpr float first_movie_scale_anim_duration = 0.5f;

	rxcpp::schedulers::run_loop rl;
	rxcpp::composite_subscription next_cs;
	rxcpp::composite_subscription zoom_cs;
	static constexpr int to_output_scale_duration_ms = 250;
	static constexpr int zoom_out_delay_ms = 400;
	static constexpr int zoom_in_after_next_delay_ms = 500;
	static constexpr int zoom_in_delay_ms = 800;
};
