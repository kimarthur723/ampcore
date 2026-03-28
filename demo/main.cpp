#include <sigchain/sigchain_capi.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>

static void check(SigchainResult r, const char* what)
{
    if (r != SIGCHAIN_OK) {
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
    SigchainNode graph, engine;
    check(sigchain_graph_create(2, &graph),       "sigchain_graph_create");
    check(sigchain_engine_create(graph, 44100, &engine), "sigchain_engine_create");

    uint32_t sr = sigchain_engine_get_sample_rate(engine);

    // --- create nodes ---
    SigchainNode input, gate, fuzz, tonestack, comp, reverb, meter;
    check(sigchain_audio_input_create(graph, 2, &input),           "sigchain_audio_input_create");
    check(sigchain_noise_gate_create(graph, 2, sr, &gate),         "sigchain_noise_gate_create");
    check(sigchain_fuzz_create(graph, 2, 8.0f, 0.7f, &fuzz),      "sigchain_fuzz_create");
    check(sigchain_tone_stack_create(graph, 2, sr, &tonestack),    "sigchain_tone_stack_create");
    check(sigchain_compressor_create(graph, 2, sr, &comp),         "sigchain_compressor_create");
    check(sigchain_reverb_create(graph, 2, &reverb),               "sigchain_reverb_create");
    check(sigchain_meter_create(graph, 2, &meter),                 "sigchain_meter_create");

    // --- tune parameters ---
    sigchain_noise_gate_set_threshold(gate, -50.0f);
    sigchain_noise_gate_set_attack(gate, 0.005f);
    sigchain_noise_gate_set_release(gate, 0.1f);

    sigchain_fuzz_set_gain(fuzz, 8.0f);
    sigchain_fuzz_set_threshold(fuzz, 0.7f);

    sigchain_tone_stack_set_bass(tonestack, 0.6f);
    sigchain_tone_stack_set_mid(tonestack, 0.4f);
    sigchain_tone_stack_set_treble(tonestack, 0.6f);

    sigchain_compressor_set_threshold(comp, -18.0f);
    sigchain_compressor_set_ratio(comp, 4.0f);
    sigchain_compressor_set_attack(comp, 0.01f);
    sigchain_compressor_set_release(comp, 0.1f);
    sigchain_compressor_set_makeup_gain(comp, 6.0f);

    sigchain_reverb_set_room_size(reverb, 0.5f);
    sigchain_reverb_set_damping(reverb, 0.5f);
    sigchain_reverb_set_mix(reverb, 0.2f);

    // --- initial chain ---
    // input -> gate -> fuzz -> tonestack -> comp -> reverb -> meter -> output
    sigchain_graph_connect(graph, input,     gate);
    sigchain_graph_connect(graph, gate,      fuzz);
    sigchain_graph_connect(graph, fuzz,      tonestack);
    sigchain_graph_connect(graph, tonestack, comp);
    sigchain_graph_connect(graph, comp,      reverb);
    sigchain_graph_connect(graph, reverb,    meter);
    sigchain_graph_connect_to_output(graph, meter);

    sigchain_engine_set_input_node(engine, input);
    check(sigchain_engine_start(engine), "sigchain_engine_start");

    std::atomic<bool> running{true};
    bool fuzz_active = true;

    // print RMS levels once per second
    std::thread meter_thread([&] {
        while (running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (!running.load()) break;
            float l = to_db(sigchain_meter_get_rms(meter, 0));
            float r = to_db(sigchain_meter_get_rms(meter, 1));
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
            sigchain_graph_post_connect(graph, gate, tonestack);
            sigchain_graph_post_disconnect(graph, fuzz);
            fuzz_active = false;
            printf("fuzz off\n");
        } else {
            // restore fuzz: gate -> fuzz -> tonestack
            sigchain_graph_post_connect(graph, gate, fuzz);
            sigchain_graph_post_connect(graph, fuzz, tonestack);
            fuzz_active = true;
            printf("fuzz on\n");
        }
    }

    running.store(false);
    meter_thread.join();

    sigchain_engine_stop(engine);
    sigchain_engine_destroy(engine);

    sigchain_meter_destroy(meter);
    sigchain_reverb_destroy(reverb);
    sigchain_compressor_destroy(comp);
    sigchain_tone_stack_destroy(tonestack);
    sigchain_fuzz_destroy(fuzz);
    sigchain_noise_gate_destroy(gate);
    sigchain_audio_input_destroy(input);
    sigchain_graph_destroy(graph);

    return 0;
}
