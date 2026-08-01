#include "Renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <vector>

#include "media/audio/dr_wav.h"

std::string quote_path(const std::string& path)
{
	return "\"" + path + "\"";
}

class RendererWavWriter
{
public:
	RendererWavWriter() = default;
	~RendererWavWriter()
	{
		close();
	}

	bool open(const std::string& path, int sampleRate, int channels)
	{
		close();

		if (path.empty() || sampleRate <= 0 || channels <= 0)
		{
			return false;
		}

		drwav_data_format format{};
		format.container = drwav_container_riff;
		format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
		format.channels = static_cast<drwav_uint16>(channels);
		format.sampleRate = static_cast<drwav_uint32>(sampleRate);
		format.bitsPerSample = 32;

		const bool ok = drwav_init_file_write(&wav, path.c_str(), &format, nullptr);
		is_open = ok;
		return ok;
	}

	void close()
	{
		if (is_open)
		{
			drwav_uninit(&wav);
			is_open = false;
		}
	}

	bool writeFrames(const float* interleaved, uint64_t frameCount)
	{
		if (!is_open || frameCount == 0)
		{
			return is_open;
		}

		if (interleaved == nullptr)
		{
			return false;
		}

		const drwav_uint64 written = drwav_write_pcm_frames(&wav, frameCount, interleaved);
		return written == frameCount;
	}

	bool isOpen() const
	{
		return is_open;
	}

private:
	drwav wav{};
	bool is_open = false;
};

Renderer::Renderer()
	: is_rendering(false)
	, to_file(false)
	, render_fps(60.0f)
	, render_frame_index(0)
	, render_audio_sample_rate(48000)
	, render_audio_channels(2)
	, render_audio_frames_written(0)
	, timestamp_for_rendering("")
	, ffmpeg_pipe(nullptr)
{
	ofFboSettings settings;
	settings.width = Constants::APP_W;
	settings.height = Constants::APP_H;
	settings.numSamples = 4;
	// Critical: enable RGBA to support alpha blending within FBO
	settings.internalformat = GL_RGBA;
	ofLog() << "width: " << settings.width << ", height: " << settings.height
			<< ", numSamples: " << settings.numSamples << ", internalFormat: GL_RGBA";
	screen_fbo.allocate(settings);
}

Renderer::~Renderer()
{
	if (wav_writer)
	{
		wav_writer->close();
		wav_writer.reset();
	}

	if (ffmpeg_pipe)
	{
		fflush(ffmpeg_pipe);
		_pclose(ffmpeg_pipe);
		ffmpeg_pipe = nullptr;
	}
}

void Renderer::open_screen_fbo()
{
	screen_fbo.begin();
	// Clear with alpha=0 for transparency support
	ofClear(0, 0, 0, 0);
	// Ensure alpha blending is active within FBO
	ofEnableBlendMode(OF_BLENDMODE_ALPHA);
}

void Renderer::close_screen_fbo()
{
	screen_fbo.end();
}

void Renderer::draw()
{
	// The FBO content is composited with premultiplied-alpha inside open_screen_fbo().
	// Use (GL_ONE, GL_ONE_MINUS_SRC_ALPHA) so the FBO is written to the screen
	// without a second alpha multiplication.
	ofEnableBlendMode(OF_BLENDMODE_ALPHA);  // enables GL_BLEND
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	screen_fbo.draw(0, 0);
}

void Renderer::begin_rendering(
	const float fps,
	const bool to_file_,
	const int audio_sample_rate,
	const int audio_channels)
{
	to_file = to_file_;
	if (!to_file)
	{
		return;
	}

	if (is_rendering)
	{
		return;
	}

	is_rendering = true;
	render_fps = fps;
	render_frame_index = 0;
	render_audio_sample_rate = std::max(1, audio_sample_rate);
	render_audio_channels = std::max(1, audio_channels);
	render_audio_frames_written = 0;

	timestamp_for_rendering = ofGetTimestampString("%Y-%m-%d-%H-%M-%S-%i");
	const std::filesystem::path base_dir(timestamp_for_rendering);
	std::error_code ec;
	std::filesystem::create_directories(base_dir, ec);
	if (ec)
	{
		ofLogWarning("Renderer::begin_rendering") << "create_directories failed: " << ec.message();
	}

	render_base_path = base_dir.generic_string();
	video_only_path = (base_dir / "video_only.mov").generic_string();
	audio_wav_path = (base_dir / "audio.wav").generic_string();
	muxed_video_path = (base_dir / "render.mov").generic_string();

	ofLogNotice("Renderer::begin_rendering")
		<< "fps=" << render_fps
		<< ", audio_sample_rate=" << render_audio_sample_rate
		<< ", audio_channels=" << render_audio_channels
		<< ", video_only_path=" << video_only_path
		<< ", audio_wav_path=" << audio_wav_path
		<< ", muxed_video_path=" << muxed_video_path;

	const std::string ffmpeg_cmd = get_ffmpeg_cmd(
		Constants::APP_W,
		Constants::APP_H,
		render_fps,
		video_only_path);

	ffmpeg_pipe = _popen(ffmpeg_cmd.c_str(), "wb");
	if (!ffmpeg_pipe)
	{
		ofLogError("Renderer::begin_rendering") << "Failed to start ffmpeg process for rendering.";
		is_rendering = false;
		return;
	}

	wav_writer = std::make_unique<RendererWavWriter>();
	if (!wav_writer->open(audio_wav_path, render_audio_sample_rate, render_audio_channels))
	{
		ofLogError("Renderer::begin_rendering") << "Failed to open wav writer: " << audio_wav_path;
		fflush(ffmpeg_pipe);
		_pclose(ffmpeg_pipe);
		ffmpeg_pipe = nullptr;
		wav_writer.reset();
		is_rendering = false;
	}
}

