#include "sigchain_capi.h"
#include "miniaudio.h"
#include <string>
#include <fstream>
#include <sstream>

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

/* ── ProcessorGraph ──────────────────────────────────────────────────────── */

SigchainResult sigchain_graph_create(uint32_t channels, SigchainNode* out)
{
    if (!out) return SIGCHAIN_ERROR_NULL_ARG;
    try {
        *out = new ProcessorGraph(static_cast<ma_uint32>(channels));
        return SIGCHAIN_OK;
    } catch (...) {
        return SIGCHAIN_ERROR_INIT_FAILED;
    }
}

void sigchain_graph_destroy(SigchainNode graph)
{
    delete static_cast<ProcessorGraph*>(graph);
}

SigchainResult sigchain_graph_connect(SigchainNode graph, SigchainNode from, SigchainNode to)
{
    if (!graph || !from || !to) return SIGCHAIN_ERROR_NULL_ARG;
    try {
        static_cast<ProcessorGraph*>(graph)->connect(
            static_cast<NodeBase*>(from), static_cast<NodeBase*>(to));
        return SIGCHAIN_OK;
    } catch (...) {
        return SIGCHAIN_ERROR_UNKNOWN;
    }
}

SigchainResult sigchain_graph_connect_to_output(SigchainNode graph, SigchainNode node)
{
    if (!graph || !node) return SIGCHAIN_ERROR_NULL_ARG;
    try {
        static_cast<ProcessorGraph*>(graph)->connectToOutput(
            static_cast<NodeBase*>(node));
        return SIGCHAIN_OK;
    } catch (...) {
        return SIGCHAIN_ERROR_UNKNOWN;
    }
}

SigchainResult sigchain_graph_post_connect(SigchainNode graph, SigchainNode from, SigchainNode to)
{
    if (!graph || !from || !to) return SIGCHAIN_ERROR_NULL_ARG;
    try {
        bool ok = static_cast<ProcessorGraph*>(graph)->postConnect(
            static_cast<NodeBase*>(from), static_cast<NodeBase*>(to));
        return ok ? SIGCHAIN_OK : SIGCHAIN_ERROR_QUEUE_FULL;
    } catch (...) {
        return SIGCHAIN_ERROR_UNKNOWN;
    }
}

SigchainResult sigchain_graph_post_connect_to_output(SigchainNode graph, SigchainNode node)
{
    if (!graph || !node) return SIGCHAIN_ERROR_NULL_ARG;
    try {
        bool ok = static_cast<ProcessorGraph*>(graph)->postConnectToOutput(
            static_cast<NodeBase*>(node));
        return ok ? SIGCHAIN_OK : SIGCHAIN_ERROR_QUEUE_FULL;
    } catch (...) {
        return SIGCHAIN_ERROR_UNKNOWN;
    }
}

SigchainResult sigchain_graph_post_disconnect(SigchainNode graph, SigchainNode node)
{
    if (!graph || !node) return SIGCHAIN_ERROR_NULL_ARG;
    try {
        bool ok = static_cast<ProcessorGraph*>(graph)->postDisconnect(
            static_cast<NodeBase*>(node));
        return ok ? SIGCHAIN_OK : SIGCHAIN_ERROR_QUEUE_FULL;
    } catch (...) {
        return SIGCHAIN_ERROR_UNKNOWN;
    }
}

/* ── AudioEngine ─────────────────────────────────────────────────────────── */

SigchainResult sigchain_engine_create(SigchainNode graph, uint32_t sample_rate, SigchainNode* out)
{
    if (!graph || !out) return SIGCHAIN_ERROR_NULL_ARG;
    try {
        *out = new AudioEngine(*static_cast<ProcessorGraph*>(graph),
                               static_cast<ma_uint32>(sample_rate));
        return SIGCHAIN_OK;
    } catch (...) {
        return SIGCHAIN_ERROR_INIT_FAILED;
    }
}

void sigchain_engine_destroy(SigchainNode engine)
{
    delete static_cast<AudioEngine*>(engine);
}

