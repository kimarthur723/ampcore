#include "ampcore_capi.h"
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
            static_cast<NodeBase*>(from), static_cast<NodeBase*>(to));
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
            static_cast<NodeBase*>(node));
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
            static_cast<NodeBase*>(from), static_cast<NodeBase*>(to));
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
            static_cast<NodeBase*>(node));
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
            static_cast<NodeBase*>(node));
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
    static_cast<AudioEngine*>(engine)->setInputNode(
        static_cast<AudioInputNode*>(input_node));
    return AMPCORE_OK;
}

/* ── AudioInputNode ──────────────────────────────────────────────────────── */

AmpcoreResult ampcore_audio_input_create(AmpcoreNode graph, uint32_t channels, AmpcoreNode* out)
{
    if (!graph || !out) return AMPCORE_ERROR_NULL_ARG;
    try {
        *out = new AudioInputNode(*static_cast<ProcessorGraph*>(graph),
                                  static_cast<ma_uint32>(channels));
        return AMPCORE_OK;
    } catch (...) {
        return AMPCORE_ERROR_INIT_FAILED;
    }
}

void ampcore_audio_input_destroy(AmpcoreNode node)
{
    delete static_cast<AudioInputNode*>(node);
}

/* ── FileInputNode ───────────────────────────────────────────────────────── */

AmpcoreResult ampcore_file_input_create(AmpcoreNode graph, const char* path,
                                uint32_t channels, uint32_t sample_rate,
                                AmpcoreNode* out)
{
    if (!graph || !path || !out) return AMPCORE_ERROR_NULL_ARG;
    try {
        *out = new FileInputNode(*static_cast<ProcessorGraph*>(graph), path,
                                 static_cast<ma_uint32>(channels),
                                 static_cast<ma_uint32>(sample_rate));
        return AMPCORE_OK;
    } catch (...) {
        return AMPCORE_ERROR_INIT_FAILED;
    }
}

void ampcore_file_input_destroy(AmpcoreNode node)
{
    delete static_cast<FileInputNode*>(node);
}

/* ── Fuzz ────────────────────────────────────────────────────────────────── */

AmpcoreResult ampcore_fuzz_create(AmpcoreNode graph, uint32_t channels,
                          float gain, float threshold, AmpcoreNode* out)
{
    if (!graph || !out) return AMPCORE_ERROR_NULL_ARG;
    try {
        *out = new Fuzz(*static_cast<ProcessorGraph*>(graph),
                       static_cast<ma_uint32>(channels), gain, threshold);
        return AMPCORE_OK;
    } catch (...) {
        return AMPCORE_ERROR_INIT_FAILED;
    }
}

void ampcore_fuzz_destroy(AmpcoreNode node)
{
    delete static_cast<Fuzz*>(node);
}

AmpcoreResult ampcore_fuzz_set_gain(AmpcoreNode node, float gain)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    static_cast<Fuzz*>(node)->setGain(gain);
    return AMPCORE_OK;
}

AmpcoreResult ampcore_fuzz_set_threshold(AmpcoreNode node, float threshold)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    static_cast<Fuzz*>(node)->setThreshold(threshold);
    return AMPCORE_OK;
}

/* ── Delay ───────────────────────────────────────────────────────────────── */

AmpcoreResult ampcore_delay_create(AmpcoreNode graph, uint32_t channels, uint32_t sample_rate,
                           float delay_sec, float feedback, float mix,
                           AmpcoreNode* out)
{
    if (!graph || !out) return AMPCORE_ERROR_NULL_ARG;
    try {
        *out = new Delay(*static_cast<ProcessorGraph*>(graph),
                        static_cast<ma_uint32>(channels),
                        static_cast<ma_uint32>(sample_rate),
                        delay_sec, feedback, mix);
        return AMPCORE_OK;
    } catch (...) {
        return AMPCORE_ERROR_INIT_FAILED;
    }
}

void ampcore_delay_destroy(AmpcoreNode node)
{
    delete static_cast<Delay*>(node);
}

AmpcoreResult ampcore_delay_set_feedback(AmpcoreNode node, float feedback)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    static_cast<Delay*>(node)->setFeedback(feedback);
    return AMPCORE_OK;
}

AmpcoreResult ampcore_delay_set_mix(AmpcoreNode node, float mix)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    static_cast<Delay*>(node)->setMix(mix);
    return AMPCORE_OK;
}

