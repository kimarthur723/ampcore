#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AMPCORE_VERSION_MAJOR 0
#define AMPCORE_VERSION_MINOR 1
#define AMPCORE_VERSION_PATCH 0

typedef enum {
    AMPCORE_OK = 0,
    AMPCORE_ERROR_UNKNOWN = -1,
    AMPCORE_ERROR_NULL_ARG = -2,
    AMPCORE_ERROR_INVALID_ARG = -3,
    AMPCORE_ERROR_QUEUE_FULL = -4,
    AMPCORE_ERROR_INIT_FAILED = -5,
    AMPCORE_ERROR_FILE_NOT_FOUND = -6
} AmpcoreResult;

typedef struct {
    const char* name;
    float min_value;
    float max_value;
    float default_value;
    const char* unit;
} AmpcoreParameterInfo;

/* Opaque handle — points to a heap-allocated C++ object.
   Callers never dereference these; they are just passed back to other functions. */
typedef void* AmpcoreNode;

/* ── ProcessorGraph ──────────────────────────────────────────────────────── */
AmpcoreResult ampcore_graph_create(uint32_t channels, AmpcoreNode* out);
void      ampcore_graph_destroy(AmpcoreNode graph);

/* Immediate wiring — safe before the engine starts */
AmpcoreResult ampcore_graph_connect(AmpcoreNode graph, AmpcoreNode from, AmpcoreNode to);
AmpcoreResult ampcore_graph_connect_to_output(AmpcoreNode graph, AmpcoreNode node);

/* Thread-safe wiring — enqueue changes for the audio thread */
AmpcoreResult ampcore_graph_post_connect(AmpcoreNode graph, AmpcoreNode from, AmpcoreNode to);
AmpcoreResult ampcore_graph_post_connect_to_output(AmpcoreNode graph, AmpcoreNode node);
AmpcoreResult ampcore_graph_post_disconnect(AmpcoreNode graph, AmpcoreNode node);

/* ── AudioEngine ─────────────────────────────────────────────────────────── */
AmpcoreResult ampcore_engine_create(AmpcoreNode graph, uint32_t sample_rate, AmpcoreNode* out);
void      ampcore_engine_destroy(AmpcoreNode engine);
AmpcoreResult ampcore_engine_start(AmpcoreNode engine);
AmpcoreResult ampcore_engine_stop(AmpcoreNode engine);
int       ampcore_engine_is_started(AmpcoreNode engine);
uint32_t  ampcore_engine_get_sample_rate(AmpcoreNode engine);
AmpcoreResult ampcore_engine_set_input_node(AmpcoreNode engine, AmpcoreNode input_node);

/* ── AudioInputNode ─────────────────────────────────────────────────────── */
AmpcoreResult ampcore_audio_input_create(AmpcoreNode graph, uint32_t channels, AmpcoreNode* out);
void      ampcore_audio_input_destroy(AmpcoreNode node);

/* ── FileInputNode ───────────────────────────────────────────────────────── */
AmpcoreResult ampcore_file_input_create(AmpcoreNode graph, const char* path,
                                uint32_t channels, uint32_t sample_rate,
                                AmpcoreNode* out);
void      ampcore_file_input_destroy(AmpcoreNode node);

/* ── Fuzz ────────────────────────────────────────────────────────────────── */
AmpcoreResult ampcore_fuzz_create(AmpcoreNode graph, uint32_t channels,
                          float gain, float threshold, AmpcoreNode* out);
void      ampcore_fuzz_destroy(AmpcoreNode node);
AmpcoreResult ampcore_fuzz_set_gain(AmpcoreNode node, float gain);
AmpcoreResult ampcore_fuzz_set_threshold(AmpcoreNode node, float threshold);

/* ── Delay ───────────────────────────────────────────────────────────────── */
AmpcoreResult ampcore_delay_create(AmpcoreNode graph, uint32_t channels, uint32_t sample_rate,
                           float delay_sec, float feedback, float mix,
                           AmpcoreNode* out);
void      ampcore_delay_destroy(AmpcoreNode node);
AmpcoreResult ampcore_delay_set_feedback(AmpcoreNode node, float feedback);
AmpcoreResult ampcore_delay_set_mix(AmpcoreNode node, float mix);

/* ── Gain ─────────────────────────────────────────────────────────────────── */
AmpcoreResult ampcore_gain_create(AmpcoreNode graph, uint32_t channels, float gain,
                          AmpcoreNode* out);
void      ampcore_gain_destroy(AmpcoreNode node);
AmpcoreResult ampcore_gain_set_gain(AmpcoreNode node, float gain);

/* ── Biquad ───────────────────────────────────────────────────────────────── */
/* mode: 0=LowPass 1=HighPass 2=BandPass 3=Notch 4=Peak 5=LowShelf 6=HighShelf */
AmpcoreResult ampcore_biquad_create(AmpcoreNode graph, uint32_t channels, uint32_t sample_rate,
                            int mode, float freq, float q, float db_gain,
                            AmpcoreNode* out);
