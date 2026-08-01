#pragma once

#include <algorithm>

#include "ofMain.h"
#include "../audio/DrWavAudioPlayer.h"
#include "ofxExtremeGpuVideo.hpp"

class ExtremeGPUVideoPlayer
{
public:

	ExtremeGPUVideoPlayer() :
		state(State::init),
		last_frm_num(0), 
		cur_time(ofGetElapsedTimef()),
		movie_finish_time(ofGetElapsedTimef()),
		speed(1.0), 
		b_loop(true),
		b_use_sound(false),
		b_audio_enabled(true),
		b_frame_new(false)
	{}

	~ExtremeGPUVideoPlayer() {}

	void load(
		ofxExtremeGpuVideo::Mode _mode,
		const std::filesystem::path _movie_path,
		const std::filesystem::path _audio_path = "")
	{
		auto data_audio_path = "data/" + _audio_path.string();

		ofLogNotice(__FUNCTION__) << "movie_path=" << _movie_path << ", audio_path=" << data_audio_path;
		std::error_code ec;
		auto cwd = std::filesystem::current_path(ec);
		if (!ec)
		{
			ofLogNotice(__FUNCTION__) << "cwd=" << cwd;
		}
		ec.clear();
		auto abs_audio = std::filesystem::absolute(data_audio_path, ec);
		if (!ec)
		{
			ofLogNotice(__FUNCTION__) << "audio_absolute_path=" << abs_audio;
		}
		ec.clear();
		if (!data_audio_path.empty())
		{
			auto exists = std::filesystem::exists(data_audio_path, ec);
			auto exists_ec = ec;
			ec.clear();
			auto is_regular = std::filesystem::is_regular_file(data_audio_path, ec);
			auto regular_ec = ec;
			ec.clear();
			auto fsize = std::filesystem::file_size(data_audio_path, ec);
			ofLogNotice(__FUNCTION__)
				<< "audio_exists=" << exists
				<< ", audio_is_regular_file=" << is_regular
				<< ", audio_file_size=" << (ec ? 0 : fsize)
				<< ", exists_ec=" << (exists_ec ? exists_ec.message() : "ok")
				<< ", regular_ec=" << (regular_ec ? regular_ec.message() : "ok")
				<< ", size_ec=" << (ec ? ec.message() : "ok");
		}

		audio.close();
		b_use_sound = false;

		extreme_gpu_video.load(_movie_path.string(), _mode);
		state = State::loaded;

		fbo.allocate(getWidth(), getHeight());
		fbo.begin();
		ofClear(0);
		fbo.end();

		if (_audio_path == "")
		{
			ofLogNotice(__FUNCTION__) << "audio_path is empty. skip audio load.";
			b_use_sound = false;
		}
		else
		{
			b_use_sound = audio.load(data_audio_path);
			ofLogNotice(__FUNCTION__) << "audio.load result=" << b_use_sound;
			if (b_use_sound)
			{
				audio.setupOutput(48000, 512, 2);
				audio.setLoop(false);
				audio.setSpeed(speed);
				ofLogNotice(__FUNCTION__) << "audio setup complete. out_sr=48000, out_buf=512, out_ch=2, speed=" << speed;
			}
			else
			{
				ofLogWarning(__FUNCTION__) << "Failed to load wav: " << data_audio_path;
			}
		}
	}

	void close()
	{
		audio.close();
		b_use_sound = false;
	}

	bool isInitialized()
	{
		if (state != State::init)
			return true;
		else
			return false;
	}

	void update()
	{
		auto update_mov = [&]() -> void
		{
			auto fnum = get_current_frame_number_based_on_speed();

			extreme_gpu_video.setFrame(fnum);
			extreme_gpu_video.update();

			feed_fbo();
		};

		cur_time = ofGetElapsedTimef();

		if (state == State::playing)
		{
			auto b_mov_done = is_mov_done();

			if (!b_loop)
			{
				if (b_mov_done)
				{
					stop();
					state = State::done;
				}
				else
					update_mov();
			}
			else
			{
				if (b_mov_done)
					resume();

				update_mov();
			}
		}
	}

	void draw(const float _x, const float _y)
	{
		draw(_x, _y, getWidth(), getHeight());
	}

	void draw(
		const float _x, const float _y,
		const float _w, const float _h)
	{
		if (state != State::init)
		{
			fbo.draw(_x, _y, _w, _h);
		}
	}

	void setPositionForOfflineRender(float pct)
	{
		if (state == State::init)
		{
			return;
		}

		const int frameCount = extreme_gpu_video.getFrameCount();
		if (frameCount <= 0)
		{
			return;
		}

		const float clampedPct = ofClamp(pct, 0.0f, 1.0f);
		const float frame = ofMap(
			clampedPct,
			0.0f,
			1.0f,
			0.0f,
			static_cast<float>(frameCount - 1),
			true);
		setFrameForOfflineRender(static_cast<int>(frame));
	}

