# ampcore

Real-time signal processing library for building audio effect chains. Nodes can be added, removed, and rewired while audio is running.

## Features

- Real-time audio engine backed by [miniaudio](https://miniaud.io/)
- Graph rewiring implemented via lock-free queue
- C API (`ampcore_capi.h`) with opaque handles and error codes for FFI
- Runtime parameter introspection (name, range, unit) for generic GUI binding
- JSON preset save/load graph state
- CMake package with `find_package(ampcore)` support

### Nodes

| Category | Nodes |
|----------|-------|
| Input | File, Live audio |
| Effects | Fuzz, Delay, Gain, Biquad filter, Noise gate, Compressor, Reverb, Tone stack, Cabinet (IR loader) |
| Utility | Meter (RMS + peak) |

## Design

### C++ behind a C API

The library is C++23 internally, exposed through a C API. The C ABI is stable and allows interop with supported languages. Callers only see `AmpcoreNode` handles and `AmpcoreResult` error codes.

### Lock-free graph rewiring

The audio callback runs on a high-priority thread with a hard deadline, blocking it causes dropouts. A mutex between the GUI thread and the audio thread risks priority inversion: the audio thread could stall waiting for a lock held by the GUI thread, which the OS scheduler may not prioritize. Instead, graph mutations (`post_connect`, `post_disconnect`) are enqueued into a lock-free queue. The audio thread drains the queue at the top of each callback before processing. The GUI thread never blocks the audio thread.

The tradeoff: the queue has a fixed capacity of 64 commands. Flooding it faster than the audio thread drains it returns `AMPCORE_ERROR_QUEUE_FULL`. For normal GUI interaction this is never reached. Probably.

### Atomic parameters

Node parameters (gain, threshold, mix, etc.) are `std::atomic<float>`. The GUI thread writes, the audio thread reads, no coordination required. Changes take effect within the next callback. This is simpler and less overhead than routing parameter changes through the command queue, and the worst-case latency is one buffer period regardless.

### Audio latency

Latency is determined by the buffer size negotiated with the OS, not by the library. Graph rewiring and parameter changes add no additional latency beyond one buffer period. To measure actual round-trip latency on your system, use a loopback latency test.

## Build

Requires CMake 3.28+, Ninja 1.11+, and a C++23 compiler (GCC 14+ or Clang 16+).

```sh
cmake -B build -G Ninja
cmake --build build
```

Install:

```sh
cmake --install build --prefix /usr/local
```

## Usage

### Engine setup

```c
AmpcoreNode graph, engine;
ampcore_graph_create(2, &graph);
ampcore_engine_create(graph, 44100, &engine);
ampcore_engine_start(engine);

// ...

ampcore_engine_stop(engine);
ampcore_engine_destroy(engine);
ampcore_graph_destroy(graph);
```

### Creating and connecting nodes

```c
AmpcoreNode fuzz, delay;
ampcore_fuzz_create(graph, 2, 8.0f, 0.7f, &fuzz);
ampcore_delay_create(graph, 2, 44100, 0.3f, 0.4f, 0.5f, &delay);

ampcore_graph_connect(graph, fuzz, delay);
ampcore_graph_connect_to_output(graph, delay);
```

Rewire while the engine is running:

```c
ampcore_graph_post_connect(graph, fuzz, delay);
ampcore_graph_post_disconnect(graph, delay);
```

### Live audio input

```c
AmpcoreNode input;
ampcore_audio_input_create(graph, 2, &input);
ampcore_engine_set_input_node(engine, input);
```

### Parameter introspection

```c
int count = ampcore_node_get_parameter_count(fuzz);
for (int i = 0; i < count; i++) {
    AmpcoreParameterInfo info;
    ampcore_node_get_parameter_info(fuzz, i, &info);
    // info.name, info.min_value, info.max_value, info.unit
    ampcore_node_set_parameter_value(fuzz, i, 0.5f);
}
```

### Presets

```c
ampcore_preset_save(graph, "my_preset.json");
ampcore_preset_load(graph, "my_preset.json");
```
