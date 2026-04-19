# Neo-81 Roadmap to Completion
_Generated 2026-04-18. Excludes custom PCB and new display work._

---

## Current state: ~72% TI-81 feature parity

---

## Phase 1 — Clear the validation backlog
_No new code. Implementation is done; these just need to run on hardware._

| Item | Test doc | Tests |
|---|---|---|
| P10 — PRGM | `docs/prgm_manual_tests.md` | 50 |
| P28 — cursor_render | `docs/p28_cursor_manual_tests.md` | 29 |
| P29h — DRAW menu | (inline in CLAUDE.md) | 8 |
| P30h — STAT | (inline in CLAUDE.md) | 9 |
| P31h — VARS | (inline in CLAUDE.md) | 6 |
| P32h — Y-VARS | (inline in CLAUDE.md) | 6 |
| P35h — Parametric graphing | (inline in CLAUDE.md) | 6 |

Do this before adding any new features. Bugs found here may require code changes.

---

## Phase 2 — Small correctness gaps
_Independent of each other; any order._

- **Startup splash screen** (CLAUDE.md Item 3)
  LVGL image object, RGB565 array in FLASH. A few hours.

- **Trace crosshair** (CLAUDE.md Item 4)
  Two fixes: (a) free-roaming crosshair on graph canvas before TRACE is pressed;
  (b) pressing any non-arrow key exits trace and processes that key normally.
  Files: `App/Src/calculator_core.c` (TOKEN_TRACE case, default fallthrough).

- **Sci / Eng notation** (MODE row 1)
  Wire `mode_committed[0]` into the number formatter in `calc_engine.c`.
  No new subsystems. Medium effort.

- **Connected / Dot** (MODE row 5)
  Toggle in `graph.c` that skips `lv_line` calls between plotted points. Low effort.

---

## Phase 3 — Simultaneous graphing (MODE row 6)
Sequential/Simul toggle. Currently `graph.c` renders each Y equation to completion
before starting the next. Simul mode interleaves them column by column.
Medium effort, isolated to `graph.c`.

---

## Phase 4 — Polar graphing mode (MODE row 8, first half)
New graphing subsystem: `r = f(θ)`, converted to `(x,y)` via `r·cos(θ)` / `r·sin(θ)`.

Required pieces:
- New Y= layout: `r₁`–`r₄` equations
- New RANGE fields: `θmin`, `θmax`, `θstep`
- Renderer loop in `graph.c`
- TRACE readout: `θ=` / `r=` / `X=` / `Y=`
- Persist version bump → adopt PersistBlock_t sub-struct design at this point
  (design documented in `docs/TECHNICAL.md` "Persist Migration Design")

Largest remaining single feature (~8–10% of original TI-81).

---

## Phase 5 — Sequence graphing mode (MODE row 8, second half)
`u(n)`, `v(n)`, `w(n)` — recursive sequences, web plots, cobweb diagrams.

Fundamentally different evaluation model: memoised recursion, not expression
evaluation over a continuous domain. All new:
- Y= screen with `u(n)` / `v(n)` / `w(n)` slots
- RANGE fields: `nMin`, `nMax`, `u(nMin)`, `v(nMin)`, etc.
- Renderer and TRACE readout

Roughly equivalent effort to parametric mode. Last major subsystem for 100% parity.

---

## Phase 6 — Code organisation debt
_Once features stabilise._

- Extract stat renderers from `graph.c` → `graph_stat.c` (P29 remaining debt)
- Split oversize files below 500-line threshold:
  - `calculator_core.c` — 1467 lines
  - `graph_ui.c` — 874 lines
  - `ui_prgm.c` — 1277 lines
  - `graph.c` — 978 lines
  - `graph_ui_range.c` — 743 lines
  - `ui_stat.c` — 703 lines

---

## Summary

| Phase | What | Effort | Fidelity gain |
|---|---|---|---|
| 1 | Hardware validation (7 items) | Low (board time) | Confidence / bug finds |
| 2 | Splash, trace fix, Sci/Eng, Dot | Low–Medium | ~4–5% |
| 3 | Simultaneous graphing | Medium | ~2% |
| 4 | Polar graphing | Large | ~8–10% |
| 5 | Sequence graphing | Large | ~8–10% |
| 6 | Code organisation | Medium | Quality, not features |

Phases 1–3 → ~80–82% parity.
Phase 4 → ~90%.
Phase 5 → ~100% TI-81 feature parity.
