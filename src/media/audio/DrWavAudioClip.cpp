#include "audio/DrWavAudioClip.h"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

bool DrWavAudioClip::load(const std::filesystem::path& path)
{
    clear();

    std::error_code ec;
    const auto cwd = std::filesystem::current_path(ec);
    if (ec) {
        std::cerr << "[DrWavAudioClip::load] failed to get cwd. ec=" << ec.message() << std::endl;
    }
    ec.clear();

    const auto absolutePath = std::filesystem::absolute(path, ec);
    if (ec) {
        std::cerr << "[DrWavAudioClip::load] failed to make absolute path. input=" << path
                  << ", ec=" << ec.message() << std::endl;
    }
    ec.clear();

    const bool exists = std::filesystem::exists(path, ec);
    const auto existsEc = ec;
    ec.clear();
    const bool isRegular = std::filesystem::is_regular_file(path, ec);
    const auto regularEc = ec;
    ec.clear();
    const auto fileSize = std::filesystem::file_size(path, ec);
    const auto sizeEc = ec;

    std::cerr
        << "[DrWavAudioClip::load] input=" << path
        << ", cwd=" << (cwd.empty() ? std::filesystem::path("<unknown>") : cwd)
        << ", absolute=" << absolutePath
        << ", exists=" << exists
        << ", is_regular_file=" << isRegular
        << ", file_size=" << (sizeEc ? 0 : fileSize)
        << ", exists_ec=" << (existsEc ? existsEc.message() : "ok")
        << ", regular_ec=" << (regularEc ? regularEc.message() : "ok")
        << ", size_ec=" << (sizeEc ? sizeEc.message() : "ok")
        << std::endl;

    unsigned int loadedChannels = 0;
    unsigned int loadedSampleRate = 0;
    drwav_uint64 loadedFrameCount = 0;

    // dr_wav is decode-only here. Realtime playback is handled by ofSoundStream.
    float* decoded = drwav_open_file_and_read_pcm_frames_f32(
        path.string().c_str(),
        &loadedChannels,
        &loadedSampleRate,
        &loadedFrameCount,
        nullptr);

    if (decoded == nullptr || loadedChannels == 0 || loadedSampleRate == 0 || loadedFrameCount == 0) {
        std::cerr
            << "[DrWavAudioClip::load] drwav_open_file_and_read_pcm_frames_f32 failed."
            << " input=" << path
            << ", absolute=" << absolutePath
            << ", decoded_null=" << (decoded == nullptr)
            << ", channels=" << loadedChannels
            << ", sample_rate=" << loadedSampleRate
            << ", frame_count=" << loadedFrameCount
            << std::endl;

        if (decoded != nullptr) {
            drwav_free(decoded, nullptr);
        }
        return false;
    }

    const uint64_t totalSamples64 = loadedFrameCount * static_cast<uint64_t>(loadedChannels);
    if (totalSamples64 > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
        std::cerr
            << "[DrWavAudioClip::load] sample buffer too large. input=" << path
            << ", total_samples=" << totalSamples64
            << ", size_t_max=" << std::numeric_limits<size_t>::max()
            << std::endl;
        drwav_free(decoded, nullptr);
        return false;
    }

    const size_t totalSamples = static_cast<size_t>(totalSamples64);
    samples.assign(decoded, decoded + totalSamples);
    drwav_free(decoded, nullptr);

    channels = static_cast<int>(loadedChannels);
    sampleRate = static_cast<int>(loadedSampleRate);
    frameCount = static_cast<uint64_t>(loadedFrameCount);

    std::cerr
        << "[DrWavAudioClip::load] decode success. input=" << path
        << ", channels=" << channels
        << ", sample_rate=" << sampleRate
        << ", frame_count=" << frameCount
        << ", duration_sec=" << getDurationSec()
        << std::endl;

    return true;
}

void DrWavAudioClip::clear()
{
    samples.clear();
    channels = 0;
    sampleRate = 0;
    frameCount = 0;
}

bool DrWavAudioClip::isLoaded() const
{
    return !samples.empty() && channels > 0 && sampleRate > 0 && frameCount > 0;
}

int DrWavAudioClip::getChannels() const
{
    return channels;
}

int DrWavAudioClip::getSampleRate() const
{
    return sampleRate;
}

uint64_t DrWavAudioClip::getFrameCount() const
{
    return frameCount;
}

double DrWavAudioClip::getDurationSec() const
{
    if (!isLoaded() || sampleRate <= 0) {
        return 0.0;
    }
    return static_cast<double>(frameCount) / static_cast<double>(sampleRate);
}

float DrWavAudioClip::getSampleNearest(uint64_t frame, int channel) const
{
    if (!isLoaded() || channel < 0 || channel >= channels || frame >= frameCount) {
        return 0.0f;
    }

    const uint64_t index64 = frame * static_cast<uint64_t>(channels) + static_cast<uint64_t>(channel);
    if (index64 >= static_cast<uint64_t>(samples.size())) {
        return 0.0f;
    }

    return samples[static_cast<size_t>(index64)];
}

float DrWavAudioClip::getSampleLinear(double framePos, int channel) const
{
    if (!isLoaded() || channel < 0 || channel >= channels) {
        return 0.0f;
    }
    if (framePos < 0.0 || framePos >= static_cast<double>(frameCount)) {
        return 0.0f;
    }

    const double floorFrame = std::floor(framePos);
    const uint64_t frame0 = static_cast<uint64_t>(floorFrame);
    const uint64_t frame1 = frame0 + 1;
    const float s0 = getSampleNearest(frame0, channel);

    if (frame1 >= frameCount) {
        return s0;
    }

    const float s1 = getSampleNearest(frame1, channel);
    const float t = static_cast<float>(framePos - floorFrame);
    return s0 + (s1 - s0) * t;
}
