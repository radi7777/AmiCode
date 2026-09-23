# AmiCode / AmiCodeIDE

An AI coding agent that runs natively on the Amiga. You describe what a program
should do; the agent writes the source, builds it with the compiler installed on
your Amiga, reads the error messages and fixes them until it builds.

- **AmiCodeIDE** – MUI IDE: project tree, editor with syntax highlighting,
  agent panel, build log with jump-to-error, sessions, settings.
- **amicode** – the same agent as a Shell command.

The user interface and the manual (`AmiCodeIDE.guide`) are in **German**.

## Features

- Agent loop with tools: read, list, find, search, write and edit files, run
  AmigaDOS commands, web search and fetch. Every change shows a diff; backups
  and undo per session.
- Permission modes *safe*, *project*, *full*; system directories and dangerous
  commands always ask.
- Providers: **OpenRouter**, **OpenAI**, **Ollama** (local LAN server),
  **Ollama Cloud**, or any OpenAI-compatible server. HTTPS via AmiSSL.
- Cost control: cost per task and in the status bar (OpenRouter), a cost limit
  per task, prompt caching, automatic context compaction.
- **Skills**: short knowledge files about AmigaDOS, C on AmigaOS 3.x and MUI,
  with tested example code. Loaded on demand (`read_skill`) or always
  included for small local models.
- Project wizard with 10 toolchain profiles (VBCC, SAS/C, gcc 2.95, AmiBlitz3,
  Amiga E, PureBasic, vasm, Lua, Python, ARexx).
- Robust against weaker models: loop guard for repeated failures, tool calls
  written as text are recognised, retries on overloaded servers.

## Requirements

- AmigaOS 3.2 or newer, 68030 or better, Fast RAM (16 MB+ recommended)
- MUI 3.8 or newer; MCCs: TextEditor, NList, NListview, NListtree (required);
  BetterString, Busy, TheBar (used if present)
- TCP/IP stack (bsdsocket.library) and AmiSSL 5
- a correctly set system clock (TLS)
- access to a language model (API key or an Ollama server)
- at least one compiler or interpreter on the Amiga

Runs on RTG and on native PAL/NTSC screens (narrow screens use a tabbed layout).

## Installation

Unpack `AmiCodeIDE-<version>.lha` into a drawer of your choice, e.g.
`Work:AmiCodeIDE`. The guide and the `Skills` drawer must stay next to the
program. Start AmiCodeIDE, open *Settings* (Amiga-E), choose a provider, enter
the API key, load the model list and save. Keys are stored in
`ENVARC:AmiCode/amicode.conf` only.

## Which model?

Tested with the same task (analog clock, then converted to a MUI application
with its own drawing class):

| Model | Result |
|---|---|
| `moonshotai/kimi-k2.7-code` via OpenRouter | best: loads the skills itself, both tasks right first time, about $0.02–0.04 per task |
| `gpt-5.6-sol` / `gpt-5.6-luna` via OpenAI | both succeed; OpenAI reports no per-request cost |
| `qwen3.8:27b` via local Ollama | free; needs skills set to *always*, slower |

## Building

Cross-compiled on macOS/Linux with an m68k-amigaos-gcc toolchain (developed
with gcc 16.2, libnix) that has the NDK 3.2, the MUI MCC headers and the
AmiSSL SDK installed. MUI 3.8 headers
are vendored in `vendor/mui`.

```sh
make                 # build/amicode, build/AmiCodeIDE, build/AmiCodeIDE.info
make dist            # build/AmiCodeIDE-<version>.lha
make TOOLCHAIN=/path/to/m68k-amigaos-gcc
```

`make push` copies the binaries to a running Amiga via the amimcp protocol
(see `push.py`; needs `AMIGA_HOST`, `AMIGA_TOKEN` and `AMIMCP_PATH`).

## Layout

```
src/        agent core (agent, tools, providers, net/TLS, JSON) and MUI GUI (gui*.c)
include/    headers
skills/     knowledge files installed as Skills/
docs/       AmiCodeIDE.guide (German manual)
tests/      test project and AmigaDOS test scripts
vendor/mui/ MUI 3.8 header subset
Projekt.md  design notes and development log (German)
```

## License

MIT License, see [LICENSE](LICENSE). The MUI 3.8 headers in `vendor/mui` are
third-party material under the terms of the MUI developer kit.
