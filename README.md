# NeuronScope 🔬

**Real-time local telemetry diagnostics for transformer models — pure C++ terminal UI.**

NeuronScope is a keyboard-driven TUI (Terminal User Interface) application that visualizes the inner workings of a transformer neural network in real time. Built entirely in C++ with FTXUI, it provides a rich, interactive dashboard for inspecting model topology, attention matrices, activation statistics, and numerical anomalies.

---

## Features

| Panel | Description |
|-------|-------------|
| **Model Topology** | Collapsible tree view of the transformer architecture (embeddings, layers, attention heads, MLP blocks) |
| **Live Packet Stream** | Real-time scrolling feed of telemetry records with layer type, compute device, and timing info |
| **Attention Matrix Visualizer** | Unicode heatmap of attention weights with pannable viewport, per-head navigation, and adjustable contrast |
| **Metrics Dashboard** | Live bar charts for activation magnitude, sparsity, and per-layer latency |
| **Anomaly Ledger** | Automatic detection and logging of NaN/Inf values, outlier activations, CPU fallbacks, and high sparsity |

## Architecture

```
src/
├── main.cpp                     # Entry point
├── core/
│   ├── ring_buffer.h            # Lock-free SPSC ring buffer
│   ├── model_topology.h         # Hierarchical tree data structure
│   └── anomaly_detector.h       # Anomaly detection engine
├── model/
│   ├── module.h                 # Base module class with hook system
│   ├── transformer.h            # Transformer model simulation
│   └── transformer.cpp          # Inference engine with telemetry emission
├── ipc/
│   ├── shared_memory_channel.cpp # (Stub) Shared memory transport
│   └── control_pipe.cpp          # (Stub) Control pipe transport
├── tui/
│   ├── app.h / app.cpp          # Main application + FTXUI event loop
│   ├── theme.h                  # Color palette & border decorators
│   ├── keybindings.h            # Keybinding constants
│   └── panels/
│       ├── topology_panel.*     # Model tree panel
│       ├── packet_stream_panel.* # Live telemetry feed
│       ├── attention_panel.*    # Attention heatmap
│       ├── metrics_panel.*      # Metrics bar charts
│       └── anomaly_panel.*      # Anomaly detection log
├── include/
│   └── neuronscope/
│       └── telemetry_record.h   # Shared telemetry data structures
└── tests/
    └── test_all.cpp             # Unit tests for core components
```

## Tech Stack

- **Language**: C++17
- **UI Framework**: [FTXUI](https://github.com/ArthurSonzogni/FTXUI) v5.0.0 (vendored)
- **Build**: MSVC 2022 (Visual Studio Build Tools)
- **Platform**: Windows (designed for Windows Terminal / ConHost)

## Building

### Prerequisites
- Visual Studio 2022 Build Tools (with C++ workload)

### Build Steps

```powershell
# Open a Developer Command Prompt for VS 2022, then:
powershell -File .\build_msvc.ps1
```

This compiles 75 FTXUI source files and 10 application source files, then links them into `neuronscope.exe`.

### Run Tests

```powershell
powershell -File .\build_tests_msvc.ps1
.\neuronscope_tests.exe
```

### Run the App

```powershell
.\neuronscope.exe
```

## Keyboard Controls

| Key | Action |
|-----|--------|
| `Tab` | Cycle focus between panels |
| `Q` | Quit |
| `↑/↓` or `j/k` | Navigate within focused panel |
| `←/→` or `h/l` | Pan attention heatmap |
| `Enter` / `Space` | Toggle tree node expansion |
| `+/-` | Adjust attention contrast |
| `H` | Cycle attention head |
| `F` | Toggle fullscreen for panel |

## How It Works

NeuronScope runs a **simulated transformer inference engine** in a background thread. The engine:

1. Builds a multi-layer transformer topology (embedding → N transformer blocks → output norm → LM head)
2. Runs continuous inference passes, generating realistic telemetry data
3. Pushes `TelemetryRecord` structs through a **lock-free SPSC ring buffer** to the UI thread
4. The TUI polls the buffer on a timer, updating all five panels in real time

Each `TelemetryRecord` contains:
- Layer name, type, and compute device
- Activation statistics (mean, max, min, sparsity)
- Attention weight matrices (64×64, per-head)
- Execution latency
- Anomaly flags (NaN, Inf, CPU fallback)

---

*Built for the Google Developer Student Clubs project.*
