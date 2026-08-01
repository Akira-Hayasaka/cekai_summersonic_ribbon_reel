#pragma once

#include "audio/DrWavAudioClip.h"
#include "ofMain.h"

#include <atomic>
#include <cstddef>
#include <filesystem>

class DrWavAudioPlayer : public ofBaseSoundOutput
{
public:
    bool load(const std::filesystem::path& path);
    void close();

    void setupOutput(int sampleRate = 48000, int bufferSize = 512, int outChannels = 2);

    void play();
    void playFromPct(float pct);
    void stop();
    void setPaused(bool pausedValue);
    bool isPlaying() const;

    void setPositionPct(float pct);
    float getPositionPct() const;

    void setSpeed(float speedValue);
    float getSpeed() const;

    void setVolume(float volumeValue);
    float getVolume() const;

    void setLoop(bool loopValue);
    bool getLoop() const;

    void triggerScrubPreview(float pct, float durationMs = 35.0f);

    void audioOut(ofSoundBuffer& outBuffer) override;

    // Shared rendering core used by soundcard output and future offline rendering.
    void renderBlock(float* out, size_t numFrames, int outChannels, double outputSampleRateValue);
    void renderBlockAtSourceFrame(
        double sourceFrameStart,
        float* out,
        size_t numFrames,
        int outChannels,
        double outputSampleRateValue,
        float speedValue,
        float volumeValue,
        bool loopValue) const;

    void renderBlockAtMovieTimeSec(
        double movieTimeSec,
        float* out,
        size_t numFrames,
        int outChannels,
        double outputSampleRateValue,
        float speedValue,
        float volumeValue,
        bool loopValue) const;

private:
    DrWavAudioClip clip;
    ofSoundStream stream;

    std::atomic<bool> loaded { false };
    std::atomic<bool> playing { false };
    std::atomic<bool> paused { false };
    std::atomic<bool> loop { false };
    std::atomic<float> speed { 1.0f };
    std::atomic<float> volume { 1.0f };
    std::atomic<double> sourceFramePos { 0.0 };

    int outputSampleRate = 48000;
    int outputBufferSize = 512;
    int outputChannels = 2;
    bool streamSetupDone = false;

    std::atomic<bool> scrubPreviewActive { false };
    std::atomic<double> scrubPreviewFramePos { 0.0 };
    std::atomic<uint64_t> scrubPreviewRemainingFrames { 0 };
    std::atomic<uint64_t> scrubPreviewTotalFrames { 0 };
};