SigchainResult sigchain_engine_start(SigchainNode engine)
{
    if (!engine) return SIGCHAIN_ERROR_NULL_ARG;
    try {
        static_cast<AudioEngine*>(engine)->start();
        return SIGCHAIN_OK;
    } catch (...) {
        return SIGCHAIN_ERROR_UNKNOWN;
    }
}

SigchainResult sigchain_engine_stop(SigchainNode engine)
{
    if (!engine) return SIGCHAIN_ERROR_NULL_ARG;
    try {
        static_cast<AudioEngine*>(engine)->stop();
        return SIGCHAIN_OK;
    } catch (...) {
        return SIGCHAIN_ERROR_UNKNOWN;
    }
}

int sigchain_engine_is_started(SigchainNode engine)
{
    if (!engine) return 0;
    return static_cast<AudioEngine*>(engine)->isStarted() ? 1 : 0;
}

uint32_t sigchain_engine_get_sample_rate(SigchainNode engine)
{
    if (!engine) return 0;
    return static_cast<AudioEngine*>(engine)->getSampleRate();
}

SigchainResult sigchain_engine_set_input_node(SigchainNode engine, SigchainNode input_node)
{
    if (!engine || !input_node) return SIGCHAIN_ERROR_NULL_ARG;
    static_cast<AudioEngine*>(engine)->setInputNode(
        static_cast<AudioInputNode*>(input_node));
    return SIGCHAIN_OK;
}

/* ── AudioInputNode ──────────────────────────────────────────────────────── */

SigchainResult sigchain_audio_input_create(SigchainNode graph, uint32_t channels, SigchainNode* out)
{
    if (!graph || !out) return SIGCHAIN_ERROR_NULL_ARG;
    try {
        *out = new AudioInputNode(*static_cast<ProcessorGraph*>(graph),
                                  static_cast<ma_uint32>(channels));
        return SIGCHAIN_OK;
    } catch (...) {
        return SIGCHAIN_ERROR_INIT_FAILED;
    }
}

void sigchain_audio_input_destroy(SigchainNode node)
{
    delete static_cast<AudioInputNode*>(node);
}

/* ── FileInputNode ───────────────────────────────────────────────────────── */

SigchainResult sigchain_file_input_create(SigchainNode graph, const char* path,
                                uint32_t channels, uint32_t sample_rate,
                                SigchainNode* out)
{
    if (!graph || !path || !out) return SIGCHAIN_ERROR_NULL_ARG;
    try {
        *out = new FileInputNode(*static_cast<ProcessorGraph*>(graph), path,
                                 static_cast<ma_uint32>(channels),
                                 static_cast<ma_uint32>(sample_rate));
        return SIGCHAIN_OK;
    } catch (...) {
        return SIGCHAIN_ERROR_INIT_FAILED;
    }
}

void sigchain_file_input_destroy(SigchainNode node)
{
    delete static_cast<FileInputNode*>(node);
}

/* ── Fuzz ────────────────────────────────────────────────────────────────── */

SigchainResult sigchain_fuzz_create(SigchainNode graph, uint32_t channels,
                          float gain, float threshold, SigchainNode* out)
{
    if (!graph || !out) return SIGCHAIN_ERROR_NULL_ARG;
    try {
        *out = new Fuzz(*static_cast<ProcessorGraph*>(graph),
                       static_cast<ma_uint32>(channels), gain, threshold);
        return SIGCHAIN_OK;
    } catch (...) {
        return SIGCHAIN_ERROR_INIT_FAILED;
    }
}

void sigchain_fuzz_destroy(SigchainNode node)
{
    delete static_cast<Fuzz*>(node);
}

SigchainResult sigchain_fuzz_set_gain(SigchainNode node, float gain)
{
    if (!node) return SIGCHAIN_ERROR_NULL_ARG;
    static_cast<Fuzz*>(node)->setGain(gain);
    return SIGCHAIN_OK;
}

SigchainResult sigchain_fuzz_set_threshold(SigchainNode node, float threshold)
{
    if (!node) return SIGCHAIN_ERROR_NULL_ARG;
    static_cast<Fuzz*>(node)->setThreshold(threshold);
    return SIGCHAIN_OK;
}

/* ── Delay ───────────────────────────────────────────────────────────────── */

