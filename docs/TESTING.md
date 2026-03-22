# Contributor Testing Guide

The STM32F429-TI81-Calculator uses a dual-track testing strategy: host-compiled unit tests for core logic and manual hardware validation for the UI and PRGM backend.

## Host-Compiled Unit Tests

The most robust part of the test suite runs on your development machine (Linux/macOS/Windows). It requires only `cmake` and a C compiler (gcc/clang).

### Running Tests

```bash
cmake -S App/Tests -B build/tests
cmake --build build/tests
./build/tests/test_calc_engine   # Expression evaluation tests (153 tests)
./build/tests/test_expr_util     # Buffer & cursor logic tests (96 tests)
./build/tests/test_persist_roundtrip # Serialization tests (52 tests)
```

### Test Executables

1.  **test_calc_engine**: Validates the shunting-yard algorithm, tokenization, and RPN evaluator. Covers arithmetic, matrices, and math functions.
2.  **test_expr_util**: Validates UTF-8 cursor movement, multi-byte character insertion/deletion, and matrix token atomicity.
3.  **test_persist_roundtrip**: Validates that state can be serialized to a buffer and restored exactly, including checksum verification.

### Adding a New Test

1.  Open the relevant `App/Tests/test_*.c` file.
2.  Create a new `static void test_your_feature(void)` function.
3.  Use the `EXPECT_TRUE`, `EXPECT_EQ`, `EXPECT_STR_EQ` macros.
4.  Register your test in the `main()` function or the appropriate test group.

### Code Coverage

To check coverage on the host:
```bash
cmake -S App/Tests -B build/tests -DCOVERAGE=ON
cmake --build build/tests
./build/tests/test_calc_engine
# View results with gcov or lcov
```
Target: **>80% branch coverage** for any new logic in `calc_engine.c`.

---

## Hardware Validation

Since the UI and hardware peripheral interactions (FLASH, LCD, Keypad) cannot be easily mocked on the host, they are validated manually.

### PRGM Hardware Test Plan
The PRGM system has a dedicated 28-item test protocol. See [docs/prgm_manual_tests.md](prgm_manual_tests.md).

### CI Quality Gate
The project enforces `-Werror` on all `App/` sources. Pull Requests will not be merged if they introduce compiler warnings.
