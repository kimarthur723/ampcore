#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle — points to a heap-allocated C++ object.
   Callers never dereference these; they are just passed back to other functions. */
typedef void* AmpNode;

/* ── ProcessorGraph ──────────────────────────────────────────────────────── */
AmpNode amp_graph_create(uint32_t channels);
void    amp_graph_destroy(AmpNode graph);

/* Immediate wiring — safe before the engine starts */
void amp_graph_connect(AmpNode graph, AmpNode from, AmpNode to);
void amp_graph_connect_to_output(AmpNode graph, AmpNode node);

/* Thread-safe wiring — enqueue changes for the audio thread */
void amp_graph_post_connect(AmpNode graph, AmpNode from, AmpNode to);
void amp_graph_post_connect_to_output(AmpNode graph, AmpNode node);
void amp_graph_post_disconnect(AmpNode graph, AmpNode node);

/* ── AudioEngine ─────────────────────────────────────────────────────────── */
AmpNode  amp_engine_create(AmpNode graph, uint32_t sample_rate);
void     amp_engine_destroy(AmpNode engine);
void     amp_engine_start(AmpNode engine);
void     amp_engine_stop(AmpNode engine);
int      amp_engine_is_started(AmpNode engine);
uint32_t amp_engine_get_sample_rate(AmpNode engine);

/* ── FileInputNode ───────────────────────────────────────────────────────── */
AmpNode amp_file_input_create(AmpNode graph, const char* path,
                               uint32_t channels, uint32_t sample_rate);
void    amp_file_input_destroy(AmpNode node);

/* ── Fuzz ────────────────────────────────────────────────────────────────── */
AmpNode amp_fuzz_create(AmpNode graph, uint32_t channels,
                         float gain, float threshold);
void    amp_fuzz_destroy(AmpNode node);
void    amp_fuzz_set_gain(AmpNode node, float gain);
void    amp_fuzz_set_threshold(AmpNode node, float threshold);

/* ── Delay ───────────────────────────────────────────────────────────────── */
AmpNode amp_delay_create(AmpNode graph, uint32_t channels, uint32_t sample_rate,
                          float delay_sec, float feedback, float mix);
void    amp_delay_destroy(AmpNode node);
void    amp_delay_set_feedback(AmpNode node, float feedback);
void    amp_delay_set_mix(AmpNode node, float mix);

/* ── Gain ─────────────────────────────────────────────────────────────────── */
AmpNode amp_gain_create(AmpNode graph, uint32_t channels, float gain);
void    amp_gain_destroy(AmpNode node);
void    amp_gain_set_gain(AmpNode node, float gain);

/* ── Biquad ───────────────────────────────────────────────────────────────── */
/* mode: 0=LowPass 1=HighPass 2=BandPass 3=Notch 4=Peak 5=LowShelf 6=HighShelf */
AmpNode amp_biquad_create(AmpNode graph, uint32_t channels, uint32_t sample_rate,
                           int mode, float freq, float q, float db_gain);
void    amp_biquad_destroy(AmpNode node);
void    amp_biquad_set_freq(AmpNode node, float freq);
void    amp_biquad_set_q(AmpNode node, float q);
void    amp_biquad_set_db_gain(AmpNode node, float db_gain);
void    amp_biquad_set_mode(AmpNode node, int mode);

#ifdef __cplusplus
}
#endif