/* ── Gain ─────────────────────────────────────────────────────────────────── */

AmpcoreResult ampcore_gain_create(AmpcoreNode graph, uint32_t channels, float gain,
                          AmpcoreNode* out)
{
    if (!graph || !out) return AMPCORE_ERROR_NULL_ARG;
    try {
        *out = new Gain(*static_cast<ProcessorGraph*>(graph),
                       static_cast<ma_uint32>(channels), gain);
        return AMPCORE_OK;
    } catch (...) {
        return AMPCORE_ERROR_INIT_FAILED;
    }
}

void ampcore_gain_destroy(AmpcoreNode node)
{
    delete static_cast<Gain*>(node);
}

AmpcoreResult ampcore_gain_set_gain(AmpcoreNode node, float gain)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    static_cast<Gain*>(node)->setGain(gain);
    return AMPCORE_OK;
}

/* ── Biquad ───────────────────────────────────────────────────────────────── */

AmpcoreResult ampcore_biquad_create(AmpcoreNode graph, uint32_t channels, uint32_t sample_rate,
                            int mode, float freq, float q, float db_gain,
                            AmpcoreNode* out)
{
    if (!graph || !out) return AMPCORE_ERROR_NULL_ARG;
    try {
        *out = new Biquad(*static_cast<ProcessorGraph*>(graph),
                         static_cast<ma_uint32>(channels),
                         static_cast<ma_uint32>(sample_rate),
                         static_cast<Biquad::Mode>(mode), freq, q, db_gain);
        return AMPCORE_OK;
    } catch (...) {
        return AMPCORE_ERROR_INIT_FAILED;
    }
}

void ampcore_biquad_destroy(AmpcoreNode node)
{
    delete static_cast<Biquad*>(node);
}

AmpcoreResult ampcore_biquad_set_freq(AmpcoreNode node, float freq)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    static_cast<Biquad*>(node)->setFreq(freq);
    return AMPCORE_OK;
}

AmpcoreResult ampcore_biquad_set_q(AmpcoreNode node, float q)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    static_cast<Biquad*>(node)->setQ(q);
    return AMPCORE_OK;
}

AmpcoreResult ampcore_biquad_set_db_gain(AmpcoreNode node, float db_gain)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    static_cast<Biquad*>(node)->setDbGain(db_gain);
    return AMPCORE_OK;
}

AmpcoreResult ampcore_biquad_set_mode(AmpcoreNode node, int mode)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    static_cast<Biquad*>(node)->setMode(static_cast<Biquad::Mode>(mode));
    return AMPCORE_OK;
}

/* ── Noise Gate ──────────────────────────────────────────────────────────── */

AmpcoreResult ampcore_noise_gate_create(AmpcoreNode graph, uint32_t channels, uint32_t sample_rate,
                                AmpcoreNode* out)
{
    if (!graph || !out) return AMPCORE_ERROR_NULL_ARG;
    try {
        *out = new NoiseGate(*static_cast<ProcessorGraph*>(graph),
                            static_cast<ma_uint32>(channels),
                            static_cast<ma_uint32>(sample_rate));
        return AMPCORE_OK;
    } catch (...) {
        return AMPCORE_ERROR_INIT_FAILED;
    }
}

void ampcore_noise_gate_destroy(AmpcoreNode node)
{
    delete static_cast<NoiseGate*>(node);
}

AmpcoreResult ampcore_noise_gate_set_threshold(AmpcoreNode node, float threshold_db)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    static_cast<NoiseGate*>(node)->setThreshold(threshold_db);
    return AMPCORE_OK;
}

AmpcoreResult ampcore_noise_gate_set_attack(AmpcoreNode node, float seconds)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    static_cast<NoiseGate*>(node)->setAttack(seconds);
    return AMPCORE_OK;
}

AmpcoreResult ampcore_noise_gate_set_release(AmpcoreNode node, float seconds)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    static_cast<NoiseGate*>(node)->setRelease(seconds);
    return AMPCORE_OK;
}

/* ── Compressor ──────────────────────────────────────────────────────────── */