SigchainResult sigchain_delay_create(SigchainNode graph, uint32_t channels, uint32_t sample_rate,
                           float delay_sec, float feedback, float mix,
                           SigchainNode* out)
{
    if (!graph || !out) return SIGCHAIN_ERROR_NULL_ARG;
    try {
        *out = new Delay(*static_cast<ProcessorGraph*>(graph),
                        static_cast<ma_uint32>(channels),
                        static_cast<ma_uint32>(sample_rate),
                        delay_sec, feedback, mix);
        return SIGCHAIN_OK;
    } catch (...) {
        return SIGCHAIN_ERROR_INIT_FAILED;
    }
}

void sigchain_delay_destroy(SigchainNode node)
{
    delete static_cast<Delay*>(node);
}

SigchainResult sigchain_delay_set_feedback(SigchainNode node, float feedback)
{
    if (!node) return SIGCHAIN_ERROR_NULL_ARG;
    static_cast<Delay*>(node)->setFeedback(feedback);
    return SIGCHAIN_OK;
}

SigchainResult sigchain_delay_set_mix(SigchainNode node, float mix)
{
    if (!node) return SIGCHAIN_ERROR_NULL_ARG;
    static_cast<Delay*>(node)->setMix(mix);
    return SIGCHAIN_OK;
}

/* ── Gain ─────────────────────────────────────────────────────────────────── */

SigchainResult sigchain_gain_create(SigchainNode graph, uint32_t channels, float gain,
                          SigchainNode* out)
{
    if (!graph || !out) return SIGCHAIN_ERROR_NULL_ARG;
    try {
        *out = new Gain(*static_cast<ProcessorGraph*>(graph),
                       static_cast<ma_uint32>(channels), gain);
        return SIGCHAIN_OK;
    } catch (...) {
        return SIGCHAIN_ERROR_INIT_FAILED;
    }
}

void sigchain_gain_destroy(SigchainNode node)
{
    delete static_cast<Gain*>(node);
}

SigchainResult sigchain_gain_set_gain(SigchainNode node, float gain)
{
    if (!node) return SIGCHAIN_ERROR_NULL_ARG;
    static_cast<Gain*>(node)->setGain(gain);
    return SIGCHAIN_OK;
}

/* ── Biquad ───────────────────────────────────────────────────────────────── */

SigchainResult sigchain_biquad_create(SigchainNode graph, uint32_t channels, uint32_t sample_rate,
                            int mode, float freq, float q, float db_gain,
                            SigchainNode* out)
{
    if (!graph || !out) return SIGCHAIN_ERROR_NULL_ARG;
    try {
        *out = new Biquad(*static_cast<ProcessorGraph*>(graph),
                         static_cast<ma_uint32>(channels),
                         static_cast<ma_uint32>(sample_rate),
                         static_cast<Biquad::Mode>(mode), freq, q, db_gain);
        return SIGCHAIN_OK;
    } catch (...) {
        return SIGCHAIN_ERROR_INIT_FAILED;
    }
}

void sigchain_biquad_destroy(SigchainNode node)
{
    delete static_cast<Biquad*>(node);
}

SigchainResult sigchain_biquad_set_freq(SigchainNode node, float freq)
{
    if (!node) return SIGCHAIN_ERROR_NULL_ARG;
    static_cast<Biquad*>(node)->setFreq(freq);
    return SIGCHAIN_OK;
}

SigchainResult sigchain_biquad_set_q(SigchainNode node, float q)
{
    if (!node) return SIGCHAIN_ERROR_NULL_ARG;
    static_cast<Biquad*>(node)->setQ(q);
    return SIGCHAIN_OK;
}

SigchainResult sigchain_biquad_set_db_gain(SigchainNode node, float db_gain)
{
    if (!node) return SIGCHAIN_ERROR_NULL_ARG;
    static_cast<Biquad*>(node)->setDbGain(db_gain);
    return SIGCHAIN_OK;
}

SigchainResult sigchain_biquad_set_mode(SigchainNode node, int mode)
{
    if (!node) return SIGCHAIN_ERROR_NULL_ARG;
    static_cast<Biquad*>(node)->setMode(static_cast<Biquad::Mode>(mode));
    return SIGCHAIN_OK;
}

