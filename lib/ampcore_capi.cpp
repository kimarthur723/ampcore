#include "ampcore_capi.h"
#include "miniaudio.h"
#include <memory>
#include <string>
#include <cstdio>
// NOTE: file I/O here uses C stdio rather than <fstream>/<sstream>. Those pull in
// <format>, and textually including <format> in a TU that also `import`s the node
// modules (whose global module fragments reach the same std::__format entities)
// trips a GCC 14 modules bug ("failed to load pendings for std::__format::...").

import node_base;
import processor_graph;
import audio_engine;
import file_input_node;
import audio_input_node;
import fuzz;
import delay;
import primitives;
import noise_gate;
import compressor;
import reverb;
import tone_stack;
import cabinet;
import meter_node;
import preset;

// Every node handle is handed out as a NodeBase*, so reading it back through
// void* is an exact round trip; specific node types are reached with a checked
// downcast that rejects a handle of the wrong type.
static AmpcoreNode node_handle(NodeBase* node) { return node; }
static NodeBase* as_node(AmpcoreNode h) { return static_cast<NodeBase*>(h); }
template <class T> static T* as(AmpcoreNode h) { return dynamic_cast<T*>(as_node(h)); }

/* ── ProcessorGraph ──────────────────────────────────────────────────────── */

AmpcoreResult ampcore_graph_create(uint32_t channels, AmpcoreNode* out)
{
    if (!out) return AMPCORE_ERROR_NULL_ARG;
    try {
        *out = new ProcessorGraph(static_cast<ma_uint32>(channels));
        return AMPCORE_OK;
    } catch (...) {
        return AMPCORE_ERROR_INIT_FAILED;
    }
}

void ampcore_graph_destroy(AmpcoreNode graph)
{
    delete static_cast<ProcessorGraph*>(graph);
}

AmpcoreResult ampcore_graph_connect(AmpcoreNode graph, AmpcoreNode from, AmpcoreNode to)
{
    if (!graph || !from || !to) return AMPCORE_ERROR_NULL_ARG;
    try {
        static_cast<ProcessorGraph*>(graph)->connect(
            as_node(from), as_node(to));
        return AMPCORE_OK;
    } catch (...) {
        return AMPCORE_ERROR_UNKNOWN;
    }
}

AmpcoreResult ampcore_graph_connect_to_output(AmpcoreNode graph, AmpcoreNode node)
{
    if (!graph || !node) return AMPCORE_ERROR_NULL_ARG;
    try {
        static_cast<ProcessorGraph*>(graph)->connectToOutput(
            as_node(node));
        return AMPCORE_OK;
    } catch (...) {
        return AMPCORE_ERROR_UNKNOWN;
    }
}

AmpcoreResult ampcore_graph_post_connect(AmpcoreNode graph, AmpcoreNode from, AmpcoreNode to)
{
    if (!graph || !from || !to) return AMPCORE_ERROR_NULL_ARG;
    try {
        bool ok = static_cast<ProcessorGraph*>(graph)->postConnect(
            as_node(from), as_node(to));
        return ok ? AMPCORE_OK : AMPCORE_ERROR_QUEUE_FULL;
    } catch (...) {
        return AMPCORE_ERROR_UNKNOWN;
    }
}

AmpcoreResult ampcore_graph_post_connect_to_output(AmpcoreNode graph, AmpcoreNode node)
{
    if (!graph || !node) return AMPCORE_ERROR_NULL_ARG;
    try {
        bool ok = static_cast<ProcessorGraph*>(graph)->postConnectToOutput(
            as_node(node));
        return ok ? AMPCORE_OK : AMPCORE_ERROR_QUEUE_FULL;
    } catch (...) {
        return AMPCORE_ERROR_UNKNOWN;
    }
}

AmpcoreResult ampcore_graph_post_disconnect(AmpcoreNode graph, AmpcoreNode node)
{
    if (!graph || !node) return AMPCORE_ERROR_NULL_ARG;
    try {
        bool ok = static_cast<ProcessorGraph*>(graph)->postDisconnect(
            as_node(node));
        return ok ? AMPCORE_OK : AMPCORE_ERROR_QUEUE_FULL;
    } catch (...) {
        return AMPCORE_ERROR_UNKNOWN;
    }
}

