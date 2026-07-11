#!/usr/bin/env python3
"""Tkinter GUI for ampcore.

Build an effect chain from the prebuilt effects, rewire it live, and tweak
parameters exposed through the C API's introspection functions.

Usage:
    python3 gui/ampcore_gui.py

The shared library is located via the AMPCORE_LIB environment variable, or
falls back to build/lib/libampcore_capi.so relative to the repository root.
"""

import ctypes
import os
import sys
import tkinter as tk
from tkinter import messagebox, ttk

CHANNELS = 2
SAMPLE_RATE = 44100

AMPCORE_OK = 0


class ParameterInfo(ctypes.Structure):
    _fields_ = [
        ("name", ctypes.c_char_p),
        ("min_value", ctypes.c_float),
        ("max_value", ctypes.c_float),
        ("default_value", ctypes.c_float),
        ("unit", ctypes.c_char_p),
    ]


def find_library():
    env = os.environ.get("AMPCORE_LIB")
    if env:
        return env
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    for name in ("libampcore_capi.so", "libampcore_capi.dylib"):
        path = os.path.join(repo_root, "build", "lib", name)
        if os.path.exists(path):
            return path
    sys.exit(
        "error: libampcore_capi not found. Build it first:\n"
        "  cmake -B build -G Ninja && cmake --build build\n"
        "or point AMPCORE_LIB at the shared library."
    )


def load_library():
    lib = ctypes.CDLL(find_library())

    node = ctypes.c_void_p
    res = ctypes.c_int

    protos = {
        "ampcore_graph_create": (res, [ctypes.c_uint32, ctypes.POINTER(node)]),
        "ampcore_graph_destroy": (None, [node]),
        "ampcore_graph_connect": (res, [node, node, node]),
        "ampcore_graph_connect_to_output": (res, [node, node]),
        "ampcore_graph_post_connect": (res, [node, node, node]),
        "ampcore_graph_post_connect_to_output": (res, [node, node]),
        "ampcore_graph_post_disconnect": (res, [node, node]),
        "ampcore_engine_create": (res, [node, ctypes.c_uint32, ctypes.POINTER(node)]),
        "ampcore_engine_destroy": (None, [node]),
        "ampcore_engine_start": (res, [node]),
        "ampcore_engine_stop": (res, [node]),
        "ampcore_engine_is_started": (ctypes.c_int, [node]),
        "ampcore_engine_get_sample_rate": (ctypes.c_uint32, [node]),
        "ampcore_engine_set_input_node": (res, [node, node]),
        "ampcore_audio_input_create": (res, [node, ctypes.c_uint32, ctypes.POINTER(node)]),
        "ampcore_audio_input_destroy": (None, [node]),
        "ampcore_fuzz_create": (res, [node, ctypes.c_uint32, ctypes.c_float, ctypes.c_float, ctypes.POINTER(node)]),
        "ampcore_fuzz_destroy": (None, [node]),
        "ampcore_delay_create": (res, [node, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.POINTER(node)]),
        "ampcore_delay_destroy": (None, [node]),
        "ampcore_gain_create": (res, [node, ctypes.c_uint32, ctypes.c_float, ctypes.POINTER(node)]),
        "ampcore_gain_destroy": (None, [node]),
        "ampcore_biquad_create": (res, [node, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_int, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.POINTER(node)]),
        "ampcore_biquad_destroy": (None, [node]),
        "ampcore_noise_gate_create": (res, [node, ctypes.c_uint32, ctypes.c_uint32, ctypes.POINTER(node)]),
        "ampcore_noise_gate_destroy": (None, [node]),
        "ampcore_compressor_create": (res, [node, ctypes.c_uint32, ctypes.c_uint32, ctypes.POINTER(node)]),
        "ampcore_compressor_destroy": (None, [node]),
        "ampcore_reverb_create": (res, [node, ctypes.c_uint32, ctypes.POINTER(node)]),
        "ampcore_reverb_destroy": (None, [node]),
        "ampcore_tone_stack_create": (res, [node, ctypes.c_uint32, ctypes.c_uint32, ctypes.POINTER(node)]),
        "ampcore_tone_stack_destroy": (None, [node]),
        "ampcore_cabinet_create": (res, [node, ctypes.c_uint32, ctypes.POINTER(node)]),
        "ampcore_cabinet_destroy": (None, [node]),
        "ampcore_node_get_parameter_count": (ctypes.c_int, [node]),
        "ampcore_node_get_parameter_info": (res, [node, ctypes.c_int, ctypes.POINTER(ParameterInfo)]),
        "ampcore_node_get_parameter_value": (ctypes.c_float, [node, ctypes.c_int]),
        "ampcore_node_set_parameter_value": (res, [node, ctypes.c_int, ctypes.c_float]),
    }
    for fname, (restype, argtypes) in protos.items():
        fn = getattr(lib, fname)
        fn.restype = restype
        fn.argtypes = argtypes
    return lib