void Renderer::stop_rendering()
{
	if (!is_rendering)
	{
		return;
	}

	is_rendering = false;

	if (to_file && ffmpeg_pipe)
	{
		fflush(ffmpeg_pipe);
		const int close_result = _pclose(ffmpeg_pipe);
		ofLogNotice("Renderer::stop_rendering") << "_pclose result=" << close_result;
		ffmpeg_pipe = nullptr;
	}

	if (wav_writer)
	{
		wav_writer->close();
		wav_writer.reset();
	}

	if (to_file)
	{
		const std::string mux_cmd =
			"ffmpeg -y -i " + quote_path(video_only_path) +
			" -i " + quote_path(audio_wav_path) +
			" -map 0:v:0 -map 1:a:0 -c:v copy -c:a aac -b:a 320k -shortest " +
			quote_path(muxed_video_path);

		ofLogNotice("Renderer::stop_rendering") << "mux cmd=" << mux_cmd;
		const int mux_result = std::system(mux_cmd.c_str());
		ofLogNotice("Renderer::stop_rendering") << "mux result=" << mux_result;
		if (mux_result != 0)
		{
			ofLogError("Renderer::stop_rendering") << "ffmpeg mux failed. result=" << mux_result;
		}

		ofLogNotice() << "Finished rendering video: " << timestamp_for_rendering
					  << " -> " << muxed_video_path;
	}
}

RenderAudioBlockRequest Renderer::get_next_audio_block_request() const
{
	RenderAudioBlockRequest req;
	req.sampleRate = render_audio_sample_rate;
	req.channels = render_audio_channels;
	req.startSampleFrame = render_audio_frames_written;
	req.startTimeSec = static_cast<double>(render_audio_frames_written) / static_cast<double>(render_audio_sample_rate);

	if (render_fps <= 0.0f || render_audio_sample_rate <= 0)
	{
		req.numFrames = 0;
		return req;
	}

	const uint64_t targetEnd = static_cast<uint64_t>(std::llround(
		(static_cast<double>(render_frame_index) + 1.0) * static_cast<double>(render_audio_sample_rate) / static_cast<double>(render_fps)));
	const uint64_t numFrames64 = (targetEnd > render_audio_frames_written)
		? (targetEnd - render_audio_frames_written)
		: 0;
	req.numFrames = static_cast<uint32_t>(numFrames64);
	return req;
}

void Renderer::feed_to_pipe(const float* interleaved_audio, const uint32_t audio_frame_count)
{
	if (!is_rendering || !to_file || !ffmpeg_pipe)
	{
		return;
	}

	ofPixels px;
	screen_fbo.readToPixels(px);
	px.setImageType(OF_IMAGE_COLOR);

	const size_t video_bytes = px.size();
	const size_t bytes_written = fwrite(px.getData(), 1, video_bytes, ffmpeg_pipe);
	fflush(ffmpeg_pipe);
	if (bytes_written != video_bytes)
	{
		ofLogError("Renderer::feed_to_pipe") << "video fwrite error. wrote=" << bytes_written << " / required=" << video_bytes;
		return;
	}

	const auto req = get_next_audio_block_request();
	if (!wav_writer || !wav_writer->isOpen())
	{
		ofLogError("Renderer::feed_to_pipe") << "wav writer is not open.";
		return;
	}

	std::vector<float> audio_buffer(static_cast<size_t>(req.numFrames) * static_cast<size_t>(req.channels), 0.0f);
	const uint32_t copyFrames = std::min(audio_frame_count, req.numFrames);
	if (audio_frame_count != req.numFrames)
	{
		ofLogWarning("Renderer::feed_to_pipe")
			<< "audio frame count mismatch. expected=" << req.numFrames
			<< ", got=" << audio_frame_count
			<< ", using=" << copyFrames;
	}

	if (interleaved_audio != nullptr && copyFrames > 0)
	{
		const size_t copySamples = static_cast<size_t>(copyFrames) * static_cast<size_t>(req.channels);
		std::copy(interleaved_audio, interleaved_audio + copySamples, audio_buffer.begin());
	}

	if (!wav_writer->writeFrames(audio_buffer.data(), req.numFrames))
	{
		ofLogError("Renderer::feed_to_pipe") << "audio wav write failed. frames=" << req.numFrames;
	}

	render_audio_frames_written += req.numFrames;
	render_frame_index++;
}

void Renderer::feed_to_pipe()
{
	const auto req = get_next_audio_block_request();
	feed_to_pipe(nullptr, req.numFrames);
}

const std::string Renderer::get_ffmpeg_cmd(
	const int w, const int h, const float fps, const std::string vid_name)
{
	// return "ffmpeg -y -f rawvideo -pix_fmt rgb24 -s " +
	// 	std::to_string(w) + "x" +
	// 	std::to_string(h) + " -r " +
	// 	ofToString(fps, 2) +
	// 	" -i - -c:v libx265 -preset fast -crf 18 -pix_fmt yuv420p " +
	// 	quote_path(vid_name);

	return
	"ffmpeg -y "
	"-f rawvideo "
	"-pix_fmt rgb24 "
	"-video_size " + std::to_string(w) + "x" + std::to_string(h) + " "
	"-framerate " + ofToString(fps, 6) + " "
	"-i pipe:0 "
	"-c:v rawvideo "
	"-pix_fmt rgb24 " +
	quote_path(vid_name);
}