/* ── AudioEngine ─────────────────────────────────────────────────────────── */

AmpcoreResult ampcore_engine_create(AmpcoreNode graph, uint32_t sample_rate, AmpcoreNode* out)
{
    if (!graph || !out) return AMPCORE_ERROR_NULL_ARG;
    try {
        *out = new AudioEngine(*static_cast<ProcessorGraph*>(graph),
                               static_cast<ma_uint32>(sample_rate));
        return AMPCORE_OK;
    } catch (...) {
        return AMPCORE_ERROR_INIT_FAILED;
    }
}

void ampcore_engine_destroy(AmpcoreNode engine)
{
    delete static_cast<AudioEngine*>(engine);
}

AmpcoreResult ampcore_engine_start(AmpcoreNode engine)
{
    if (!engine) return AMPCORE_ERROR_NULL_ARG;
    try {
        static_cast<AudioEngine*>(engine)->start();
        return AMPCORE_OK;
    } catch (...) {
        return AMPCORE_ERROR_UNKNOWN;
    }
}

AmpcoreResult ampcore_engine_stop(AmpcoreNode engine)
{
    if (!engine) return AMPCORE_ERROR_NULL_ARG;
    try {
        static_cast<AudioEngine*>(engine)->stop();
        return AMPCORE_OK;
    } catch (...) {
        return AMPCORE_ERROR_UNKNOWN;
    }
}

int ampcore_engine_is_started(AmpcoreNode engine)
{
    if (!engine) return 0;
    return static_cast<AudioEngine*>(engine)->isStarted() ? 1 : 0;
}

uint32_t ampcore_engine_get_sample_rate(AmpcoreNode engine)
{
    if (!engine) return 0;
    return static_cast<AudioEngine*>(engine)->getSampleRate();
}

AmpcoreResult ampcore_engine_set_input_node(AmpcoreNode engine, AmpcoreNode input_node)
{
    if (!engine || !input_node) return AMPCORE_ERROR_NULL_ARG;
    auto* input = as<AudioInputNode>(input_node);
    if (!input) return AMPCORE_ERROR_INVALID_ARG;
    static_cast<AudioEngine*>(engine)->setInputNode(input);
    return AMPCORE_OK;
}

/* ── AudioInputNode ──────────────────────────────────────────────────────── */

AmpcoreResult ampcore_audio_input_create(AmpcoreNode graph, uint32_t channels, AmpcoreNode* out)
{
    if (!graph || !out) return AMPCORE_ERROR_NULL_ARG;
    try {
        auto node = std::make_unique<AudioInputNode>(*static_cast<ProcessorGraph*>(graph),
                                  static_cast<ma_uint32>(channels));
        static_cast<ProcessorGraph*>(graph)->registerNode("AudioInput", node.get());
        *out = node_handle(node.release());
        return AMPCORE_OK;
    } catch (...) {
        return AMPCORE_ERROR_INIT_FAILED;
    }
}

void ampcore_audio_input_destroy(AmpcoreNode node)
{
    delete as_node(node);
}

/* ── FileInputNode ───────────────────────────────────────────────────────── */

AmpcoreResult ampcore_file_input_create(AmpcoreNode graph, const char* path,
                                uint32_t channels, uint32_t sample_rate,
                                AmpcoreNode* out)
{
    if (!graph || !path || !out) return AMPCORE_ERROR_NULL_ARG;
    try {
        *out = node_handle(new FileInputNode(*static_cast<ProcessorGraph*>(graph), path,
                                 static_cast<ma_uint32>(channels),
                                 static_cast<ma_uint32>(sample_rate)));
        return AMPCORE_OK;
    } catch (...) {
        return AMPCORE_ERROR_INIT_FAILED;
    }
}

void ampcore_file_input_destroy(AmpcoreNode node)
{
    delete as_node(node);
}

void ampcore_file_input_set_looping(AmpcoreNode node, int loop)
{
    auto* n = as<FileInputNode>(node);
    if (!n) return;
    n->setLooping(loop != 0);
}

/* ── Fuzz ────────────────────────────────────────────────────────────────── */

