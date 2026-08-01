#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

class DrWavAudioClip
{
public:
    bool load(const std::filesystem::path& path);
    void clear();

    bool isLoaded() const;
    int getChannels() const;
    int getSampleRate() const;
    uint64_t getFrameCount() const;
    double getDurationSec() const;

    float getSampleNearest(uint64_t frame, int channel) const;
    float getSampleLinear(double framePos, int channel) const;

private:
    std::vector<float> samples;
    int channels = 0;
    int sampleRate = 0;
    uint64_t frameCount = 0;
};
