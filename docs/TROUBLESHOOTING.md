# Troubleshooting Guide

This guide covers common issues encountered when building, flashing, or running the STM32F429-TI81-Calculator.

## Build Issues

### Floating Point Output is Empty
**Symptoms:** Calculator results are blank or `snprintf` produces empty strings for `%f` or `%g`.
**Cause:** The GNU ARM toolchain (nano.specs) disables floating-point support in `printf`/`scanf` by default to save space.
**Fix:** Ensure `-u _printf_float` is in your linker flags. This is handled automatically by `CMakeLists.txt`, but check the [Technical Reference](TECHNICAL.md#float-printf-support) if you are using a custom build system.
**Runtime Guard:** The firmware includes a startup check that will blink the heartbeat LED at 10 Hz if float support is missing.

### IntelliSense Errors in VS Code
**Symptoms:** Red squiggles on `#include` lines or missing definitions.
**Cause:** Conflict between the Microsoft C/C++ extension and the `stm32-cube-clangd` extension.
**Fix:** 
1. Disable the Microsoft C/C++ extension for this workspace.
2. Ensure `compile_commands.json` is generated (run `cmake` once).
3. Restart the Clangd language server.

## Flashing & Debugging

### White Screen After Flash
**Symptoms:** The display stays white after successful flashing and reset.
**Cause:** The ILI9341 display controller doesn't always perform a full hardware reset when the MCU is reset via SWD.
**Fix:** Unplug and replug the USB cable to power-cycle the board.

### GDB Cannot Connect
**Symptoms:** `Configured debug type 'stlinkgdbtarget' is not supported` or similar errors.
**Cause:** Incorrect debug configuration in `.vscode/launch.json`.
**Fix:** Use the `STM32Cube: STM32 Launch ST-Link GDB Server` configuration provided by the `stm32-cube-clangd` extension.

## Hardware Issues

### Keypad Not Responding
**Symptoms:** No keys work, or keys produce the wrong tokens.
**Cause:** Incorrect wiring or bad ribbon cable connection.
**Fix:** Refer to the [GPIO Wiring Table](GETTING_STARTED.md#gpio-wiring-table) and verify every connection with a multimeter.

---

## Still having trouble?
Please open an issue on GitHub with:
1. Your hardware revision (original TI-81 or custom PCB).
2. The specific error message or a photo of the display.
3. Your development environment (OS, GCC version).
