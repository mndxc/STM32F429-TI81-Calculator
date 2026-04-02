# CLAUDE.md

**Purpose:** AI session continuity and feature backlog. Contains project context, architectural decisions, gotchas, known issues, the active feature/bug backlog (`Next session priorities`), and standing rules for AI-assisted development. Read in full at the start of every session.

## Standards & Maintenance

Read **[docs/MAINTENANCE_STANDARDS.md](docs/MAINTENANCE_STANDARDS.md)** before starting any significant work. It defines:
- What to update after each commit (Update Rules and Full Update Checklist)
- Which numbers must stay in sync across files (Numbers to Keep in Sync)
- File structure rules and module naming conventions
- **The grading criteria for every scorecard dimension below** (Rises when / Falls when)
- Standing rules that must never regress regardless of current rating (Do Not Regress)
- **Complexity delta rating** — rate neutral / increase / decrease before every commit; if `increase`, add a `[complexity]` item to "Next session priorities"

Use `/update-project` to trigger a full sync. All open work items live in "Next session priorities" below; resolved items and milestone history are in [docs/PROJECT_HISTORY.md](docs/PROJECT_HISTORY.md).

**Before removing any item from "Next session priorities":** add a session log bullet and a Resolved Items row to `docs/PROJECT_HISTORY.md`. This applies even to small items — if the decision is to skip an entry, note that explicitly here rather than silently omitting it.

## Quality Scorecard

Snapshot as of **2026-04-02**. Grading criteria (what causes each dimension to rise or fall) are defined in [docs/MAINTENANCE_STANDARDS.md](docs/MAINTENANCE_STANDARDS.md). When a rating changes: update this table, then add a Milestone Reviews entry to `docs/PROJECT_HISTORY.md`.

| Dimension | Rating |
|---|---|
| Documentation | A- |
| API / header design | A |
| Memory safety & FLASH handling | A |
| RTOS integration | A |
| Error handling | A- |
| Naming conventions | B+ |
| Code organisation | B |
| Function complexity | B |
| Magic numbers / constants | A- |
| Testing | A |

Overall: **91–93% production-ready**. Key remaining gaps: PRGM hardware validation pending; code organisation (6/10 files over 500-line threshold). Key strengths: RTOS integration (A), FLASH/memory-safety (A), API/header design (A), CI quality gates (-Werror), 516-test host suite with CI including property-based invariant tests and handle_normal_mode coverage.

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

## Feature Completion Status (~72% of original TI-81, as of 2026-03-22)

Session log and completed features: [docs/PROJECT_HISTORY.md](docs/PROJECT_HISTORY.md). Full area-by-area breakdown: [README.md](../README.md) Status section.

### Partially implemented (decision-relevant for AI sessions)

| Area | Est. Done | Notes |
|---|---|---|
| MATRIX | ~95% | Variable dimensions 1–6×6 per matrix; scrolling cell editor with dim mode; all 6 explicit ops + arithmetic (+, −, ×, scalar×matrix) fully evaluated; `det(ANS)` / `[A]+ANS` chains work; persist across power-off; `[A]`/`[B]`/`[C]` cursor/DEL atomicity fixed; matrix tokens blocked in Y= editor |
| PRGM | ~95% | UI (menus, editor, CTL/I/O sub-menus) and executor (`prgm_exec.c`) fully implemented. Supported: `If` (single-line), `Goto/Lbl`, `Disp/Input/ClrHome/Pause/Stop/prgm(subroutine)/STO/IS>(DS</DispHome/DispGraph`. Removed per TI-81 spec: `Then/Else/While/For/Return/Prompt/Output(/Menu(`. Execution model: EXEC inserts `prgmNAME` into expression; ENTER runs and shows `Done`. Remaining: hardware validation (P10). |

---

## Deliberate Deviations from Original TI-81

Behaviours that differ from the original hardware by design:

