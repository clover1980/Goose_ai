# Goose_AI 🦢⚡

**A zero-heap, self-evolving local terminal AI assistant (`goose_ai`) written in the [Goose](https://aardappel.github.io/goose/) systems language and C.**

Built for high-memory Linux workstations and multi-core Xeon / CUDA hybrid servers running local models completely offline—with zero Python, zero PyTorch, and zero runtime bloat.

---

## Architecture Overview

```text
┌──────────────────────────────────────────────────────────────────────┐
│  goose_ai (Self-Evolving Local CLI Assistant)                        │
│  ├── goose_ai.goose   ─► Control flow, command routing, zero-heap UI │
│  └── ai_bridge.h      ─► libcurl SSE streaming, 16-turn sliding      │
│                          window, binary session persistence,         │
│                          hardware telemetry & execv() hot-swap       │
│                               │                                      │
│                               ▼ (OpenAI-compatible SSE HTTP)         │
│                    Local llama-server / Ollama                       │
└──────────────────────────────────────────────────────────────────────┘
```

---

## Key Features

### `goose_ai` — Self-Evolving Terminal Assistant
* **Zero-Heap Runtime:** All conversation history, JSON payloads, and code-capture buffers live in bounded static BSS memory (`MAX_HISTORY = 16`) with zero dynamic `malloc`/`free` calls.
* **Live Self-Modification (`/evolve <goal>`):** Reads its own `goose_ai.goose` source code, asks your local LLM to implement a new feature, compiles the candidate via `goose` + `gcc -O3`, automatically rolls back if compilation fails, and hot-swaps into the upgraded binary via `execv()` while preserving active chat history.
* **Persistent Session Management:**
  * `/save [name]` & `/load [name]` — Instant binary snapshots (`~/.goose_session_<name>.bin`)
  * `/sessions` — List saved slots with message counts and timestamps
  * `/delete <name>` — Remove old session files
  * `/export [name]` — Export active or saved conversations into clean Markdown (`.md`)
  * `/clear` — Reset active conversation memory while keeping your system prompt
* **Persistent System Persona (`/system`):** View or update your assistant's persona mid-session, automatically saved to `~/.goose_system_prompt.txt`.
* **Built-In Hardware Telemetry:**
  * `/health` — Streams `nvidia-smi` GPU metrics, RAM telemetry, and kernel `EDAC`/`MCE` ECC error logs to the LLM for diagnostics
  * `/gpu` — Checks GPU VRAM usage, temperature, utilization, and power draw
  * `/top` — Inspects top physical RAM-consuming processes

---

## Quick Start

### Prerequisites
* [Goose compiler](https://aardappel.github.io/goose/) (`goose`)
* `gcc` with OpenMP support
* `libcurl` development headers (`sudo apt install libcurl4-openssl-dev`)

### Build & Run `goose_ai`
```bash
cd ~
goose -o goose_ai.c goose_ai.goose
gcc -O3 -include ai_bridge.h goose_ai.c -lcurl -o goose_ai
./goose_ai
```
*(Connects to `http://127.0.0` by default for `llama-server`, or change the port to `11434` for Ollama.)*

---

## Command Reference (`goose_ai`)

| Command | Description |
| :--- | :--- |
| `/help` | Display the full command cheat sheet |
| `/system [prompt]` | View or update & save the persistent system persona |
| `/save [name]` | Save the current conversation to a named slot |
| `/load [name]` | Restore a previously saved conversation slot |
| `/sessions` | List all saved session slots, message counts, and dates |
| `/delete <name>` | Delete a saved session file |
| `/export [name]` | Export active or saved chat history to a Markdown (`.md`) file |
| `/clear` | Clear active conversation history (keeps system prompt) |
| `/health` | Run full GPU + RAM + EDAC/MCE ECC hardware diagnostics |
| `/gpu` | Inspect GPU temperature, VRAM, utilization, and wattage |
| `/top` | Inspect top physical RAM-consuming processes |
| `/evolve <goal>` | Self-modify `goose_ai.goose`, verify compilation, and hot-swap live |
| `/quit` or `exit` | Exit GooseAI |

---

## License
    Idea: mine.
    Execution: GOOGLE Ai Machine generated code.
    No Authorship : No Copyright by law
