# Contributor Testing Guide

> **Canonical source for test counts.** This file is the single source of truth for suite names, per-suite assertion counts, and totals. All other docs link here rather than repeating numbers.

The STM32F429-TI81-Calculator uses a dual-track testing strategy: host-compiled unit tests for core logic and manual hardware validation for the UI and PRGM backend.

## Host-Compiled Unit Tests

The most robust part of the test suite runs on your development machine (Linux/macOS/Windows). It requires only `cmake` and a C compiler (gcc/clang).

### Running Tests

Run these commands from the **repo root** (the directory containing `CMakeLists.txt`):

```bash
cmake -S App/Tests -B build-tests
cmake --build build-tests
ctest --test-dir build-tests   # runs all 16 suites (1112 assertions total)
```

Or run individual suites:
```bash
./build-tests/test_calc_engine        # Expression evaluation (265 tests)
./build-tests/test_expr_util          # Buffer & cursor logic (96 tests)
./build-tests/test_expr_buffer        # ExprBuffer_t wrapper (48 tests)
./build-tests/test_persist_roundtrip  # Serialization (52 tests)
./build-tests/test_prgm_exec          # PRGM executor (119 tests)
./build-tests/test_normal_mode        # handle_normal_mode dispatch (137 tests)
./build-tests/test_param              # Parametric eval (28 tests)
./build-tests/test_stat               # Statistical calculations (76 tests)
./build-tests/test_yvars              # Y-VARS calc_engine integration (20 tests)
./build-tests/test_menu_state         # MenuState_t navigation helpers (43 tests)
./build-tests/test_prgm_cmd_table     # PRGM cmd_table prefix-ordering guard (1 test)
./build-tests/test_parse_eval         # Calc_Parse + Calc_Eval unified API — T3-B (103 tests)
./build-tests/test_mode_topology      # CalcMode_t routing topology — F2 (1 test)
./build-tests/test_graph_render       # Graph render integration — F3 (21 tests)
./build-tests/test_error_codes        # Error string format + error_offset propagation (25 tests)
./build-tests/test_ui_menu_screen     # MenuScreen_t navigation: HandleToken, SetTab, IsMenuOpeningKey, DefaultExtra (69 tests)
```

### Test Executables

1.  **test_calc_engine**: Validates the shunting-yard algorithm, tokenization, and RPN evaluator. Covers arithmetic, matrices, and math functions.
2.  **test_expr_util**: Validates UTF-8 cursor movement, multi-byte character insertion/deletion, and matrix token atomicity.
3.  **test_expr_buffer**: Validates `ExprBuffer_t` — Clear, Insert (insert/overwrite modes), Delete, Left/Right cursor movement, and overflow guard.
4.  **test_persist_roundtrip**: Validates that state can be serialized to a buffer and restored exactly, including checksum verification.
5.  **test_prgm_exec**: Validates the PRGM executor — `If`, `Goto/Lbl`, `IS>/DS<`, `Input/Disp`, subroutine calls, `Stop`.
6.  **test_normal_mode**: Validates `handle_normal_mode()` and all static sub-handlers — digit/operator/function insert, history navigation, STO (including STO→matrix, STO→matrix-element, STO→Y= slot), INS/DEL, mode-dispatch transitions, and menu-to-menu return chain (Follow-up #1).
7.  **test_param**: Validates `Calc_PrepareParamEquation` and `Calc_EvalParamEquation` — T variable substitution, circle identity, independence from stored variable 'T', degrees mode, error propagation.
8.  **test_stat**: Validates `calc_stat.c` — 1-Var statistics, LinReg (including variable storage and Pearson r), LnReg, ExpReg, SortX, SortY, Clear, and degenerate/empty-input guards.
9.  **test_yvars**: Validates `Calc_RegisterYEquations`, Y₁–Y₄ tokenization, evaluation, and reentrancy guard.
10. **test_menu_state**: Validates `MenuState_t` navigation helpers — `MoveUp/Down` boundary behaviour, `PrevTab/NextTab` reset, `DigitToIndex` mapping, and `AbsoluteIndex`.
11. **test_prgm_cmd_table**: Validates `prgm_cmd_table_validate()` — cmd_table[] prefix-ordering invariant and command dispatch verification.
12. **test_parse_eval**: Validates `Calc_Parse` + `Calc_Eval` unified API (T3-B) — equivalence to `Calc_Evaluate`, `ParsedExpr_t` reuse, parametric T substitution, nDeriv nested field population, two-`ParsedExpr_t` nDeriv independence (no shared static cross-contamination), parse-error propagation, and equivalence to `Calc_PrepareGraphEquation`.
13. **test_mode_topology**: Validates `CalcMode_t` routing topology (F2) — every mode value appears in exactly one of the route table or the known-special-cases list; no mode is silently unhandled.
14. **test_graph_render**: End-to-end integration test for the graph render pipeline (F3). Compiles `graph.c` and `graph_draw.c` under HOST_TEST (SDRAM buffers redirected to in-memory arrays); asserts correct RGB565 pixel values at computed canvas coordinates for constant equations, validates cache invalidation on equation change, and confirms disabled/malformed equations draw nothing. Also verifies all four `graph_coord_*` transforms against known boundary values.
15. **test_error_codes**: Validates `Calc_GetErrorString()` produces correct TI-81-format error strings for all `CalcError_t` values; validates `error_offset` propagation through `Calc_Evaluate`, `Calc_Parse`, and `Calc_Eval` for errors with cursor-positioning requirements.
16. **test_ui_menu_screen**: Validates `MenuScreen_t` navigation logic — digit shortcuts, UP/DOWN boundary behaviour with scroll, LEFT/RIGHT within-menu tab switching (wrap and no-wrap), sibling-mode tab switching via `on_tab_switch`, CLEAR callback, unknown-token fall-through, `on_extra` precedence, `MenuScreen_DefaultExtra` close-and-pass-through, and `MenuScreen_IsMenuOpeningKey`.

### Adding a New Test

1.  Open the relevant `App/Tests/test_*.c` file.
2.  Create a new `static void test_your_feature(void)` function.
3.  Use the `EXPECT_TRUE`, `EXPECT_EQ`, `EXPECT_STR_EQ` macros.
4.  Register your test in the `main()` function or the appropriate test group.

### Code Coverage

To check coverage on the host (run from repo root):
```bash
cmake -S App/Tests -B build-tests -DCOVERAGE=ON
cmake --build build-tests
./build-tests/test_calc_engine
# View results with gcov or lcov
```
Target: **>80% branch coverage** for any new logic in `calc_engine.c`.

---

## Hardware Validation

Since the UI and hardware peripheral interactions (FLASH, LCD, Keypad) cannot be easily mocked on the host, they are validated manually.

### Hardware Test Checklist
All hardware validation (normal mode, MODE menu, VARS/Y-VARS, MATRIX, graphing, DRAW, STAT, PRGM) is in [docs/hardware_checklist.md](hardware_checklist.md).

### CI Quality Gate
The project enforces `-Werror` on all `App/` sources. Pull Requests will not be merged if they introduce compiler warnings.