AmpcoreResult ampcore_compressor_create(AmpcoreNode graph, uint32_t channels, uint32_t sample_rate,
                                AmpcoreNode* out)
{
    if (!graph || !out) return AMPCORE_ERROR_NULL_ARG;
    try {
        *out = new Compressor(*static_cast<ProcessorGraph*>(graph),
                             static_cast<ma_uint32>(channels),
                             static_cast<ma_uint32>(sample_rate));
        return AMPCORE_OK;
    } catch (...) {
        return AMPCORE_ERROR_INIT_FAILED;
    }
}

void ampcore_compressor_destroy(AmpcoreNode node)
{
    delete static_cast<Compressor*>(node);
}

AmpcoreResult ampcore_compressor_set_threshold(AmpcoreNode node, float threshold_db)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    static_cast<Compressor*>(node)->setThreshold(threshold_db);
    return AMPCORE_OK;
}

AmpcoreResult ampcore_compressor_set_ratio(AmpcoreNode node, float ratio)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    static_cast<Compressor*>(node)->setRatio(ratio);
    return AMPCORE_OK;
}

AmpcoreResult ampcore_compressor_set_attack(AmpcoreNode node, float seconds)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    static_cast<Compressor*>(node)->setAttack(seconds);
    return AMPCORE_OK;
}

AmpcoreResult ampcore_compressor_set_release(AmpcoreNode node, float seconds)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    static_cast<Compressor*>(node)->setRelease(seconds);
    return AMPCORE_OK;
}

AmpcoreResult ampcore_compressor_set_makeup_gain(AmpcoreNode node, float db)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    static_cast<Compressor*>(node)->setMakeupGain(db);
    return AMPCORE_OK;
}

/* ── Reverb ──────────────────────────────────────────────────────────────── */

AmpcoreResult ampcore_reverb_create(AmpcoreNode graph, uint32_t channels, AmpcoreNode* out)
{
    if (!graph || !out) return AMPCORE_ERROR_NULL_ARG;
    try {
        *out = new Reverb(*static_cast<ProcessorGraph*>(graph),
                         static_cast<ma_uint32>(channels));
        return AMPCORE_OK;
    } catch (...) {
        return AMPCORE_ERROR_INIT_FAILED;
    }
}

void ampcore_reverb_destroy(AmpcoreNode node)
{
    delete static_cast<Reverb*>(node);
}

AmpcoreResult ampcore_reverb_set_room_size(AmpcoreNode node, float size)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    static_cast<Reverb*>(node)->setRoomSize(size);
    return AMPCORE_OK;
}

AmpcoreResult ampcore_reverb_set_damping(AmpcoreNode node, float damping)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    static_cast<Reverb*>(node)->setDamping(damping);
    return AMPCORE_OK;
}

AmpcoreResult ampcore_reverb_set_mix(AmpcoreNode node, float mix)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    static_cast<Reverb*>(node)->setMix(mix);
    return AMPCORE_OK;
}

/* ── Tone Stack ──────────────────────────────────────────────────────────── */

AmpcoreResult ampcore_tone_stack_create(AmpcoreNode graph, uint32_t channels, uint32_t sample_rate,
                                AmpcoreNode* out)
{
    if (!graph || !out) return AMPCORE_ERROR_NULL_ARG;
    try {
        *out = new ToneStack(*static_cast<ProcessorGraph*>(graph),
                            static_cast<ma_uint32>(channels),
                            static_cast<ma_uint32>(sample_rate));
        return AMPCORE_OK;
    } catch (...) {
        return AMPCORE_ERROR_INIT_FAILED;
    }
}

void ampcore_tone_stack_destroy(AmpcoreNode node)
{
    delete static_cast<ToneStack*>(node);
}

AmpcoreResult ampcore_tone_stack_set_bass(AmpcoreNode node, float value)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    static_cast<ToneStack*>(node)->setBass(value);
    return AMPCORE_OK;
}

AmpcoreResult ampcore_tone_stack_set_mid(AmpcoreNode node, float value)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    static_cast<ToneStack*>(node)->setMid(value);
    return AMPCORE_OK;
}

AmpcoreResult ampcore_tone_stack_set_treble(AmpcoreNode node, float value)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    static_cast<ToneStack*>(node)->setTreble(value);
    return AMPCORE_OK;
}

/* ── Cabinet ─────────────────────────────────────────────────────────────── */