	void setFrameForOfflineRender(int frame)
	{
		if (state == State::init)
		{
			return;
		}

		const int frameCount = extreme_gpu_video.getFrameCount();
		if (frameCount <= 0)
		{
			return;
		}

		const int fnum = ofClamp(frame, 0, frameCount - 1);
		extreme_gpu_video.setFrame(fnum);
		extreme_gpu_video.update();
		feed_fbo();
	}

	void renderAudioAtMovieTimeSec(
		double movieTimeSec,
		float* out,
		size_t numFrames,
		int outChannels,
		double outputSampleRate) const
	{
		if (out == nullptr || numFrames == 0 || outChannels <= 0)
		{
			return;
		}

		std::fill(out, out + (numFrames * static_cast<size_t>(outChannels)), 0.0f);

		if (!b_use_sound || !b_audio_enabled)
		{
			return;
		}

		audio.renderBlockAtMovieTimeSec(
			movieTimeSec,
			out,
			numFrames,
			outChannels,
			outputSampleRate,
			1.0f,
			audio.getVolume(),
			b_loop);
	}

	bool isFrameNew()
	{
		return b_frame_new;
	}

	void play()
	{
		// Video is master. Audio is aligned only at transition points (resume/seek/pause/stop).
		if (state != State::init)
		{
			if (state == State::scrubbing)
			{
				auto& vid = extreme_gpu_video;
				auto mov_dur = vid.getFrameCount() / (vid.getFramePerSecond() * speed);

				auto scrub_timeat_mov = ofMap(
					vid.getFrameAt(), 0, vid.getFrameCount(),
					0.0, mov_dur, true);
				auto diff_time = (scrub_begin_timeat_mov - scrub_timeat_mov);
				auto pausing_dur = cur_time - pausing_begin_time;

				movie_finish_time += pausing_dur + diff_time;

				if (b_use_sound)
				{
					auto scrub_pct = ofMap(vid.getFrameAt(), 0, vid.getFrameCount(), 0.0, 1.0, true);
					audio.playFromPct(scrub_pct);
				}
			}
			else
				resume();

			state = State::playing;
		}
	}

	void stop()
	{
		if (state != State::init)
		{
			state = State::stop;
			reset();

			if (b_use_sound)
				audio.stop();
		}
	}

	void setPaused(const bool _b_paused)
	{
		if (_b_paused)
		{
			if (state == State::playing)
			{
				state = State::paused;
				pausing_begin_time = cur_time;

				if (b_use_sound)
					audio.setPaused(true);
			}
		}
		else
		{
			if (state == State::paused)
			{
				state = State::playing;

				auto pausing_dur = cur_time - pausing_begin_time;
				movie_finish_time += pausing_dur;

				if (b_use_sound)
					audio.setPaused(false);
			}
		}
	}

	bool isPaused()
	{
		if (state == State::paused)
			return true;
		else
			return false;
	}

	float getWidth()
	{
		if (state != State::init)
			return extreme_gpu_video.getWidth();
		else
			return 0.0;
	}

	float getHeight()
	{
		if (state != State::init)
			return extreme_gpu_video.getHeight();
		else
			return 0.0;
	}

	bool isLoaded()
	{
		if (state != State::init)
			return true;
		else
			return false;
	}

	bool isPlaying()
	{
		if (state == State::playing)
			return true;
		else
			return false;
	}

	ofPixels& getPixels()
	{
		fbo.readToPixels(px);
		return px;
	}

	ofTexture& getTexture()
	{
		return fbo.getTexture();
	}

	float getDuration()
	{
		if (state != State::init)
			return extreme_gpu_video.getDuration();
		else
			return 0.0;
	}

	bool getIsMovieDone()
	{
		if (state == State::done)
			return true;
		else
			return false;
	}

	int getCurrentFrame()
	{
		if (state != State::init)
			return extreme_gpu_video.getFrameAt();
		else
			return 0;
	}

	int getTotalNumFrames()
	{
		if (state != State::init)
			return extreme_gpu_video.getFrameCount();
		else
			return 0;
	}

	float getPosition()
	{
		if (state != State::init)
			return ofMap(extreme_gpu_video.getFrameAt(), 0, extreme_gpu_video.getFrameCount(), 0.0, 1.0, true);
		else
			return 0.0;
	}

	void setPosition(const float _pct)
	{
		auto frm = ofMap(_pct, 0.0, 1.0, 0, extreme_gpu_video.getFrameCount(), true);
		setFrame(frm);
	}

