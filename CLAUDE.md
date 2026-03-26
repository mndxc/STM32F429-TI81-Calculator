# CLAUDE.md

**Purpose:** AI session continuity and feature backlog. Contains project context, architectural decisions, gotchas, known issues, the active feature/bug backlog (`Next session priorities`), and standing rules for AI-assisted development. Read in full at the start of every session.

- [x] **Verification and Bug Fixes**
    - [x] Build and verify logic (Walkthrough created).
    - [x] Fixed startup white screen (restored `graph_lbl_y` init).
    - [x] Fixed coordinate overlap (switched to absolute positioning).
    - [x] Fixed freeze during Trace/Graph switching (added scale guards, loop clamping, and proper mutex locking).

**For code quality items** (CI gates, refactoring, test coverage, contributor docs) see [docs/QUALITY_TRACKER.md](docs/QUALITY_TRACKER.md) — that document is the single source of truth for all P-numbered improvement items. Items are never duplicated between the two files.

---

## Project Quality

**[QUALITY_TRACKER.md](docs/QUALITY_TRACKER.md)** — read this before starting any significant work.

**Purpose of QUALITY_TRACKER:** Permanent register for code quality reviews. Tracks a rated scorecard across 10 dimensions, P-numbered improvement items with effort/impact rankings, and full resolution history. It is the single source of truth for all quality, CI, refactoring, and contributor-docs work. Items are not duplicated in this file.

Current overall rating: **93–95% production-ready** (up from 92–94%; gain from P20 resolution: testing B+ → A-). Key remaining gaps: PRGM hardware validation pending (P10). Key strengths: documentation (A+), RTOS integration (A), FLASH/memory-safety (A), CI quality gates (-Werror), 422-test host suite with CI.

---

## Complexity Management Requirement (standing rule — applies to every commit)

**The codebase must not grow in complexity faster than it is simplified.** This is a hard constraint, not a guideline.

### After every commit

Before closing a session or presenting a commit as complete, perform a **complexity impact review**:

1. **State what changed** — which files were touched, roughly how many lines were added/removed.
2. **Rate the complexity delta** — one of:
   - `neutral` — no net change in cognitive load (e.g. pure refactor, test addition, config change)
   - `increase` — new logic, new state, new abstractions, or a file grew significantly
   - `decrease` — logic removed, files split, dead code deleted, abstractions simplified
3. **If `increase`:** immediately add one or more follow-up items to the "Next session priorities" list in this file describing exactly how to pay down the complexity debt introduced. Each item must be concrete and actionable (name the file, the function, the technique). Do not leave an `increase` commit without a follow-up plan.

### What counts as a complexity increase

- A file grows by more than ~100 lines without a corresponding extraction or removal elsewhere
- A new module is added without a clear, bounded responsibility
- New global or shared state is introduced
- A function exceeds ~80–100 lines
- A new conditional branch is added to an already-large switch or if-chain
- A workaround or special-case is added rather than fixing the underlying model

### What counts as paying down complexity

- Extracting a cohesive group of functions into a new module (following the `ui_matrix.c` / `expr_util.c` pattern)
- Replacing magic numbers or colours with named constants
- Reducing a large switch to a dispatch table or handler chain
- Deleting dead code or unused state
- Splitting a multi-responsibility function into focused helpers

### Format for the follow-up item

Add to "Next session priorities" with a tag `[complexity]`:

```
**[complexity] <short title>** — <one sentence explaining what grew and why>. <one sentence describing the planned simplification>. Files: <file list>.
```

---

## To-Do Routing

When the user asks to add something to the to-do list, place it in the correct location based on item type — never duplicate it in both files.

| Item type | Where it goes |
|---|---|
| **Feature work** — new calculator behaviour, TI-81 accuracy, UI improvements | `Next session priorities` in this file (with files + implementation detail) |
| **Bug fix** — incorrect behaviour, crashes, display glitches | `Next session priorities` in this file |
| **Complexity debt** — complexity introduced by a commit that needs paying down | `Next session priorities` in this file (tag `[complexity]`) |
| **Code quality** — compiler warnings, CI gates, refactoring, static analysis | [docs/QUALITY_TRACKER.md](docs/QUALITY_TRACKER.md) as a new P-item |
| **Testing** — new test coverage, test infrastructure, coverage targets | [docs/QUALITY_TRACKER.md](docs/QUALITY_TRACKER.md) as a new P-item |
| **Contributor/open-source docs** — architecture diagrams, guides, onboarding | [docs/QUALITY_TRACKER.md](docs/QUALITY_TRACKER.md) as a new P-item |

**Rule of thumb:** if the item is about *what the calculator does*, it goes in `Next session priorities`. If it is about *how the code is structured, validated, or documented for contributors*, it goes in QUALITY_TRACKER.

---

## Project Maintenance

**Project Update Procedure:** use [docs/PROJECT_UPDATE_PROCEDURE.md](docs/PROJECT_UPDATE_PROCEDURE.md) to sync documentation and status after any significant work.

**Triggering Updates:** use the `/update-project` shortcut in Antigravity or follow the checklist in the procedure document.

---

## Feature Completion Status (~72% of original TI-81, as of 2026-03-22)

