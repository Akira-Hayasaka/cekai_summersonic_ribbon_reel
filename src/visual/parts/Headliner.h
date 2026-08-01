#pragma once
#include <limits>

#include "ofMain.h"
#include "ExtremeGPUVideoPlayer.h"
#include "Sequencer.h"

class Headliner
{
public:
	enum class MediaType { Movie, Img };

	Headliner(
		const int year,
		const std::string artist_name_,
		const std::string media_path,
		const std::string audio_path,
		MediaType media_type = MediaType::Movie);

	void update();
	void draw();
	void draw(
		const float x = 0, const float y = 0,
		const float w = 1080, const float h = 1350,
		const float sx = 0, const float sy = 0,
		const float sw = 1080, const float sh = 1350);
	bool isActive() const;
	bool hasAudio() const { return media_type == MediaType::Movie; }
	void setActivationWindow(float startSec, float durationSec);
	void setFileOffsetFromBeginning(float offsetSec);
	void setPosition(float x, float y);
	void setSubsection(float sx, float sy, float sw, float sh);
	void setScale(float scale);
	void setArtistNameViewPadding(float left, float bottom);
	void setAudioEnabled(bool enabled);
	bool isAudioEnabled() const;
	void setOfflineRenderMode(bool enabled);
	bool isOfflineRenderMode() const;

	float sample_audio_at(const float _sec);
	void renderAudioForOfflineRender(
		float* out,
		size_t numFrames,
		int outChannels,
		double outputSampleRate) const;

	const int getYear() const { return year; }
	const std::string getArtistName() const { return artist_name; }
	const float getActivationStartSec() const { return activation_start_sec; }
	const float getActivationDurationSec() const { return activation_duration_sec; }
	const std::string get_videoPath() const { return video_path; }
	const std::string get_audioPath() const { return audio_path; }
	const glm::vec4 getCropSubsection() const { return crop_subsection; }
	bool isImageType() const { return media_type == MediaType::Img; }
	const float getArtistNameViewPaddingLeft() const { return artist_name_view_padding_left; }
	const float getArtistNameViewPaddingBottom() const { return artist_name_view_padding_bottom; }

private:
	bool isActiveAtTime(float timelineSec) const;

	void on_SequencerFrameEvaluatedEvent(SequencerFrameEvaluatedEvent & e);
	void on_SequencerEndReachedEvent(SequencerEndReachedEvent & e);

	int year;
	std::string artist_name;
	std::string video_path, audio_path;

	std::unique_ptr<ExtremeGPUVideoPlayer> video_player;
	ofImage static_image;
	MediaType media_type = MediaType::Movie;

	float activation_start_sec = 0.0f;
	float activation_duration_sec = std::numeric_limits<float>::max();
	float file_offset_sec_from_beginning = 0.0f;

	glm::vec4 crop_subsection;
	glm::vec2 draw_position;
	float draw_scale = 1.0f;
	float artist_name_view_padding_left = 80.0f;
	float artist_name_view_padding_bottom = 70.0f;

	bool active = true;
	bool offline_render_mode = false;
	double last_evaluated_timeline_time_sec = 0.0;
	double last_evaluated_movie_time_sec = 0.0;
};
