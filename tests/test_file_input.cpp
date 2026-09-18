#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

import processor_graph;
import file_input_node;

namespace {

// Writes a mono 16-bit PCM WAV at 44100 Hz containing `samples`.
std::string writeWav(const std::string& name, const std::vector<int16_t>& samples)
{
    auto path = (std::filesystem::temp_directory_path() / name).string();
    std::ofstream f(path, std::ios::binary);

    auto u32 = [&](uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); };
    auto u16 = [&](uint16_t v) { f.write(reinterpret_cast<const char*>(&v), 2); };

    uint32_t dataBytes = static_cast<uint32_t>(samples.size() * sizeof(int16_t));
    f.write("RIFF", 4); u32(36 + dataBytes); f.write("WAVE", 4);
    f.write("fmt ", 4); u32(16); u16(1); u16(1); u32(44100); u32(44100 * 2); u16(2); u16(16);
    f.write("data", 4); u32(dataBytes);
    f.write(reinterpret_cast<const char*>(samples.data()), dataBytes);
    return path;
}

constexpr int kFrames = 8;

std::vector<int16_t> ramp()
{
    std::vector<int16_t> s;
    for (int i = 0; i < kFrames; ++i) s.push_back(static_cast<int16_t>((i + 1) * 1000));
    return s;
}

float expected(int frame) { return ((frame % kFrames) + 1) * 1000.0f / 32768.0f; }

} // namespace

TEST_CASE("FileInputNode goes silent at EOF when not looping", "[file_input]")
{
    ProcessorGraph graph(1);
    FileInputNode node(graph, writeWav("ampcore_ramp.wav", ramp()).c_str(), 1, 44100);
    REQUIRE_FALSE(node.isLooping());

    float out[20];
    std::memset(out, 0x7f, sizeof(out));
    node.process(out, nullptr, 20);

    for (int i = 0; i < kFrames; ++i) REQUIRE(out[i] == Catch::Approx(expected(i)));
    for (int i = kFrames; i < 20; ++i) REQUIRE(out[i] == 0.0f);
}

TEST_CASE("FileInputNode wraps around at EOF when looping", "[file_input]")
{
    ProcessorGraph graph(1);
    FileInputNode node(graph, writeWav("ampcore_ramp.wav", ramp()).c_str(), 1, 44100);
    node.setLooping(true);

    // 20 frames from an 8-frame file spans two wraps inside one call
    float out[20];
    node.process(out, nullptr, 20);
    for (int i = 0; i < 20; ++i) REQUIRE(out[i] == Catch::Approx(expected(i)));

    // and the wrap position carries across calls
    float next[4];
    node.process(next, nullptr, 4);
    for (int i = 0; i < 4; ++i) REQUIRE(next[i] == Catch::Approx(expected(20 + i)));
}

TEST_CASE("FileInputNode looping an empty file yields silence, not a hang", "[file_input]")
{
    ProcessorGraph graph(1);
    FileInputNode node(graph, writeWav("ampcore_empty.wav", {}).c_str(), 1, 44100);
    node.setLooping(true);

    float out[16];
    std::memset(out, 0x7f, sizeof(out));
    node.process(out, nullptr, 16);
    for (float v : out) REQUIRE(v == 0.0f);
}