/* ── Noise Gate ──────────────────────────────────────────────────────────── */

SigchainResult sigchain_noise_gate_create(SigchainNode graph, uint32_t channels, uint32_t sample_rate,
                                SigchainNode* out)
{
    if (!graph || !out) return SIGCHAIN_ERROR_NULL_ARG;
    try {
        *out = new NoiseGate(*static_cast<ProcessorGraph*>(graph),
                            static_cast<ma_uint32>(channels),
                            static_cast<ma_uint32>(sample_rate));
        return SIGCHAIN_OK;
    } catch (...) {
        return SIGCHAIN_ERROR_INIT_FAILED;
    }
}

void sigchain_noise_gate_destroy(SigchainNode node)
{
    delete static_cast<NoiseGate*>(node);
}

SigchainResult sigchain_noise_gate_set_threshold(SigchainNode node, float threshold_db)
{
    if (!node) return SIGCHAIN_ERROR_NULL_ARG;
    static_cast<NoiseGate*>(node)->setThreshold(threshold_db);
    return SIGCHAIN_OK;
}

SigchainResult sigchain_noise_gate_set_attack(SigchainNode node, float seconds)
{
    if (!node) return SIGCHAIN_ERROR_NULL_ARG;
    static_cast<NoiseGate*>(node)->setAttack(seconds);
    return SIGCHAIN_OK;
}

SigchainResult sigchain_noise_gate_set_release(SigchainNode node, float seconds)
{
    if (!node) return SIGCHAIN_ERROR_NULL_ARG;
    static_cast<NoiseGate*>(node)->setRelease(seconds);
    return SIGCHAIN_OK;
}

/* ── Compressor ──────────────────────────────────────────────────────────── */

SigchainResult sigchain_compressor_create(SigchainNode graph, uint32_t channels, uint32_t sample_rate,
                                SigchainNode* out)
{
    if (!graph || !out) return SIGCHAIN_ERROR_NULL_ARG;
    try {
        *out = new Compressor(*static_cast<ProcessorGraph*>(graph),
                             static_cast<ma_uint32>(channels),
                             static_cast<ma_uint32>(sample_rate));
        return SIGCHAIN_OK;
    } catch (...) {
        return SIGCHAIN_ERROR_INIT_FAILED;
    }
}

void sigchain_compressor_destroy(SigchainNode node)
{
    delete static_cast<Compressor*>(node);
}

SigchainResult sigchain_compressor_set_threshold(SigchainNode node, float threshold_db)
{
    if (!node) return SIGCHAIN_ERROR_NULL_ARG;
    static_cast<Compressor*>(node)->setThreshold(threshold_db);
    return SIGCHAIN_OK;
}

SigchainResult sigchain_compressor_set_ratio(SigchainNode node, float ratio)
{
    if (!node) return SIGCHAIN_ERROR_NULL_ARG;
    static_cast<Compressor*>(node)->setRatio(ratio);
    return SIGCHAIN_OK;
}

SigchainResult sigchain_compressor_set_attack(SigchainNode node, float seconds)
{
    if (!node) return SIGCHAIN_ERROR_NULL_ARG;
    static_cast<Compressor*>(node)->setAttack(seconds);
    return SIGCHAIN_OK;
}

SigchainResult sigchain_compressor_set_release(SigchainNode node, float seconds)
{
    if (!node) return SIGCHAIN_ERROR_NULL_ARG;
    static_cast<Compressor*>(node)->setRelease(seconds);
    return SIGCHAIN_OK;
}

SigchainResult sigchain_compressor_set_makeup_gain(SigchainNode node, float db)
{
    if (!node) return SIGCHAIN_ERROR_NULL_ARG;
    static_cast<Compressor*>(node)->setMakeupGain(db);
    return SIGCHAIN_OK;
}

/* ── Reverb ──────────────────────────────────────────────────────────────── */

