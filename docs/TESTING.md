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
ctest --test-dir build-tests   # runs all 10 suites (694 assertions total)
```

Or run individual suites:
```bash
./build-tests/test_calc_engine        # Expression evaluation (169 tests)
./build-tests/test_expr_util          # Buffer & cursor logic (96 tests)
./build-tests/test_expr_buffer        # ExprBuffer_t wrapper (48 tests)
./build-tests/test_persist_roundtrip  # Serialization (52 tests)
./build-tests/test_prgm_exec          # PRGM executor (95 tests)
./build-tests/test_normal_mode        # handle_normal_mode dispatch (104 tests)
./build-tests/test_param              # Parametric eval (28 tests)
./build-tests/test_stat               # Statistical calculations (39 tests)
./build-tests/test_yvars              # Y-VARS calc_engine integration (20 tests)
./build-tests/test_menu_state         # MenuState_t navigation helpers (43 tests)
```

### Test Executables

1.  **test_calc_engine**: Validates the shunting-yard algorithm, tokenization, and RPN evaluator. Covers arithmetic, matrices, and math functions.
2.  **test_expr_util**: Validates UTF-8 cursor movement, multi-byte character insertion/deletion, and matrix token atomicity.
3.  **test_expr_buffer**: Validates `ExprBuffer_t` — Clear, Insert (insert/overwrite modes), Delete, Left/Right cursor movement, and overflow guard.
4.  **test_persist_roundtrip**: Validates that state can be serialized to a buffer and restored exactly, including checksum verification.
5.  **test_prgm_exec**: Validates the PRGM executor — `If`, `Goto/Lbl`, `IS>/DS<`, `Input/Disp`, subroutine calls, `Stop`.
6.  **test_normal_mode**: Validates `handle_normal_mode()` and all 8 static sub-handlers — digit/operator/function insert, history navigation, STO, INS/DEL, and mode-dispatch transitions.
7.  **test_param**: Validates `Calc_PrepareParamEquation` and `Calc_EvalParamEquation` — T variable substitution, circle identity, independence from stored variable 'T', degrees mode, error propagation.
8.  **test_stat**: Validates `calc_stat.c` — 1-Var statistics, LinReg (including variable storage and Pearson r), LnReg, ExpReg, SortX, SortY, Clear, and degenerate/empty-input guards.
9.  **test_yvars**: Validates `Calc_RegisterYEquations`, Y₁–Y₄ tokenization, evaluation, and reentrancy guard.
10. **test_menu_state**: Validates `MenuState_t` navigation helpers — `MoveUp/Down` boundary behaviour, `PrevTab/NextTab` reset, `DigitToIndex` mapping, and `AbsoluteIndex`.

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

### PRGM Hardware Test Plan
The PRGM system has a dedicated 50-item test protocol. See [docs/prgm_manual_tests.md](prgm_manual_tests.md).

### CI Quality Gate
The project enforces `-Werror` on all `App/` sources. Pull Requests will not be merged if they introduce compiler warnings.
