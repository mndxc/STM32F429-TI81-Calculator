# Open Source Readiness Recommendations

**Last reviewed:** 2026-03-21

Overall, the project is already in an exceptionally good state for documentation compared to most personal projects (the `CLAUDE.md`, `TECHNICAL.md`, and `QUALITY_TRACKER.md` files are fantastic). The open-source scaffolding has been substantially completed since the initial assessment.

---

## Status Summary

| Item | Status |
|---|---|
| `CONTRIBUTING.md` | ✅ Done |
| `CODE_OF_CONDUCT.md` | ✅ Done |
| Issue templates (bug report, feature request) | ✅ Done |
| PR template | ✅ Done |
| README badges (Build, License, PRs Welcome) | ✅ Done |
| CI: GitHub Actions build on push/PR | ✅ Done |
| Wiring table in `GETTING_STARTED.md` | 🟡 Partial — STM32 GPIO side documented; physical TI-81 ribbon ↔ wire mapping not yet verified |
| Bill of Materials | ✅ Done |
| `-Wall -Wextra` compiler warnings | ✅ Done (partial — `-Werror` not yet set) |
| `const` on `TI81_LookupTable` | ✅ Done |
| Module extraction from `calculator_core.c` | 🟡 Partial (37% smaller; `expr_util.c` extracted 2026-03-21; `graph_ui.c` not yet extracted) |
| Automated test suite | ✅ Done — 301 tests across 3 host executables; 80.28% branch coverage on `calc_engine.c`; CI integrated |

---

## 1. Governance & Community Standards

**Fully completed.** All standard open-source scaffolding is now in place:

- **`CONTRIBUTING.md`** — explains PR flow, code style (naming conventions, formatting, documentation expectations), and development guidelines.
- **`CODE_OF_CONDUCT.md`** — standard community conduct document.
- **Issue templates** — `.github/ISSUE_TEMPLATE/bug_report.md` and `feature_request.md` prompt contributors for hardware revision, reproduction steps, and expected vs actual behaviour.
- **PR template** — `.github/pull_request_template.md` in place.
- **README badges** — Build status, License, and "PRs Welcome" badges visible at the top of `README.md`.

No further action needed in this category.

---

## 2. CI/CD & Automated Verification

**Partially completed.** A GitHub Actions workflow (`.github/workflows/build.yml`) now builds the firmware on every push and pull request to `main`. It installs `gcc-arm-none-eabi` + CMake + Ninja, configures via the ARM toolchain file, and runs the build. Contributors cannot merge broken builds.

**Remaining items:**

- **`-Werror`:** `-Wall -Wextra` is in `CMakeLists.txt` and all test targets. Next step is to resolve the current warning baseline and add `-Werror` so CI rejects new warnings on PRs. See P6 in QUALITY_TRACKER.md.
- **`CMAKE_BUILD_TYPE` in CI:** `build.yml` does not pass `-DCMAKE_BUILD_TYPE=Debug`. This works but is worth aligning with the local developer build for consistency.
- **Automated tests (P1 from Tracker — B+ score reached 2026-03-21):** The host test suite is now operational:
  - **`test_calc_engine`** (153 tests / 20 groups) — tokenizer, shunting-yard, RPN evaluator, matrix ops, boundary cases; 80.28% branch coverage on `calc_engine.c` via gcov
  - **`test_expr_util`** (96 tests / 12 groups) — pure expression-buffer functions extracted from `calculator_core.c`: UTF-8 cursor movement, insert/delete/overwrite modes, matrix token atomicity
  - **`test_persist_roundtrip`** (52 tests / 5 groups) — `PersistBlock_t` checksum stability, valid/invalid block detection, field round-trip, struct size/alignment
  - All three executables run on the build host with no embedded dependencies; CI runs them on every push/PR with gcov branch coverage summary
  - **Remaining to reach A rating:** property-based tests (e.g. sin²+cos²=1 for 1000 x values), PRGM backend extraction enabling host testing, signed-off manual test protocol for 28-item PRGM hardware test plan

---

## 3. Hardware Onboarding

**Fully completed.** Both previously missing items are now present in `docs/GETTING_STARTED.md`:

- **Bill of Materials** — lists STM32F429I-DISC1, donor TI-81, ribbon connector, hookup wire, and power source. ✅
- **STM32 GPIO assignments** — the complete 15-pin A-line/B-line/ON mapping is documented in the wiring table and cross-referenced against `keypad.h`. ✅
- **Physical TI-81 ribbon ↔ wire mapping** — ⚠️ **not yet verified.** The wiring table documents which STM32 GPIO each signal uses, but the physical correspondence between numbered pads on the TI-81 PCB and those signal names has not been manually traced and photographed. This is the remaining blocker for independent hardware replication. Annotated photos of the physical wiring will be added when the maintainer has time; contributors who trace their own boards are encouraged to open a PR.

