#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace to
{
struct VoiceClip {
    std::span<const std::uint8_t> format, samples, packetTable;
    unsigned durationMs{};
};

inline std::uint32_t voiceLE(std::span<const std::uint8_t> bytes)
{
    std::uint32_t value{};
    for (std::size_t i=0; i<bytes.size(); ++i)
        value |= std::uint32_t(bytes[i])<<(8*i);
    return value;
}

inline bool voiceTag(std::span<const std::uint8_t> bytes, std::string_view tag)
{
    return bytes.size()>=tag.size() &&
        std::string_view(reinterpret_cast<const char*>(bytes.data()),tag.size())==tag;
}

inline std::optional<VoiceClip> readVoiceClip(std::span<const std::uint8_t> bytes)
{
    if (bytes.size()<12 || !voiceTag(bytes,"FUZE")) return std::nullopt;
    const auto lipSize = voiceLE(bytes.subspan(8,4));
    if (lipSize>bytes.size()-12) return std::nullopt;
    bytes = bytes.subspan(12+lipSize);
    if (bytes.size()<12 || !voiceTag(bytes,"RIFF")) return std::nullopt;
    const auto riffSize = voiceLE(bytes.subspan(4,4));
    if (riffSize<4 || riffSize>bytes.size()-8) return std::nullopt;
    const bool compressed = voiceTag(bytes.subspan(8),"XWMA");
    if (!compressed && !voiceTag(bytes.subspan(8),"WAVE")) return std::nullopt;

    VoiceClip clip;
    auto chunks = bytes.subspan(12,riffSize-4);
    while (!chunks.empty()) {
        if (chunks.size()<8) return std::nullopt;
        const auto size = voiceLE(chunks.subspan(4,4));
        const auto padded = std::size_t(size)+(size&1u);
        if (padded>chunks.size()-8) return std::nullopt;
        const auto data = chunks.subspan(8,size);
        if (voiceTag(chunks,"fmt ")) clip.format = data;
        else if (voiceTag(chunks,"data")) clip.samples = data;
        else if (voiceTag(chunks,"dpds")) clip.packetTable = data;
        chunks = chunks.subspan(8+padded);
    }

    if (clip.format.size()<16 || clip.format.size()>64 || clip.samples.empty()) return std::nullopt;
    const auto tag = voiceLE(clip.format.first(2));
    const auto channels = voiceLE(clip.format.subspan(2,2));
    const auto sampleRate = voiceLE(clip.format.subspan(4,4));
    const auto blockSize = voiceLE(clip.format.subspan(12,2));
    const auto bits = voiceLE(clip.format.subspan(14,2));
    if (!channels || channels>2 || sampleRate<8000 || sampleRate>96000 || !blockSize ||
        (bits!=8 && bits!=16)) return std::nullopt;
    const auto bytesPerSample = channels*(bits/8);
    std::size_t decodedBytes = clip.samples.size();
    if (compressed) {
        if ((tag!=0x161 && tag!=0x162) || clip.format.size()<18 ||
            voiceLE(clip.format.subspan(16,2))>clip.format.size()-18 ||
            clip.packetTable.empty() || clip.packetTable.size()%4 ||
            clip.samples.size()%blockSize || clip.samples.size()/blockSize!=clip.packetTable.size()/4)
            return std::nullopt;
        std::uint32_t previous{};
        for (std::size_t i=0; i<clip.packetTable.size(); i+=4) {
            const auto total = voiceLE(clip.packetTable.subspan(i,4));
            if (total<previous || total%bytesPerSample) return std::nullopt;
            previous = total;
        }
        decodedBytes = previous;
    } else if (tag!=1 || blockSize!=bytesPerSample || decodedBytes%blockSize) {
        return std::nullopt;
    }
    const auto duration = (std::uint64_t(decodedBytes)*1000)/(sampleRate*bytesPerSample);
    if (!duration || duration>12000) return std::nullopt;
    clip.durationMs = static_cast<unsigned>(duration);
    return clip;
}
}