AmpcoreResult ampcore_fuzz_create(AmpcoreNode graph, uint32_t channels,
                          float gain, float threshold, AmpcoreNode* out)
{
    if (!graph || !out) return AMPCORE_ERROR_NULL_ARG;
    try {
        auto node = std::make_unique<Fuzz>(*static_cast<ProcessorGraph*>(graph),
                       static_cast<ma_uint32>(channels), gain, threshold);
        static_cast<ProcessorGraph*>(graph)->registerNode("Fuzz", node.get());
        *out = node_handle(node.release());
        return AMPCORE_OK;
    } catch (...) {
        return AMPCORE_ERROR_INIT_FAILED;
    }
}

void ampcore_fuzz_destroy(AmpcoreNode node)
{
    delete as_node(node);
}

AmpcoreResult ampcore_fuzz_set_gain(AmpcoreNode node, float gain)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    auto* n = as<Fuzz>(node);
    if (!n) return AMPCORE_ERROR_INVALID_ARG;
    n->setGain(gain);
    return AMPCORE_OK;
}

AmpcoreResult ampcore_fuzz_set_threshold(AmpcoreNode node, float threshold)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    auto* n = as<Fuzz>(node);
    if (!n) return AMPCORE_ERROR_INVALID_ARG;
    n->setThreshold(threshold);
    return AMPCORE_OK;
}

/* ── Delay ───────────────────────────────────────────────────────────────── */

AmpcoreResult ampcore_delay_create(AmpcoreNode graph, uint32_t channels, uint32_t sample_rate,
                           float delay_sec, float feedback, float mix,
                           AmpcoreNode* out)
{
    if (!graph || !out) return AMPCORE_ERROR_NULL_ARG;
    try {
        auto node = std::make_unique<Delay>(*static_cast<ProcessorGraph*>(graph),
                        static_cast<ma_uint32>(channels),
                        static_cast<ma_uint32>(sample_rate),
                        delay_sec, feedback, mix);
        static_cast<ProcessorGraph*>(graph)->registerNode("Delay", node.get());
        *out = node_handle(node.release());
        return AMPCORE_OK;
    } catch (...) {
        return AMPCORE_ERROR_INIT_FAILED;
    }
}

void ampcore_delay_destroy(AmpcoreNode node)
{
    delete as_node(node);
}

AmpcoreResult ampcore_delay_set_feedback(AmpcoreNode node, float feedback)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    auto* n = as<Delay>(node);
    if (!n) return AMPCORE_ERROR_INVALID_ARG;
    n->setFeedback(feedback);
    return AMPCORE_OK;
}

AmpcoreResult ampcore_delay_set_mix(AmpcoreNode node, float mix)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    auto* n = as<Delay>(node);
    if (!n) return AMPCORE_ERROR_INVALID_ARG;
    n->setMix(mix);
    return AMPCORE_OK;
}

/* ── Gain ─────────────────────────────────────────────────────────────────── */

AmpcoreResult ampcore_gain_create(AmpcoreNode graph, uint32_t channels, float gain,
                          AmpcoreNode* out)
{
    if (!graph || !out) return AMPCORE_ERROR_NULL_ARG;
    try {
        auto node = std::make_unique<Gain>(*static_cast<ProcessorGraph*>(graph),
                       static_cast<ma_uint32>(channels), gain);
        static_cast<ProcessorGraph*>(graph)->registerNode("Gain", node.get());
        *out = node_handle(node.release());
        return AMPCORE_OK;
    } catch (...) {
        return AMPCORE_ERROR_INIT_FAILED;
    }
}

void ampcore_gain_destroy(AmpcoreNode node)
{
    delete as_node(node);
}

AmpcoreResult ampcore_gain_set_gain(AmpcoreNode node, float gain)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    auto* n = as<Gain>(node);
    if (!n) return AMPCORE_ERROR_INVALID_ARG;
    n->setGain(gain);
    return AMPCORE_OK;
}

/* ── Biquad ───────────────────────────────────────────────────────────────── */