Minor note: the flash command in step 4 shows `st-flash write build/*.bin 0x8000000`. Per `CLAUDE.md`, `st-flash` is not installed on the dev machine; the correct flashing method is OpenOCD. Consider updating this to the OpenOCD command for consistency with the actual toolchain.

---

## 4. Architectural Debt

**Partially addressed.** `calculator_core.c` has been reduced from 5,820 LOC to ~3,650 LOC (~37%) via extraction of:
- `App/Src/ui_prgm.c` (1,712 LOC) — PRGM menu, editor, and sub-menus
- `App/Src/ui_matrix.c` (543 LOC) — matrix cell editor UI
- `App/Src/prgm.c` (111 LOC) — PRGM execution helpers
- `App/Src/expr_util.c` (9 pure functions, 2026-03-21) — UTF-8 cursor, expression buffer manipulation, matrix token detection; zero LVGL/HAL dependencies; host-testable

**Remaining work:** The file is still the dominant file in the project. The next extraction target is graph screen handlers (Y=, RANGE, ZOOM, ZBox, Trace — approximately 800 lines) into `App/Src/graph_ui.c`. This would bring `calculator_core.c` closer to 2,850 LOC and make `handle_normal_mode` (~300 lines) the last major oversized handler to address.

**Extraction pattern (established 2026-03-21):** When extracting a module:
1. Identify functions with no LVGL/HAL circular dependency.
2. Move them to a new `*_util.c` / `*_ui.c` with explicit state parameters.
3. Replace the original static bodies with thin wrapper calls.
4. Write a host-side test suite targeting the new module.
5. Add the new source to both `CMakeLists.txt` (firmware) and `App/Tests/CMakeLists.txt` (tests).

See **P2** in [QUALITY_TRACKER.md](QUALITY_TRACKER.md) for the full detail and line counts.

---

## 5. Onboarding Gaps (identified 2026-03-21)

The following gaps were identified in a quality review pass. None are blocking — the existing documentation is already exceptional — but addressing them would further reduce contributor ramp-up time.

### High priority

**Architecture diagram** — No visual diagram exists showing module dependencies, data flow, or task boundaries. New contributors must infer relationships from include files. A Mermaid diagram in `README.md` or a new `docs/ARCHITECTURE.md` showing the three-task RTOS structure (KeypadTask → CalcCoreTask → DefaultTask), the module hierarchy (`keypad.c` → `calculator_core.c` → `calc_engine.c` / `graph.c` / `persist.c` / `expr_util.c`), and which modules are host-testable vs embedded-only would substantially reduce onboarding time.

**Testing guide** — `CLAUDE.md` documents how to build and run the tests, but not what each suite covers or how to add new tests. A brief `docs/TESTING.md` should explain:
- What each executable tests (calc_engine: expression pipeline; expr_util: cursor/UTF-8/buffer manipulation; persist: serialization/checksum)
- How to add a test (copy a `static void test_*()` function, use `EXPECT_*` macros, call from `main()`)
- Coverage expectations (>80% branch coverage on `calc_engine.c` required; run with `-DCOVERAGE=ON`)
- Link to `TEMP-prgm_manual_tests.md` for the PRGM hardware test protocol

**PRGM completion roadmap** — `ui_prgm.h` warns that the backend is incomplete but doesn't list concrete tasks. A `docs/PRGM_COMPLETION.md` with 5–10 specific tasks (tokenization bridge, `prgm_flatten_to_store`, I/O execution loop, `Goto`/`Lbl` lookup table, `Menu(` support), effort estimates, and acceptance criteria (all 28 manual tests in `TEMP-prgm_manual_tests.md`) would give any PRGM contributor a clear starting point.

### Medium priority

**Expression pipeline documentation** — `calc_engine.h` sketches the pipeline but doesn't show a worked example. A section in `TECHNICAL.md` walking through `"2 + sin(45)"` step by step (token list → postfix → RPN stack trace → result) would significantly reduce the time required to safely modify `calc_engine.c`.

**`st-flash` reference** — `GETTING_STARTED.md` and `OPEN_SOURCE_RECOMMENDATIONS.md` both reference `st-flash`. Per `CLAUDE.md`, `st-flash` is not installed on the dev machine; the correct flashing method is OpenOCD. The flash command in `GETTING_STARTED.md` step 4 should be updated to the OpenOCD command.

**FLASH sector map in onboarding docs** — The sector layout is documented in `persist.h` and `prgm.h` but not in any onboarding document. A short table in `TECHNICAL.md` (Sector 10 = calculator state, 860 B; Sector 11 = program storage, 37 slots) would help contributors avoid the Sector 7 hazard documented in `CLAUDE.md` gotcha #15.

### Low priority

**Troubleshooting section** — Common pitfalls (silent float printf failure, white screen after flash, GDB connection during Stop mode) are documented in `CLAUDE.md` as gotchas but not in `GETTING_STARTED.md` where a first-time builder would look. A `docs/TROUBLESHOOTING.md` or appendix to `GETTING_STARTED.md` covering these would reduce first-contact friction.