Sessions:
- 2026-03-20: PRGM UI polish, colour palette extraction (`ui_palette.h`), PRGM module extraction to `ui_prgm.c`
- 2026-03-21: `expr_util.c` extraction (9 pure functions), 301-test host suite, persist round-trip tests, HAL guards in `persist.c`, full quality review pass
- 2026-03-21 (Session 6): `graph_ui.c` extraction (P2), float printf runtime guard (P8), FLASH sector map docs (P16)
- 2026-03-21 (Session 7): Integrate project update procedure (workflow, documentation, guidelines)
- 2026-03-21 (Session 8): Implement Global Hard QUIT (2nd+CLEAR) navigation
- 2026-03-21 (Session 9): IDE/Build fixes (IntelliSense header fix, recursive include resolve, debug config fix, CMake build fix)
- 2026-03-22 (Session 10): P6, P12, P13, P17 resolved (Sweet Spot items). -Werror enabled for App; Architecture/Testing/Troubleshooting docs created.
- 2026-03-22 (Session 11): Implement Y= equation enable/disable toggle functionality. Update graph renderer and persistence (v4). Fixed a startup crash and multiple trace/graph transition freezes in `graph.c` and `graph_ui.c` (guards for zero-scale ticks, loop clamping for singularities, and LVGL mutex synchronization).
- 2026-03-22 (Session 12): [P15] Expression pipeline documented in `TECHNICAL.md` with "2 + sin(45)" worked example. `QUALITY_TRACKER.md` updated.
- 2026-03-22 (Session 13): Matrix history display refactored — column-aligned rows, horizontal scroll via LEFT/RIGHT when expression is empty, `<`/`>` clip indicators. `HistoryEntry_t` now embeds `CalcMatrix_t` copy. Build at 82.44% RAM.
- 2026-03-22 (Session 15): P18 resolved — all 10 CODE_REVIEW_PENDING items complete. PRGM execution logic moved from `ui_prgm.c` to `prgm_exec.c` (−545 L / +540 L). Over-100-line functions split: `ShuntingYard` (3 helpers), `handle_yeq_mode` (navigation+insertion), `ui_init_graph_screens` (4 per-screen), `handle_history_nav` (`commit_history_entry`), `ui_refresh_display` (`render_result_row`), `try_tokenize_number` (2 helpers). Docs: README links, ARCHITECTURE diagram, `calc_internal.h` scope comment. `CODE_REVIEW_PENDING.md` deleted. Function complexity C+ → B. 301/301 tests pass. **Complexity delta: `decrease`** — pure function extractions and module responsibility correction; no new logic, state, or abstractions introduced.
- 2026-03-22 (Session 16): Graph render speed — added `MATH_VAR_X` token and `GraphEquation_t` postfix cache API to `calc_engine`. `Graph_Render` now runs Tokenize+ShuntingYard once per equation per render (on equation change only) and calls `Calc_EvalGraphEquation` per pixel column. At x_res=1 parse cost drops from 320× to 1× per equation per frame. 301/301 tests pass. **Complexity delta: `neutral`** — performance optimization with no new logic; cache is bounded state replacing repeated identical work; new public API surface is focused (2 functions, 1 type).
- 2026-03-22 (Session 17): `HistoryEntry_t` matrix ring buffer refactor — replaced embedded `CalcMatrix_t` (148 B) with 3-byte ring reference. 8-slot `matrix_ring[]` stores last 8 matrix results; generation counter detects eviction and falls back to pre-formatted `result` string. RAM: 82.44% → 81.82%. 301/301 tests pass. **Complexity delta: `decrease`** — resolves [complexity] debt from Session 13; no new logic or state abstractions.
- 2026-03-22 (Session 18): RAM audit (P12) — root cause: LVGL heap (`work_mem_int`, 64 KB) and FreeRTOS heap (`ucHeap`, 64 KB) = 128 KB = 65% of 192 KB internal RAM; SDRAM had 63.5 MB free. Fix: `SDRAM` region added to linker script (`0xD0070800`), `.sdram (NOLOAD)` section, `LV_ATTRIBUTE_LARGE_RAM_ARRAY` redirects LVGL heap to SDRAM. RAM: 81.82% → 48.49%. 301/301 tests pass. **Complexity delta: `neutral`** — two-line config change and linker extension; no new logic or state.
- 2026-03-22 (Session 19): PRGM system feature-complete — stale warning comments updated (`ui_prgm.c`/`ui_prgm.h`); `IS>(` and `DS<(` implemented in `prgm_exec.c` + CTL menu (items 13–14); `DispHome` and `DispGraph` implemented; `Output(row,col,"str")` implemented with `ui_output_row()` helper in `calculator_core.c`; `Menu("title","opt",Lbl,…)` fully implemented with dedicated LVGL overlay screen (`ui_prgm_menu_screen`) and UP/DOWN/1–9/ENTER/CLEAR key handling during execution. All 5 tasks from `docs/PRGM_COMPLETION.md` resolved. PRGM: ~50% → ~95% (hardware validation pending, P10). 301/301 tests pass. **Complexity delta: `neutral`** — all new handlers follow existing patterns; new `ui_prgm_menu_screen` follows the same LVGL screen creation pattern as all other PRGM screens.
- 2026-03-22 (Session 20): PRGM command reference created — `docs/PRGM_COMMANDS.md` documents all 14 CTL commands (If/Then/Else/End/While/For/Goto/Lbl/Pause/Stop/Return/prgm/IS>/DS<), all 6 I/O commands, Output(, Menu(, expr->VAR, and expression lines with syntax, edge cases, and limits table. `docs/PRGM_COMPLETION.md` deleted (all tasks resolved; pre-flight checklist folded into QUALITY_TRACKER.md P10; file list coverage moved to `PRGM_COMMANDS.md`). 301/301 tests pass. **Complexity delta: `neutral`** — documentation only; no code changes.
- 2026-03-22 (Session 21): Periodic code review — structural scan + Phase 2 direct reads. P19 (`prgm_execute_line` dispatch table, 495-line hotspot) and P20 (prgm exec host tests, ~80 tests) opened. `docs/CODE_REVIEW_PENDING.md` re-created with 7 action items. VARS/Y-VARS menu specs added to CLAUDE.md. Cross-reference audit clean. **Complexity delta: `neutral`** — documentation and review only; no code changes.
- 2026-03-22 (Session 22): P19 resolved — `prgm_execute_line` dispatch table refactor. 22 static `cmd_*` handler functions extracted (3–50 lines each); `parse_incdec_args` shared helper eliminates IS>/DS< duplication; `CmdHandler_t`/`CmdEntry_t` dispatch table replaces 495-line if/else chain; `prgm_execute_line` body reduced to ~30 lines. Zero logic changes. `docs/CODE_REVIEW_PENDING.md` item 7 resolved. Function complexity "at risk" qualifier removed. 301/301 tests pass. **Complexity delta: `decrease`** — pure mechanical extraction; no new logic, state, or abstractions.
- 2026-03-22 (Session 23): P20 resolved — program execution host test suite. `#ifndef HOST_TEST` guards added throughout `prgm_exec.c`/`.h` to strip LVGL/HAL/FreeRTOS dependencies. `App/Tests/prgm_exec_test_stubs.h` provides inline stubs for `prgm_parse_from_store`, `prgm_slot_is_used`, `prgm_slot_id_str`, `format_calc_result`. `App/Tests/test_prgm_exec.c`: 121 tests / 14 groups covering all command types (Goto/Lbl, If, Then/Else/End, While, For, IS>/DS<, Stop/Pause/Return, STO, Disp, Input/Prompt, ClrHome, subroutine call, complex programs). Bug fixed: `prgm_run_loop` Stop/Return/Goto-abort path did not reset `current_mode = MODE_NORMAL` — fixed by separating `!prgm_run_active` and `prgm_waiting_input` early-exit paths. Testing B+ → A-. 422/422 tests pass. **Complexity delta: `neutral`** — guards and test file; no new logic; bug fix is a one-line correction in the run loop.
- 2026-03-25 (Session 27): PRGM manual test plan rewrite — comprehensive 50-test plan targeting Session 26 regressions. Added T09b/T09c (ENTER/CLEAR first-press in alpha name-entry), T11b (EDIT digit shortcut), T12b (insert mode default), T16b (ERASE all 37 slots), T35b (prgmNAME unnamed slot), T43b (ALPHA_LOCK editor), T45 (body-only editor open), T46 (sub-menu tab wrap). Notes section cross-references each Session 26 fix. Test count: 40 → 50. **Complexity delta: `neutral`** — documentation only; no code changes.
- 2026-03-25 (Session 26): PRGM hardware test fixes — all 5 groups from manual test plan executed. **Group A** (UI bugs): ERASE shows all 37 slots; ALPHA fallback to key.normal in name-entry; EXEC/EDIT digit shortcuts; ERASE confirm immediate on digit key; DispGraph LVGL mutex fix; ALPHA_LOCK routes to editor. **Group B** (feature removals per spec): CTL menu → 8 items (Lbl/Goto/If/IS>/DS</Pause/End/Stop); I/O menu → 5 items (Disp/Input/DispHome/DispGraph/ClrHome); removed Then/Else/While/For/Return/Prompt/Output(/Menu( handlers and prgm_ctrl_stack/prgm_waiting_menu state; single-char Lbl/Goto constraint. **Group C** (execution model): EXEC tab inserts `prgmNAME` into expression; TOKEN_ENTER detects prgm prefix, runs program, shows `Done`; `prgm_lookup_slot` added to public API. **Group D** (editor): insert_mode=false on open; tab wrap; body-only slots open editor directly. **Group E** (alignment): Disp strings left-aligned in expression row, variables right-aligned in result row. Test suite: 378/378 pass (−44 tests for removed commands). Firmware: 48.45% RAM, 36.28% FLASH. **Complexity delta: `decrease`** — 8 handlers removed from dispatch table, prgm_ctrl_stack eliminated, prgm_build_occupied removed, 843 lines deleted vs 258 inserted.

### Completed features

| Feature | Log date | Notes |
|---|---|---|
| **Build**: Successful (fixed an implicit fallthrough error in `graph_ui.c` and multiple stability issues in `graph.c`, including a startup crash and a trace-related freeze).
| **Flash**: Successful using OpenOCD to STM32F429I-DISC1.
| ZBox rubber-band zoom | 2026-03-20 | Final validation on hardware pending |
| UTF-8 cursor navigation | 2026-03-21 | 12 test groups; `expr_util.c` extracted |
| Persist checksums/HAL | 2026-03-21 | FLASH sector 7 guard added; versioned header |
| Graph UI extraction | 2026-03-21 | Removed 1.5k LOC from `calculator_core.c` (P2) |
| Project Update Procedure | 2026-03-21 | Canonical sync rules + workflow automated |
| Global Hard QUIT | 2026-03-22 | 2nd+CLEAR hard exit to main screen implemented |
| Y= Toggle | 2026-03-22 | Equation enable/disable toggle from Y= editor implemented |

### Well-implemented (60–100%)

| Area | Est. Done | Notes |
|---|---|---|
| Basic arithmetic | ~95% | +, −, ×, ÷, ^, parentheses, precedence all solid |
| Standard math functions | ~80% | sin/cos/tan, asin/acos/atan, ln, log, √, abs, round, iPart, fPart, int, rand, nPr, nCr work; factorial, cube root, ∛, nDeriv NOT evaluated |
| Variables (A–Z, ANS) | ~90% | STO, ANS, X in graph all work; list variables missing |
| Display / UI / navigation | ~90% | Expression wrap, wrapped history, Fix/Float mode, MATH from Y=, UTF-8 cursor, history scroll, ENTER re-run all solid; Sci/Eng notation display not wired |
| Graphing (function mode) | ~80% | 4 equations with enable/disable toggle, axes, grid (toggle from MODE), trace, ZBox, zoom, RANGE, Xres step, interpolated curves; stability fixes for zero-scale and singularities; Connected/Dot mode not wired |
| TEST operators | ~100% | Menu (2nd+MATH), UP/DOWN/ENTER/number-key selection, inserts =, ≠, >, ≥, <, ≤; all 6 operators fully evaluated (return 1/0); accessible from Y= editor; and/or/not not present on TI-81 hardware — not planned |

### Partially implemented

| Area | Est. Done | Notes |
|---|---|---|
| MATRIX | ~95% | Variable dimensions 1–6×6 per matrix; scrolling cell editor with dim mode; all 6 explicit ops + arithmetic (+, −, ×, scalar×matrix) fully evaluated; `det(ANS)` / `[A]+ANS` chains work; persist across power-off; `[A]`/`[B]`/`[C]` cursor/DEL atomicity fixed; matrix tokens blocked in Y= editor |
| PRGM | ~95% | UI (menus, editor, CTL/I/O sub-menus) and executor (`prgm_exec.c`) fully implemented. Supported: `If` (single-line), `Goto/Lbl`, `Disp/Input/ClrHome/Pause/Stop/prgm(subroutine)/STO/IS>(DS</DispHome/DispGraph`. Removed per TI-81 spec: `Then/Else/While/For/Return/Prompt/Output(/Menu(`. Execution model: EXEC inserts `prgmNAME` into expression; ENTER runs and shows `Done`. Remaining: hardware validation (P10). |

### Entirely missing (0%)

| Area | TI-81 weight | Notes |
|---|---|---|
| STAT | ~15% | 1-Var/2-Var stats, regression, stat plots — nothing implemented |
| DRAW | ~5% | Line, Horizontal, Vertical, DrawF, Shade — stub only |
| Parametric / Polar / Seq graphing | ~5% | Only function mode works |
| VARS menu | ~3% | Window, Zoom, GDB, Picture, Statistics vars — stub only |

The core calculator (arithmetic + standard functions + function graphing + TEST comparisons + matrix math) covers ~85% of day-to-day TI-81 usage. STAT is entirely absent. Matrix is ~95% complete. PRGM UI code has been extracted and reorganized, but **its execution backend is disconnected/incomplete** and requires future work.

---

## Deliberate Deviations from Original TI-81

Behaviours that differ from the original hardware by design:

| Feature | Original TI-81 | This implementation |
|---------|---------------|---------------------|
| History scroll | Not present — `2nd+ENTRY` recalled last entry only | UP/DOWN arrows scroll backward/forward through history |
| `2nd+ENTRY` | Recalled the single most recent entry | Recalls most recent entry AND positions `history_recall_offset` at 1, so UP/DOWN continue working from there |
| Menu vs. expression glyph inconsistency | Menu labels and expression buffer used the same internal token glyphs throughout | **Known inconsistency:** menu labels now display proper Unicode glyphs (³, ³√(, sin⁻¹( etc.) but the expression buffer still renders the underlying ASCII/multi-char insert strings (`^3`, `^(1/3)`, `^-1`). Consequence: selecting ³√( from the MATH menu inserts `^(1/3)` into the expression, while pressing `2nd`+√ inserts `√(` — the two paths produce visually different expression text even though both evaluate correctly. The root cause is that menu display labels were separated from insert strings (cosmetic fix) but the expression buffer has no glyph-substitution layer. Full resolution requires either a token-based expression renderer (replaces raw string display with rendered glyphs per token) or a post-insert rewrite pass that maps ASCII sequences to their Unicode equivalents. This is a known gap, not a regression. |

---

## Project Overview

A TI-81 calculator recreation running on an STM32F429I-DISC1 discovery board.

- **MCU:** STM32F429ZIT6 (Cortex-M4, 2MB Flash, 192KB RAM, 64KB CCMRAM)
- **Display:** ILI9341 240×320 RGB565 via LTDC, landscape orientation (software rotated)
- **UI:** LVGL v9.x
- **RTOS:** FreeRTOS via CMSIS-OS v1
- **Toolchain:** GCC ARM None EABI 14.3.1, CMake, VSCode + stm32-cube-clangd extension
- **Build system:** CMake with `cmake/gcc-arm-none-eabi.cmake`
- **Repository:** https://github.com/mndxc/STM32F429-TI81-Calculator

---

## Build & Flash

### Configure and build (command line)

VSCode uses the `cube-cmake` wrapper from the stm32-cube-clangd extension, which sets up the toolchain automatically. In a plain terminal the toolchain must be on PATH first.

`CMakePresets.json` at the repo root wraps the toolchain file and generator, so the shorthand works:
```bash
export PATH="$HOME/Library/Application Support/stm32cube/bundles/gnu-tools-for-stm32/14.3.1+st.2/bin:$PATH"
cmake --preset Debug
cmake --build build/Debug
```

Equivalent explicit form (same effect, no presets):
```bash
export PATH="$HOME/Library/Application Support/stm32cube/bundles/gnu-tools-for-stm32/14.3.1+st.2/bin:$PATH"
cmake -B build/Debug \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake \
      -G Ninja
cmake --build build/Debug
```
Output: `build/Debug/STM32F429-TI81-Calculator.elf` (and `.map`, `.bin` via objcopy).

Incremental rebuild only (PATH must still be set):
```bash
export PATH="$HOME/Library/Application Support/stm32cube/bundles/gnu-tools-for-stm32/14.3.1+st.2/bin:$PATH"
cmake --build build/Debug
```

**Preferred:** use the VSCode CMake build button — the extension handles the toolchain automatically via `cube-cmake`.

### Flash & debug
**CLI (preferred for quick iteration):**
```bash
openocd \
  -f /opt/homebrew/Cellar/open-ocd/0.12.0_1/share/openocd/scripts/board/stm32f429disc1.cfg \
  -c "program build/Debug/STM32F429-TI81-Calculator.elf verify reset exit"
```
OpenOCD uses the ST-Link on the discovery board directly. No PATH setup needed — OpenOCD is installed via Homebrew.

**VSCode alternative:** `F5` / Run and Debug → `STM32Cube: STM32 Launch ST-Link GDB Server` (stm32-cube-clangd extension).

**After flashing:** do a full USB power cycle (unplug/replug) if the display shows white after reset — the ILI9341 does not always recover cleanly from SWD reset alone.

### Host Tests

```bash
cmake -S App/Tests -B build/tests && cmake --build build/tests
./build/tests/test_calc_engine        # 153 tests: tokenizer, shunting-yard, RPN, matrix, edge cases
./build/tests/test_expr_util          # 96 tests:  UTF-8 cursor, insert/delete, matrix token atomicity
./build/tests/test_persist_roundtrip  # 52 tests:  PersistBlock_t checksum, validation, field round-trip
./build/tests/test_prgm_exec          # 121 tests: all 22 command handlers, control flow, subroutine call
```

All four executables exit 0 on full pass (422 total tests). CI runs them automatically on every push/PR with gcov branch coverage measurement. Enable coverage locally with `-DCOVERAGE=ON`.

---

## CubeMX Setup (Fresh Project)

A new contributor can reproduce the CubeMX base from scratch with a single non-default step. All other project-specific configuration is handled by App code.

### Steps in CubeMX

1. **New Project → Board Selector → `STM32F429I-DISC1`** → click "Initialize All Peripherals with Default Mode"
2. **Middleware → FreeRTOS → CMSIS V1** — enable it. This is the only non-default peripheral to add.
   - In FreeRTOS settings, also enable: `USE_IDLE_HOOK`, `USE_MALLOC_FAILED_HOOK` (these control generated hook stubs in `freertos.c` and must be set in the .ioc — they cannot be overridden from user code safely)
3. **Project Manager** → set Toolchain to **CMake**, set project name
4. **Generate Code**

### Steps after generating

1. Copy the `App/` folder from the repo into the generated project root
2. In `CMakeLists.txt`, add the App sources (copy the `target_sources` and `target_include_directories` blocks from the repo's `CMakeLists.txt`)
3. Add `-u _printf_float` to `CMAKE_EXE_LINKER_FLAGS` in `CMakeLists.txt` (required for `%f`/`%g` in `snprintf` with `--specs=nano.specs`)
4. Paste the FreeRTOS USER CODE overrides into `Core/Inc/FreeRTOSConfig.h` — already present if you copied from repo; see `/* USER CODE BEGIN Defines */` section

### What App code handles (no CubeMX entries needed)

| Concern | Where |
|---|---|
| Keypad A-line GPIO init (PE2–PE5, PB3, PB4, PB7) | `Keypad_GPIO_Init()` in `keypad.c` |
| Keypad B-line GPIO init (PA5, PC3, PC8, PC11, PD7, PG2, PG3, PG9) | `Keypad_GPIO_Init()` in `keypad.c` |
| ON button EXTI (PE6) | `on_button_init()` in `app_init.c` |
| LTDC framebuffer address (0xD0000000) | `BSP_LCD_LayerDefaultInit()` in `App_DefaultTask_Run()` |
| FreeRTOS heap size, stack overflow check, mutex/semaphore APIs | USER CODE overrides in `FreeRTOSConfig.h` |

---

## Current Project State (as of 2026-03-21)

All custom application code lives under `App/`. `Core/` contains only CubeMX-generated files. The `main.c` touch points are `#include "app_init.h"` and `App_RTOS_Init()`.

### Completed features (all committed)
- JetBrains Mono font wired into LVGL (`jetbrains_mono_24.c`, `lv_conf.h` updated)
- MODE screen — arrow-key navigation, row highlight, ENTER commits
- MATH menu — four tabs (MATH/NUM/HYP/PRB), scrollable item list, overflow indicators
- ZOOM cursor navigation — UP/DOWN highlight; ENTER selects; scroll indicators for items 7–8
- RANGE Xres= field — seventh field added; combined name=value labels
- Overwrite mode — INS toggles; `expr_insert_char/str` respects mode
- Arrow key hold-to-repeat (400ms delay, 80ms rate; arrows only)
- Y= cursor navigation (LEFT/RIGHT within equation, DEL at cursor, UP/DOWN between rows)
- Free graph screen navigation (any graph key works from any graph screen)
- Context-aware CLEAR (wipes eq/field; exits if already empty)
- RANGE ZOOM bug fix (ZOOM from RANGE opens ZOOM menu, not ZStandard)
- UP arrow history recall (UP scrolls back through history; DOWN scrolls forward)
- Input text wrap; MATH HYP items renamed; √ tokeniser; ZOOM Set Factors sub-screen
- NUM tab functions (round, iPart, fPart, int); Fix decimal mode from MODE; grid toggle from MODE
- MATH menu from Y= editor; UTF-8 aware Y= cursor; wrapped history entries; full-height graph canvas
- Split X=/Y= trace readouts; x_res interpolation; Xres clamped to 1–8
- TEST menu — full UI + all 6 comparison operators evaluated (=, ≠, >, ≥, <, ≤ → 1/0)
- UTF-8 cursor integrity fix — LEFT/RIGHT/DEL/overwrite all handle multi-byte sequences correctly
- Font regeneration — ↑↓ (U+2191/U+2193) and ≠/≥/≤ (U+2260/U+2264/U+2265) added to both font sizes
- Scroll indicator glyphs — ZOOM and MATH menus use ↓/↑ (U+2193/U+2191) amber overlays
- Heartbeat LED fixed to 1 Hz (100 × 5 ms in DefaultTask render loop)
- **Persistent storage** — `App/Inc/persist.h`, `App/Src/persist.c`. Saves A–Z, ANS, MODE, graph equations, RANGE, zoom factors to FLASH sector 10 (0x080C0000). `Calc_BuildPersistBlock` / `Calc_ApplyPersistBlock` in `calculator_core.c`. Load on boot, save on plain ON and 2nd+ON. On boot-load, all screens are synced: Y= labels, MODE highlight, RANGE field labels, and ZOOM FACTORS labels all reflect the restored state. `PersistBlock_t` size: 864 B.
- **ON button EXTI** — PE6 configured as EXTI falling-edge with pull-up in `on_button_init()` (called from `App_RTOS_Init`). `EXTI9_5_IRQHandler` and `HAL_GPIO_EXTI_Callback` defined in `app_init.c`. Pin referenced via `KEYPAD_ON_PIN` / `KEYPAD_ON_PORT` from `keypad.h` — no dependency on CubeMX `main.h` macros.
- **CubeMX decoupling** — Keypad GPIO pins (all 15 Matrix* signals) removed from CubeMX dependency. `keypad.h` defines all pin constants (`KEYPAD_A1_PORT`/`PIN` … `KEYPAD_B8_PORT`/`PIN`, `KEYPAD_ON_PORT`/`PIN`). `Keypad_GPIO_Init()` in `keypad.c` initialises them at task start. `app_init.c` uses `KEYPAD_ON_*` for the ON button EXTI. FreeRTOS config values protected by `#undef`/`#define` overrides in the `USER CODE BEGIN Defines` section of `Core/Inc/FreeRTOSConfig.h` — survive CubeMX regeneration.
- **Power management / Stop mode** — `Power_EnterStop()` in `app_init.c`. `2nd+ON` saves state then enters STM32 Stop mode. Wake on ON button press restores PLL, PLLSAI, SDRAM, LTDC, and resumes DefaultTask. `App_SystemClock_Reinit()` wrapper in `main.c` USER CODE BEGIN 4. `g_sleeping` flag guards ISR from posting spurious TOKEN_ON during wake.
- **Power-off substitute screen** — `Power_DisplayBlankAndMessage()` in `app_init.c`. Currently called instead of `Power_EnterStop()` (see Known Issues — "Display fade on power-off"). Full-screen black LVGL overlay + dim "Powered off" label; blocks on keypad queue until TOKEN_ON; tears down overlay on return. See Known Issues for the one-line swap to restore real Stop mode on a custom PCB.
- **History scroll** — UP/DOWN arrow keys scroll backward/forward through previous expressions. `history_recall_offset` tracks position; 0 = live input. Replaces original `2nd+ENTRY`-only recall with a richer scroll model (see Deliberate Deviations).
- **2nd+ENTRY** — recalls most recent expression and sets `history_recall_offset = 1` so UP/DOWN continue scrolling from there without requiring a CLEAR first.
- **ENTER on blank line re-run** — pressing ENTER with empty input re-evaluates the most recent history expression and appends a full history entry (expression + result), identical to a normal evaluation. Repeated ENTER presses each add a copy, so UP/DOWN and 2nd+ENTRY always land on a real expression with no special-case handling needed. Implemented in `TOKEN_ENTER` handler (`calculator_core.c`).
- **MATH PRB** — rand, nPr, nCr fully implemented. `rand` evaluated at tokenize time using `srand(HAL_GetTick())`; nPr/nCr in `EvaluateRPN()` in `calc_engine.c` with domain checking.
- **MATRIX math fully evaluated** — All 6 operations fully wired end-to-end: `det([A])` → scalar; `[A]T` → transpose; `rowSwap([A],r1,r2)`, `row+([A],r1,r2)`, `*row(k,[A],r)`, `*row+(k,[A],r1,r2)` → matrix result. Row indices are 1-based (matching TI-81). Results stored in `calc_matrices[3]` (ANS slot); source matrix unchanged. Matrix results display as column-aligned rows in history (Session 13 refactor; `HistoryEntry_t` + `matrix_format_row()`). `CalcMatrix_t` and `calc_matrices[4]` in `calc_engine.c`/`calc_engine.h`. Comma tokenized as `MATH_COMMA`; ShuntingYard handles argument separation. Parallel `is_matrix[]` stack in `EvaluateRPN` tracks which stack slots hold matrix indices vs scalars. `MAX_RESULT_LEN` bumped 32→96.
- **Variable matrix dimensions (1–6×6)** — `CALC_MATRIX_DIM` (fixed 3) replaced by `CALC_MATRIX_MAX_DIM = 6`; each `CalcMatrix_t` carries its own `rows`/`cols`. Default is 3×3. `det()` now uses Gaussian elimination with partial pivoting (handles 1×1–6×6; previously Sarrus 3×3 only). All row ops (`rowSwap`, `row+`, `*row`, `*row+`, transpose) iterate over actual dimensions. Row index bounds checked against actual matrix size. `round()` now works element-wise on matrix operands (e.g. `round([A],2)` rounds every element to 2 decimal places).
- **Matrix cell editor — scrolling + dim mode** — Editor shows 7 visible cell rows with ↑/↓ amber scroll indicators when the matrix is larger than the viewport. Navigating UP past the first cell enters dim mode: cursor lands on the title label (`[A] RxC`); LEFT/RIGHT switches between the rows digit and cols digit; digit keys resize the matrix live. Dim-mode title renders in yellow; normal cell mode in white. `PERSIST_VERSION` bumped to 3; persist block now stores `matrix_rows[3]`/`matrix_cols[3]` and full 6×6 data arrays (432 B vs 108 B previously). `PersistBlock_t` size: 532 B → 860 B (documented as 856 B in header comment until 2026-03-21; corrected after test_persist_roundtrip verification).
- **Screen navigation refactor** — five helpers centralise all cross-screen transitions in `calculator_core.c`: `hide_all_screens()`, `nav_to(target)`, `menu_open(token, return_to)`, `menu_close(token)`, `tab_move()`. Every graph-nav key (Y=, RANGE, ZOOM, GRAPH, TRACE) now works from every screen including MATH/TEST/MATRIX menus and ZBox/Trace modes. Unhandled keys in menus close the menu and fall through to the main switch (or drop the key if the return context is Y=, matching original TI-81 behaviour).
- **ENTER in Y= editor** — moves cursor to next equation, same as DOWN.
- **TRACE exit speed** — pressing any non-navigation key from TRACE now hides the graph canvas immediately without re-rendering before processing the key.
- **Matrix arithmetic (+, −, ×, scalar×matrix)** — `[A]+[B]`, `[A]-[B]`, `[A]*[B]`, `scalar*[M]`, `[M]*scalar` fully evaluated. `mat_add`, `mat_sub`, `mat_mul`, `mat_scale` helpers in `calc_engine.c`; matrix dispatch block runs before the scalar binary-op block in `EvaluateRPN`. Result always written to `calc_matrices[3]` (ANS slot). Chaining works: `det(ANS)` after a matrix result correctly resolves ANS as a matrix reference via `ans_is_matrix` (see Variables section).
- **Execute_Token refactor** — the 1,724-line monolithic function was mechanically split into 13 named static handler functions (`handle_yeq_mode`, `handle_range_mode`, `handle_zoom_mode`, `handle_zoom_factors_mode`, `handle_zbox_mode`, `handle_trace_mode`, `handle_mode_screen`, `handle_math_menu`, `handle_test_menu`, `handle_matrix_menu`, `handle_matrix_edit`, `handle_sto_pending`, `handle_normal_mode`). `Execute_Token` itself reduced to ~60 lines (TOKEN_ON and TOKEN_MODE inline + dispatcher chain). Zero logic changes.
- **PRGM executor (Session 2)** — `prgm_run_start()`, `prgm_run_loop()`, `prgm_execute_line()`, `prgm_skip_to_target()`, and `handle_prgm_running()` all in `calculator_core.c`. Text interpreter runs `\n`-delimited program lines synchronously from CalcCoreTask. Supported: `If/Then/Else/End`, `While`, `For(V,begin,end,step)`, `Goto/Lbl`, `Pause`, `Stop`, `Return`, `prgm<name>` subroutine call (call stack depth 4), `Disp "str"`/`Disp expr`, `Input V`, `Prompt V`, `ClrHome`, `expr->VAR` assignment, general expression lines (result → ANS). Control flow stack depth 8 (`CtrlFrame_t`). `CLEAR` aborts; `ENTER` resumes after `Pause`/`Input`/`Prompt`. `TOKEN_ON` (SAVE) clears executor state. Programs persist in FLASH sector 11. Deferred: `IS>(`, `DS<(`, `Menu(`, `Output(`.
- **PRGM UI polish (Session 3)** — EXEC and EDIT tabs both list all 37 program slots (previously EXEC showed occupied slots only). Display format changed to `N:PrgmN` (canonical slot name) with optional second column `  USER_NAME` when a user name has been given. Tabs renamed from EXEC/EDIT/NEW to EXEC/EDIT/ERASE; program creation now triggered from the EDIT tab by selecting any empty slot (user name is optional — pressing ENTER with no name opens the editor immediately). Program names now accept A–Z and 0–9 (previously letters only). All PRGM menu highlights and titles changed from amber (0xFFAA00) to yellow (0xFFFF00) to match MATH, TEST, and MATRIX menus. PRGM editor and name-entry cursors now blink and reflect 2nd (amber `^`) and ALPHA (green `A`) state, wired into `cursor_timer_cb` and `ui_update_status_bar` via `prgm_editor_cursor_update()` and `prgm_new_cursor_update()`. Scroll indicators fixed across PRGM, MATH, and ZOOM menus: previously initialized with empty text so no arrow was visible; now initialized with ↑/↓ glyphs. All scroll indicator overlay labels given opaque black background (`LV_OPA_COVER`) to cover the underlying colon character in item text. Editor scroll indicators moved from X=18 to X=4 to sit on the `:` line prefix rather than the first content character.
- **PRGM extraction (Session 4)** — Extracted PRGM menu, navigation, and sub-menus into `ui_prgm.c` and `ui_prgm.h` adhering to the UI Extensibility Pattern. Re-wired `cursor_visible` for hardware timers and regenerated the dual-tab CTL/IO screen elements. Note: Execution backend is disconnected and incomplete.
- **Colour palette extraction (Session 5)** — All magic colour hex literals replaced with named constants in `App/Inc/ui_palette.h` (14 constants: `COLOR_BLACK`, `COLOR_WHITE`, `COLOR_BG`, `COLOR_YELLOW`, `COLOR_AMBER`, `COLOR_GREY_LIGHT/MED/INACTIVE/DARK/TICK`, `COLOR_2ND`, `COLOR_ALPHA`, `COLOR_CURVE_Y1–Y4`). Orphaned local `#define` block removed from `calculator_core.c`. Inline literals removed from `calculator_core.c`, `graph.c`, `app_init.c`, `ui_prgm.c`, `ui_matrix.c`. One intentional exception: `0x00FF00` trace crosshair green in `graph.c` (single-use, no semantic peer).
- **`expr_util.c` extracted (Session 5)** — 9 pure expression-buffer functions moved from `calculator_core.c` to `App/Src/expr_util.c` + `App/Inc/expr_util.h`: `ExprUtil_Utf8CharSize`, `ExprUtil_Utf8ByteToGlyph`, `ExprUtil_MatrixTokenSizeBefore`, `ExprUtil_MatrixTokenSizeAt`, `ExprUtil_MoveCursorLeft`, `ExprUtil_MoveCursorRight`, `ExprUtil_InsertChar`, `ExprUtil_InsertStr`, `ExprUtil_DeleteAtCursor`, `ExprUtil_PrependAns`. Zero LVGL/HAL/RTOS dependencies — all state passed explicitly. Static functions in `calculator_core.c` are now thin wrappers; TOKEN_LEFT/RIGHT handlers simplified. 96-test suite (`test_expr_util.c`, 12 groups) covers all functions including UTF-8 multi-byte sequences, matrix token atomicity, insert/overwrite mode, and round-trip cursor navigation.
- **`persist.c` host-testable (Session 5)** — `Persist_Checksum` and `Persist_Validate` exposed as public API (no FLASH dependency). HAL-dependent code (`Persist_Save`, `Persist_Load`, `persist_erase_sector`, `persist_write_block`) guarded with `#ifndef HOST_TEST`. Round-trip test suite (`test_persist_roundtrip.c`, 52 tests / 5 groups) validates checksum stability, valid/invalid block detection, 6 corruption patterns, field round-trip, and struct size/alignment. `PersistBlock_t` header-comment size corrected 856 → 860 B.
- **301-test CI suite (Session 5)** — `App/Tests/CMakeLists.txt` now builds three executables (`test_calc_engine`, `test_expr_util`, `test_persist_roundtrip`). `.github/workflows/build.yml` `host-tests` job runs all three and reports gcov branch coverage across `calc_engine.c`, `expr_util.c`, and `persist.c`.
- **`graph_ui.c` extracted (Session 6)** — Extracted 6 graph handler functions (`handle_yeq_mode`, `handle_range_mode`, `handle_zoom_mode`, `handle_zoom_factors_mode`, `handle_zbox_mode`, `handle_trace_mode`), `nav_to`, and `ui_init_graph_screens` into `App/Src/graph_ui.c` and `App/Inc/graph_ui.h`. `calculator_core.c` reduced from 3,603 to 1,989 LOC (−45%). P2 resolved.
- **Float printf runtime guard (Session 6)** — Startup assertion in `App_DefaultTask_Run()` ensures `-u _printf_float` is linked. P8 resolved.
- **FLASH sector map docs (Session 6)** — Detailed memory layout and sector mapping added to `TECHNICAL.md`. P16 resolved.
- **UART debug retarget removed** — `_write()` syscall override in `app_init.c` deleted. No `printf` calls exist in App code; newlib's default stub (discards output) now applies. Saves ~640 bytes FLASH.

### Known issues
- **Display fade on power-off (hardware limitation — prototype substitute implemented)** — The ILI9341 in RGB interface mode has no internal frame buffer. When LTDC stops clocking pixels, the panel's liquid crystal capacitors discharge to their resting state, which the panel renders as white. There is no hardware path to hold the display black after LTDC is halted. **Current prototype behaviour:** `2nd+ON` calls `Power_DisplayBlankAndMessage()` (`app_init.c`) instead of `Power_EnterStop()`. It shows a full-screen black LVGL overlay with a centred "Powered off" label in dim grey (`0x444444`) and blocks the CalcCoreTask on `xQueueReceive` until the ON button is pressed again — no actual Stop mode is entered, no display fade occurs. **Custom PCB migration (one-line change):** in `Execute_Token()` in `calculator_core.c`, in the `TOKEN_ON` / `power_down` branch, replace the `Power_DisplayBlankAndMessage()` call with `Power_EnterStop()`. Both functions are defined in `app_init.c` and declared in `app_init.h`; no other files need to change.
- **ZBox arrow key lag** — Screen update rate cannot keep up with held arrow keys during ZBox rubber-band zoom selection. `Graph_DrawZBox()` in `graph.c` redraws from `graph_buf_clean` on every arrow key event; at 80ms repeat rate this may be saturating the LVGL render pipeline. Likely fix: throttle redraws in ZBox mode (skip frames if previous draw not yet flushed), or move crosshair/rectangle rendering to a lightweight overlay rather than full frame restore + redraw each keypress.

### Next session priorities (in order)

> **Quality and refactoring items** are tracked in [docs/QUALITY_TRACKER.md](docs/QUALITY_TRACKER.md), not here.
> Open items: **P1, P3, P7, P10**. Highest ease-to-impact: P10 (hardware validation), P1 (property-based tests).

**1. Y= equation enable/disable toggle** — ✅ Resolved 2026-03-22

**3. Startup splash image** — Display a bitmap or splash screen on boot before the calculator UI initialises. LVGL supports image objects natively; asset format is RGB565 array in FLASH.

**4. Trace crosshair behaviour differs from original TI-81** — On the original hardware, pressing any non-arrow key while in trace exits trace and processes that key (e.g. GRAPH re-renders, CLEAR exits to calculator). Currently TRACE is a toggle (press again to exit), which is not original behaviour. Additionally, on the original TI-81 there is a free-roaming crosshair cursor visible on the plain graph screen (before pressing TRACE); pressing TRACE snaps the crosshair to the nearest curve. This free-roaming crosshair is not implemented — the graph canvas currently shows no cursor at all until TRACE is pressed. Investigate original behaviour and decide which deviations to correct.
- Files: `App/Src/calculator_core.c` (trace mode handler `TOKEN_TRACE` case, `default` fallthrough behaviour)

**5. Graph render speed** — ✅ Resolved 2026-03-22 (Session 16). Postfix cache added: `MATH_VAR_X` token lets ShuntingYard output be reused across pixel columns; `Calc_PrepareGraphEquation` / `Calc_EvalGraphEquation` API caches per-equation postfix in `graph.c`; parse cost 320× → 1× per equation per frame. ZBox rubber-band lag (item #6) is a separate per-keypress redraw issue — not addressed by this fix.

**6. ZBox render speed** — See Known Issues entry "ZBox arrow key lag" for root cause and suggested fix (throttle redraws / lightweight overlay).

**7. QUIT (2nd+CLEAR) always exits to main calculator screen** — ✅ Resolved 2026-03-22

**8. Matrix result display — left-aligned columns with horizontal scroll** — ✅ Resolved 2026-03-22
- `HistoryEntry_t` now embeds a `CalcMatrix_t matrix_data` copy + `bool has_matrix` flag. Matrix data is copied from `calc_matrices[result.matrix_idx]` at evaluation time so it survives beyond ANS slot reuse.
- `matrix_format_row()` builds column-aligned rows on-the-fly using per-column max widths; `<`/`>` ASCII indicators show clip edges. History scroll state (`matrix_scroll_focus`, `matrix_scroll_offset`) set on each matrix ENTER result.
- LEFT/RIGHT when `expr_len==0` and scroll focus is active pan the matrix view; all other keys use normal expression cursor logic.
- **Note:** `<`/`>` were used instead of `…` (U+2026) because U+2026 is not in the current font. To use `…` instead, add `-r 0x2026` to the `lv_font_conv` regeneration commands in CLAUDE.md gotcha #14.
- RAM impact: `HistoryEntry_t` grew from 192 B to ~344 B; 32×344=11 KB total history (up from 6 KB). Build RAM at 82.44% at end of Session 13; reduced to 81.82% in Session 17 (matrix ring buffer refactor — see complexity item).

**9. Verify cursor activity uniformity across all screens** — ✅ Resolved 2026-03-22
- Audited all 6 cursor implementations (main, Y=, RANGE, ZOOM FACTORS, MATRIX EDIT, PRGM editor/new-name).
- **Bug fixed:** `matrix_edit_cursor_update()` in `calculator_core.c` was a no-op stub that shadowed the real implementation in `ui_matrix.c`, preventing the matrix editor cursor from blinking. Fixed by removing the stub and forward decl from `calculator_core.c`, making `matrix_edit_cursor_update()` non-static in `ui_matrix.c`, and declaring it in `ui_matrix.h`.
- Remaining architecture note (low severity): RANGE, ZOOM FACTORS, and PRGM editor use character offsets (ASCII assumed); Y= and main use byte offsets with UTF-8 conversion. No functional impact since those editors only accept ASCII digits and letters.

**10. Verify menu user interaction uniformity across all screens** — ✅ Resolved 2026-03-22
- Audited UP/DOWN/ENTER/number-key navigation, tab switching, overflow indicators, CLEAR/exit behaviour, and return-to-caller logic across MATH, TEST, MATRIX, ZOOM, and MODE menus.
- **Bug fixed:** `handle_matrix_menu` TOKEN_ZOOM case (`ui_matrix.c`) returned `false` with a stale comment claiming `zoom_menu_reset()` was not exposed. In fact `nav_to(MODE_GRAPH_ZOOM)` calls `zoom_menu_reset()` internally. Replaced `return false` with `nav_to(MODE_GRAPH_ZOOM); return true;`, making it consistent with TOKEN_Y_EQUALS, TOKEN_RANGE, TOKEN_GRAPH, and TOKEN_TRACE in the same handler.
- All other menu behaviours are uniform and correct: MATH/TEST/MATRIX use `menu_open`/`menu_close` with `return_mode`; ZOOM is a graph screen with no `return_mode` (by design); MODE uses inline early-exit (non-modal); overflow indicators (↑/↓) are present where needed (MATH, ZOOM — 8 items each); TEST (6 items) and MATRIX (6/3 items) fit the viewport without scrolling.
- No further irregularities requiring action.

**12. Review RAM usage — LVGL and video interface consumption** — ✅ Resolved 2026-03-22
- **Root cause found and fixed.** Two heap pools defaulted to internal RAM: LVGL heap (`work_mem_int`, 64 KB from `LV_MEM_SIZE`) and FreeRTOS heap (`ucHeap`, 64 KB from `configTOTAL_HEAP_SIZE`). Together = 128 KB = 65% of all 192 KB internal RAM. SDRAM had 63.5 MB free.
- **Fix:** Added `SDRAM` region to `STM32F429XX_FLASH.ld` (origin `0xD0070800`, after the three framebuffers) with a `.sdram (NOLOAD)` section. Set `LV_ATTRIBUTE_LARGE_RAM_ARRAY __attribute__((section(".sdram")))` in `lv_conf.h` to relocate LVGL's heap pool to SDRAM. FreeRTOS heap stays in internal RAM (CCMRAM is not DMA-accessible; internal RAM is the correct location for task stacks).
- **Result: internal RAM 81.8% → 48.5%** (64 KB freed). SDRAM usage: 0.70% → 0.80%. 301/301 tests pass.
- **Confirmed via `arm-none-eabi-nm`:** `work_mem_int` is at `0xD0070800` (SDRAM); `ucHeap` remains at `0x20000ADC` (internal RAM). All three framebuffers confirmed at fixed SDRAM pointers (`graph_buf`, `graph_buf_clean` at `0xD0025800`/`0xD004B000`), not in linker sections.
- **SDRAM layout (complete):**
  ```
  0xD0000000  LCD framebuffer    320×240×2 = 153,600 B  (fixed pointer in app_init.c)
  0xD0025800  graph_buf          320×240×2 = 153,600 B  (fixed pointer in graph.c)
  0xD004B000  graph_buf_clean    320×240×2 = 153,600 B  (fixed pointer in graph.c)
  0xD0070800  .sdram section     64 KB = LVGL heap      (linker-placed, NOLOAD)
  0xD0080800  free SDRAM         ~63.5 MB remaining
  ```

**13. Audit and unify colour scheme between calculator screen and menu screens** — ✅ Resolved 2026-03-22
- All files already used named `COLOR_*` constants — no bare hex literals found. The only inconsistency was the main screen background using `COLOR_BG` (0x1A1A1A) while all menu/graph screens used `COLOR_BLACK` (0x000000). Fixed by replacing both `COLOR_BG` usages in `calculator_core.c` (screen background style and cursor-inner text colour) with `COLOR_BLACK`, then removing the now-unused `COLOR_BG` constant from `ui_palette.h`. Result: background is uniformly pure black across the calculator screen and all menus; grey history-expression text (`COLOR_GREY_MED`) and live-expression text (`COLOR_GREY_LIGHT`) have improved contrast on the darker background.

**17. Fix STO> to evaluate the current expression, not just store ANS** — ✅ Resolved 2026-03-22
- `TOKEN_STO` now calls `expr_prepend_ans_if_empty()` when `expr_len == 0`, so pressing STO on an empty line shows "ANS" before the STO prompt.
- `handle_sto_pending()` now calls `Calc_Evaluate(expression, ...)` instead of storing `ans` directly. History expression shows `<expr>->A`; result shows the evaluated value. Expression buffer cleared after store (same as ENTER). Errors (syntax, data type for matrix results) abort the store and display the error string in the result row.

**[complexity] Reduce HistoryEntry_t memory footprint** — ✅ Resolved 2026-03-22. Replaced `CalcMatrix_t matrix_data` (148 B) in `HistoryEntry_t` with a 3-byte ring reference (`matrix_ring_idx`, `matrix_ring_gen`, `matrix_rows_cache`). An 8-slot `CalcMatrix_t matrix_ring[]` holds the last 8 matrix results; generation-based eviction detection falls back to the pre-formatted `result` string. RAM: 82.44% → 81.82%. 301/301 tests pass. **Complexity delta: `decrease`** — no new logic; pure data layout improvement.

**18. Expand font glyph set for VARS/STAT display** — Regenerate both LVGL font files adding the following codepoints to the `lv_font_conv` commands in gotcha #14:
- ȳ — U+0233 (`-r 0x0233`)
- x̄ — **resolved via custom PUA glyph U+E000** in `JetBrainsMono-Regular-Custom.ttf`. No precomposed Unicode codepoint exists; the custom TTF adds a dedicated x-with-macron glyph at U+E000 (Private Use Area), counterpart to U+0233 ȳ. Use `\uE000` in code to render x̄.
- Subscripts ₁₂₃₄ — U+2081–U+2084 (`-r 0x2081-0x2084`)
- Subscript t — ₜ U+209C (`-r 0x209C`); note: subscript capital T does not exist in Unicode — ₜ (lowercase) is the only option. **Not present in the custom TTF** — requires further font editing.
- Superscript T — ᵀ U+1D40 (`-r 0x1D40`). **Not present in the custom TTF** — requires further font editing.
- Superscript ⁻ alone — U+207B **not present in the custom TTF**; use U+E001 (see below) for the combined ⁻¹ glyph instead.
- Superscript ⁻¹ — **resolved via custom PUA glyph U+E001** in `JetBrainsMono-Regular-Custom.ttf`. No single precomposed ⁻¹ codepoint exists in Unicode; the custom TTF adds a dedicated superscript-negative-one glyph at U+E001 (Private Use Area). Use `\uE001` in code wherever ⁻¹ is needed (e.g. cos⁻¹, sin⁻¹, tan⁻¹). Renders as one tight unit at both 20 and 24 px.
- **Already in font — no change needed:** ¹ (U+00B9), ² (U+00B2), ³ (U+00B3), Σ (U+03A3), σ (U+03C3)
- **Source font:** use `JetBrainsMono-Regular-Custom.ttf` (not the stock TTF) for all regeneration — it contains the two PUA glyphs above in addition to all standard JetBrains Mono glyphs.
- Files: `App/Fonts/jetbrains_mono_24.c`, `App/Fonts/jetbrains_mono_20.c`, `CLAUDE.md` gotcha #14 (update `-r` ranges to include new codepoints)
- **Follow-up — known ASCII workaround replacement sites** (scan, replace, flash-verify each):

  | UI location | Item(s) | Current ASCII | Target display | Codepoint(s) | In font? |
  |---|---|---|---|---|---|
  | 2nd+SIN key label | — | `sin^-1(` | `sin⁻¹(` | U+E001 | ✅ |
  | 2nd+COS key label | — | `cos^-1(` | `cos⁻¹(` | U+E001 | ✅ |
  | 2nd+TAN key label | — | `tan^-1(` | `tan⁻¹(` | U+E001 | ✅ |
  | x⁻¹ key label | — | `x^-1` | `x⁻¹` | U+E001 | ✅ |
  | MATH tab | item 3 | `^3` | `³` (cubed) | U+00B3 | ✅ already |
  | MATH tab | item 4 | `3sqrt(` | `³√(` (cube root) | U+00B3 + U+221A | ✅ already |
  | MATH HYP tab | items 4–6 | `asinh(` / `acosh(` / `atanh(` | `sinh⁻¹(` / `cosh⁻¹(` / `tanh⁻¹(` | U+E001 | ✅ |
  | VARS XY tab | item 2 | `x-bar` / `xbar` | `x̄` | U+E000 | ✅ |
  | VARS XY tab | item 4 | `ox` / `sigmax` | `σx` | U+03C3 + ASCII x | ✅ already |
  | VARS XY tab | item 5 | `y-bar` / `ybar` | `ȳ` | U+0233 | ✅ |
  | VARS XY tab | item 7 | `oy` / `sigmay` | `σy` | U+03C3 + ASCII y | ✅ already |
  | VARS Σ tab | items 1–5 | `Ex` / `Ex2` / `Ey` / `Ey2` / `Exy` | `Σx` / `Σx²` / `Σy` / `Σy²` / `Σxy` | U+03A3 + U+00B2 where needed | ✅ already |
  | Y-VARS Y/ON/OFF tabs | items 1–4+ | `Y1` / `Y2` / `Y3` / `Y4` | `Y₁` / `Y₂` / `Y₃` / `Y₄` | U+2081–U+2084 | ✅ |
  | Y= menu equation labels | Y1–Y4 rows | `Y1` / `Y2` / `Y3` / `Y4` | `Y₁` / `Y₂` / `Y₃` / `Y₄` | U+2081–U+2084 | ✅ |

  After all replacements: build, flash to hardware, visually verify each glyph at the relevant font size (menu items use 20px; key labels / Y= labels use 24px).

---

## Menu Specs

These specs describe the intended final state of each menu screen.

### General menu rules
- The menu top bar uses the same font as the items below.
- When a menu scrolls, the top tab bar stays fixed and items scroll into the visible window.
- Normal cursor entry applies in menus (INS toggles overwrite/insert; LEFT/RIGHT move cursor).
- Overflow indicators: ↓ (U+2193) at bottom means list continues below; ↑ (U+2191) at top means list continues above. Both glyphs are in the font and implemented in code (`\xE2\x86\x93` / `\xE2\x86\x91`).

---

### MODE screen
No title text. Screen filled with option rows; arrow keys navigate.

| Row | Options | Wired? |
|-----|---------|--------|
| 1 | Normal \| Sci \| Eng | No — display notation not implemented |
| 2 | Float \| 0 1 2 3 4 5 6 7 8 9 | Yes — `mode_committed[1]`, `Calc_SetDecimalMode()` |
| 3 | Radian \| Degree | Yes — `mode_committed[2]`, `angle_degrees` |
| 4 | Function \| Param | No — parametric graphing not implemented |
| 5 | Connected \| Dot | No — Connected/Dot curve rendering not implemented |
| 6 | Sequential \| Simul | No — simultaneous graphing not implemented |
| 7 | Grid off \| Grid on | Yes — `mode_committed[6]`, `graph_state.grid_on` |
| 8 | Polar \| Seq | No — polar/sequence graphing not implemented |

- LEFT/RIGHT moves selection within a row. UP/DOWN changes active row.
- ENTER commits the highlighted selection; stays in MODE screen.
- Active selections stored in `mode_committed[8]`; wired rows take effect immediately on ENTER.
- MODE key opens the MODE screen from any screen (handled as an early-return check before all mode handlers in `Execute_Token`).

---

### MATH menu
Four tabs. Tab LEFT/RIGHT; item UP/DOWN; ENTER or number key inserts.

**MATH tab:**
```
MATH NUM HYP PRB
1:R>P(
2:P>R(
3:³           (cubed symbol)
4:∛(          (cube root symbol)
5: !
6:°           (degree symbol)
7↓r           (↓ = overflow indicator; list continues)
  8:NDeriv(   (visible after scrolling)
```

**NUM tab:**
```
MATH NUM HYP PRB
1:Round(
2:iPart
3:fPart
4:int(
```

**HYP tab:**
```
MATH NUM HYP PRB
1:sinh(
2:cosh(
3:tanh(
4:asinh(
5:acosh(
6:atanh(
```

**PRB tab:**
```
MATH NUM HYP PRB
1:rand
2: nPr        (spaces before and after are inserted into expression)
3: nCr        (spaces before and after are inserted into expression)
```

---

### ZOOM menu
```
ZOOM
1:Box
2:Zoom In
3:Zoom Out
4:Set Factors
5:Square
6:Standard
7↓Trig        (↓ = overflow indicator)
  8:Integer   (visible after scrolling)
```
Navigation: UP/DOWN cursor; ENTER selects. Number keys 1–8 are direct shortcuts.

**Set Factors sub-screen (implemented):**
```
ZOOM FACTORS
XFact=4
YFact=4
```

---

### RANGE menu
```
RANGE
Xmin=
Xmax=
Xscl=
Ymin=
Ymax=
Yscl=
Xres=
```
Cursor edits value directly after the `=` sign. ENTER/UP/DOWN commit and move between fields.

---

### MATRIX menu (deferred)

**Navigation model — important:** The MATRIX menu does **not** work like MATH or TEST. Selecting an item from the MATRIX tab inserts a function token (e.g. `det(`, `rowSwap(`) into the calling expression and closes the menu. But selecting a matrix from the EDIT tab **never** inserts a `[A]`/`[B]`/`[C]` token anywhere — it always drills down into that matrix's cell editor (`MODE_MATRIX_EDIT`), regardless of what screen was active before the menu was opened. There is no "insert matrix name into previous screen" flow; matrix names reach the expression only via the dedicated MTRX_A/B/C key tokens (2nd layer on keypad).

**MATRIX tab:**
```
MATRIX EDIT
1:RowSwap(
2:Row+(
3:*Row(
4:*Row+(
5:det(
6:T            (transpose superscript)
```

**EDIT tab:**
```
MATRIX EDIT
1:[A] 3×3
2:[B] 3×3
3:[C] 3×3
```
Pressing ENTER on any EDIT item opens the cell editor for that matrix (`MODE_MATRIX_EDIT`). It does not insert `[A]`, `[B]`, or `[C]` into any expression.

---

### TEST menu

No tabs. "TEST" title at top row (yellow), 6 items below.

```
TEST
1:=
2:≠
3:>
4:≥
5:<
6:≤
```

Navigation: UP/DOWN cursor; ENTER selects. Number keys 1–6 are direct shortcuts.
CLEAR or 2nd+MATH exits. Accessible from normal mode (2nd+MATH) and from the Y= editor.
Selected operator is inserted at the cursor position as a UTF-8 string.
All 6 operators are fully evaluated by `calc_engine.c` — return 1 (true) or 0 (false).
and/or/not are not present on the TI-81 and are not planned.


### STAT menu

**CALC tab:**
```
CALC DRAW DATA
1:1-Var
2:LinReg
3:LnReg
4:ExpReg
5:PwrReg
```

**DRAW tab:**
```
CALC DRAW DATA
1:Hist
2:Scatter
3:xyLine
```

**DATA tab:**
```
CALC DRAW DATA
1:Edit
2:ClrStat
3:xSort
4:ySort
```

---

### DRAW menu

**DRAW tab:**
```
DRAW
1:ClrDraw
2:Line(
3:PT-On(
4:PT-Off(
5:PT-Chg(
6:DrawF
7:Shade(
```

---

### VARS menu (VARS key)

Five tabs: XY / Σ / LR / DIM / RNG. Tab LEFT/RIGHT; item UP/DOWN or number key; ENTER inserts the variable name at the cursor.

> **Font note:** x̄ (U+0305 combining overline), ȳ (U+0233), and Σ used as tab label all require font additions before display. Σ (U+03A3) is already in the font; x̄ and ȳ must be added (e.g. `-r 0x0078,0x0233` / combining overline approach or dedicated glyphs).

**XY tab** (statistics summary variables):
```
XY Σ LR DIM RNG
1:n
2:x̄
3:Sx
4:σx
5:ȳ
6:Sy
7:σy    
```

**Σ tab** (summation variables):
```
XY Σ LR DIM RNG
1:Σx
2:Σx²
3:Σy
4:Σy²
5:Σxy
```

**LR tab** (linear regression variables):
```
XY Σ LR DIM RNG
1:a
2:b
3:r
4:RegEQ
```

**DIM tab** (matrix dimension variables):
```
XY Σ LR DIM RNG
1:Arow
2:Acol
3:Brow
4:Bcol
5:Crow
6:Ccol
7:Dim{x}
```

**RNG tab** (window range variables):
```
XY Σ LR DIM RNG
1:Xmin
2:Xmax
3:Xscl
4:Ymin
5:Ymax
6:Yscl
7↓Xres   (↓ = overflow indicator; list continues)
  8:Tmin
  9:Tmax
  0:Tstep
```

---

### Y-VARS menu (2nd+VARS)

Three tabs: Y / ON / OFF. Tab LEFT/RIGHT; item UP/DOWN or number key. Y tab inserts equation reference into expression; ON/OFF tabs turn named equations on or off directly (no insertion).

**Y tab** (equation references):
```
Y ON OFF
1:Y₁
2:Y₂
3:Y₃
4:Y₄
5:X₁t
6:Y₁t
7↓X₂t   (↓ = overflow indicator; list continues)
  8:Y₂t
  9:X₃t
  0:Y₃t
```

**ON tab** (enable equations):
```
Y ON OFF
1:All-On
2:Y₁-On
3:Y₂-On
4:Y₃-On
5:Y₄-On
6:X₁t-On
7↓X₂t-On   (↓ = overflow indicator; list continues)
  8:X₃t-On
```

**OFF tab** (disable equations):
```
Y ON OFF
1:All-Off
2:Y₁-Off
3:Y₂-Off
4:Y₃-Off
5:Y₄-Off
6:X₁t-Off
7↓X₂t-Off   (↓ = overflow indicator; list continues)
  8:X₃t-Off
```

---

### PRGM editor sub-menus

Accessible only while editing a program (PRGM → EDIT → select a slot). A three-tab bar
(CTL / I/O / EXEC) replaces the standard calculator interface. Tab LEFT/RIGHT switches tabs;
UP/DOWN highlights items; ENTER or a number key inserts the selected token at the cursor.
Overflow indicator (↓/↑) overwrites the `:` prefix glyph, same as MATH and ZOOM menus.

**CTL tab:**
```
CTL I/O EXEC
1:Lbl
2:Goto
3:If
4:IS>(
5:DS<(
6:Pause
7↓End
8:Stop
```

**I/O tab:**
```
CTL I/O EXEC
1:Disp
2:Input
3:DispHome
4:DispGraph
5:ClrHome
```

**EXEC tab:**
```
CTL I/O EXEC
1:Prgm1
2:Prgm2
…
```
Lists all 37 program slots with user-assigned names (same `N:PrgmN  USER_NAME` format as the
main PRGM EXEC screen). Selecting a slot inserts a `prgm<name>` subroutine-call token at the
cursor. Overflow indicators (↓/↑) overwrite the `:` prefix glyph when the list scrolls.

---

## Critical Build Settings

### Float printf support
`--specs=nano.specs` disables float in `snprintf` by default. Fixed with:
```cmake
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --specs=nano.specs -u _printf_float")
```
Without `-u _printf_float`, `%f`, `%g`, and `%e` produce empty strings silently.

### Memory regions
```
RAM:     192 KB @ 0x20000000   (~49% used — LVGL heap moved to SDRAM in Session 18)
CCMRAM:   64 KB @ 0x10000000   (59% used: g_prgm_store 19KB + s_prgm_flash_buf 19KB + persist RamFunc code)
FLASH:     2 MB @ 0x08000000   (~36% used)
SDRAM:    64 MB @ 0xD0000000   (external, initialised in main.c; see SDRAM layout below)
```

### SDRAM layout
```
0xD0000000  LCD framebuffer     320×240×2 = 153,600 bytes  (fixed pointer in app_init.c)
0xD0025800  graph_buf           320×240×2 = 153,600 bytes  (fixed pointer in graph.c)
0xD004B000  graph_buf_clean     320×240×2 = 153,600 bytes  (fixed pointer in graph.c — trace cache)
0xD0070800  .sdram section      64 KB = LVGL heap pool     (linker-placed, NOLOAD — work_mem_int)
0xD0080800  free SDRAM          ~63.5 MB remaining
```
`graph_buf` and `graph_buf_clean` are pointers into SDRAM, not static arrays:
```c
static uint16_t * const graph_buf       = (uint16_t *)0xD0025800;
static uint16_t * const graph_buf_clean = (uint16_t *)(0xD0025800 + GRAPH_W * GRAPH_H * 2);  /* = 0xD004B000 */
```

### VSCode extensions
- Use **stm32-cube-clangd** for IntelliSense, NOT Microsoft C/C++ (cpptools)
- Disable cpptools for workspace only: Extensions → C/C++ → gear → Disable (Workspace)

### FreeRTOS stack sizes (do not let CubeMX reset these)
```c
osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 4096 * 2);  // 8192 words
osThreadDef(keypadTask,  StartKeypadTask,  osPriorityNormal, 0, 1024 * 2);  // 2048 words
osThreadDef(calcCore,    StartCalcCoreTask,osPriorityNormal, 0, 1024 * 2);  // 2048 words
#define configTOTAL_HEAP_SIZE ((size_t)65536)
```
CubeMX resets these when regenerating — always check after any `.ioc` changes.

---

## Architecture

### File structure
```
App/Src/                        ← application sources (custom, not CubeMX)
    app_init.c          — RTOS objects, hardware bring-up, LVGL init, render loop, Power_EnterStop
    calculator_core.c   — UI creation, token processing, calculator state
    calc_engine.c       — tokenizer, shunting-yard, RPN evaluator
    expr_util.c         — pure expression-buffer helpers (UTF-8, insert, delete, cursor)
    graph.c             — graph canvas, renderer, axes, curve plotting
    graph_ui.c          — graph screen UI and handlers (extracted module)
    persist.c           — FLASH erase/write/load for calculator state (.RamFunc routines)
    prgm_exec.c         — program execution interpreter + FLASH sector 11 storage (execution functions moved here from ui_prgm.c in Session 15)
    ui_matrix.c         — matrix cell editor UI (extracted module)
    ui_prgm.c           — program menu and editor UI (extracted module)
App/Inc/                        ← application headers (custom, not CubeMX)
    app_init.h          — App_RTOS_Init() and App_DefaultTask_Run() declarations
    app_common.h        — shared types, extern declarations, CalcMode_t enum
    calc_engine.h       — math engine public API
    calc_internal.h     — shared internal state for calculator UI modules
    expr_util.h         — expression buffer utility API
    graph.h             — graphing subsystem public API
    graph_ui.h          — graph screen UI interface
    persist.h           — persistent storage API
    prgm_exec.h         — program execution API (prgm_run_start, prgm_run_loop, prgm_reset_execution_state) + storage API + shared execution types (CtrlFrame_t, CallFrame_t)
    ui_matrix.h         — matrix editor UI interface
    ui_prgm.h           — program menu UI interface
    ui_palette.h        — named colour constants (COLOR_BLACK, COLOR_YELLOW, etc.)
App/Fonts/
    JetBrainsMono-Regular.ttf — source font (Apache 2.0; committed so regeneration is always possible)
    jetbrains_mono_20.c — JetBrains Mono 20px LVGL font (generated)
    jetbrains_mono_24.c — JetBrains Mono 24px LVGL font (generated)
App/HW/Keypad/
    keypad.c/h          — hardware key matrix scanning
    keypad_map.c/h      — Token_t enum, hardware key → token lookup table
App/Display/
    lv_conf.h           — LVGL configuration
    lv_port_disp.c/h    — LVGL display driver (LTDC port layer)
    lv_port_indev.c/h   — LVGL input driver (keypad port layer)
App/Tests/
    CMakeLists.txt      — host test build (4 executables, 422 tests total)
    test_calc_engine.c  — 153 tests: tokenizer, shunting-yard, RPN, matrix
    test_expr_util.c    — 96 tests: UTF-8 cursor, insert/delete, matrix atomicity
    test_persist_roundtrip.c — 52 tests: PersistBlock_t checksum and round-trip
    test_prgm_exec.c    — 121 tests: all 22 command handlers, control flow, subroutine call
    prgm_exec_test_stubs.h — inline stubs for host-compilation of prgm_exec.c
Core/Inc/                       ← CubeMX generated (regenerated from .ioc)
    main.h, stm32f4xx_hal_conf.h, stm32f4xx_it.h, FreeRTOSConfig.h
Core/Src/                       ← CubeMX generated (regenerated from .ioc)
    main.c              — HAL init, SDRAM init, LTDC/LCD setup, task creation
    freertos.c          — FreeRTOS task definitions
    stm32f4xx_it.c, stm32f4xx_hal_msp.c, system_stm32f4xx.c, sysmem.c, syscalls.c
```

### Task architecture
```
KeypadTask   → scans matrix every 20ms → posts Token_t to keypadQueueHandle
CalcCoreTask → blocks on queue → calls Execute_Token()
DefaultTask  → runs lv_task_handler() in a loop (LVGL tick + cursor blink timer)
```

### LVGL thread safety
All LVGL calls must be wrapped:
```c
lvgl_lock();
// lv_* calls here
lvgl_unlock();
```
Defined in `calculator_core.c` using `xLVGL_Mutex`.

**Critical:** Never call `lvgl_lock()` inside `cursor_timer_cb()` — it runs inside
`lv_task_handler()` which DefaultTask already holds the mutex for; a second lock deadlocks.

---

## Input Mode System

`CalcMode_t` in `app_common.h`:
```c
typedef enum {
    MODE_NORMAL,        // standard calculator input
    MODE_2ND,           // 2nd function layer (sticky, resets after one keypress)
    MODE_ALPHA,         // alpha character layer (sticky, resets after one keypress)
    MODE_ALPHA_LOCK,    // alpha locked on (2nd+ALPHA); stays until ALPHA pressed again
    MODE_GRAPH_YEQ,     // Y= equation editor active
    MODE_GRAPH_RANGE,   // RANGE field editor active
    MODE_GRAPH_ZOOM,    // ZOOM preset menu active
    MODE_GRAPH_TRACE,   // trace cursor active on graph
    MODE_GRAPH_ZBOX,    // ZBox rubber-band zoom active
    MODE_MODE_SCREEN,        // MODE settings screen active
    MODE_MATH_MENU,          // MATH/NUM/HYP/PRB menu active
    MODE_GRAPH_ZOOM_FACTORS, // ZOOM FACTORS sub-screen (XFact/YFact editing)
    MODE_TEST_MENU,          // TEST comparison-operator menu active
    MODE_MATRIX_MENU,        // MATRIX menu active (MATRX/EDIT tabs)
    MODE_MATRIX_EDIT,        // MATRIX cell editor active ([A]/[B]/[C])
    MODE_PRGM_MENU,          // PRGM EXEC/EDIT/NEW tab selection
    MODE_PRGM_EDITOR,        // Program line editor
    MODE_PRGM_CTL_MENU,      // PRGM CTL sub-menu (If, For, While…)
    MODE_PRGM_IO_MENU,       // PRGM I/O sub-menu (Disp, Input…)
    MODE_PRGM_RUNNING,       // Program execution in progress
    MODE_PRGM_NEW_NAME,      // Name-entry dialog for new program
} CalcMode_t;
```

`Execute_Token()` in `calculator_core.c` is structured as early-return checks at the top,
followed by mode-specific handlers, then the main `switch(t)`. Handler order:
1. TOKEN_ON ← always fires first; saves state, handles power-down
2. TOKEN_MODE ← always fires second; hides everything, opens MODE screen
3. MODE_GRAPH_YEQ
4. MODE_GRAPH_RANGE
5. MODE_GRAPH_ZOOM
6. MODE_GRAPH_ZOOM_FACTORS
7. MODE_GRAPH_ZBOX
8. MODE_GRAPH_TRACE ← exits trace then **falls through** to main switch
9. MODE_MODE_SCREEN
10. MODE_MATH_MENU
11. MODE_TEST_MENU
12. MODE_MATRIX_MENU
13. MODE_MATRIX_EDIT
14. STO pending check ← fires if `sto_pending`, then falls through
15. Main switch (MODE_NORMAL)

Navigation helpers (all static in `calculator_core.c`):
- `hide_all_screens()` — hides all overlays and graph canvas; call inside `lvgl_lock()`
- `nav_to(target)` — single entry point for all graph screen transitions; acquires lock internally
- `menu_open(token, return_to)` — opens MATH/TEST/MATRIX with correct return mode
- `menu_close(token)` — closes a menu, restores calling screen, returns the restored mode
- `tab_move(tab, cursor, scroll, count, left, update)` — shared tab-switching for MATH and MATRIX

### Modifier key behaviour
- Pressing 2ND or ALPHA a second time cancels the mode (toggle)
- `TOKEN_A_LOCK` (2nd+ALPHA) enters MODE_ALPHA_LOCK — stays active per keypress
- Pressing ALPHA while in MODE_ALPHA_LOCK exits to previous mode
- `TOKEN_STO` sets `sto_pending = true`; next keypress uses the `key.alpha` layer automatically
- Mode state shown entirely through the block cursor — no status bar:
  - Normal: blinking light-grey block, no inner character
  - 2nd: steady amber block, `^` inside
  - ALPHA / A-LOCK: steady green block, `A` inside
  - STO pending: steady green block, `A` inside
- `ui_lbl_modifier` is an invisible LVGL label on the main screen. It holds the current
  modifier text/color so `ui_update_status_bar()` can mirror it to `ui_lbl_yeq_modifier`
  and `ui_lbl_range_modifier` on graph editing screens.

### Cursor implementation
`cursor_update(row_label, char_pos)` in `calculator_core.c`:
- Uses `lv_label_get_letter_pos()` to find pixel X of the insertion point
- Positions `cursor_box` (sized for JetBrains Mono, `lv_obj`) over that point
- Sets `cursor_box` background color and `cursor_inner` label text based on mode / `sto_pending`
- LVGL timer `cursor_timer_cb` fires every `CURSOR_BLINK_MS` (530 ms)

### Insert / Overwrite mode
- Default: overwrite mode (`insert_mode = false`)
- INS key toggles `insert_mode`
- `expr_insert_char` / `expr_insert_str` replace character at cursor in overwrite mode, shift right in insert mode
- Cursor appearance: block for overwrite, underscore for insert (TBD)

### Auto-ANS insertion
When the expression is empty and a binary operator is pressed, `expr_prepend_ans_if_empty()`
prepends `"ANS"` before the operator. Triggers on: `TOKEN_ADD`, `TOKEN_SUB`, `TOKEN_MULT`,
`TOKEN_DIV`, `TOKEN_POWER`, `TOKEN_SQUARE`, `TOKEN_X_INV`.

---

## Calculator UI

No status bar. The full 320×240 display is a scrolling console.

### Display layout
```
DISP_ROW_COUNT = 8    rows
DISP_ROW_H     = 30   px per row   (8 × 30 = 240px — fills screen exactly)
Font: JetBrains Mono 24 (monospaced)
MAX_EXPR_LEN   = 96   chars (~4 wrapped rows)
```

Each history entry occupies two rows:
- Even row: expression — left-aligned, grey (`0x888888`)
- Odd row: result — right-aligned, white (`0xFFFFFF`)

Current expression being typed: one or more rows (wraps), left-aligned, light grey
(`0xCCCCCC`). `expr_chars_per_row` is measured at init from the JetBrains Mono glyph
width; `ui_refresh_display` slices `expression[]` into `cpr`-char segments and renders
each onto its own display row. The cursor is placed on the sub-row containing `cursor_pos`.

### Key display functions
```c
void Update_Calculator_Display(void);   // called after every keypress that changes expression
static void ui_refresh_display(void);   // raw redraw — must be called under lvgl_lock
static void ui_update_history(void);    // called after ENTER commits a result
```

---

## Graphing System

### State
```c
GraphState_t graph_state;   // defined in calculator_core.c, extern in app_common.h

typedef struct {
    char    equations[GRAPH_NUM_EQ][64];
    bool    enabled[GRAPH_NUM_EQ];       // true if equation is plotted
    float   x_min, x_max, y_min, y_max;
    float   x_scl, y_scl;
    float   x_res;   // render step (1 = every pixel column, integer 1–8)
    bool    active;
    bool    grid_on; // true when grid dots enabled (MODE row 7)
} GraphState_t;
```

### Graph screens (all children of lv_scr_act(), all hidden at startup)
- `ui_graph_yeq_screen` — Y= equation editor
- `ui_graph_range_screen` — RANGE value editor
- `ui_graph_zoom_screen` — ZOOM preset menu
- `graph_screen` (in graph.c) — full-height canvas (320×240) + split X=/Y= readout labels

### Navigation key flow
Any of these keys works from any graph screen (free navigation):
```
Y=    → MODE_GRAPH_YEQ,   show ui_graph_yeq_screen
RANGE → MODE_GRAPH_RANGE, show ui_graph_range_screen
ZOOM  → MODE_GRAPH_ZOOM,  show ui_graph_zoom_screen
GRAPH → Graph_SetVisible(true), Graph_Render()
TRACE → MODE_GRAPH_TRACE, Graph_DrawTrace() at midpoint
CLEAR → clears active content (eq/field), or exits to calculator if nothing to clear
```

### Y= editor cursor
`yeq_cursor_pos` (uint8_t in calculator_core.c) tracks insertion point within the selected equation as a **byte offset** (not glyph index). UTF-8 multi-byte sequences (e.g. √ U+221A) are handled by `utf8_char_size()` / `utf8_byte_to_glyph()`.
- LEFT/RIGHT move it; DEL deletes at cursor; characters insert at cursor
- Overwrite mode handles multi-byte chars: replaces all bytes of the current char with the incoming ASCII byte
- Reset to end-of-equation when switching rows (UP/DOWN) or opening Y= from main screen
- MATH key opens the MATH menu; on selection, inserts at `yeq_cursor_pos` and restores Y= screen

### ZOOM menu
Eight options (number keys 1–8 or UP/DOWN + ENTER):
- 1: Box → MODE_GRAPH_ZBOX (rubber-band zoom)
- 2: Zoom In (uses `zoom_x_fact` / `zoom_y_fact`)
- 3: Zoom Out (uses `zoom_x_fact` / `zoom_y_fact`)
- 4: Set Factors → MODE_GRAPH_ZOOM_FACTORS, opens `ui_graph_zoom_factors_screen`
- 5–8: Fixed presets via `apply_zoom_preset()` → renders graph immediately

**ZOOM FACTORS sub-screen** (`ui_graph_zoom_factors_screen`):
- Two rows: `XFact=` and `YFact=` (defaults 4.0)
- State: `zoom_x_fact`, `zoom_y_fact`, `zoom_factors_field` (0/1), `zoom_factors_buf[16]`, `zoom_factors_len`, `zoom_factors_cursor`
- UP/DOWN move between fields; digits and DEL edit value; ENTER commits and exits to ZOOM menu
- Cursor box (`zoom_factors_cursor_box` / `zoom_factors_cursor_inner`) works same as RANGE editor

### RANGE editor
`range_field_selected` (0=Xmin … 6=Xres), `range_field_buf[16]`, `range_field_len`.
Fields commit on UP/DOWN/ENTER. CLEAR clears in-progress edit; if already empty, exits screen.
ZOOM from RANGE navigates to the ZOOM menu (does not reset to ZStandard).

### Renderer
`Graph_Render(bool angle_degrees)` in `graph.c`:
1. Clears canvas to black
2. Draws grid dots if `graph_state.grid_on` (`draw_grid()`) at every (x_scl, y_scl) intersection
3. Draws axes (grey lines at x=0, y=0 if in window)
4. Draws tick marks at x_scl, y_scl intervals
5. Per `x_res` pixel columns: maps to x_math → `Calc_EvaluateAt` → y_px; linearly interpolates between sampled points to fill gaps when `x_res > 1`

### Trace mode
`Graph_DrawTrace()` in `graph.c`:
- memcpy `graph_buf_clean` → `graph_buf` if `graph_clean_valid` (fast path)
- Otherwise calls `Graph_Render` to populate cache
- Draws green crosshair ±5px at cursor; updates `graph_lbl_x` (X=) and `graph_lbl_y` (Y=)
- `graph_clean_valid` invalidated by `Graph_SetVisible(false)`

### ZBox mode
`Graph_DrawZBox()` in `graph.c`:
- Restores clean frame, draws yellow crosshair, white rectangle (once corner1 set)
- ENTER sets first corner or commits zoom; ZOOM cancels and stays on graph; CLEAR exits to calculator

---

## Calculator Engine

### Pipeline
```
string → Tokenize() → infix TokenList_t
       → ShuntingYard() → postfix TokenList_t
       → EvaluateRPN() → CalcResult_t
```

### Public API
```c
CalcResult_t Calc_Evaluate(const char *expr, float ans, bool ans_is_matrix, bool angle_degrees);
CalcResult_t Calc_EvaluateAt(const char *expr, float x_val, float ans, bool angle_degrees);
void         Calc_FormatResult(float value, char *buf, uint8_t buf_len);
```

### Variables
- `ANS` — last result. When the last evaluation produced a matrix, `ans` holds the matrix slot index (3.0f) and `ans_is_matrix = true`; `Tokenize` then emits `MATH_MATRIX_VAL` instead of `MATH_NUMBER` for the `ANS` token. This lets expressions like `det(ANS)` or `[A]+ANS` chain correctly after matrix arithmetic. `ans_is_matrix` is a static local in `calculator_core.c`, passed explicitly to `Calc_Evaluate` — there is no shared global for this state. `Calc_EvaluateAt` (graphing) always passes `false` since Y= equations cannot reference a matrix ANS.
- `x` / `X` — graph variable; `Calc_EvaluateAt` passes x_val; `Calc_Evaluate` uses `calc_variables['X'-'A']`
- `A`–`Z` — user variables in `calc_variables[26]` in `calc_engine.c`; stored via STO→

### Float formatting
`%.6g` unreliable on newlib-nano. Current approach: `%.6f` with manual trailing-zero trim.
Switches to `%.4e` for values ≥1e7 or <1e-4 (non-zero).

### Tokenizer special case
`-` immediately after `^` followed by a digit is folded into a negative number literal to prevent
shunting-yard from treating it as binary subtraction. `-3^2` still evaluates as `-(3^2) = -9`.

---

## Keypad Driver

`App/HW/Keypad/keypad.c`:
- 7×8 matrix: A-lines (cols) driven HIGH one at a time, B-lines (rows) read
- Key ID = `(row * 7) + col`; range 1–55; 0xFF = no key
- `StartKeypadTask` scans every 20ms, fires `Process_Hardware_Key()` on new keypress
- Arrow keys auto-repeat: 400ms initial delay, 80ms repeat rate (KEY_REPEAT_DELAY_TICKS=20, KEY_REPEAT_RATE_TICKS=4)
- Only arrow keys repeat; all other keys fire once on initial press

`Process_Hardware_Key()` in `calculator_core.c`:
- Translates raw key ID → Token_t using `TI81_LookupTable`
- Handles 2ND/ALPHA mode layers and STO before queuing the token
- Sticky 2ND/ALPHA: pressing twice cancels; `return_mode` saves the pre-modifier mode to restore after one keypress

---

## Planned Features (backlog)

In priority order:
1. Matrix math evaluation (det, transpose, row ops) — UI done, engine not wired
2. PRGM — program editor and runner
3. Battery voltage ADC (custom PCB only)
5. Red flashing LED — decide: remove or regularize at 1 Hz heartbeat

---

## PCB Design (paused, resuming after software)

All ICs verified available on JLCPCB:

| IC | Purpose |
|----|---------|
| STM32H7B0VBT6 | Main MCU LQFP100 |
| W25Q128JV | 16MB OctoSPI NOR flash on OCTOSPI1 — firmware XIP + user data (variables, programs, settings); partitioned by sector |
| RT9471 | LiPo charger with power path management — I²C programmable, 3A, USB OTG boost, WQFN-24L 4×4 |
| RT8059 | 3.3V main buck (R_upper=100K, R_lower=22.1K 1%) |
| RT4812 | 5V boost — **DNF Rev1; reserved for Rev2 when Raspberry Pi Zero 2 W is integrated** |
| TPD4E05U06DQAR | USB ESD protection (TI, 4-channel, SOT-563) |

**Power architecture notes:**

Power flows as follows:
```
USB ──► RT9471 (SYS rail) ──► RT8059 (buck) ──► 3.3V system rail
         ▲        │
        BAT ──────┘  (RT9471 power path selects best source automatically)
```

- **RT9471 SYS rail** is a managed output — not a raw battery passthrough. It has a guaranteed minimum of 3.5V (VSYS_MIN) at all times while any power source is available. When USB is connected, SYS is regulated from VBUS and the system runs normally even with a fully depleted battery.
- **RT8059** bucks the SYS rail down to 3.3V. Minimum headroom at worst case (SYS = 3.5V, VOUT = 3.3V) is 200mV — tight but workable. At 300mA load the buck dropout is ~100mV, well within margin.
- **Low-battery threshold:** set the ADC monitor to flag low battery at ~3.6V (battery terminal, not SYS) to ensure graceful shutdown before SYS hits its 3.5V floor. At 3.6V battery the system still has ~200mV headroom on the RT8059.
- **RT8059 feedback resistors:** R_upper = 100kΩ, R_lower = 22.1kΩ 1% → 3.3V output. Optionally bias to 3.28V to gain a few mV of extra dropout margin.
- **RT9471 I²C:** SCL/SDA to STM32 with 10kΩ pull-ups to 3.3V. Default power-on settings (2A charge current, 4.2V charge voltage, 0.5A input current limit) are safe without firmware initialisation, but the STM32 should configure AICR to 1.5A on boot to allow faster charging when a capable adapter is present. CE pin pulled low to enable charging; INT pin to STM32 GPIO for fault notification.
- **RT9471 package:** WQFN-24L 4×4 with exposed thermal pad — thermal pad must be soldered to a solid GND copper pour for heat dissipation. Requires reflow; not hand-solderable.
- **TPD4E05U06DQAR:** placed on USB D+/D− lines as close to the connector as possible, before the RT9471 D+/D− pins. SOT-563 package.
- **RT4812 (DNF Rev1):** footprint placed on board but unpopulated. Reserved for Rev2 when a Raspberry Pi Zero 2 W is added to the design — the Pi requires a 5V supply rail that the RT4812 will provide by boosting from the LiPo.

**External flash notes:**
- Single W25Q128JV (16MB) on OCTOSPI1 serves both firmware XIP and user data storage. 16MB is far more than enough — current firmware is ~684KB; even with generous growth headroom, the top 1–2MB is more than sufficient for user data.
- **Proposed partition layout:**
  - `0x000000 – 0x0FFFFF` (first 1MB): firmware image (XIP, never written at runtime)
  - `0x100000 – 0xFFFFFF` (remaining 15MB): user data — variables, programs, settings
- **Write-freeze gotcha (critical):** W25Q128JV is single-bank NOR. Erasing or writing any sector while OCTOSPI1 is in memory-mapped (XIP) mode stalls the AHB bus and freezes execution. Any erase/write routine that touches the user data partition **must** run from RAM:
  - Declare with `__attribute__((section(".RamFunc")))` or equivalent linker section
  - Switch OCTOSPI1 to indirect mode before erase/write; switch back to memory-mapped mode before returning
  - Keep the routine short — execution is stalled during the switch and any cache misses will fault
- **Alternative approach (simpler):** only write flash on `2nd+ON` power-off gesture (same plan as STM32F429 persistent storage). This limits erase/write operations to once per session and makes the RAM-execution window very short and predictable.
- Flash wear negligible at this scale — effectively unlimited for calculator use
- **VBAT supply:** no coin cell. Route LiPo BAT pin → BAT54 Schottky diode (SOD-323) → STM32 VBAT. The diode prevents back-feed into the RT9471 power path. VBAT draws ~1–5μA; a 1000mAh LiPo powering only VBAT would last decades. RTC survives soft power-off as long as the LiPo is installed. If LiPo is removed the RTC resets — acceptable since no timekeeping feature is planned.
- Battery monitoring: voltage divider R1=100K / R2=82K 1% into STM32 ADC; max ADC voltage at 4.2V LiPo = 1.89V (safe for 3.3V VDDA reference)

---

## Git Workflow

```bash
git add App/Src/calculator_core.c App/Src/app_init.c App/Inc/app_common.h \
        App/Src/graph.c App/Inc/graph.h \
        App/HW/Keypad/keypad.c App/HW/Keypad/keypad.h \
        Core/Inc/FreeRTOSConfig.h CLAUDE.md
git commit -m "description"
git push
```

Commit when a feature works end to end or before starting a risky change.
Do not commit half-finished work that doesn't build.

### .gitignore key exclusions
```
build/
LVGL/
Middlewares/Third_Party/lvgl/tests/
Middlewares/Third_Party/lvgl/docs/
Middlewares/Third_Party/lvgl/examples/
Middlewares/Third_Party/lvgl/demos/
Drivers/STM32F4xx_HAL_Driver/
Drivers/CMSIS/
Middlewares/Third_Party/FreeRTOS/
Middlewares/ST/
```

---

## Common Gotchas

1. **CubeMX regeneration safety** — FreeRTOS config values (heap size, stack overflow level, mutex/semaphore APIs) are protected by `#undef`/`#define` overrides in the `/* USER CODE BEGIN Defines */` section of `FreeRTOSConfig.h` and survive regeneration automatically. The defaultTask stack size (4096 words) is safe as long as the `.ioc` is not modified in CubeMX GUI — it is driven by `FREERTOS.Tasks01` in the `.ioc`. The two hook flags (`configUSE_IDLE_HOOK`, `configUSE_MALLOC_FAILED_HOOK`) must stay set in the `.ioc` since they control generated code in `freertos.c`, not just config values — do not reset them.
2. **nano.specs drops float printf** — always include `-u _printf_float`
3. **LVGL calls outside mutex** — hard faults or display corruption
4. **Never call lvgl_lock() inside cursor_timer_cb** — deadlock (already holds mutex)
5. **CCMRAM is partially used** — `graph_buf` was moved to SDRAM. Some statics now occupy CCMRAM; the full breakdown requires a build map inspection (see item 12 in Next session priorities). Do not assume CCMRAM is fully free.
6. **SDRAM must be initialised before use** — happens in `main.c` before tasks start
7. **White screen after flash** — usually stale binary; power cycle the board
8. **`%.6g` unreliable on ARM newlib-nano** — use `%.6f` with manual trimming
9. **graph.h include guard is `GRAPH_MODULE_H`** — not `GRAPH_H` (conflicts with the height constant `GRAPH_H`)
10. **`2^-3` tokenizer** — `-` after `^` before digit/dot is a negative literal, not subtraction
11. **strncpy does not null-terminate** — always add `buf[n-1] = '\0'` after strncpy
12. **MODE_GRAPH_TRACE falls through** — after exiting trace mode, execution continues into the main switch to process the triggering key normally. This is intentional.
13. **UTF-8 cursor integrity** — `cursor_pos` in the main expression is a byte offset. Any code that moves or edits at `cursor_pos` must account for multi-byte characters (π=2B, √/≠/≥/≤=3B). Stepping by 1 byte can land inside a sequence; LVGL silently skips invalid UTF-8 so the display looks fine but `Tokenize()` returns `CALC_ERR_SYNTAX`. Rules: LEFT steps back past all `10xxxxxx` continuation bytes; RIGHT steps forward past the full sequence; DEL walks back to the start byte and removes all N bytes; overwrite uses `utf8_char_size()` to remove the full char before writing the replacement. The Y= cursor (`yeq_cursor_pos`) was correct already — use it as the reference implementation.
14. **Font regeneration** — the LVGL `.c` font files are generated from `App/Fonts/JetBrainsMono-Regular-Custom.ttf` using `lv_font_conv` (install: `npm i -g lv_font_conv`). **Always use the Custom TTF** — it contains two additional Private Use Area glyphs not present in the stock JetBrains Mono: U+E000 (x̄, xbar) and U+E001 (⁻¹, superscript negative one). Regenerate both sizes with:
    ```bash
    lv_font_conv --font App/Fonts/JetBrainsMono-Regular-Custom.ttf \
      -r 0x20-0x7E \
      -r 0x00B0,0x00B2,0x00B3,0x00B9 \
      -r 0x0233 \
      -r 0x03A3,0x03B8,0x03C0,0x03C3 \
      -r 0x2081-0x2084 \
      -r 0x221A -r 0x25B6 -r 0x2191,0x2193 -r 0x2260,0x2264,0x2265 \
      -r 0xE000,0xE001 \
      --size 24 --format lvgl --bpp 4 -o App/Fonts/jetbrains_mono_24.c --no-compress

    lv_font_conv --font App/Fonts/JetBrainsMono-Regular-Custom.ttf \
      -r 0x20-0x7E \
      -r 0x00B0,0x00B2,0x00B3,0x00B9 \
      -r 0x0233 \
      -r 0x03A3,0x03B8,0x03C0,0x03C3 \
      -r 0x2081-0x2084 \
      -r 0x221A -r 0x25B6 -r 0x2191,0x2193 -r 0x2260,0x2264,0x2265 \
      -r 0xE000,0xE001 \
      --size 20 --format lvgl --bpp 4 -o App/Fonts/jetbrains_mono_20.c --no-compress
    ```
    Current Unicode ranges included: ASCII (0x20–0x7E), °²³¹ (superscripts/degree), ȳ (U+0233), Σθπσ (Greek), ₁₂₃₄ (U+2081–2084), √ ▶ ↑↓ (math/UI), ≠≤≥ (TEST operators), U+E000 x̄ / U+E001 ⁻¹ (custom PUA glyphs).
    **Not in custom TTF (requires further font editing before they can be included):** ᵀ U+1D40 (superscript T), ⁻ U+207B (superscript minus), ₜ U+209C (subscript t).
15. **FLASH sector map — FLASH_SECTOR_7 is NOT 0x080C0000** — On STM32F429ZIT6 (2MB, 12 sectors per bank), the sector layout is: sectors 0–3 = 16 KB, sector 4 = 64 KB, sectors 5–11 = 128 KB. `FLASH_SECTOR_7` is at **0x08060000** (inside the firmware for a ~684 KB image). The persist sector is `FLASH_SECTOR_10` at **0x080C0000**. Never use `FLASH_SECTOR_7` for user data — it will erase firmware code, causing a HardFault loop and a board that fails to boot until reflashed.
16. **Never call lv_timer_handler() from CalcCoreTask while holding xLVGL_Mutex** — `xLVGL_Mutex` is a standard (non-recursive) FreeRTOS mutex. Calling `lv_timer_handler()` inside `lvgl_lock()` from CalcCoreTask will deadlock: LVGL's internal flush handshake waits for `lv_disp_flush_ready()` which only fires when DefaultTask runs — but DefaultTask is blocked on the same mutex. Pattern to show UI feedback before a long operation: `lvgl_lock(); /* create label */; lvgl_unlock(); osDelay(20); /* DefaultTask renders */; /* long operation */`.
17. **ON button EXTI is on EXTI9_5_IRQn** — `EXTI9_5_IRQHandler` is defined in `app_init.c`, not in the CubeMX-generated `stm32f4xx_it.c`. If CubeMX ever regenerates `stm32f4xx_it.c` and adds a duplicate `EXTI9_5_IRQHandler`, there will be a linker error. Keep the handler in `app_init.c` and ensure `stm32f4xx_it.c` does not define it. PE6 is not configured in the `.ioc` — `on_button_init()` sets it up entirely in App code using `KEYPAD_ON_PIN` / `KEYPAD_ON_PORT` from `keypad.h`.
18. **Keypad pin constants live in `keypad.h`, not `main.h`** — `Matrix*_Pin` / `Matrix*_GPIO_Port` macros in the CubeMX-generated `main.h` are now redundant (the `.ioc` still has them until a CubeMX cleanup pass is done, but App code no longer depends on them). All keypad wiring is authoritative in `keypad.h`: `KEYPAD_A1_PORT/PIN` … `KEYPAD_B8_PORT/PIN`, `KEYPAD_ON_PORT/PIN`. Do not add new keypad-pin references to `main.h`.
19. **Power_EnterStop LTDC/SDRAM order** — LTDC must be disabled BEFORE SDRAM enters self-refresh. In RGB interface mode LTDC continuously reads from the SDRAM framebuffer; if SDRAM enters self-refresh while LTDC is still active, LTDC receives bus errors and drives random pixels to the display. Correct order: zero framebuffer → delay 20 ms → disable LTDC → BSP_LCD_DisplayOff → SDRAM self-refresh → HAL_SuspendTick → WFI.
20. **VSCode build button — `cube-cmake` PATH must include build-cmake extension** — The workspace `.vscode/settings.json` sets `cmake.environment.PATH`, which overrides user-level settings. This PATH must include BOTH the core extension binaries (`stm32cube-ide-core-.../resources/binaries/darwin/aarch64`) AND the cmake extension binary (`stm32cube-ide-build-cmake-.../resources/cube-cmake/darwin/aarch64`). If only the core path is present, `cube-cmake` is not found and CMake configure fails. If the `stm32cube-ide-build-cmake` extension is updated, the version number in this path must be updated too. The ARM toolchain path (`gnu-tools-for-stm32/14.3.1+st.2/bin`) must also be present.