AmpcoreResult ampcore_biquad_create(AmpcoreNode graph, uint32_t channels, uint32_t sample_rate,
                            int mode, float freq, float q, float db_gain,
                            AmpcoreNode* out)
{
    if (!graph || !out) return AMPCORE_ERROR_NULL_ARG;
    try {
        auto node = std::make_unique<Biquad>(*static_cast<ProcessorGraph*>(graph),
                         static_cast<ma_uint32>(channels),
                         static_cast<ma_uint32>(sample_rate),
                         static_cast<Biquad::Mode>(mode), freq, q, db_gain);
        static_cast<ProcessorGraph*>(graph)->registerNode("Biquad", node.get());
        *out = node_handle(node.release());
        return AMPCORE_OK;
    } catch (...) {
        return AMPCORE_ERROR_INIT_FAILED;
    }
}

void ampcore_biquad_destroy(AmpcoreNode node)
{
    delete as_node(node);
}

AmpcoreResult ampcore_biquad_set_freq(AmpcoreNode node, float freq)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    auto* n = as<Biquad>(node);
    if (!n) return AMPCORE_ERROR_INVALID_ARG;
    n->setFreq(freq);
    return AMPCORE_OK;
}

AmpcoreResult ampcore_biquad_set_q(AmpcoreNode node, float q)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    auto* n = as<Biquad>(node);
    if (!n) return AMPCORE_ERROR_INVALID_ARG;
    n->setQ(q);
    return AMPCORE_OK;
}

AmpcoreResult ampcore_biquad_set_db_gain(AmpcoreNode node, float db_gain)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    auto* n = as<Biquad>(node);
    if (!n) return AMPCORE_ERROR_INVALID_ARG;
    n->setDbGain(db_gain);
    return AMPCORE_OK;
}

AmpcoreResult ampcore_biquad_set_mode(AmpcoreNode node, int mode)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    auto* n = as<Biquad>(node);
    if (!n) return AMPCORE_ERROR_INVALID_ARG;
    n->setMode(static_cast<Biquad::Mode>(mode));
    return AMPCORE_OK;
}

/* ── Noise Gate ──────────────────────────────────────────────────────────── */

AmpcoreResult ampcore_noise_gate_create(AmpcoreNode graph, uint32_t channels, uint32_t sample_rate,
                                AmpcoreNode* out)
{
    if (!graph || !out) return AMPCORE_ERROR_NULL_ARG;
    try {
        auto node = std::make_unique<NoiseGate>(*static_cast<ProcessorGraph*>(graph),
                            static_cast<ma_uint32>(channels),
                            static_cast<ma_uint32>(sample_rate));
        static_cast<ProcessorGraph*>(graph)->registerNode("NoiseGate", node.get());
        *out = node_handle(node.release());
        return AMPCORE_OK;
    } catch (...) {
        return AMPCORE_ERROR_INIT_FAILED;
    }
}

void ampcore_noise_gate_destroy(AmpcoreNode node)
{
    delete as_node(node);
}

AmpcoreResult ampcore_noise_gate_set_threshold(AmpcoreNode node, float threshold_db)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    auto* n = as<NoiseGate>(node);
    if (!n) return AMPCORE_ERROR_INVALID_ARG;
    n->setThreshold(threshold_db);
    return AMPCORE_OK;
}

AmpcoreResult ampcore_noise_gate_set_attack(AmpcoreNode node, float seconds)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    auto* n = as<NoiseGate>(node);
    if (!n) return AMPCORE_ERROR_INVALID_ARG;
    n->setAttack(seconds);
    return AMPCORE_OK;
}

AmpcoreResult ampcore_noise_gate_set_release(AmpcoreNode node, float seconds)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    auto* n = as<NoiseGate>(node);
    if (!n) return AMPCORE_ERROR_INVALID_ARG;
    n->setRelease(seconds);
    return AMPCORE_OK;
}

/* ── Compressor ──────────────────────────────────────────────────────────── */

AmpcoreResult ampcore_compressor_create(AmpcoreNode graph, uint32_t channels, uint32_t sample_rate,
                                AmpcoreNode* out)
{
    if (!graph || !out) return AMPCORE_ERROR_NULL_ARG;
    try {
        auto node = std::make_unique<Compressor>(*static_cast<ProcessorGraph*>(graph),
                             static_cast<ma_uint32>(channels),
                             static_cast<ma_uint32>(sample_rate));
        static_cast<ProcessorGraph*>(graph)->registerNode("Compressor", node.get());
        *out = node_handle(node.release());
        return AMPCORE_OK;
    } catch (...) {
        return AMPCORE_ERROR_INIT_FAILED;
    }
}

