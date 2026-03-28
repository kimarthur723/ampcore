#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SIGCHAIN_VERSION_MAJOR 0
#define SIGCHAIN_VERSION_MINOR 1
#define SIGCHAIN_VERSION_PATCH 0

typedef enum {
    SIGCHAIN_OK = 0,
    SIGCHAIN_ERROR_UNKNOWN = -1,
    SIGCHAIN_ERROR_NULL_ARG = -2,
    SIGCHAIN_ERROR_INVALID_ARG = -3,
    SIGCHAIN_ERROR_QUEUE_FULL = -4,
    SIGCHAIN_ERROR_INIT_FAILED = -5,
    SIGCHAIN_ERROR_FILE_NOT_FOUND = -6
} SigchainResult;

typedef struct {
    const char* name;
    float min_value;
    float max_value;
    float default_value;
    const char* unit;
} SigchainParameterInfo;

/* Opaque handle — points to a heap-allocated C++ object.
   Callers never dereference these; they are just passed back to other functions. */
typedef void* SigchainNode;

/* ── ProcessorGraph ──────────────────────────────────────────────────────── */
SigchainResult sigchain_graph_create(uint32_t channels, SigchainNode* out);
void      sigchain_graph_destroy(SigchainNode graph);

/* Immediate wiring — safe before the engine starts */
SigchainResult sigchain_graph_connect(SigchainNode graph, SigchainNode from, SigchainNode to);
SigchainResult sigchain_graph_connect_to_output(SigchainNode graph, SigchainNode node);

/* Thread-safe wiring — enqueue changes for the audio thread */
SigchainResult sigchain_graph_post_connect(SigchainNode graph, SigchainNode from, SigchainNode to);
SigchainResult sigchain_graph_post_connect_to_output(SigchainNode graph, SigchainNode node);
SigchainResult sigchain_graph_post_disconnect(SigchainNode graph, SigchainNode node);

/* ── AudioEngine ─────────────────────────────────────────────────────────── */
SigchainResult sigchain_engine_create(SigchainNode graph, uint32_t sample_rate, SigchainNode* out);
void      sigchain_engine_destroy(SigchainNode engine);
SigchainResult sigchain_engine_start(SigchainNode engine);
SigchainResult sigchain_engine_stop(SigchainNode engine);
int       sigchain_engine_is_started(SigchainNode engine);
uint32_t  sigchain_engine_get_sample_rate(SigchainNode engine);
SigchainResult sigchain_engine_set_input_node(SigchainNode engine, SigchainNode input_node);

/* ── AudioInputNode ─────────────────────────────────────────────────────── */
SigchainResult sigchain_audio_input_create(SigchainNode graph, uint32_t channels, SigchainNode* out);
void      sigchain_audio_input_destroy(SigchainNode node);

/* ── FileInputNode ───────────────────────────────────────────────────────── */
SigchainResult sigchain_file_input_create(SigchainNode graph, const char* path,
                                uint32_t channels, uint32_t sample_rate,
                                SigchainNode* out);
void      sigchain_file_input_destroy(SigchainNode node);

/* ── Fuzz ────────────────────────────────────────────────────────────────── */
SigchainResult sigchain_fuzz_create(SigchainNode graph, uint32_t channels,
                          float gain, float threshold, SigchainNode* out);
void      sigchain_fuzz_destroy(SigchainNode node);
SigchainResult sigchain_fuzz_set_gain(SigchainNode node, float gain);
SigchainResult sigchain_fuzz_set_threshold(SigchainNode node, float threshold);

/* ── Delay ───────────────────────────────────────────────────────────────── */
SigchainResult sigchain_delay_create(SigchainNode graph, uint32_t channels, uint32_t sample_rate,
                           float delay_sec, float feedback, float mix,
                           SigchainNode* out);
void      sigchain_delay_destroy(SigchainNode node);
SigchainResult sigchain_delay_set_feedback(SigchainNode node, float feedback);
SigchainResult sigchain_delay_set_mix(SigchainNode node, float mix);

/* ── Gain ─────────────────────────────────────────────────────────────────── */
SigchainResult sigchain_gain_create(SigchainNode graph, uint32_t channels, float gain,
                          SigchainNode* out);
void      sigchain_gain_destroy(SigchainNode node);
SigchainResult sigchain_gain_set_gain(SigchainNode node, float gain);