| Feature | Original TI-81 | This implementation |
|---------|---------------|---------------------|
| Menu vs. expression glyph inconsistency | Menu labels and expression buffer used the same internal token glyphs throughout | **Known inconsistency:** menu labels display proper Unicode glyphs (³, ³√(, sin⁻¹( etc.) but the expression buffer renders the underlying ASCII insert strings (`^3`, `^(1/3)`, `^-1`). Both paths evaluate correctly; only the display differs. Root cause: the expression buffer has no glyph-substitution layer — full fix requires a token-based renderer. Known gap, not a regression. |

---

## Current Project State

All custom application code lives under `App/`. `Core/` contains only CubeMX-generated files. The `main.c` touch points are `#include "app_init.h"` and `App_RTOS_Init()`. Full session history: [docs/PROJECT_HISTORY.md](docs/PROJECT_HISTORY.md).

### Known issues
- **Display fade on power-off (hardware limitation — prototype substitute implemented)** — The ILI9341 in RGB interface mode has no internal frame buffer. When LTDC stops clocking pixels, the panel's liquid crystal capacitors discharge to their resting state, which the panel renders as white. There is no hardware path to hold the display black after LTDC is halted. **Current prototype behaviour:** `2nd+ON` calls `Power_DisplayBlankAndMessage()` (`app_init.c`) instead of `Power_EnterStop()`. It shows a full-screen black LVGL overlay with a centred "Powered off" label in dim grey (`0x444444`) and blocks the CalcCoreTask on `xQueueReceive` until the ON button is pressed again — no actual Stop mode is entered, no display fade occurs. **Custom PCB migration (one-line change):** in `Execute_Token()` in `calculator_core.c`, in the `TOKEN_ON` / `power_down` branch, replace the `Power_DisplayBlankAndMessage()` call with `Power_EnterStop()`. Both functions are defined in `app_init.c` and declared in `app_init.h`; no other files need to change.
- **ZBox arrow key lag** — Screen update rate cannot keep up with held arrow keys during ZBox rubber-band zoom selection. `Graph_DrawZBox()` in `graph.c` redraws from `graph_buf_clean` on every arrow key event; at 80ms repeat rate this may be saturating the LVGL render pipeline. Likely fix: throttle redraws in ZBox mode (skip frames if previous draw not yet flushed), or move crosshair/rectangle rendering to a lightweight overlay rather than full frame restore + redraw each keypress.

### Next session priorities

#### Active

**3. Startup splash image** — Display a bitmap or splash screen on boot before the calculator UI initialises. LVGL supports image objects natively; asset format is RGB565 array in FLASH.

**4. Trace crosshair behaviour differs from original TI-81** — On the original hardware, pressing any non-arrow key while in trace exits trace and processes that key (e.g. GRAPH re-renders, CLEAR exits to calculator). Currently TRACE is a toggle (press again to exit), which is not original behaviour. Additionally, on the original TI-81 there is a free-roaming crosshair cursor visible on the plain graph screen (before pressing TRACE); pressing TRACE snaps the crosshair to the nearest curve. This free-roaming crosshair is not implemented — the graph canvas currently shows no cursor at all until TRACE is pressed. Investigate original behaviour and decide which deviations to correct.
- Files: `App/Src/calculator_core.c` (trace mode handler `TOKEN_TRACE` case, `default` fallthrough behaviour)

**6. ZBox render speed** — See Known Issues entry "ZBox arrow key lag" for root cause and suggested fix (throttle redraws / lightweight overlay).

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


**[hardware] P10 — PRGM hardware validation** — Implementation complete; execute all 50 tests in `docs/prgm_manual_tests.md`. Pre-flight: firmware builds 0 errors, 516 host tests pass, flash and power-cycle. When all 50 tests pass, add a row to `docs/PROJECT_HISTORY.md` Resolved Items and update the MAINTENANCE_STANDARDS.md scorecard if the Testing rating changed. Files: `App/Src/ui_prgm.c`, `App/Src/prgm_exec.c`, `docs/prgm_manual_tests.md`.

**[refactor] P24 — (resolved)** — `try_tokenize_identifier` dispatch table was already in place from a prior session; named-function chain replaced. `try_tokenize_number` sub-parsers also already extracted.


**[refactor] Split `handle_prgm_menu` (194 lines)** — The EXEC/EDIT/ERASE tab handler mixes tab switching, ENTER dispatch, and ERASE confirmation; extract per-tab helpers. Coordinate with existing `[complexity] ui_prgm.c EXEC sub-menu extraction` item before starting — overlapping scope. Zero logic change. Files: `App/Src/ui_prgm.c`.

#### Backlog

**[hardware] P7 — Physical TI-81 ribbon pad ↔ STM32 GPIO wiring table** — STM32 GPIO side complete. Remaining: trace each TI-81 PCB ribbon pad to A-line/B-line with a multimeter on a donor board. Requires physical hardware access; indefinite timeline. Files: `docs/GETTING_STARTED.md`.

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
15. **FLASH sector map — FLASH_SECTOR_7 is NOT 0x080C0000** — On STM32F429ZIT6 (2MB, 12 sectors per bank), the sector layout is: sectors 0–3 = 16 KB, sector 4 = 64 KB, sectors 5–11 = 128 KB. `FLASH_SECTOR_7` is at **0x08060000** (inside the firmware for a ~684 KB image). The persist sector is `FLASH_SECTOR_10` at **0x080C0000**. Never use `FLASH_SECTOR_7` for user data — it will erase firmware code, causing a HardFault loop and a board that fails to boot until reflashed.
16. **Never call lv_timer_handler() from CalcCoreTask while holding xLVGL_Mutex** — `xLVGL_Mutex` is a standard (non-recursive) FreeRTOS mutex. Calling `lv_timer_handler()` inside `lvgl_lock()` from CalcCoreTask will deadlock: LVGL's internal flush handshake waits for `lv_disp_flush_ready()` which only fires when DefaultTask runs — but DefaultTask is blocked on the same mutex. Pattern to show UI feedback before a long operation: `lvgl_lock(); /* create label */; lvgl_unlock(); osDelay(20); /* DefaultTask renders */; /* long operation */`.
17. **ON button EXTI is on EXTI9_5_IRQn** — `EXTI9_5_IRQHandler` is defined in `app_init.c`, not in the CubeMX-generated `stm32f4xx_it.c`. If CubeMX ever regenerates `stm32f4xx_it.c` and adds a duplicate `EXTI9_5_IRQHandler`, there will be a linker error. Keep the handler in `app_init.c` and ensure `stm32f4xx_it.c` does not define it. PE6 is not configured in the `.ioc` — `on_button_init()` sets it up entirely in App code using `KEYPAD_ON_PIN` / `KEYPAD_ON_PORT` from `keypad.h`.
18. **Keypad pin constants live in `keypad.h`, not `main.h`** — `Matrix*_Pin` / `Matrix*_GPIO_Port` macros in the CubeMX-generated `main.h` are now redundant (the `.ioc` still has them until a CubeMX cleanup pass is done, but App code no longer depends on them). All keypad wiring is authoritative in `keypad.h`: `KEYPAD_A1_PORT/PIN` … `KEYPAD_B8_PORT/PIN`, `KEYPAD_ON_PORT/PIN`. Do not add new keypad-pin references to `main.h`.
19. **Power_EnterStop LTDC/SDRAM order** — LTDC must be disabled BEFORE SDRAM enters self-refresh. In RGB interface mode LTDC continuously reads from the SDRAM framebuffer; if SDRAM enters self-refresh while LTDC is still active, LTDC receives bus errors and drives random pixels to the display. Correct order: zero framebuffer → delay 20 ms → disable LTDC → BSP_LCD_DisplayOff → SDRAM self-refresh → HAL_SuspendTick → WFI.
20. **VSCode build button — `cube-cmake` PATH** — `.vscode/settings.json` overrides PATH; must include core extension binaries, build-cmake extension `cube-cmake` binary, and ARM toolchain. If the build-cmake extension is updated, update its version path too. See `docs/GETTING_STARTED.md` Build section.
21. **`cursor_render()` — pass `MODE_STO` synthesis for the main expression cursor** — When calling `cursor_render()` from the main expression editor, pass `sto_pending ? MODE_STO : current_mode`, not just `current_mode`. `MODE_STO` is a synthetic value (never set as `current_mode`) that makes the cursor show the green-'A' STO-pending state. Overlay editors (Y=, RANGE, ZOOM FACTORS, matrix, PRGM) pass `current_mode` directly — they can never be in STO state. Copying an overlay-editor call site into the main expression editor without adding the STO synthesis will silently drop STO-pending cursor feedback.
