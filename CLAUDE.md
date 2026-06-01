# CLAUDE.md

**Purpose:** AI session continuity and feature backlog. Contains project context, architectural decisions, gotchas, known issues, the active feature/bug backlog (`Next session priorities`), and standing rules for AI-assisted development. Read in full at the start of every session.

## Standards & Maintenance

Read **[docs/MAINTENANCE_STANDARDS.md](docs/MAINTENANCE_STANDARDS.md)** before starting any significant work. It defines the grading criteria for every scorecard dimension (Rises when / Falls when), standing rules that must never regress, and the Numbers to Keep in Sync across files.

**`docs/Datasheets/TI81Guidebook.md` is the behavioral specification for all calculator features.** Before planning or implementing any feature, read the relevant guidebook chapter. When behavior is ambiguous, the guidebook is authoritative. Any deviation from guidebook behavior must be documented in "Deliberate Deviations from Original TI-81" below — an undocumented deviation is a defect, not a design choice.

**Complexity delta rating** — rate neutral / increase / decrease before every commit; if `increase`, add a `[complexity]` item to "Next session priorities".

Use `/update-project` to trigger a full sync. All open work items live in "Next session priorities" below; resolved items and milestone history are in [docs/PROJECT_HISTORY.md](docs/PROJECT_HISTORY.md).

**Before removing any item from "Next session priorities":** add a session log bullet and a Resolved Items row to `docs/PROJECT_HISTORY.md`. This applies even to small items — if the decision is to skip an entry, note that explicitly here rather than silently omitting it.

## Quality Scorecard

Snapshot as of **2026-05-07** (all INTERFACE_REFACTOR_PLAN items complete; all COUPLING_REFACTOR tasks T1–T11 complete; all architecture review Opportunities 1–5 complete: T1-A cmd_table prefix-ordering guard, T1-B CalcMode_t topology comments, T2-A/T2-B/T2-C arch reviews, T3-A PrgmOutput_t callback seam, T3-B Calc_Parse/Calc_Eval split, F1 calc_internal.h narrowed, F2 CalcMode_t topology enforcement, F3 graph render integration test, F4 GraphState_t ownership annotations, Opp-1 expr editor state encapsulated, Opp-2 graph circular include broken, Opp-3 MenuScreen_t generic driver extracted, Opp-4 prgm_exec UI seam completed, Opp-5 ModeRegistration_t named; ARCHITECTURE_OPPORTUNITIES.md items 1–4 complete: Item 1 graph_coord.h deduplication, Item 2 CalcMode_t transition enforcement seam, Item 3 PrgmEditor module extraction, Item 4 ExprEditor module extraction — expr/sto_pending/cursor_visible/cursor_box moved to expr_editor.c, STO synthesis rule consolidated in ExprEditor_CursorUpdate, ExprEditor_* API used throughout). Grading criteria (what causes each dimension to rise or fall) are defined in [docs/MAINTENANCE_STANDARDS.md](docs/MAINTENANCE_STANDARDS.md). When a rating changes: update this table, then add a Milestone Reviews entry to `docs/PROJECT_HISTORY.md`.

| Dimension | Rating |
|---|---|
| Documentation | A- |
| API / header design | A+ |
| Memory safety & FLASH handling | A |
| RTOS integration | A |
| Error handling | A- |
| Naming conventions | B+ |
| Code organisation | B |
| Function complexity | B |
| Magic numbers / constants | A- |
| Testing | A |

Overall: **91–93% production-ready**. Key remaining gaps: PRGM hardware validation pending; code organisation (ui_prgm.c 973 lines, prgm_editor.c 538 lines, graph_ui.c 1403 lines, calculator_core.c 1702 lines, graph.c 1374 lines, graph_ui_range.c 743 lines, ui_matrix.c 579 lines — graph_ui.c/calculator_core.c/graph.c all over 500-line threshold). Key strengths: RTOS integration (A), FLASH/memory-safety (A), API/header design (A+), CI quality gates (-Werror), host test suite (see [docs/TESTING.md](docs/TESTING.md)) — 16 suites, 1112 assertions — with property-based invariant tests, handle_normal_mode coverage, parametric eval tests, stat math tests, MenuState_t navigation tests, cmd_table[] prefix-ordering guard, PrgmOutput_t callback seam (T3-A), Calc_Parse/Eval split (T3-B), CalcMode_t topology enforcement (F2), graph render integration test (F3), STO→matrix/element/Y= host tests, error code string + error_offset tests, CalcMode_IsValidTransition guard tests, and MenuScreen_t navigation tests.