SigchainResult sigchain_reverb_create(SigchainNode graph, uint32_t channels, SigchainNode* out)
{
    if (!graph || !out) return SIGCHAIN_ERROR_NULL_ARG;
    try {
        *out = new Reverb(*static_cast<ProcessorGraph*>(graph),
                         static_cast<ma_uint32>(channels));
        return SIGCHAIN_OK;
    } catch (...) {
        return SIGCHAIN_ERROR_INIT_FAILED;
    }
}

void sigchain_reverb_destroy(SigchainNode node)
{
    delete static_cast<Reverb*>(node);
}

SigchainResult sigchain_reverb_set_room_size(SigchainNode node, float size)
{
    if (!node) return SIGCHAIN_ERROR_NULL_ARG;
    static_cast<Reverb*>(node)->setRoomSize(size);
    return SIGCHAIN_OK;
}

SigchainResult sigchain_reverb_set_damping(SigchainNode node, float damping)
{
    if (!node) return SIGCHAIN_ERROR_NULL_ARG;
    static_cast<Reverb*>(node)->setDamping(damping);
    return SIGCHAIN_OK;
}

SigchainResult sigchain_reverb_set_mix(SigchainNode node, float mix)
{
    if (!node) return SIGCHAIN_ERROR_NULL_ARG;
    static_cast<Reverb*>(node)->setMix(mix);
    return SIGCHAIN_OK;
}

/* ── Tone Stack ──────────────────────────────────────────────────────────── */

SigchainResult sigchain_tone_stack_create(SigchainNode graph, uint32_t channels, uint32_t sample_rate,
                                SigchainNode* out)
{
    if (!graph || !out) return SIGCHAIN_ERROR_NULL_ARG;
    try {
        *out = new ToneStack(*static_cast<ProcessorGraph*>(graph),
                            static_cast<ma_uint32>(channels),
                            static_cast<ma_uint32>(sample_rate));
        return SIGCHAIN_OK;
    } catch (...) {
        return SIGCHAIN_ERROR_INIT_FAILED;
    }
}

void sigchain_tone_stack_destroy(SigchainNode node)
{
    delete static_cast<ToneStack*>(node);
}

SigchainResult sigchain_tone_stack_set_bass(SigchainNode node, float value)
{
    if (!node) return SIGCHAIN_ERROR_NULL_ARG;
    static_cast<ToneStack*>(node)->setBass(value);
    return SIGCHAIN_OK;
}

SigchainResult sigchain_tone_stack_set_mid(SigchainNode node, float value)
{
    if (!node) return SIGCHAIN_ERROR_NULL_ARG;
    static_cast<ToneStack*>(node)->setMid(value);
    return SIGCHAIN_OK;
}

SigchainResult sigchain_tone_stack_set_treble(SigchainNode node, float value)
{
    if (!node) return SIGCHAIN_ERROR_NULL_ARG;
    static_cast<ToneStack*>(node)->setTreble(value);
    return SIGCHAIN_OK;
}

/* ── Cabinet ─────────────────────────────────────────────────────────────── */

SigchainResult sigchain_cabinet_create(SigchainNode graph, uint32_t channels, SigchainNode* out)
{
    if (!graph || !out) return SIGCHAIN_ERROR_NULL_ARG;
    try {
        *out = new CabinetNode(*static_cast<ProcessorGraph*>(graph),
                               static_cast<ma_uint32>(channels));
        return SIGCHAIN_OK;
    } catch (...) {
        return SIGCHAIN_ERROR_INIT_FAILED;
    }
}

void sigchain_cabinet_destroy(SigchainNode node)
{
    delete static_cast<CabinetNode*>(node);
}

SigchainResult sigchain_cabinet_load_ir(SigchainNode node, const char* wav_path, uint32_t sample_rate)
{
    if (!node || !wav_path) return SIGCHAIN_ERROR_NULL_ARG;
    bool ok = static_cast<CabinetNode*>(node)->loadIR(
        wav_path, static_cast<ma_uint32>(sample_rate));
    return ok ? SIGCHAIN_OK : SIGCHAIN_ERROR_UNKNOWN;
}

SigchainResult sigchain_cabinet_set_mix(SigchainNode node, float mix)
{
    if (!node) return SIGCHAIN_ERROR_NULL_ARG;
    static_cast<CabinetNode*>(node)->setMix(mix);
    return SIGCHAIN_OK;
}

