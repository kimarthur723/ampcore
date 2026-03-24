# amp

Modular guitar amp simulator (more accurately, a real-time audio DSP) built on a real-time audio processing graph. Nodes can be added, removed, and rewired while audio is running.

## Features

- Real-time audio engine backed by [miniaudio](https://miniaud.io/)
- Lock-free graph rewiring via SPSC command queue
- C API (`amp_capi.h`) with opaque handles and error codes for FFI
- Per-node parameter introspection (name, range, unit)
- JSON preset save/load
- CMake package with `find_package(amp)` support

### Nodes

| Category | Nodes |
|----------|-------|
| Input | File, Live audio |
| Effects | Fuzz, Delay, Gain, Biquad filter, Noise gate, Compressor, Reverb, Tone stack, Cabinet (IR loader) |
| Utility | Meter (RMS + peak) |

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
AmpNode graph, engine;
amp_graph_create(2, &graph);
amp_engine_create(graph, 44100, &engine);
amp_engine_start(engine);

// ...

amp_engine_stop(engine);
amp_engine_destroy(engine);
amp_graph_destroy(graph);
```

### Creating and connecting nodes

```c
AmpNode fuzz, delay;
amp_fuzz_create(graph, 2, 8.0f, 0.7f, &fuzz);
amp_delay_create(graph, 2, 44100, 0.3f, 0.4f, 0.5f, &delay);

amp_graph_connect(graph, fuzz, delay);
amp_graph_connect_to_output(graph, delay);
```

Rewire while the engine is running:

```c
amp_graph_post_connect(graph, fuzz, delay);
amp_graph_post_disconnect(graph, delay);
```

### Live audio input

```c
AmpNode input;
amp_audio_input_create(graph, 2, &input);
amp_engine_set_input_node(engine, input);
```

### Parameter introspection

```c
int count = amp_node_get_parameter_count(fuzz);
for (int i = 0; i < count; i++) {
    AmpParameterInfo info;
    amp_node_get_parameter_info(fuzz, i, &info);
    // info.name, info.min_value, info.max_value, info.unit
    amp_node_set_parameter_value(fuzz, i, 0.5f);
}
```

### Presets

```c
amp_preset_save(graph, "my_preset.json");
amp_preset_load(graph, "my_preset.json");
```
