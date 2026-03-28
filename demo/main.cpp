#include <ampcore/ampcore_capi.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>

static void check(AmpcoreResult r, const char* what)
{
    if (r != AMPCORE_OK) {
        fprintf(stderr, "error: %s failed (code %d)\n", what, r);
        exit(1);
    }
}

static float to_db(float linear)
{
    if (linear < 1e-9f) return -90.0f;
    return 20.0f * std::log10(linear);
}

int main()
{
    AmpcoreNode graph, engine;
    check(ampcore_graph_create(2, &graph),       "ampcore_graph_create");
    check(ampcore_engine_create(graph, 44100, &engine), "ampcore_engine_create");

    uint32_t sr = ampcore_engine_get_sample_rate(engine);

    // --- create nodes ---
    AmpcoreNode input, gate, fuzz, tonestack, comp, reverb, meter;
    check(ampcore_audio_input_create(graph, 2, &input),           "ampcore_audio_input_create");
    check(ampcore_noise_gate_create(graph, 2, sr, &gate),         "ampcore_noise_gate_create");
    check(ampcore_fuzz_create(graph, 2, 8.0f, 0.7f, &fuzz),      "ampcore_fuzz_create");
    check(ampcore_tone_stack_create(graph, 2, sr, &tonestack),    "ampcore_tone_stack_create");
    check(ampcore_compressor_create(graph, 2, sr, &comp),         "ampcore_compressor_create");
    check(ampcore_reverb_create(graph, 2, &reverb),               "ampcore_reverb_create");
    check(ampcore_meter_create(graph, 2, &meter),                 "ampcore_meter_create");

    // --- tune parameters ---
    ampcore_noise_gate_set_threshold(gate, -50.0f);
    ampcore_noise_gate_set_attack(gate, 0.005f);
    ampcore_noise_gate_set_release(gate, 0.1f);

    ampcore_fuzz_set_gain(fuzz, 8.0f);
    ampcore_fuzz_set_threshold(fuzz, 0.7f);

    ampcore_tone_stack_set_bass(tonestack, 0.6f);
    ampcore_tone_stack_set_mid(tonestack, 0.4f);
    ampcore_tone_stack_set_treble(tonestack, 0.6f);

    ampcore_compressor_set_threshold(comp, -18.0f);
    ampcore_compressor_set_ratio(comp, 4.0f);
    ampcore_compressor_set_attack(comp, 0.01f);
    ampcore_compressor_set_release(comp, 0.1f);
    ampcore_compressor_set_makeup_gain(comp, 6.0f);

    ampcore_reverb_set_room_size(reverb, 0.5f);
    ampcore_reverb_set_damping(reverb, 0.5f);
    ampcore_reverb_set_mix(reverb, 0.2f);

    // --- initial chain ---
    // input -> gate -> fuzz -> tonestack -> comp -> reverb -> meter -> output
    ampcore_graph_connect(graph, input,     gate);
    ampcore_graph_connect(graph, gate,      fuzz);
    ampcore_graph_connect(graph, fuzz,      tonestack);
    ampcore_graph_connect(graph, tonestack, comp);
    ampcore_graph_connect(graph, comp,      reverb);
    ampcore_graph_connect(graph, reverb,    meter);
    ampcore_graph_connect_to_output(graph, meter);

    ampcore_engine_set_input_node(engine, input);
    check(ampcore_engine_start(engine), "ampcore_engine_start");

    std::atomic<bool> running{true};
    bool fuzz_active = true;

    // print RMS levels once per second
    std::thread meter_thread([&] {
        while (running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (!running.load()) break;
            float l = to_db(ampcore_meter_get_rms(meter, 0));
            float r = to_db(ampcore_meter_get_rms(meter, 1));
            printf("  RMS  L: %+5.1f dB  R: %+5.1f dB\n", l, r);
        }
    });

    printf("Chain: input -> gate -> [fuzz] -> tone -> comp -> reverb -> meter -> out\n");
    printf("Press Enter to toggle fuzz. Type 'q' + Enter to quit.\n\n");

    char line[64];
    while (fgets(line, sizeof(line), stdin)) {
        if (line[0] == 'q') break;

        if (fuzz_active) {
            // bypass fuzz: reconnect gate directly to tonestack
            ampcore_graph_post_connect(graph, gate, tonestack);
            ampcore_graph_post_disconnect(graph, fuzz);
            fuzz_active = false;
            printf("fuzz off\n");
        } else {
            // restore fuzz: gate -> fuzz -> tonestack
            ampcore_graph_post_connect(graph, gate, fuzz);
            ampcore_graph_post_connect(graph, fuzz, tonestack);
            fuzz_active = true;
            printf("fuzz on\n");
        }
    }

    running.store(false);
    meter_thread.join();

    ampcore_engine_stop(engine);
    ampcore_engine_destroy(engine);

    ampcore_meter_destroy(meter);
    ampcore_reverb_destroy(reverb);
    ampcore_compressor_destroy(comp);
    ampcore_tone_stack_destroy(tonestack);
    ampcore_fuzz_destroy(fuzz);
    ampcore_noise_gate_destroy(gate);
    ampcore_audio_input_destroy(input);
    ampcore_graph_destroy(graph);

    return 0;
}
