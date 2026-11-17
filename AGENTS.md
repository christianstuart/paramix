# AGENTS.md — Mixxx Project Instructions

See [README.md](README.md) for a project overview, and
[CONTRIBUTING.md](CONTRIBUTING.md) for build instructions, code style,
pre-commit setup, Git workflow, and pull request guidelines.

## AI Agent Policy

> **Important:** This fork of the Mixxx project is aided by the use of AI agents for development, and not all AI-generated code will be disclosed.

## Key Architecture

- **ControlObject/ControlProxy**: `[Group], key_name` inter-component communication.
- **Engine thread**: Real-time audio — no allocations, no locks, may emit Qt signals but cannot receive them.
- **parented_ptr/make_parented**: Qt object-tree ownership. Object must get a parent before `parented_ptr` destructs.

## Project Layout

```text
src/          C++ source (engine/, controllers/, library/, mixer/, effects/, qml/, preferences/, util/, test/)
res/          Resources (controllers/ JS/XML, skins/, qml/)
cmake/        CMake modules
tools/        Python helper scripts
```

## Running the app

The Paramix QML interface in `res/qml/` is the only interface; `src/main.cpp`
always loads it. The classic QtWidgets skins were removed from `res/skins`, so
there is nothing to fall back to.

```bash
build/paramix.exe
```

`--new-ui` is still accepted but no longer changes which UI loads. It only
re-enables the Mixxx 3.0 data-corruption gate that blocks an existing profile;
pass `--allow-dangerous-data-corruption-risk` to get past it. The gate is about
Mixxx 3.0's unstable upgrade paths, not about Paramix's own data, which uses the
same schema as the classic UI.

QML edits need only a restart: the resource path resolves to the source `res/`
via `CMakeCache.txt` (`src/preferences/configobject.cpp`), so the app reads
`res/qml/` off disk. C++ edits still need a rebuild — the `.exe` does not
contain the QML.
