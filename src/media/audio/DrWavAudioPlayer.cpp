#include "audio/DrWavAudioPlayer.h"

#include <algorithm>
#include <cmath>

namespace
{
float clampSample(float value)
{
    return ofClamp(value, -1.0f, 1.0f);
}

float readStereoAsChannel(const DrWavAudioClip& clip, double framePos, int outChannel)
{
    const int srcChannels = clip.getChannels();
    if (srcChannels <= 0) {
        return 0.0f;
    }

    if (srcChannels == 1) {
        return clip.getSampleLinear(framePos, 0);
    }

    if (outChannel == 0) {
        return clip.getSampleLinear(framePos, 0);
    }

    if (outChannel == 1) {
        return clip.getSampleLinear(framePos, 1);
    }

    return 0.0f;
}
}

bool DrWavAudioPlayer::load(const std::filesystem::path& path)
{
    ofLogNotice("DrWavAudioPlayer::load") << "start. path=" << path;

    stop();
    const bool ok = clip.load(path);
    loaded.store(ok);
    sourceFramePos.store(0.0);

    if (ok) {
        ofLogNotice("DrWavAudioPlayer::load")
            << "success. path=" << path
            << ", channels=" << clip.getChannels()
            << ", sample_rate=" << clip.getSampleRate()
            << ", frame_count=" << clip.getFrameCount();
    } else {
        ofLogWarning("DrWavAudioPlayer::load") << "failed. path=" << path;
    }

    return ok;
}

void DrWavAudioPlayer::close()
{
    stop();

    if (streamSetupDone) {
        stream.stop();
        stream.close();
        streamSetupDone = false;
    }

    clip.clear();
    loaded.store(false);
}

void DrWavAudioPlayer::setupOutput(int sampleRate, int bufferSize, int outChannels)
{
    outputSampleRate = std::max(8000, sampleRate);
    outputBufferSize = std::max(64, bufferSize);
    outputChannels = std::max(1, outChannels);

    if (streamSetupDone) {
        return;
    }

    ofSoundStreamSettings settings;
    settings.setOutListener(this);
    settings.sampleRate = outputSampleRate;
    settings.numOutputChannels = outputChannels;
    settings.numInputChannels = 0;
    settings.bufferSize = outputBufferSize;

    stream.setup(settings);
    streamSetupDone = true;
}

void DrWavAudioPlayer::play()
{
    if (!loaded.load()) {
        return;
    }

    paused.store(false);
    playing.store(true);
}

void DrWavAudioPlayer::playFromPct(float pct)
{
    setPositionPct(pct);
    play();
}

void DrWavAudioPlayer::stop()
{
    playing.store(false);
    paused.store(false);
    sourceFramePos.store(0.0);

    scrubPreviewActive.store(false);
    scrubPreviewRemainingFrames.store(0);
    scrubPreviewTotalFrames.store(0);
}

void DrWavAudioPlayer::setPaused(bool pausedValue)
{
    if (!loaded.load()) {
        return;
    }

    paused.store(pausedValue);
}

bool DrWavAudioPlayer::isPlaying() const
{
    return loaded.load() && playing.load() && !paused.load();
}

void DrWavAudioPlayer::setPositionPct(float pct)
{
    if (!loaded.load()) {
        sourceFramePos.store(0.0);
        return;
    }

    const uint64_t frameCount = clip.getFrameCount();
    if (frameCount == 0) {
        sourceFramePos.store(0.0);
        return;
    }

    const float clampedPct = ofClamp(pct, 0.0f, 1.0f);
    const double maxFrame = static_cast<double>(frameCount - 1);
    sourceFramePos.store(maxFrame * static_cast<double>(clampedPct));
}

float DrWavAudioPlayer::getPositionPct() const
{
    if (!loaded.load()) {
        return 0.0f;
    }

    const uint64_t frameCount = clip.getFrameCount();
    if (frameCount <= 1) {
        return 0.0f;
    }

    const double maxFrame = static_cast<double>(frameCount - 1);
    const double pct = sourceFramePos.load() / maxFrame;
    return ofClamp(static_cast<float>(pct), 0.0f, 1.0f);
}

void DrWavAudioPlayer::setSpeed(float speedValue)
{
    speed.store(std::max(0.001f, speedValue));
}

float DrWavAudioPlayer::getSpeed() const
{
    return speed.load();
}