void ampcore_compressor_destroy(AmpcoreNode node)
{
    delete as_node(node);
}

AmpcoreResult ampcore_compressor_set_threshold(AmpcoreNode node, float threshold_db)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    auto* n = as<Compressor>(node);
    if (!n) return AMPCORE_ERROR_INVALID_ARG;
    n->setThreshold(threshold_db);
    return AMPCORE_OK;
}

AmpcoreResult ampcore_compressor_set_ratio(AmpcoreNode node, float ratio)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    auto* n = as<Compressor>(node);
    if (!n) return AMPCORE_ERROR_INVALID_ARG;
    n->setRatio(ratio);
    return AMPCORE_OK;
}

AmpcoreResult ampcore_compressor_set_attack(AmpcoreNode node, float seconds)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    auto* n = as<Compressor>(node);
    if (!n) return AMPCORE_ERROR_INVALID_ARG;
    n->setAttack(seconds);
    return AMPCORE_OK;
}

AmpcoreResult ampcore_compressor_set_release(AmpcoreNode node, float seconds)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    auto* n = as<Compressor>(node);
    if (!n) return AMPCORE_ERROR_INVALID_ARG;
    n->setRelease(seconds);
    return AMPCORE_OK;
}

AmpcoreResult ampcore_compressor_set_makeup_gain(AmpcoreNode node, float db)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    auto* n = as<Compressor>(node);
    if (!n) return AMPCORE_ERROR_INVALID_ARG;
    n->setMakeupGain(db);
    return AMPCORE_OK;
}

/* ── Reverb ──────────────────────────────────────────────────────────────── */

AmpcoreResult ampcore_reverb_create(AmpcoreNode graph, uint32_t channels, AmpcoreNode* out)
{
    if (!graph || !out) return AMPCORE_ERROR_NULL_ARG;
    try {
        auto node = std::make_unique<Reverb>(*static_cast<ProcessorGraph*>(graph),
                         static_cast<ma_uint32>(channels));
        static_cast<ProcessorGraph*>(graph)->registerNode("Reverb", node.get());
        *out = node_handle(node.release());
        return AMPCORE_OK;
    } catch (...) {
        return AMPCORE_ERROR_INIT_FAILED;
    }
}

void ampcore_reverb_destroy(AmpcoreNode node)
{
    delete as_node(node);
}

AmpcoreResult ampcore_reverb_set_room_size(AmpcoreNode node, float size)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    auto* n = as<Reverb>(node);
    if (!n) return AMPCORE_ERROR_INVALID_ARG;
    n->setRoomSize(size);
    return AMPCORE_OK;
}

AmpcoreResult ampcore_reverb_set_damping(AmpcoreNode node, float damping)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    auto* n = as<Reverb>(node);
    if (!n) return AMPCORE_ERROR_INVALID_ARG;
    n->setDamping(damping);
    return AMPCORE_OK;
}

AmpcoreResult ampcore_reverb_set_mix(AmpcoreNode node, float mix)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    auto* n = as<Reverb>(node);
    if (!n) return AMPCORE_ERROR_INVALID_ARG;
    n->setMix(mix);
    return AMPCORE_OK;
}

/* ── Tone Stack ──────────────────────────────────────────────────────────── */

AmpcoreResult ampcore_tone_stack_create(AmpcoreNode graph, uint32_t channels, uint32_t sample_rate,
                                AmpcoreNode* out)
{
    if (!graph || !out) return AMPCORE_ERROR_NULL_ARG;
    try {
        auto node = std::make_unique<ToneStack>(*static_cast<ProcessorGraph*>(graph),
                            static_cast<ma_uint32>(channels),
                            static_cast<ma_uint32>(sample_rate));
        static_cast<ProcessorGraph*>(graph)->registerNode("ToneStack", node.get());
        *out = node_handle(node.release());
        return AMPCORE_OK;
    } catch (...) {
        return AMPCORE_ERROR_INIT_FAILED;
    }
}