/* ── Biquad ───────────────────────────────────────────────────────────────── */
/* mode: 0=LowPass 1=HighPass 2=BandPass 3=Notch 4=Peak 5=LowShelf 6=HighShelf */
SigchainResult sigchain_biquad_create(SigchainNode graph, uint32_t channels, uint32_t sample_rate,
                            int mode, float freq, float q, float db_gain,
                            SigchainNode* out);
void      sigchain_biquad_destroy(SigchainNode node);
SigchainResult sigchain_biquad_set_freq(SigchainNode node, float freq);
SigchainResult sigchain_biquad_set_q(SigchainNode node, float q);
SigchainResult sigchain_biquad_set_db_gain(SigchainNode node, float db_gain);
SigchainResult sigchain_biquad_set_mode(SigchainNode node, int mode);

/* ── Noise Gate ──────────────────────────────────────────────────────────── */
SigchainResult sigchain_noise_gate_create(SigchainNode graph, uint32_t channels, uint32_t sample_rate,
                                SigchainNode* out);
void      sigchain_noise_gate_destroy(SigchainNode node);
SigchainResult sigchain_noise_gate_set_threshold(SigchainNode node, float threshold_db);
SigchainResult sigchain_noise_gate_set_attack(SigchainNode node, float seconds);
SigchainResult sigchain_noise_gate_set_release(SigchainNode node, float seconds);

/* ── Compressor ──────────────────────────────────────────────────────────── */
SigchainResult sigchain_compressor_create(SigchainNode graph, uint32_t channels, uint32_t sample_rate,
                                SigchainNode* out);
void      sigchain_compressor_destroy(SigchainNode node);
SigchainResult sigchain_compressor_set_threshold(SigchainNode node, float threshold_db);
SigchainResult sigchain_compressor_set_ratio(SigchainNode node, float ratio);
SigchainResult sigchain_compressor_set_attack(SigchainNode node, float seconds);
SigchainResult sigchain_compressor_set_release(SigchainNode node, float seconds);
SigchainResult sigchain_compressor_set_makeup_gain(SigchainNode node, float db);

/* ── Reverb ──────────────────────────────────────────────────────────────── */
SigchainResult sigchain_reverb_create(SigchainNode graph, uint32_t channels, SigchainNode* out);
void      sigchain_reverb_destroy(SigchainNode node);
SigchainResult sigchain_reverb_set_room_size(SigchainNode node, float size);
SigchainResult sigchain_reverb_set_damping(SigchainNode node, float damping);
SigchainResult sigchain_reverb_set_mix(SigchainNode node, float mix);

/* ── Tone Stack ──────────────────────────────────────────────────────────── */
SigchainResult sigchain_tone_stack_create(SigchainNode graph, uint32_t channels, uint32_t sample_rate,
                                SigchainNode* out);
void      sigchain_tone_stack_destroy(SigchainNode node);
SigchainResult sigchain_tone_stack_set_bass(SigchainNode node, float value);
SigchainResult sigchain_tone_stack_set_mid(SigchainNode node, float value);
SigchainResult sigchain_tone_stack_set_treble(SigchainNode node, float value);

/* ── Cabinet ─────────────────────────────────────────────────────────────── */
SigchainResult sigchain_cabinet_create(SigchainNode graph, uint32_t channels, SigchainNode* out);
void      sigchain_cabinet_destroy(SigchainNode node);
SigchainResult sigchain_cabinet_load_ir(SigchainNode node, const char* wav_path, uint32_t sample_rate);
SigchainResult sigchain_cabinet_set_mix(SigchainNode node, float mix);

/* ── Meter ───────────────────────────────────────────────────────────────── */
SigchainResult sigchain_meter_create(SigchainNode graph, uint32_t channels, SigchainNode* out);
void      sigchain_meter_destroy(SigchainNode node);
float     sigchain_meter_get_rms(SigchainNode node, uint32_t channel);
float     sigchain_meter_get_peak(SigchainNode node, uint32_t channel);

/* ── Parameter Introspection ─────────────────────────────────────────────── */
int       sigchain_node_get_parameter_count(SigchainNode node);
SigchainResult sigchain_node_get_parameter_info(SigchainNode node, int index, SigchainParameterInfo* out);
float     sigchain_node_get_parameter_value(SigchainNode node, int index);
SigchainResult sigchain_node_set_parameter_value(SigchainNode node, int index, float value);

/* ── Presets ─────────────────────────────────────────────────────────────── */
SigchainResult sigchain_preset_save(SigchainNode graph, const char* path);
SigchainResult sigchain_preset_load(SigchainNode graph, const char* path);

#ifdef __cplusplus
}
#endif