AmpcoreResult ampcore_cabinet_create(AmpcoreNode graph, uint32_t channels, AmpcoreNode* out)
{
    if (!graph || !out) return AMPCORE_ERROR_NULL_ARG;
    try {
        *out = new CabinetNode(*static_cast<ProcessorGraph*>(graph),
                               static_cast<ma_uint32>(channels));
        return AMPCORE_OK;
    } catch (...) {
        return AMPCORE_ERROR_INIT_FAILED;
    }
}

void ampcore_cabinet_destroy(AmpcoreNode node)
{
    delete static_cast<CabinetNode*>(node);
}

AmpcoreResult ampcore_cabinet_load_ir(AmpcoreNode node, const char* wav_path, uint32_t sample_rate)
{
    if (!node || !wav_path) return AMPCORE_ERROR_NULL_ARG;
    bool ok = static_cast<CabinetNode*>(node)->loadIR(
        wav_path, static_cast<ma_uint32>(sample_rate));
    return ok ? AMPCORE_OK : AMPCORE_ERROR_UNKNOWN;
}

AmpcoreResult ampcore_cabinet_set_mix(AmpcoreNode node, float mix)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    static_cast<CabinetNode*>(node)->setMix(mix);
    return AMPCORE_OK;
}

/* ── Meter ───────────────────────────────────────────────────────────────── */

AmpcoreResult ampcore_meter_create(AmpcoreNode graph, uint32_t channels, AmpcoreNode* out)
{
    if (!graph || !out) return AMPCORE_ERROR_NULL_ARG;
    try {
        *out = new MeterNode(*static_cast<ProcessorGraph*>(graph),
                             static_cast<ma_uint32>(channels));
        return AMPCORE_OK;
    } catch (...) {
        return AMPCORE_ERROR_INIT_FAILED;
    }
}

void ampcore_meter_destroy(AmpcoreNode node)
{
    delete static_cast<MeterNode*>(node);
}

float ampcore_meter_get_rms(AmpcoreNode node, uint32_t channel)
{
    if (!node) return 0.0f;
    return static_cast<MeterNode*>(node)->getRMS(channel);
}

float ampcore_meter_get_peak(AmpcoreNode node, uint32_t channel)
{
    if (!node) return 0.0f;
    return static_cast<MeterNode*>(node)->getPeak(channel);
}

/* ── Parameter Introspection ─────────────────────────────────────────────── */

int ampcore_node_get_parameter_count(AmpcoreNode node)
{
    if (!node) return 0;
    return static_cast<NodeBase*>(node)->getParameterCount();
}

AmpcoreResult ampcore_node_get_parameter_info(AmpcoreNode node, int index, AmpcoreParameterInfo* out)
{
    if (!node || !out) return AMPCORE_ERROR_NULL_ARG;
    if (index < 0 || index >= static_cast<NodeBase*>(node)->getParameterCount())
        return AMPCORE_ERROR_INVALID_ARG;
    try {
        ParameterInfo info = static_cast<NodeBase*>(node)->getParameterInfo(index);
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
    return static_cast<NodeBase*>(node)->getParameterValue(index);
}

AmpcoreResult ampcore_node_set_parameter_value(AmpcoreNode node, int index, float value)
{
    if (!node) return AMPCORE_ERROR_NULL_ARG;
    if (index < 0 || index >= static_cast<NodeBase*>(node)->getParameterCount())
        return AMPCORE_ERROR_INVALID_ARG;
    try {
        static_cast<NodeBase*>(node)->setParameterValue(index, value);
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
        std::ofstream ofs(path);
        if (!ofs) return AMPCORE_ERROR_FILE_NOT_FOUND;
        ofs << json;
        return ofs.good() ? AMPCORE_OK : AMPCORE_ERROR_UNKNOWN;
    } catch (...) {
        return AMPCORE_ERROR_UNKNOWN;
    }
}

AmpcoreResult ampcore_preset_load(AmpcoreNode graph, const char* path)
{
    if (!graph || !path) return AMPCORE_ERROR_NULL_ARG;
    try {
        std::ifstream ifs(path);
        if (!ifs) return AMPCORE_ERROR_FILE_NOT_FOUND;
        std::ostringstream ss;
        ss << ifs.rdbuf();
        bool ok = static_cast<ProcessorGraph*>(graph)->loadFromJson(ss.str());
        return ok ? AMPCORE_OK : AMPCORE_ERROR_UNKNOWN;
    } catch (...) {
        return AMPCORE_ERROR_UNKNOWN;
    }
}