void DrWavAudioPlayer::setVolume(float volumeValue)
{
    volume.store(ofClamp(volumeValue, 0.0f, 1.0f));
}

float DrWavAudioPlayer::getVolume() const
{
    return volume.load();
}

void DrWavAudioPlayer::setLoop(bool loopValue)
{
    loop.store(loopValue);
}

bool DrWavAudioPlayer::getLoop() const
{
    return loop.load();
}

void DrWavAudioPlayer::triggerScrubPreview(float pct, float durationMs)
{
    if (!loaded.load()) {
        return;
    }

    const uint64_t frameCount = clip.getFrameCount();
    if (frameCount == 0) {
        return;
    }

    const float clampedPct = ofClamp(pct, 0.0f, 1.0f);
    const double maxFrame = static_cast<double>(frameCount - 1);
    const double startFramePos = maxFrame * static_cast<double>(clampedPct);

    const float clampedDurationMs = std::max(1.0f, durationMs);
    const uint64_t totalFrames = static_cast<uint64_t>(
        std::max(1.0, static_cast<double>(outputSampleRate) * (static_cast<double>(clampedDurationMs) * 0.001)));

    // Do not call play() while scrubbing: avoid continuous audio after scrub stops.
    scrubPreviewFramePos.store(startFramePos);
    scrubPreviewTotalFrames.store(totalFrames);
    scrubPreviewRemainingFrames.store(totalFrames);
    scrubPreviewActive.store(true);
}

void DrWavAudioPlayer::audioOut(ofSoundBuffer& outBuffer)
{
    renderBlock(
        outBuffer.getBuffer().data(),
        static_cast<size_t>(outBuffer.getNumFrames()),
        outBuffer.getNumChannels(),
        static_cast<double>(outBuffer.getSampleRate()));
}

void DrWavAudioPlayer::renderBlock(float* out, size_t numFrames, int outChannels, double outputSampleRateValue)
{
    // Keep rendering independent from soundcard APIs so the same core can be reused for offline WAV export.
    if (out == nullptr || numFrames == 0 || outChannels <= 0) {
        return;
    }

    const size_t totalOutputSamples = numFrames * static_cast<size_t>(outChannels);
    std::fill(out, out + totalOutputSamples, 0.0f);

    if (!loaded.load() || !clip.isLoaded()) {
        return;
    }

    const uint64_t frameCount = clip.getFrameCount();
    if (frameCount == 0 || outputSampleRateValue <= 0.0) {
        return;
    }

    const double sourceSampleRate = static_cast<double>(clip.getSampleRate());
    const double srcStep = static_cast<double>(std::max(0.001f, speed.load())) * sourceSampleRate / outputSampleRateValue;
    const float gain = volume.load();

    bool nowPlaying = playing.load() && !paused.load();
    bool nowLoop = loop.load();
    double mainPos = sourceFramePos.load();

    bool previewActive = scrubPreviewActive.load();
    double previewPos = scrubPreviewFramePos.load();
    uint64_t previewRemaining = scrubPreviewRemainingFrames.load();
    const uint64_t previewTotal = std::max<uint64_t>(1, scrubPreviewTotalFrames.load());

    for (size_t i = 0; i < numFrames; ++i) {
        float left = 0.0f;
        float right = 0.0f;

        if (nowPlaying) {
            left += readStereoAsChannel(clip, mainPos, 0);
            right += readStereoAsChannel(clip, mainPos, 1);

            mainPos += srcStep;
            if (mainPos >= static_cast<double>(frameCount)) {
                if (nowLoop) {
                    mainPos = std::fmod(mainPos, static_cast<double>(frameCount));
                } else {
                    nowPlaying = false;
                    mainPos = static_cast<double>(frameCount > 0 ? (frameCount - 1) : 0);
                }
            }
        }

        if (previewActive && previewRemaining > 0) {
            float pLeft = readStereoAsChannel(clip, previewPos, 0);
            float pRight = readStereoAsChannel(clip, previewPos, 1);

            const float phase = 1.0f - static_cast<float>(previewRemaining) / static_cast<float>(previewTotal);
            const float fade = 0.15f;
            float env = 1.0f;
            if (phase < fade) {
                env = phase / fade;
            } else if (phase > (1.0f - fade)) {
                env = (1.0f - phase) / fade;
            }
            env = ofClamp(env, 0.0f, 1.0f);

            left += pLeft * env;
            right += pRight * env;

            previewPos += srcStep;
            --previewRemaining;
            if (previewRemaining == 0) {
                previewActive = false;
            }
        }

        left *= gain;
        right *= gain;

        if (outChannels == 1) {
            out[i] = clampSample((left + right) * 0.5f);
        } else {
            const size_t base = i * static_cast<size_t>(outChannels);
            out[base + 0] = clampSample(left);
            out[base + 1] = clampSample(right);
            for (int ch = 2; ch < outChannels; ++ch) {
                out[base + static_cast<size_t>(ch)] = 0.0f;
            }
        }
    }

    sourceFramePos.store(mainPos);
    playing.store(nowPlaying);
    scrubPreviewFramePos.store(previewPos);
    scrubPreviewRemainingFrames.store(previewRemaining);
    scrubPreviewActive.store(previewActive);
}

