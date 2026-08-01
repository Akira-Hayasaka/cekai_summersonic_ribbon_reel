#include <algorithm>

#include "Headliner.h"

Headliner::Headliner(
	const int year,
	const std::string artist_name_,
	const std::string media_path,
	const std::string audio_path,
	MediaType media_type)
	: year(year)
	, artist_name(artist_name_)
	, draw_position(50.0f, 50.0f)
	, crop_subsection(500.0f, 80.0f, 850.0f, 900.0f)
	, video_path(media_path)
	, audio_path(audio_path)
	, media_type(media_type)
{
	ofAddListener(Sequencer::frameEvaluatedEvent, this, &Headliner::on_SequencerFrameEvaluatedEvent);
	ofAddListener(Sequencer::endReachedEvent, this, &Headliner::on_SequencerEndReachedEvent);

	if (media_type == MediaType::Movie)
	{
		video_player = std::make_unique<ExtremeGPUVideoPlayer>();
		video_player->load(
			ofxExtremeGpuVideo::Mode::GPU_VIDEO_STREAMING_FROM_STORAGE,
			media_path,
			audio_path);
		//video_player->play();
	}
	else
	{
		static_image.load(media_path);
	}
}

void Headliner::update()
{
	//video_player->update();
}

void Headliner::draw() {
	if (!active) {
		return;
	}

	const ofTexture& tex = (media_type == MediaType::Movie)
		? video_player->getTexture()
		: static_image.getTexture();
	auto x = draw_position.x;
	auto y = draw_position.y;
	auto w = crop_subsection.z;
	auto h = crop_subsection.w;
	auto sx = crop_subsection.x;
	auto sy = crop_subsection.y;
	auto sw = crop_subsection.z;
	auto sh = crop_subsection.w;

	const auto safe_scale = draw_scale > 0.0f ? draw_scale : 1.0f;

	ofPushMatrix();
	ofTranslate(x, y);
	ofScale(safe_scale, safe_scale);
	tex.drawSubsection(0, 0, w, h, sx, sy, sw, sh);
	ofPopMatrix();
}

void Headliner::draw(
	const float x, const float y,
	const float w, const float h,
	const float sx, const float sy,
	const float sw, const float sh) {

	if (!active)
	{
		return;
	}

	const ofTexture& tex = (media_type == MediaType::Movie)
		? video_player->getTexture()
		: static_image.getTexture();

	ofPushMatrix();
	ofTranslate(x, y);
	ofScale(draw_scale, draw_scale);
	tex.drawSubsection(0, 0, w, h, sx, sy, sw, sh);
	ofPopMatrix();

}
bool Headliner::isActive() const
{
	return active;
}

void Headliner::setActivationWindow(float startSec, float durationSec)
{
	activation_start_sec = startSec;
	activation_duration_sec = std::max(0.0f, durationSec);
	active = isActiveAtTime(static_cast<float>(last_evaluated_timeline_time_sec));
}

void Headliner::setFileOffsetFromBeginning(float offsetSec)
{
	file_offset_sec_from_beginning = offsetSec;
}

void Headliner::setPosition(float x, float y)
{
	draw_position = glm::vec2(x, y);
}

void Headliner::setSubsection(float sx, float sy, float sw, float sh)
{
	crop_subsection = glm::vec4(sx, sy, sw, sh);
}

void Headliner::setScale(float scale)
{
	draw_scale = scale > 0.0f ? scale : 1.0f;
}

void Headliner::setArtistNameViewPadding(float left, float bottom)
{
	artist_name_view_padding_left = left;
	artist_name_view_padding_bottom = bottom;
}

void Headliner::setAudioEnabled(bool enabled)
{
	if (!video_player)
	{
		return;
	}
	video_player->setAudioEnabled(enabled);
}

bool Headliner::isAudioEnabled() const
{
	if (!video_player)
	{
		return false;
	}
	return video_player->isAudioEnabled();
}

void Headliner::setOfflineRenderMode(bool enabled)
{
	offline_render_mode = enabled;
	ofLogNotice("Headliner::setOfflineRenderMode") << (enabled ? "enabled" : "disabled");
}

bool Headliner::isOfflineRenderMode() const
{
	return offline_render_mode;
}

bool Headliner::isActiveAtTime(float timelineSec) const
{
	const float active_end_sec = activation_start_sec + activation_duration_sec;
	return timelineSec >= activation_start_sec && timelineSec < active_end_sec;
}

void Headliner::on_SequencerFrameEvaluatedEvent(SequencerFrameEvaluatedEvent & e)
{
	last_evaluated_timeline_time_sec = e.timeSec;
	active = isActiveAtTime(static_cast<float>(e.timeSec));
	if (!active)
	{
		return;
	}

	const double movieTimeSec =
		static_cast<double>(e.timeSec)
		- static_cast<double>(activation_start_sec)
		+ static_cast<double>(file_offset_sec_from_beginning);
	last_evaluated_movie_time_sec = movieTimeSec;

	if (!video_player)
	{
		return;
	}

	const float duration = video_player->getDuration();
	if (duration > 0.0f)
	{
		const float pct = ofClamp(static_cast<float>(movieTimeSec / static_cast<double>(duration)), 0.0f, 1.0f);
		if (offline_render_mode)
		{
			video_player->setPositionForOfflineRender(pct);
		}
		else
		{
			video_player->setPosition(pct);
		}
	}
}

void Headliner::renderAudioForOfflineRender(
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

	if (!active)
	{
		return;
	}

	if (!video_player)
	{
		return;
	}

	video_player->renderAudioAtMovieTimeSec(
		last_evaluated_movie_time_sec,
		out,
		numFrames,
		outChannels,
		outputSampleRate);
}

float Headliner::sample_audio_at(const float _sec)
{
	(void)_sec;
	return 0.0f;
}

void Headliner::on_SequencerEndReachedEvent(SequencerEndReachedEvent & e)
{
	ofLog() << "End reached event: frame " << e.frame
			<< ", source " << static_cast<int>(e.source);
}
