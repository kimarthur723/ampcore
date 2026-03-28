# sigchain

Real-time signal processing library for building audio effect chains. Nodes can be added, removed, and rewired while audio is running.

## Features

- Real-time audio engine backed by [miniaudio](https://miniaud.io/)
- Graph rewiring implemented via lock-free queue
- C API (`sigchain_capi.h`) with opaque handles and error codes for FFI
- Runtime parameter introspection (name, range, unit) for generic GUI binding
- JSON preset save/load graph state
- CMake package with `find_package(sigchain)` support

### Nodes

| Category | Nodes |
|----------|-------|
| Input | File, Live audio |
| Effects | Fuzz, Delay, Gain, Biquad filter, Noise gate, Compressor, Reverb, Tone stack, Cabinet (IR loader) |
| Utility | Meter (RMS + peak) |

## Design

### C++ behind a C API

The library is C++23 internally, exposed through a C API. The C ABI is stable and allows interop with supported languages. Callers only see `SigchainNode` handles and `SigchainResult` error codes.

### Lock-free graph rewiring

The audio callback runs on a high-priority thread with a hard deadline, blocking it causes dropouts. A mutex between the GUI thread and the audio thread risks priority inversion: the audio thread could stall waiting for a lock held by the GUI thread, which the OS scheduler may not prioritize. Instead, graph mutations (`post_connect`, `post_disconnect`) are enqueued into a lock-free queue. The audio thread drains the queue at the top of each callback before processing. The GUI thread never blocks the audio thread.

The tradeoff: the queue has a fixed capacity of 64 commands. Flooding it faster than the audio thread drains it returns `SIGCHAIN_ERROR_QUEUE_FULL`. For normal GUI interaction this is never reached. Probably.

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
SigchainNode graph, engine;
sigchain_graph_create(2, &graph);
sigchain_engine_create(graph, 44100, &engine);
sigchain_engine_start(engine);

// ...

sigchain_engine_stop(engine);
sigchain_engine_destroy(engine);
sigchain_graph_destroy(graph);
```

### Creating and connecting nodes

```c
SigchainNode fuzz, delay;
sigchain_fuzz_create(graph, 2, 8.0f, 0.7f, &fuzz);
sigchain_delay_create(graph, 2, 44100, 0.3f, 0.4f, 0.5f, &delay);

sigchain_graph_connect(graph, fuzz, delay);
sigchain_graph_connect_to_output(graph, delay);
```

Rewire while the engine is running:

```c
sigchain_graph_post_connect(graph, fuzz, delay);
sigchain_graph_post_disconnect(graph, delay);
```

### Live audio input

```c
SigchainNode input;
sigchain_audio_input_create(graph, 2, &input);
sigchain_engine_set_input_node(engine, input);
```

### Parameter introspection

```c
int count = sigchain_node_get_parameter_count(fuzz);
for (int i = 0; i < count; i++) {
    SigchainParameterInfo info;
    sigchain_node_get_parameter_info(fuzz, i, &info);
    // info.name, info.min_value, info.max_value, info.unit
    sigchain_node_set_parameter_value(fuzz, i, 0.5f);
}
```

### Presets

```c
sigchain_preset_save(graph, "my_preset.json");
sigchain_preset_load(graph, "my_preset.json");
```