void DrWavAudioPlayer::renderBlockAtSourceFrame(
    double sourceFrameStart,
    float* out,
    size_t numFrames,
    int outChannels,
    double outputSampleRateValue,
    float speedValue,
    float volumeValue,
    bool loopValue) const
{
    if (out == nullptr || numFrames == 0 || outChannels <= 0)
    {
        return;
    }

    const size_t totalOutputSamples = numFrames * static_cast<size_t>(outChannels);
    std::fill(out, out + totalOutputSamples, 0.0f);

    if (!loaded.load() || !clip.isLoaded() || outputSampleRateValue <= 0.0)
    {
        return;
    }

    const uint64_t sourceFrameCount = clip.getFrameCount();
    if (sourceFrameCount == 0)
    {
        return;
    }

    const double sourceSampleRate = static_cast<double>(clip.getSampleRate());
    const double srcStep = static_cast<double>(std::max(0.001f, speedValue)) * sourceSampleRate / outputSampleRateValue;
    const float gain = ofClamp(volumeValue, 0.0f, 1.0f);

    double srcPos = sourceFrameStart;
    if (loopValue && sourceFrameCount > 0)
    {
        srcPos = std::fmod(srcPos, static_cast<double>(sourceFrameCount));
        if (srcPos < 0.0)
        {
            srcPos += static_cast<double>(sourceFrameCount);
        }
    }
    else if (srcPos < 0.0 || srcPos >= static_cast<double>(sourceFrameCount))
    {
        return;
    }

    for (size_t i = 0; i < numFrames; ++i)
    {
        if (!loopValue && srcPos >= static_cast<double>(sourceFrameCount))
        {
            break;
        }

        if (loopValue && sourceFrameCount > 0)
        {
            srcPos = std::fmod(srcPos, static_cast<double>(sourceFrameCount));
            if (srcPos < 0.0)
            {
                srcPos += static_cast<double>(sourceFrameCount);
            }
        }

        float left = readStereoAsChannel(clip, srcPos, 0);
        float right = readStereoAsChannel(clip, srcPos, 1);

        left *= gain;
        right *= gain;

        if (outChannels == 1)
        {
            out[i] = clampSample((left + right) * 0.5f);
        }
        else
        {
            const size_t base = i * static_cast<size_t>(outChannels);
            out[base + 0] = clampSample(left);
            out[base + 1] = clampSample(right);
            for (int ch = 2; ch < outChannels; ++ch)
            {
                out[base + static_cast<size_t>(ch)] = 0.0f;
            }
        }

        srcPos += srcStep;
    }
}

void DrWavAudioPlayer::renderBlockAtMovieTimeSec(
    double movieTimeSec,
    float* out,
    size_t numFrames,
    int outChannels,
    double outputSampleRateValue,
    float speedValue,
    float volumeValue,
    bool loopValue) const
{
    if (out == nullptr || numFrames == 0 || outChannels <= 0)
    {
        return;
    }

    std::fill(out, out + (numFrames * static_cast<size_t>(outChannels)), 0.0f);

    if (!loaded.load() || !clip.isLoaded())
    {
        return;
    }

    const double sourceFrameStart = movieTimeSec * static_cast<double>(clip.getSampleRate());
    renderBlockAtSourceFrame(
        sourceFrameStart,
        out,
        numFrames,
        outChannels,
        outputSampleRateValue,
        speedValue,
        volumeValue,
        loopValue);
}
