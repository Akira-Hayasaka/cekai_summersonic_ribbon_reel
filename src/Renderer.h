#include "ofMain.h"
#include "Constants.h"

#include <cstdint>
#include <memory>

struct RenderAudioBlockRequest
{
	double startTimeSec = 0.0;
	uint64_t startSampleFrame = 0;
	uint32_t numFrames = 0;
	int sampleRate = 48000;
	int channels = 2;
};

class Renderer {
public:
	Renderer();
	~Renderer();

	void open_screen_fbo();
	void close_screen_fbo();
	void draw();

	void begin_rendering(
		const float fps,
		const bool to_file_ = false,
		const int audio_sample_rate = 48000,
		const int audio_channels = 2);
	void stop_rendering();
	RenderAudioBlockRequest get_next_audio_block_request() const;
	void feed_to_pipe(const float* interleaved_audio, const uint32_t audio_frame_count);
	void feed_to_pipe();

private:

	const std::string get_ffmpeg_cmd(
		const int w, const int h, const float fps, const std::string vid_name);

	ofFbo screen_fbo;
	bool is_rendering, to_file;
	float render_fps = 60.0f;
	uint64_t render_frame_index = 0;
	int render_audio_sample_rate = 48000;
	int render_audio_channels = 2;
	uint64_t render_audio_frames_written = 0;
	std::string render_base_path;
	std::string video_only_path;
	std::string audio_wav_path;
	std::string muxed_video_path;

	std::string timestamp_for_rendering;
	FILE * ffmpeg_pipe;
	std::unique_ptr<class RendererWavWriter> wav_writer;
};
