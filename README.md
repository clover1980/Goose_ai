<i>Testing results:
1. i really like that model running there is x2 faster than in other apps, esp DSH (via llama.cpp local server). It would be great to reproduce Deepseek harness here. 
2. self-improvement really works, but i've connected a very powerful and large model to it (not tested with small models yet)-the Deepseek V4.1 Flash 1017 in Q2_K quality (230Gb Ram). Also in model it require a big context size because it fully copied the current source code and after analyzing it adding changes to it, everything need to fit, i'm using a huge context like 230K by default, not tested the exact sizes [View Result ver 1.8-1.9](self-improve.txt)
3. i'm naming self-improvement function "Ai tree architecture", which is really random and unpredictable, you growing your own Ai from scratch, it's also hard to revert back all successed improvements.
4. We need to be concentrated on 2 main goals - keeping code size small as possible and in self-improvement would be great to offer user 3 random improving ways (branches of "Ai tree").
5. more maintenance tests planned.</i>


# Goose_AI 🦢⚡

**A zero-heap, self-evolving local terminal AI assistant (`goose_ai`) written in the [Goose](https://aardappel.github.io/goose/) systems language and C.**

Built for Linux servers running local models completely offline—with zero Python, zero PyTorch, and zero runtime bloat.

---

## Architecture Overview

```text
┌──────────────────────────────────────────────────────────────────────┐
│  goose_ai (Self-Evolving Local CLI Assistant)                        │
│  ├── goose_ai.goose   ─► Control flow, command routing, zero-heap UI │
│  └── ai_bridge.c      ─► libcurl SSE streaming, 16-turn sliding      │
│                          window, binary session persistence,         │
│                          hardware telemetry & execv() hot-swap       │
│                               │                                      │
│                               ▼ (OpenAI-compatible SSE HTTP)         │
│                    Local llama-server / Ollama                       │
└──────────────────────────────────────────────────────────────────────┘
```
## 🌱 The AI Tree Architecture

Unlike static AI wrappers, **`goose_ai`** is built as a **living, self-improving tree architecture**:
- **Immutable Roots (`ai_bridge.c` + Static BSS):** Bounded zero-heap memory (`MAX_HISTORY = 16`) that anchors the runtime so self-modification never corrupts core state.
- **Recursive Trunk (`/evolve` + `execv()`):** Reads its own source, generates new features via your local LLM, verifies compilation with `goose + gcc -O3` (rolling back automatically on error), and hot-swaps into the upgraded binary live.
- **Unbounded Canopy:** Every `/evolve` command grows new branches—you don't know ahead of time what tools or capabilities your binary will grow into next.

* **goose_ai.goose:** Manages control flow, command routing, diagnostics dispatch, and the zero-heap terminal UI.
* **ai_bridge.c:** Provides a native Goose u8[:] C FFI bridge, libcurl SSE streaming, a 16-turn sliding context window, binary session persistence, tool helpers, hardware telemetry, and live execv() hot-swapping.
* **Local Backend:** Connects via OpenAI-compatible SSE HTTP to llama-server (default http://127.0.0) or Ollama (11434).
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

## Installation

Installation & Build Guide (Ubuntu / Linux)
### 1. Install System Dependencies
##### Install the C compiler, C library headers, libcurl development headers, and git:
    sudo apt update
    sudo apt install -y build-essential libc6-dev gcc libgomp1 libcurl4-openssl-dev git
### 2. Install the Goose Compiler
##### Download the latest Linux x64 prebuilt release of the Goose programming language (aardappel/goose), extract it into ~/goose, and add it to your PATH:
    mkdir -p ~/goose
    cd ~/Downloads
    wget github.com
    tar -xzf goose-linux-x64.tar.gz -C ~/goose --strip-components=1
    echo 'export PATH="HOME/goose:PATH"' >> ~/.bashrc
    source ~/.bashrc
(Note: If the release archive filename on the GitHub Releases page includes a version tag, adjust the wget and tar filename accordingly).
##### Verify that goose is working:
    goose --version
### 3. Clone goose_ai and Stage Working Files
##### Clone the repository and copy goose_ai.goose and ai_bridge.c into your home directory (~), where /evolve expects them during live self-modification:
    git clone github.com ~/goose_ai
    cp ~/goose_ai/goose_ai.goose ~/goose_ai/ai_bridge.c ~
### 4. Transpile Goose to C and Compile
##### Use goose --standalone to generate a self-contained C file (goose_ai.c), compile both C translation units with gcc -O3, and link them into ./goose_ai_bin:
    cd ~
    goose --standalone -o goose_ai.c goose_ai.goose
    gcc -O3 -c ai_bridge.c -o ai_bridge.o
    gcc -O3 -c goose_ai.c -o goose_ai.o
    gcc -O3 goose_ai.o ai_bridge.o -lcurl -lm -lpthread -o goose_ai_bin
(Why goose_ai_bin? Naming the executable goose_ai_bin avoids colliding with the ~/goose_ai repository directory).
### 5. Start Your Local LLM Server & Launch goose_ai_bin
##### Start llama-server on port 9931 (or update the endpoint URL in ~/goose_ai.goose to match your port/host, such as 11434 for Ollama, and re-run Step 4):
    llama-server -m /path/to/your-model.gguf --host 127.0.0.1 --port 9931 --alias local-model -c 8192 -ngl 99
##### In another terminal, launch goose_ai_bin:
    cd ~
    ./goose_ai_bin

### Quick Test in ./goose_ai_bin

##### Start ./goose_ai_bin and run an evolution to hook up the new helpers:
    bash
    ./goose_ai_bin

##### Then inside the prompt, enter:
    /evolve add /read <filepath> to inspect files and /sh <command> to run and analyze shell commands

or

    /evolve add /read <filepath> using prepare_file_prompt, /sh <cmd> using prepare_exec_prompt, and /save_reply using save_last_reply
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
    No Authorship : Machine made code not protected by law worldwide
