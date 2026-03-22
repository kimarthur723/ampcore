#include "amp_capi.h"
#include "miniaudio.h"

import node_base;
import processor_graph;
import audio_engine;
import file_input_node;
import fuzz;
import delay;
import primitives;

/* ── ProcessorGraph ──────────────────────────────────────────────────────── */

AmpNode amp_graph_create(uint32_t channels)
{
    return new ProcessorGraph(static_cast<ma_uint32>(channels));
}

void amp_graph_destroy(AmpNode graph)
{
    delete static_cast<ProcessorGraph*>(graph);
}

void amp_graph_connect(AmpNode graph, AmpNode from, AmpNode to)
{
    static_cast<ProcessorGraph*>(graph)->connect(
        static_cast<NodeBase*>(from), static_cast<NodeBase*>(to));
}

void amp_graph_connect_to_output(AmpNode graph, AmpNode node)
{
    static_cast<ProcessorGraph*>(graph)->connectToOutput(
        static_cast<NodeBase*>(node));
}

void amp_graph_post_connect(AmpNode graph, AmpNode from, AmpNode to)
{
    static_cast<ProcessorGraph*>(graph)->postConnect(
        static_cast<NodeBase*>(from), static_cast<NodeBase*>(to));
}

void amp_graph_post_connect_to_output(AmpNode graph, AmpNode node)
{
    static_cast<ProcessorGraph*>(graph)->postConnectToOutput(
        static_cast<NodeBase*>(node));
}

void amp_graph_post_disconnect(AmpNode graph, AmpNode node)
{
    static_cast<ProcessorGraph*>(graph)->postDisconnect(
        static_cast<NodeBase*>(node));
}

/* ── AudioEngine ─────────────────────────────────────────────────────────── */

AmpNode amp_engine_create(AmpNode graph, uint32_t sample_rate)
{
    return new AudioEngine(*static_cast<ProcessorGraph*>(graph),
                           static_cast<ma_uint32>(sample_rate));
}

void amp_engine_destroy(AmpNode engine)
{
    delete static_cast<AudioEngine*>(engine);
}

void amp_engine_start(AmpNode engine)
{
    static_cast<AudioEngine*>(engine)->start();
}

void amp_engine_stop(AmpNode engine)
{
    static_cast<AudioEngine*>(engine)->stop();
}

int amp_engine_is_started(AmpNode engine)
{
    return static_cast<AudioEngine*>(engine)->isStarted() ? 1 : 0;
}

uint32_t amp_engine_get_sample_rate(AmpNode engine)
{
    return static_cast<AudioEngine*>(engine)->getSampleRate();
}

/* ── FileInputNode ───────────────────────────────────────────────────────── */

AmpNode amp_file_input_create(AmpNode graph, const char* path,
                               uint32_t channels, uint32_t sample_rate)
{
    return new FileInputNode(*static_cast<ProcessorGraph*>(graph), path,
                             static_cast<ma_uint32>(channels),
                             static_cast<ma_uint32>(sample_rate));
}

void amp_file_input_destroy(AmpNode node)
{
    delete static_cast<FileInputNode*>(node);
}

/* ── Fuzz ────────────────────────────────────────────────────────────────── */

AmpNode amp_fuzz_create(AmpNode graph, uint32_t channels,
                         float gain, float threshold)
{
    return new Fuzz(*static_cast<ProcessorGraph*>(graph),
                   static_cast<ma_uint32>(channels), gain, threshold);
}

void amp_fuzz_destroy(AmpNode node)
{
    delete static_cast<Fuzz*>(node);
}

void amp_fuzz_set_gain(AmpNode node, float gain)
{
    static_cast<Fuzz*>(node)->setGain(gain);
}

void amp_fuzz_set_threshold(AmpNode node, float threshold)
{
    static_cast<Fuzz*>(node)->setThreshold(threshold);
}

/* ── Delay ───────────────────────────────────────────────────────────────── */

AmpNode amp_delay_create(AmpNode graph, uint32_t channels, uint32_t sample_rate,
                          float delay_sec, float feedback, float mix)
{
    return new Delay(*static_cast<ProcessorGraph*>(graph),
                    static_cast<ma_uint32>(channels),
                    static_cast<ma_uint32>(sample_rate),
                    delay_sec, feedback, mix);
}

void amp_delay_destroy(AmpNode node)
{
    delete static_cast<Delay*>(node);
}

void amp_delay_set_feedback(AmpNode node, float feedback)
{
    static_cast<Delay*>(node)->setFeedback(feedback);
}

void amp_delay_set_mix(AmpNode node, float mix)
{
    static_cast<Delay*>(node)->setMix(mix);
}

/* ── Gain ─────────────────────────────────────────────────────────────────── */

AmpNode amp_gain_create(AmpNode graph, uint32_t channels, float gain)
{
    return new Gain(*static_cast<ProcessorGraph*>(graph),
                   static_cast<ma_uint32>(channels), gain);
}

void amp_gain_destroy(AmpNode node)
{
    delete static_cast<Gain*>(node);
}

void amp_gain_set_gain(AmpNode node, float gain)
{
    static_cast<Gain*>(node)->setGain(gain);
}

/* ── Biquad ───────────────────────────────────────────────────────────────── */

AmpNode amp_biquad_create(AmpNode graph, uint32_t channels, uint32_t sample_rate,
                           int mode, float freq, float q, float db_gain)
{
    return new Biquad(*static_cast<ProcessorGraph*>(graph),
                     static_cast<ma_uint32>(channels),
                     static_cast<ma_uint32>(sample_rate),
                     static_cast<Biquad::Mode>(mode), freq, q, db_gain);
}

void amp_biquad_destroy(AmpNode node)
{
    delete static_cast<Biquad*>(node);
}

void amp_biquad_set_freq(AmpNode node, float freq)
{
    static_cast<Biquad*>(node)->setFreq(freq);
}

void amp_biquad_set_q(AmpNode node, float q)
{
    static_cast<Biquad*>(node)->setQ(q);
}

void amp_biquad_set_db_gain(AmpNode node, float db_gain)
{
    static_cast<Biquad*>(node)->setDbGain(db_gain);
}

void amp_biquad_set_mode(AmpNode node, int mode)
{
    static_cast<Biquad*>(node)->setMode(static_cast<Biquad::Mode>(mode));
}