Full scorecard change history: [docs/PROJECT_HISTORY.md — Scorecard Change Log](docs/PROJECT_HISTORY.md).

---

## To-Do Routing

All actionable items go in `Next session priorities` in this file. Use tags to distinguish type.

| Item type | Tag |
|---|---|
| **Feature work** — new calculator behaviour, TI-81 accuracy, UI improvements | (none) |
| **Bug fix** — incorrect behaviour, crashes, display glitches | `[bug]` |
| **Complexity debt** — complexity introduced by a commit | `[complexity]` |
| **Refactoring** — function extraction, dispatch tables, code organisation | `[refactor]` |
| **Testing** — new test coverage, property tests, test infrastructure | `[testing]` |
| **Contributor/open-source docs** — architecture diagrams, guides, onboarding | `[docs]` |
| **Hardware** — physical wiring, validation requiring a board | `[hardware]` |

**Rule of thumb:** if there is work to do, it goes here. `MAINTENANCE_STANDARDS.md` describes standards; this file tracks work.

---

## Feature Completion Status (~95% of original TI-81 guidebook, as of 2026-04-23)

Session log and completed features: [docs/PROJECT_HISTORY.md](docs/PROJECT_HISTORY.md). Full area-by-area breakdown: [README.md](../README.md) Status section.

### Partially implemented (decision-relevant for AI sessions)