void ampcore_tone_stack_destroy(AmpcoreNode node)
{
    delete as_node(node);
}

AmpcoreResult ampcore_tone_stack_set_bass(AmpcoreNode node, float value)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    auto* n = as<ToneStack>(node);
    if (!n) return AMPCORE_ERROR_INVALID_ARG;
    n->setBass(value);
    return AMPCORE_OK;
}

AmpcoreResult ampcore_tone_stack_set_mid(AmpcoreNode node, float value)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    auto* n = as<ToneStack>(node);
    if (!n) return AMPCORE_ERROR_INVALID_ARG;
    n->setMid(value);
    return AMPCORE_OK;
}

AmpcoreResult ampcore_tone_stack_set_treble(AmpcoreNode node, float value)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    auto* n = as<ToneStack>(node);
    if (!n) return AMPCORE_ERROR_INVALID_ARG;
    n->setTreble(value);
    return AMPCORE_OK;
}

/* ── Cabinet ─────────────────────────────────────────────────────────────── */

AmpcoreResult ampcore_cabinet_create(AmpcoreNode graph, uint32_t channels, AmpcoreNode* out)
{
    if (!graph || !out) return AMPCORE_ERROR_NULL_ARG;
    try {
        auto node = std::make_unique<CabinetNode>(*static_cast<ProcessorGraph*>(graph),
                               static_cast<ma_uint32>(channels));
        static_cast<ProcessorGraph*>(graph)->registerNode("Cabinet", node.get());
        *out = node_handle(node.release());
        return AMPCORE_OK;
    } catch (...) {
        return AMPCORE_ERROR_INIT_FAILED;
    }
}

void ampcore_cabinet_destroy(AmpcoreNode node)
{
    delete as_node(node);
}

AmpcoreResult ampcore_cabinet_load_ir(AmpcoreNode node, const char* wav_path, uint32_t sample_rate)
{
    if (!node || !wav_path) return AMPCORE_ERROR_NULL_ARG;
    auto* n = as<CabinetNode>(node);
    if (!n) return AMPCORE_ERROR_INVALID_ARG;
    bool ok = n->loadIR(
        wav_path, static_cast<ma_uint32>(sample_rate));
    return ok ? AMPCORE_OK : AMPCORE_ERROR_UNKNOWN;
}

AmpcoreResult ampcore_cabinet_set_mix(AmpcoreNode node, float mix)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    auto* n = as<CabinetNode>(node);
    if (!n) return AMPCORE_ERROR_INVALID_ARG;
    n->setMix(mix);
    return AMPCORE_OK;
}

/* ── Meter ───────────────────────────────────────────────────────────────── */

AmpcoreResult ampcore_meter_create(AmpcoreNode graph, uint32_t channels, AmpcoreNode* out)
{
    if (!graph || !out) return AMPCORE_ERROR_NULL_ARG;
    try {
        auto node = std::make_unique<MeterNode>(*static_cast<ProcessorGraph*>(graph),
                             static_cast<ma_uint32>(channels));
        static_cast<ProcessorGraph*>(graph)->registerNode("Meter", node.get());
        *out = node_handle(node.release());
        return AMPCORE_OK;
    } catch (...) {
        return AMPCORE_ERROR_INIT_FAILED;
    }
}

void ampcore_meter_destroy(AmpcoreNode node)
{
    delete as_node(node);
}

float ampcore_meter_get_rms(AmpcoreNode node, uint32_t channel)
{
    auto* n = as<MeterNode>(node);
    if (!n) return 0.0f;
    return n->getRMS(channel);
}

float ampcore_meter_get_peak(AmpcoreNode node, uint32_t channel)
{
    auto* n = as<MeterNode>(node);
    if (!n) return 0.0f;
    return n->getPeak(channel);
}

/* ── Parameter Introspection ─────────────────────────────────────────────── */

int ampcore_node_get_parameter_count(AmpcoreNode node)
{
    if (!node) return 0;
    return as_node(node)->getParameterCount();
}