	void setFrame(const int _frame)
	{
		if (state == State::paused)
			return;

		if (state != State::init)
		{
			auto& vid = extreme_gpu_video;
			
			if (state != State::scrubbing)
			{
				pausing_begin_time = cur_time;
				auto mov_dur = vid.getFrameCount() / (vid.getFramePerSecond() * speed);
				scrub_begin_timeat_mov = ofMap(vid.getFrameAt(), 0, vid.getFrameCount(), 0.0, mov_dur, true);;
				state = State::scrubbing;
			}

			last_frm_num = 0;

			auto fnum = ofClamp(_frame, 0, vid.getFrameCount());
			vid.setFrame(fnum);
			vid.update();

			if (b_use_sound)
			{
				auto scrub_pct = ofMap((float)fnum, 0.0f, (float)vid.getFrameCount(), 0.0f, 1.0f, true);
				// Do not start normal playback while scrubbing. Play only a short preview.
				audio.triggerScrubPreview(scrub_pct, 35.0f);
			}

			feed_fbo();
		}
	}

	float getSpeed()
	{
		return speed;
	}

	void setSpeed(const float _speed)
	{
		speed = _speed;

		if (b_use_sound)
			audio.setSpeed(speed);
	}

	void setLoopState(const bool _b_loop)
	{
		b_loop = _b_loop;
	}

	void setVolume(const float _volume)
	{
		if (b_use_sound)
			audio.setVolume(_volume);
	}

	void setAudioEnabled(bool enabled)
	{
		b_audio_enabled = enabled;
	}

	bool isAudioEnabled() const
	{
		return b_audio_enabled;
	}

	ofEvent<void> mov_finish_event;

protected:

	void resume()
	{
		reset();

		if (b_use_sound)
		{
			audio.playFromPct(0.0f);
		}
	}

	void reset()
	{
		last_frm_num = 0;
		movie_finish_time = cur_time;
		extreme_gpu_video.setFrame(0);
		extreme_gpu_video.update();
	}

	int get_current_frame_number_based_on_speed()
	{
		auto& vid = extreme_gpu_video;
		auto total_frame_on_true_fps = vid.getFrameCount();
		auto dur = get_movie_dur_by_speed();
		auto elapsed = cur_time - movie_finish_time;
		auto cur_time = ofClamp(fmod(elapsed, dur), 0.0, dur);
		auto the_fps = vid.getFramePerSecond() * speed;
		int frame = ofClamp(cur_time * the_fps, 0, total_frame_on_true_fps);
		//ofLogNotice(__FUNCTION__) << id_str << " " << _movie->type_str << " time = " << cur_time << " / " << dur;
		return frame;
	}

	bool is_mov_done()
	{
		auto& vid = extreme_gpu_video;
		int cur_frm_num = vid.getFrameAt();

		if (cur_frm_num != last_frm_num)
			b_frame_new = true;
		else
			b_frame_new = false;

		if (cur_frm_num == vid.getFrameCount() - 1)
		{
			//ofLogNotice(__FUNCTION__) << id_str << " done " << type_str << " movie at " << ofToString(cur_frm_num) << ", elp:" << Globals::ELAPSED_TIME;
			last_frm_num = 0;
			vid.setFrame(0);
			vid.update();
			movie_finish_time = cur_time;
			ofNotifyEvent(mov_finish_event);
			return true;
		}
		else if (cur_frm_num < last_frm_num)
		{
			//ofLogNotice(__FUNCTION__) << id_str << " done " << type_str << " movie at " << ofToString(cur_frm_num) << " and last frm is " << last_frm_num << ", elp:" << Globals::ELAPSED_TIME;
			last_frm_num = 0;
			vid.setFrame(0);
			vid.update();
			movie_finish_time = cur_time;
			ofNotifyEvent(mov_finish_event);
			return true;
		}
		else
		{
			//ofLogNotice(__FUNCTION__) << id_str << " playing " << type_str << " movie at " << ofToString(cur_frm_num) << " and last frm is " << last_frm_num << ", elp:" << Globals::ELAPSED_TIME;
			last_frm_num = cur_frm_num;
			return false;
		}
	}

	float get_movie_dur_by_speed()
	{
		auto& vid = extreme_gpu_video;
		auto dur = vid.getDuration() / speed;
		return dur;
	}

	void feed_fbo()
	{
		fbo.begin();
		ofClear(0);
		extreme_gpu_video.begin();
		extreme_gpu_video.getPlaceHolderTexture().draw(0, 0);
		extreme_gpu_video.end();
		fbo.end();
	}

	enum struct State { init, loaded, stop, paused, playing, scrubbing, done };
	State state;
	float speed;
	float cur_time, movie_finish_time, pausing_begin_time, scrub_begin_timeat_mov;
	int last_frm_num;
	bool b_loop;
	bool b_frame_new;

	ofxExtremeGpuVideo extreme_gpu_video;
	ofFbo fbo;
	ofPixels px;

	DrWavAudioPlayer audio;
	bool b_use_sound;
	bool b_audio_enabled;
};