void      ampcore_biquad_destroy(AmpcoreNode node);
AmpcoreResult ampcore_biquad_set_freq(AmpcoreNode node, float freq);
AmpcoreResult ampcore_biquad_set_q(AmpcoreNode node, float q);
AmpcoreResult ampcore_biquad_set_db_gain(AmpcoreNode node, float db_gain);
AmpcoreResult ampcore_biquad_set_mode(AmpcoreNode node, int mode);

/* ── Noise Gate ──────────────────────────────────────────────────────────── */
AmpcoreResult ampcore_noise_gate_create(AmpcoreNode graph, uint32_t channels, uint32_t sample_rate,
                                AmpcoreNode* out);
void      ampcore_noise_gate_destroy(AmpcoreNode node);
AmpcoreResult ampcore_noise_gate_set_threshold(AmpcoreNode node, float threshold_db);
AmpcoreResult ampcore_noise_gate_set_attack(AmpcoreNode node, float seconds);
AmpcoreResult ampcore_noise_gate_set_release(AmpcoreNode node, float seconds);

/* ── Compressor ──────────────────────────────────────────────────────────── */
AmpcoreResult ampcore_compressor_create(AmpcoreNode graph, uint32_t channels, uint32_t sample_rate,
                                AmpcoreNode* out);
void      ampcore_compressor_destroy(AmpcoreNode node);
AmpcoreResult ampcore_compressor_set_threshold(AmpcoreNode node, float threshold_db);
AmpcoreResult ampcore_compressor_set_ratio(AmpcoreNode node, float ratio);
AmpcoreResult ampcore_compressor_set_attack(AmpcoreNode node, float seconds);
AmpcoreResult ampcore_compressor_set_release(AmpcoreNode node, float seconds);
AmpcoreResult ampcore_compressor_set_makeup_gain(AmpcoreNode node, float db);

/* ── Reverb ──────────────────────────────────────────────────────────────── */
AmpcoreResult ampcore_reverb_create(AmpcoreNode graph, uint32_t channels, AmpcoreNode* out);
void      ampcore_reverb_destroy(AmpcoreNode node);
AmpcoreResult ampcore_reverb_set_room_size(AmpcoreNode node, float size);
AmpcoreResult ampcore_reverb_set_damping(AmpcoreNode node, float damping);
AmpcoreResult ampcore_reverb_set_mix(AmpcoreNode node, float mix);

/* ── Tone Stack ──────────────────────────────────────────────────────────── */
AmpcoreResult ampcore_tone_stack_create(AmpcoreNode graph, uint32_t channels, uint32_t sample_rate,
                                AmpcoreNode* out);
void      ampcore_tone_stack_destroy(AmpcoreNode node);
AmpcoreResult ampcore_tone_stack_set_bass(AmpcoreNode node, float value);
AmpcoreResult ampcore_tone_stack_set_mid(AmpcoreNode node, float value);
AmpcoreResult ampcore_tone_stack_set_treble(AmpcoreNode node, float value);

/* ── Cabinet ─────────────────────────────────────────────────────────────── */
AmpcoreResult ampcore_cabinet_create(AmpcoreNode graph, uint32_t channels, AmpcoreNode* out);
void      ampcore_cabinet_destroy(AmpcoreNode node);
AmpcoreResult ampcore_cabinet_load_ir(AmpcoreNode node, const char* wav_path, uint32_t sample_rate);
AmpcoreResult ampcore_cabinet_set_mix(AmpcoreNode node, float mix);

/* ── Meter ───────────────────────────────────────────────────────────────── */
AmpcoreResult ampcore_meter_create(AmpcoreNode graph, uint32_t channels, AmpcoreNode* out);
void      ampcore_meter_destroy(AmpcoreNode node);
float     ampcore_meter_get_rms(AmpcoreNode node, uint32_t channel);
float     ampcore_meter_get_peak(AmpcoreNode node, uint32_t channel);

/* ── Parameter Introspection ─────────────────────────────────────────────── */
int       ampcore_node_get_parameter_count(AmpcoreNode node);
AmpcoreResult ampcore_node_get_parameter_info(AmpcoreNode node, int index, AmpcoreParameterInfo* out);
float     ampcore_node_get_parameter_value(AmpcoreNode node, int index);
AmpcoreResult ampcore_node_set_parameter_value(AmpcoreNode node, int index, float value);

/* ── Presets ─────────────────────────────────────────────────────────────── */
AmpcoreResult ampcore_preset_save(AmpcoreNode graph, const char* path);
AmpcoreResult ampcore_preset_load(AmpcoreNode graph, const char* path);

#ifdef __cplusplus
}
#endif