AmpcoreResult ampcore_node_get_parameter_info(AmpcoreNode node, int index, AmpcoreParameterInfo* out)
{
    if (!node || !out) return AMPCORE_ERROR_NULL_ARG;
    if (index < 0 || index >= as_node(node)->getParameterCount())
        return AMPCORE_ERROR_INVALID_ARG;
    try {
        ParameterInfo info = as_node(node)->getParameterInfo(index);
        out->name = info.name;
        out->min_value = info.min_value;
        out->max_value = info.max_value;
        out->default_value = info.default_value;
        out->unit = info.unit;
        return AMPCORE_OK;
    } catch (...) {
        return AMPCORE_ERROR_UNKNOWN;
    }
}

float ampcore_node_get_parameter_value(AmpcoreNode node, int index)
{
    if (!node) return 0.0f;
    return as_node(node)->getParameterValue(index);
}

AmpcoreResult ampcore_node_set_parameter_value(AmpcoreNode node, int index, float value)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    if (index < 0 || index >= as_node(node)->getParameterCount())
        return AMPCORE_ERROR_INVALID_ARG;
    try {
        as_node(node)->setParameterValue(index, value);
        return AMPCORE_OK;
    } catch (...) {
        return AMPCORE_ERROR_UNKNOWN;
    }
}

/* ── Presets ─────────────────────────────────────────────────────────────── */

AmpcoreResult ampcore_preset_save(AmpcoreNode graph, const char* path)
{
    if (!graph || !path) return AMPCORE_ERROR_NULL_ARG;
    try {
        std::string json = static_cast<ProcessorGraph*>(graph)->serializeToJson();
        std::FILE* f = std::fopen(path, "wb");
        if (!f) return AMPCORE_ERROR_FILE_NOT_FOUND;
        std::size_t written = std::fwrite(json.data(), 1, json.size(), f);
        bool ok = (written == json.size()) && (std::fclose(f) == 0);
        return ok ? AMPCORE_OK : AMPCORE_ERROR_UNKNOWN;
    } catch (...) {
        return AMPCORE_ERROR_UNKNOWN;
    }
}

static NodeBase* preset_create_node(const std::string& type, ProcessorGraph& graph, void* user)
{
    ma_uint32 ch = graph.getChannels();
    ma_uint32 sr = *static_cast<ma_uint32*>(user);
    if (type == "AudioInput") return new AudioInputNode(graph, ch);
    if (type == "Fuzz")       return new Fuzz(graph, ch);
    if (type == "Delay")      return new Delay(graph, ch, sr);
    if (type == "Gain")       return new Gain(graph, ch);
    if (type == "Biquad")     return new Biquad(graph, ch, sr);
    if (type == "NoiseGate")  return new NoiseGate(graph, ch, sr);
    if (type == "Compressor") return new Compressor(graph, ch, sr);
    if (type == "Reverb")     return new Reverb(graph, ch, sr);
    if (type == "ToneStack")  return new ToneStack(graph, ch, sr);
    if (type == "Cabinet")    return new CabinetNode(graph, ch);
    if (type == "Meter")      return new MeterNode(graph, ch);
    return nullptr;
}

static void preset_destroy_node(NodeBase* node, void*)
{
    delete node;
}

AmpcoreResult ampcore_preset_load(AmpcoreNode graph, const char* path, uint32_t sample_rate)
{
    if (!graph || !path) return AMPCORE_ERROR_NULL_ARG;
    try {
        std::FILE* f = std::fopen(path, "rb");
        if (!f) return AMPCORE_ERROR_FILE_NOT_FOUND;
        std::string contents;
        char buf[4096];
        std::size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
            contents.append(buf, n);
        bool read_ok = (std::ferror(f) == 0);
        std::fclose(f);
        if (!read_ok) return AMPCORE_ERROR_UNKNOWN;
        ma_uint32 sr = static_cast<ma_uint32>(sample_rate);
        bool ok = static_cast<ProcessorGraph*>(graph)->loadFromJson(
            contents, preset_create_node, preset_destroy_node, &sr);
        return ok ? AMPCORE_OK : AMPCORE_ERROR_UNKNOWN;
    } catch (...) {
        return AMPCORE_ERROR_UNKNOWN;
    }
}