# name -> (create(lib, graph, out), destroy_fn_name)
EFFECTS = {
    "Fuzz": (
        lambda lib, g, out: lib.ampcore_fuzz_create(g, CHANNELS, 8.0, 0.7, out),
        "ampcore_fuzz_destroy",
    ),
    "Delay": (
        lambda lib, g, out: lib.ampcore_delay_create(g, CHANNELS, SAMPLE_RATE, 0.3, 0.4, 0.5, out),
        "ampcore_delay_destroy",
    ),
    "Gain": (
        lambda lib, g, out: lib.ampcore_gain_create(g, CHANNELS, 1.0, out),
        "ampcore_gain_destroy",
    ),
    "Biquad Filter": (
        lambda lib, g, out: lib.ampcore_biquad_create(g, CHANNELS, SAMPLE_RATE, 0, 1000.0, 0.707, 0.0, out),
        "ampcore_biquad_destroy",
    ),
    "Noise Gate": (
        lambda lib, g, out: lib.ampcore_noise_gate_create(g, CHANNELS, SAMPLE_RATE, out),
        "ampcore_noise_gate_destroy",
    ),
    "Compressor": (
        lambda lib, g, out: lib.ampcore_compressor_create(g, CHANNELS, SAMPLE_RATE, out),
        "ampcore_compressor_destroy",
    ),
    "Reverb": (
        lambda lib, g, out: lib.ampcore_reverb_create(g, CHANNELS, out),
        "ampcore_reverb_destroy",
    ),
    "Tone Stack": (
        lambda lib, g, out: lib.ampcore_tone_stack_create(g, CHANNELS, SAMPLE_RATE, out),
        "ampcore_tone_stack_destroy",
    ),
    "Cabinet": (
        lambda lib, g, out: lib.ampcore_cabinet_create(g, CHANNELS, out),
        "ampcore_cabinet_destroy",
    ),
}


class ChainEffect:
    """An instantiated effect node in the chain."""

    def __init__(self, name, handle, destroy_fn_name):
        self.name = name
        self.handle = handle
        self.destroy_fn_name = destroy_fn_name