| Area | Est. Done | Notes |
|---|---|---|
| MATRIX | ~92% | Variable dimensions 1–6×6 per matrix; scrolling cell editor with dim mode; `det(` and `T` ops in MATRX menu; row operations (RowSwap/Row+/*Row/*Row+) fully implemented in MATRX MATH menu and engine; arithmetic (+, −, ×, scalar×matrix) fully evaluated; `det(ANS)` / `[A]+ANS` chains work; persist across power-off; `[A]`/`[B]`/`[C]` cursor/DEL atomicity fixed; matrix tokens blocked in Y= editor; matrix inversion (`[C]^-1`, Gauss-Jordan), squaring (`[B]^2`), and negation (`-(−)[C]`) implemented; element read `[A](r,c)` as expression operand implemented (guidebook p. 6-10). Pending: hardware validation. |
| PRGM | ~95% | UI (menus, editor, CTL/I/O sub-menus) and executor (`prgm_exec.c`) fully implemented. Supported: `If` (single-line), `Goto/Lbl`, `Disp/Input/ClrHome/Pause/Stop/prgm(subroutine)/STO/IS>(DS</DispHome/DispGraph`. Removed per TI-81 spec: `Then/Else/While/For/Return/Prompt/Output(/Menu(`. Execution model: EXEC inserts `prgmNAME` into expression; ENTER runs and shows `Done`. Remaining: hardware validation (P10). |

---

## Deliberate Deviations from Original TI-81

Behaviours that differ from the original hardware by design:

| Feature | Original TI-81 | This implementation |
|---------|---------------|---------------------|
| Menu vs. expression glyph inconsistency | Menu labels and expression buffer used the same internal token glyphs throughout | **Known inconsistency:** menu labels, Y= row labels, and display-only token→string mappings use proper Unicode glyphs (³, ³√(, sin⁻¹(, Y₁–Y₄ etc.) but the expression buffer retains ASCII insert strings (`^3`, `^(1/3)`, `^-1`). Both paths evaluate correctly; only the display differs. Root cause: the expression buffer has no glyph-substitution layer — full fix requires a token-based renderer. Intentional deviation (Option B), not a regression. |
| `EE` key (`TOKEN_EE`) inserts `*10^` instead of `E` glyph | The TI-81 `EE` key inserts a dedicated scientific-notation `E` glyph (e.g. `1E3`) | `TOKEN_EE` inserts the string `*10^` into the expression buffer (e.g. `1*10^3`). Both evaluate correctly. The deviation exists because the expression buffer has no glyph-substitution layer and the engine already parses `*10^` as the correct RPN sequence. A dedicated `E` token would require a new keyword-table entry and a display-layer substitution; deferred. |
| APD (Automatic Power Down after ~5 min idle) | TI-81 powers off automatically after ~5 minutes of inactivity | Not implemented — no idle timer exists. Omitted intentionally: the prototype uses `Power_DisplayBlankAndMessage()` (a pseudo-off overlay) rather than true Stop mode, making APD less useful. Will revisit when custom PCB migration to `Power_EnterStop()` is complete. |
| Display contrast adjustment (`[2nd][UP/DOWN]`) | 32-level contrast scale controlled by `[2nd][UP]`/`[2nd][DOWN]` | Not implemented. The ILI9341 TFT in RGB interface mode has no backlight PWM or contrast register wired in the current design. There is no hardware path equivalent to the TI-81's contrast mechanism. |
| Last Entry not persisted across power-off | TI-81 retains the Last Entry buffer across power cycles | `PersistBlock_t` has no history-expression field; after a power cycle `[2nd][ENTRY]` buffer is empty. Low-priority omission — the RAM/FLASH cost is small but the benefit is modest for embedded use. |
| Busy indicator (upper-right highlight during calculation/graphing) | Highlighted block in upper-right corner of display while the TI-81 is calculating or graphing | Not implemented — no LVGL object, no flag, no show/hide around `Calc_Evaluate()` or `Graph_Render()`. Cosmetic omission; `Graph_Render()` runs synchronously so the display is simply unresponsive until complete. |

---

## Current Project State

All custom application code lives under `App/`. `Core/` contains only CubeMX-generated files. The `main.c` touch points are `#include "app_init.h"` and `App_RTOS_Init()`. Full session history: [docs/PROJECT_HISTORY.md](docs/PROJECT_HISTORY.md).

### Known issues
- **Display fade on power-off (hardware limitation — prototype substitute implemented)** — The ILI9341 in RGB interface mode has no internal frame buffer. When LTDC stops clocking pixels, the panel's liquid crystal capacitors discharge to their resting state, which the panel renders as white. There is no hardware path to hold the display black after LTDC is halted. **Current prototype behaviour:** `2nd+ON` calls `Power_DisplayBlankAndMessage()` (`app_init.c`) instead of `Power_EnterStop()`. It shows a full-screen black LVGL overlay with a centred "Powered off" label in dim grey (`0x444444`) and blocks the CalcCoreTask on `xQueueReceive` until the ON button is pressed again — no actual Stop mode is entered, no display fade occurs. **Custom PCB migration (one-line change):** in `Execute_Token()` in `calculator_core.c`, in the `TOKEN_ON` / `power_down` branch, replace the `Power_DisplayBlankAndMessage()` call with `Power_EnterStop()`. Both functions are defined in `app_init.c` and declared in `app_init.h`; no other files need to change.

### Next session priorities

Items are ordered so prerequisites come before the items that depend on them; within a dependency tier, easiest first.

#### Complexity debt — surfaced by 2026-05-07 periodic code review

**[complexity] calculator_core.c (1702 lines) carries 4 independent responsibilities** — UI-object creation, expression editing, history management, and mode routing live in one file; begin with migrating the matrix-ring history state and callbacks into `App/Src/calc_history.c` (currently underused). Zero behaviour change. Files: `App/Src/calculator_core.c`, `App/Src/calc_history.c`.

**[complexity] handle_stat_edit (224 lines, depth 6) and handle_stat_menu (85 lines, depth 5) exceed both thresholds** — E17 sweep confirmed both functions in `App/Src/ui_stat_edit.c` and `App/Src/ui_stat.c` exceed 60 lines at nesting depth > 4. `handle_stat_edit` is the priority: extract the display-update block, the data-entry switch arms, and the ENTER/DEL dispatch into static helpers. Files: `App/Src/ui_stat_edit.c`, `App/Src/ui_stat.c`.

**[complexity] Graph_Render (176 lines, depth 6) and Graph_RenderParametric (203 lines, depth 7) exceed both thresholds** — E17 sweep confirmed both in `App/Src/graph.c`. Extract the per-pixel sample-and-plot inner loop, the asymptote/discontinuity detection, and the connected-dot segment draw into static helpers. Files: `App/Src/graph.c`.

**[complexity] handle_vars_menu (65 lines, depth 5) and handle_yvars_menu (73 lines, depth 5) exceed both thresholds** — E17 sweep. Both are dispatch tables wrapped in nested conditional logic; extract the token-dispatch switch body into a static helper that returns the insert string, then call it from a thin top-level function. Files: `App/Src/ui_vars.c`, `App/Src/ui_yvars.c`.

**[complexity] ui_sto.c crossed 500-line threshold (now 523 lines)** — `{x}(n)/{y}(n)` STO state machine added 133 lines in 2026-06-01 session. Consider extracting `StoListPhase_t` state machine into a dedicated helper similar to how `StoMatPhase_t` is structured. File: `App/Src/ui_sto.c`.

**[complexity] calc_engine.c grew to 1988 lines** — Matrix inversion/squaring/negation + element-read tokenizer path added 128 lines in 2026-06-01 session. Long-term: consider splitting matrix-operator eval helpers (`mat_invert`, `mat_negate`, `eval_mat_pow`, `eval_mat_negate`) into a `calc_engine_matrix.c` translation unit. File: `App/Src/calc_engine.c`.

#### Hardware validation — no new code, test on device

Hardware validation checklist: [docs/hardware_checklist.md](docs/hardware_checklist.md).

| Item | Feature | Relevant files |
|---|---|---|
| P28 | cursor_render() refactor | App/Src/calculator_core.c, App/Src/ui_input.c |
| P10 | PRGM execution | [docs/hardware_checklist.md](docs/hardware_checklist.md), App/Src/ui_prgm.c, App/Src/prgm_exec.c |
| — | Free-cursor + TRACE toggle | App/Src/graph_ui_cursor.c, App/Src/graph.c |
| — | `Input` no-arg graph exploration | App/Src/prgm_exec.c, App/Src/graph_ui_cursor.c, App/Src/ui_prgm.c |
| P33h | Connected/Dot mode | App/Src/graph.c, App/Src/ui_mode.c |
| P35h | Parametric graphing | App/Src/graph_ui.c, App/Src/graph.c, App/Src/calculator_core.c |
| P38h | Sequential/Simultaneous | App/Src/graph.c, App/Src/ui_mode.c |
| P40h | Polar coordinate display | App/Src/graph.c, App/Src/ui_mode.c, App/Src/calc_engine.c, App/Src/ui_input.c |
| — | Interactive DRAW cursor-pick | App/Src/graph_ui.c, App/Src/ui_draw.c, App/Src/graph.c |
| P29h | DRAW menu | App/Src/ui_draw.c, App/Src/graph.c, App/Src/calculator_core.c |
| P30h | STAT | App/Src/ui_stat.c, App/Src/calc_stat.c, App/Src/graph.c |
| P31h | VARS menu | App/Src/ui_vars.c, App/Inc/ui_vars.h |
| P32h | Y-VARS menu | App/Src/ui_yvars.c, App/Inc/ui_yvars.h |
| P7 | Ribbon pad wiring | docs/GETTING_STARTED.md (physical access required, indefinite timeline) |

---

## Menu Specs

See **[docs/MENU_SPECS.md](docs/MENU_SPECS.md)** — single source of truth for all menu layouts, navigation rules, and implementation status. Read it before working on any menu UI.

---

## Architecture

See [docs/TECHNICAL.md](docs/TECHNICAL.md) for the full technical reference — directory map, build configuration, keypad driver, input mode system, calculator engine, graphing system, and memory layout. See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for visual task and module diagrams. PCB design notes (paused): [docs/PCB_DESIGN.md](docs/PCB_DESIGN.md). The gotchas below cover traps not obvious from those docs.

---

## Common Gotchas

1. **CubeMX regeneration safety** — FreeRTOS config values (heap size, stack overflow level, mutex/semaphore APIs) are protected by `#undef`/`#define` overrides in the `/* USER CODE BEGIN Defines */` section of `FreeRTOSConfig.h` and survive regeneration automatically. The defaultTask stack size (4096 words) is safe as long as the `.ioc` is not modified in CubeMX GUI — it is driven by `FREERTOS.Tasks01` in the `.ioc`. The two hook flags (`configUSE_IDLE_HOOK`, `configUSE_MALLOC_FAILED_HOOK`) must stay set in the `.ioc` since they control generated code in `freertos.c`, not just config values — do not reset them. Required stack sizes: defaultTask = `4096 * 2` words, keypadTask = `1024 * 2` words, calcCore = `1024 * 2` words, `configTOTAL_HEAP_SIZE = 65536`. Always verify after any `.ioc` change.
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
14. **Font regeneration** — always use `JetBrainsMono-Regular-Custom.ttf` (not the stock TTF) — it contains U+E000 (x̄) and U+E001 (⁻¹) PUA glyphs absent from the stock font. Full commands and codepoint ranges: `docs/TECHNICAL.md` → Font Regeneration section.
15. **FLASH sector map — current layout** — On STM32F429ZIT6 (2MB, 12 sectors per bank), the sector layout is: sectors 0–3 = 16 KB, sector 4 = 64 KB, sectors 5–11 = 128 KB. `FLASH_SECTOR_7` is at **0x08060000**. As of 2026-04-28 the firmware is ~820 KB, which extends ~33 KB into `FLASH_SECTOR_10` (0x080C0000). The persist block is at **`FLASH_SECTOR_11` (0x080E0000)**; program storage (`prgm_exec`) is at **`FLASH_SECTOR_12` (0x08100000, Bank 2)**. The linker FLASH region is capped at 896 KB so any future overflow is a link error. If firmware ever exceeds 896 KB, move persist to another Bank 2 sector and update `PERSIST_SECTOR`/`PERSIST_FLASH_ADDR` in `persist.h` and the linker cap in `STM32F429XX_FLASH.ld`.
16. **Never call lv_timer_handler() from CalcCoreTask while holding xLVGL_Mutex** — `xLVGL_Mutex` is a standard (non-recursive) FreeRTOS mutex. Calling `lv_timer_handler()` inside `lvgl_lock()` from CalcCoreTask will deadlock: LVGL's internal flush handshake waits for `lv_disp_flush_ready()` which only fires when DefaultTask runs — but DefaultTask is blocked on the same mutex. Pattern to show UI feedback before a long operation: `lvgl_lock(); /* create label */; lvgl_unlock(); osDelay(20); /* DefaultTask renders */; /* long operation */`.
17. **ON button EXTI is on EXTI9_5_IRQn** — `EXTI9_5_IRQHandler` is defined in `app_init.c`, not in the CubeMX-generated `stm32f4xx_it.c`. If CubeMX ever regenerates `stm32f4xx_it.c` and adds a duplicate `EXTI9_5_IRQHandler`, there will be a linker error. Keep the handler in `app_init.c` and ensure `stm32f4xx_it.c` does not define it. PE6 is not configured in the `.ioc` — `on_button_init()` sets it up entirely in App code using `KEYPAD_ON_PIN` / `KEYPAD_ON_PORT` from `keypad.h`.
18. **Keypad pin constants live in `keypad.h`, not `main.h`** — `Matrix*_Pin` / `Matrix*_GPIO_Port` macros in the CubeMX-generated `main.h` are now redundant (the `.ioc` still has them until a CubeMX cleanup pass is done, but App code no longer depends on them). All keypad wiring is authoritative in `keypad.h`: `KEYPAD_A1_PORT/PIN` … `KEYPAD_B8_PORT/PIN`, `KEYPAD_ON_PORT/PIN`. Do not add new keypad-pin references to `main.h`.
19. **Power_EnterStop LTDC/SDRAM order** — LTDC must be disabled BEFORE SDRAM enters self-refresh. In RGB interface mode LTDC continuously reads from the SDRAM framebuffer; if SDRAM enters self-refresh while LTDC is still active, LTDC receives bus errors and drives random pixels to the display. Correct order: zero framebuffer → delay 20 ms → disable LTDC → BSP_LCD_DisplayOff → SDRAM self-refresh → HAL_SuspendTick → WFI.
20. **VSCode build button — `cube-cmake` PATH** — `.vscode/settings.json` overrides PATH; must include core extension binaries, build-cmake extension `cube-cmake` binary, and ARM toolchain. If the build-cmake extension is updated, update its version path too. See `docs/GETTING_STARTED.md` Build section.
22. **LVGL heap is 128 KB in SDRAM — exhaustion causes a silent `while(1)` deadlock** — `LV_MEM_SIZE` in `App/Display/lv_conf.h` defines the LVGL heap pool (placed in SDRAM at `0xD0070800` via `LV_ATTRIBUTE_LARGE_RAM_ARRAY`). If it runs out during `StartCalcCoreTask`'s UI init, `LV_ASSERT_HANDLER` fires (`while(1)`) while `xLVGL_Mutex` is held. DefaultTask then blocks on the mutex forever → red LED never blinks; 2nd/ALPHA freeze keypadTask too; display shows light grey (LTDC running, LVGL never renders). Each new `lv_label_create`/`lv_obj_create` call costs ~240 bytes of LVGL heap. After adding significant UI (new screens, expanded menus), verify the build still boots. Increased from 64 KB → 128 KB in April 2026 when parametric Y-VARS + RESET menu pushed ~250 objects over the 64 KB limit. SDRAM has ample headroom; increasing further is safe.

21. **`cursor_render()` — pass `MODE_STO` synthesis for the main expression cursor** — When calling `cursor_render()` from the main expression editor, pass `sto_pending ? MODE_STO : current_mode`, not just `current_mode`. `MODE_STO` is a synthetic value (never set as `current_mode`) that makes the cursor show the green-'A' STO-pending state. Overlay editors (Y=, RANGE, ZOOM FACTORS, matrix, PRGM) pass `current_mode` directly — they can never be in STO state. Copying an overlay-editor call site into the main expression editor without adding the STO synthesis will silently drop STO-pending cursor feedback.
