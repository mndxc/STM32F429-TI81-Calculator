# Open Source Readiness Recommendations

**Last reviewed:** 2026-03-20

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
| Module extraction from `calculator_core.c` | 🟡 Partial (37% smaller; `graph_ui.c` not yet extracted) |
| Automated test suite (`calc_engine.c`) | ❌ Not done |

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

- **`-Werror`:** `-Wall -Wextra` is now in `CMakeLists.txt`. The next step is to resolve the current warning baseline, then add `-Werror` so that CI rejects sloppy C code on PRs. Until then, warnings are visible but non-blocking.
- **`CMAKE_BUILD_TYPE` in CI:** The `build.yml` does not pass `-DCMAKE_BUILD_TYPE=Debug` (or `Release`). CMake defaults to an empty/no-optimisation build. This works but is worth aligning with the `Debug` build that local developers use.
- **Automated tests (P1 from Tracker):** `calc_engine.c` has no LVGL or HAL dependencies. Its three public functions (`Tokenize`, `ShuntingYard`, `EvaluateRPN`) can be compiled and tested on the host with a plain C runner (Unity, CMocka, or a simple `assert`-based harness). This is the single highest-ROI remaining improvement — it would let contributors validate expression parsing without hardware and give CI a meaningful correctness gate.

---

## 3. Hardware Onboarding

**Fully completed.** Both previously missing items are now present in `docs/GETTING_STARTED.md`:

- **Bill of Materials** — lists STM32F429I-DISC1, donor TI-81, ribbon connector, hookup wire, and power source. ✅
- **STM32 GPIO assignments** — the complete 15-pin A-line/B-line/ON mapping is documented in the wiring table and cross-referenced against `keypad.h`. ✅
- **Physical TI-81 ribbon ↔ wire mapping** — ⚠️ **not yet verified.** The wiring table documents which STM32 GPIO each signal uses, but the physical correspondence between numbered pads on the TI-81 PCB and those signal names has not been manually traced and photographed. This is the remaining blocker for independent hardware replication. Annotated photos of the physical wiring will be added when the maintainer has time; contributors who trace their own boards are encouraged to open a PR.

Minor note: the flash command in step 4 shows `st-flash write build/*.bin 0x8000000`. Per `CLAUDE.md`, `st-flash` is not installed on the dev machine; the correct flashing method is OpenOCD. Consider updating this to the OpenOCD command for consistency with the actual toolchain.

---

## 4. Architectural Debt

**Partially addressed.** `calculator_core.c` has been reduced from 5,820 LOC to 3,654 LOC (~37%) via extraction of:
- `App/Src/ui_prgm.c` (1,712 LOC) — PRGM menu, editor, and sub-menus
- `App/Src/ui_matrix.c` (543 LOC) — matrix cell editor UI
- `App/Src/prgm.c` (111 LOC) — PRGM execution helpers

**Remaining work:** The file is still the dominant file in the project. The next extraction target is graph screen handlers (Y=, RANGE, ZOOM, ZBox, Trace — approximately 800 lines) into `App/Src/graph_ui.c`. This would bring `calculator_core.c` closer to 2,900 LOC and make `handle_normal_mode` (~300 lines) the last major oversized handler to address.

See **P2** in [QUALITY_TRACKER.md](QUALITY_TRACKER.md) for the full detail and line counts.