class App:
    def __init__(self, root):
        self.root = root
        self.lib = load_library()
        self.effects = []  # ChainEffect list, in signal order

        graph = ctypes.c_void_p()
        self._check(self.lib.ampcore_graph_create(CHANNELS, ctypes.byref(graph)), "graph create")
        self.graph = graph

        engine = ctypes.c_void_p()
        self._check(self.lib.ampcore_engine_create(self.graph, SAMPLE_RATE, ctypes.byref(engine)), "engine create")
        self.engine = engine

        inp = ctypes.c_void_p()
        self._check(self.lib.ampcore_audio_input_create(self.graph, CHANNELS, ctypes.byref(inp)), "input create")
        self.input_node = inp
        self._check(self.lib.ampcore_engine_set_input_node(self.engine, self.input_node), "set input")

        self._build_ui()
        self._rewire()
        root.protocol("WM_DELETE_WINDOW", self._on_close)

    # ── engine / graph ───────────────────────────────────────────────────

    def _check(self, result, what):
        if result != AMPCORE_OK:
            raise RuntimeError(f"{what} failed (code {result})")

    def _engine_running(self):
        return bool(self.lib.ampcore_engine_is_started(self.engine))

    def _connect(self, a, b):
        if self._engine_running():
            return self.lib.ampcore_graph_post_connect(self.graph, a, b)
        return self.lib.ampcore_graph_connect(self.graph, a, b)

    def _connect_to_output(self, a):
        if self._engine_running():
            return self.lib.ampcore_graph_post_connect_to_output(self.graph, a)
        return self.lib.ampcore_graph_connect_to_output(self.graph, a)

    def _rewire(self):
        """Reattach the whole serial chain: input -> effects... -> output.

        Attaching a node's output bus replaces its previous attachment, so a
        full pass is enough; stale links are overwritten.
        """
        nodes = [self.input_node] + [e.handle for e in self.effects]
        for a, b in zip(nodes, nodes[1:]):
            self._check(self._connect(a, b), "connect")
        self._check(self._connect_to_output(nodes[-1]), "connect to output")
        self._refresh_chain_list()

    def _destroy_effect(self, effect):
        getattr(self.lib, effect.destroy_fn_name)(effect.handle)

    # ── ui construction ──────────────────────────────────────────────────

    def _build_ui(self):
        self.root.title("ampcore")
        self.root.minsize(720, 420)

        main = ttk.Frame(self.root, padding=8)
        main.pack(fill=tk.BOTH, expand=True)

        # available effects
        palette = ttk.LabelFrame(main, text="Effects", padding=6)
        palette.pack(side=tk.LEFT, fill=tk.Y)
        self.palette_list = tk.Listbox(palette, exportselection=False, width=16)
        for name in EFFECTS:
            self.palette_list.insert(tk.END, name)
        self.palette_list.pack(fill=tk.BOTH, expand=True)
        self.palette_list.bind("<Double-Button-1>", lambda e: self._add_effect())
        ttk.Button(palette, text="Add →", command=self._add_effect).pack(fill=tk.X, pady=(6, 0))

        # chain
        chain = ttk.LabelFrame(main, text="Chain", padding=6)
        chain.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=8)
        self.chain_list = tk.Listbox(chain, exportselection=False)
        self.chain_list.pack(fill=tk.BOTH, expand=True)
        self.chain_list.bind("<<ListboxSelect>>", lambda e: self._refresh_params())
        btns = ttk.Frame(chain)
        btns.pack(fill=tk.X, pady=(6, 0))
        ttk.Button(btns, text="↑ Up", command=lambda: self._move_effect(-1)).pack(side=tk.LEFT)
        ttk.Button(btns, text="↓ Down", command=lambda: self._move_effect(1)).pack(side=tk.LEFT, padx=4)
        ttk.Button(btns, text="Remove", command=self._remove_effect).pack(side=tk.LEFT)

        # parameters
        params = ttk.LabelFrame(main, text="Parameters", padding=6)
        params.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        self.params_frame = params
        self.param_widgets = []

        # transport
        bottom = ttk.Frame(self.root, padding=(8, 0, 8, 8))
        bottom.pack(fill=tk.X)
        self.start_btn = ttk.Button(bottom, text="Start Engine", command=self._toggle_engine)
        self.start_btn.pack(side=tk.LEFT)
        self.status = ttk.Label(bottom, text="engine stopped")
        self.status.pack(side=tk.LEFT, padx=10)

    # ── chain operations ─────────────────────────────────────────────────

    def _selected_effect_index(self):
        sel = self.chain_list.curselection()
        if not sel:
            return None
        idx = sel[0] - 1  # entry 0 is Input
        if 0 <= idx < len(self.effects):
            return idx
        return None

    def _add_effect(self):
        sel = self.palette_list.curselection()
        if not sel:
            return
        name = self.palette_list.get(sel[0])
        create, destroy_fn_name = EFFECTS[name]
        out = ctypes.c_void_p()
        try:
            self._check(create(self.lib, self.graph, ctypes.byref(out)), f"create {name}")
        except RuntimeError as exc:
            messagebox.showerror("ampcore", str(exc))
            return
        self.effects.append(ChainEffect(name, out, destroy_fn_name))
        self._rewire()
        self.chain_list.selection_clear(0, tk.END)
        self.chain_list.selection_set(len(self.effects))
        self._refresh_params()

    def _remove_effect(self):
        idx = self._selected_effect_index()
        if idx is None:
            return
        effect = self.effects.pop(idx)
        if self._engine_running():
            # detach it, rewire around it, and only free the node after the
            # audio thread has had time to drain the command queue
            self.lib.ampcore_graph_post_disconnect(self.graph, effect.handle)
            self._rewire()
            self.root.after(250, lambda: self._destroy_effect(effect))
        else:
            self._rewire()
            self._destroy_effect(effect)
        self._refresh_params()

    def _move_effect(self, delta):
        idx = self._selected_effect_index()
        if idx is None:
            return
        new = idx + delta
        if not (0 <= new < len(self.effects)):
            return
        self.effects[idx], self.effects[new] = self.effects[new], self.effects[idx]
        self._rewire()
        self.chain_list.selection_clear(0, tk.END)
        self.chain_list.selection_set(new + 1)
        self._refresh_params()

    def _refresh_chain_list(self):
        sel = self.chain_list.curselection()
        self.chain_list.delete(0, tk.END)
        self.chain_list.insert(tk.END, "▶ Input")
        for e in self.effects:
            self.chain_list.insert(tk.END, f"    ↓ {e.name}")
        self.chain_list.insert(tk.END, "■ Output")
        if sel:
            self.chain_list.selection_set(sel[0])

    # ── parameter panel ──────────────────────────────────────────────────

    def _refresh_params(self):
        for w in self.param_widgets:
            w.destroy()
        self.param_widgets = []

        idx = self._selected_effect_index()
        if idx is None:
            label = ttk.Label(self.params_frame, text="Select an effect in the chain")
            label.pack(anchor=tk.W)
            self.param_widgets.append(label)
            return

        effect = self.effects[idx]
        count = self.lib.ampcore_node_get_parameter_count(effect.handle)
        for i in range(count):
            info = ParameterInfo()
            if self.lib.ampcore_node_get_parameter_info(effect.handle, i, ctypes.byref(info)) != AMPCORE_OK:
                continue
            name = (info.name or b"?").decode()
            unit = (info.unit or b"").decode()
            value = self.lib.ampcore_node_get_parameter_value(effect.handle, i)

            row = ttk.Frame(self.params_frame)
            row.pack(fill=tk.X, pady=2)
            self.param_widgets.append(row)

            title = f"{name} ({unit})" if unit else name
            ttk.Label(row, text=title).pack(anchor=tk.W)
            value_label = ttk.Label(row, text=f"{value:.3g}", width=8)
            value_label.pack(side=tk.RIGHT)
            scale = ttk.Scale(
                row,
                from_=info.min_value,
                to=info.max_value,
                value=value,
                command=lambda v, h=effect.handle, i=i, lbl=value_label: self._set_param(h, i, float(v), lbl),
            )
            scale.pack(fill=tk.X, expand=True, side=tk.LEFT)

    def _set_param(self, handle, index, value, label):
        self.lib.ampcore_node_set_parameter_value(handle, index, value)
        label.config(text=f"{value:.3g}")

    # ── transport ────────────────────────────────────────────────────────

    def _toggle_engine(self):
        if self._engine_running():
            self.lib.ampcore_engine_stop(self.engine)
            self.start_btn.config(text="Start Engine")
            self.status.config(text="engine stopped")
        else:
            result = self.lib.ampcore_engine_start(self.engine)
            if result != AMPCORE_OK:
                messagebox.showerror("ampcore", f"engine start failed (code {result})")
                return
            sr = self.lib.ampcore_engine_get_sample_rate(self.engine)
            self.start_btn.config(text="Stop Engine")
            self.status.config(text=f"engine running @ {sr} Hz")

    def _on_close(self):
        if self._engine_running():
            self.lib.ampcore_engine_stop(self.engine)
        self.lib.ampcore_engine_destroy(self.engine)
        for e in self.effects:
            self._destroy_effect(e)
        self.lib.ampcore_audio_input_destroy(self.input_node)
        self.lib.ampcore_graph_destroy(self.graph)
        self.root.destroy()


def main():
    root = tk.Tk()
    App(root)
    root.mainloop()


if __name__ == "__main__":
    main()