/* ── Meter ───────────────────────────────────────────────────────────────── */

SigchainResult sigchain_meter_create(SigchainNode graph, uint32_t channels, SigchainNode* out)
{
    if (!graph || !out) return SIGCHAIN_ERROR_NULL_ARG;
    try {
        *out = new MeterNode(*static_cast<ProcessorGraph*>(graph),
                             static_cast<ma_uint32>(channels));
        return SIGCHAIN_OK;
    } catch (...) {
        return SIGCHAIN_ERROR_INIT_FAILED;
    }
}

void sigchain_meter_destroy(SigchainNode node)
{
    delete static_cast<MeterNode*>(node);
}

float sigchain_meter_get_rms(SigchainNode node, uint32_t channel)
{
    if (!node) return 0.0f;
    return static_cast<MeterNode*>(node)->getRMS(channel);
}

float sigchain_meter_get_peak(SigchainNode node, uint32_t channel)
{
    if (!node) return 0.0f;
    return static_cast<MeterNode*>(node)->getPeak(channel);
}

/* ── Parameter Introspection ─────────────────────────────────────────────── */

int sigchain_node_get_parameter_count(SigchainNode node)
{
    if (!node) return 0;
    return static_cast<NodeBase*>(node)->getParameterCount();
}

SigchainResult sigchain_node_get_parameter_info(SigchainNode node, int index, SigchainParameterInfo* out)
{
    if (!node || !out) return SIGCHAIN_ERROR_NULL_ARG;
    if (index < 0 || index >= static_cast<NodeBase*>(node)->getParameterCount())
        return SIGCHAIN_ERROR_INVALID_ARG;
    try {
        ParameterInfo info = static_cast<NodeBase*>(node)->getParameterInfo(index);
        out->name = info.name;
        out->min_value = info.min_value;
        out->max_value = info.max_value;
        out->default_value = info.default_value;
        out->unit = info.unit;
        return SIGCHAIN_OK;
    } catch (...) {
        return SIGCHAIN_ERROR_UNKNOWN;
    }
}

float sigchain_node_get_parameter_value(SigchainNode node, int index)
{
    if (!node) return 0.0f;
    return static_cast<NodeBase*>(node)->getParameterValue(index);
}

SigchainResult sigchain_node_set_parameter_value(SigchainNode node, int index, float value)
{
    if (!node) return SIGCHAIN_ERROR_NULL_ARG;
    if (index < 0 || index >= static_cast<NodeBase*>(node)->getParameterCount())
        return SIGCHAIN_ERROR_INVALID_ARG;
    try {
        static_cast<NodeBase*>(node)->setParameterValue(index, value);
        return SIGCHAIN_OK;
    } catch (...) {
        return SIGCHAIN_ERROR_UNKNOWN;
    }
}

/* ── Presets ─────────────────────────────────────────────────────────────── */

SigchainResult sigchain_preset_save(SigchainNode graph, const char* path)
{
    if (!graph || !path) return SIGCHAIN_ERROR_NULL_ARG;
    try {
        std::string json = static_cast<ProcessorGraph*>(graph)->serializeToJson();
        std::ofstream ofs(path);
        if (!ofs) return SIGCHAIN_ERROR_FILE_NOT_FOUND;
        ofs << json;
        return ofs.good() ? SIGCHAIN_OK : SIGCHAIN_ERROR_UNKNOWN;
    } catch (...) {
        return SIGCHAIN_ERROR_UNKNOWN;
    }
}

SigchainResult sigchain_preset_load(SigchainNode graph, const char* path)
{
    if (!graph || !path) return SIGCHAIN_ERROR_NULL_ARG;
    try {
        std::ifstream ifs(path);
        if (!ifs) return SIGCHAIN_ERROR_FILE_NOT_FOUND;
        std::ostringstream ss;
        ss << ifs.rdbuf();
        bool ok = static_cast<ProcessorGraph*>(graph)->loadFromJson(ss.str());
        return ok ? SIGCHAIN_OK : SIGCHAIN_ERROR_UNKNOWN;
    } catch (...) {
        return SIGCHAIN_ERROR_UNKNOWN;
    }
}
