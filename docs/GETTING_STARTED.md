# 🚀 Getting Started with Neo-81

Welcome! This guide is designed to take you from "zero knowledge" to a working build. The **Neo-81** project brings new life to vintage TI-81 hardware by integrating a modern STM32-based brain and a high-resolution display using the LVGL library.

---

## 1. Prerequisites (The "Zero Knowledge" Phase)
Before we start, it helps to understand the components we are using. If any of these terms are new to you, check out the provided resources:

* **The Microcontroller (STM32F4-Discovery):** This is the "brain" of the project. It handles all the logic and calculations.
    * [Learn: What is a Microcontroller?](https://www.youtube.com/watch?v=7uVadQd5XHo)
* **The Graphics Library (LVGL):** This is the software engine used to draw the buttons, menus, and graphs on the new screen.
    * [Learn: Introduction to LVGL](https://docs.lvgl.io/master/intro/index.html)
* **The Language (C):** The firmware is written in C to ensure it runs fast and efficiently on the hardware.
    * [Learn: C Programming in 100 Seconds](https://www.youtube.com/watch?v=U3aXWizVCQ4)

---

## 2. Hardware Setup
The core of the Neo-81 is the interface between the original keypad and your new processor.

### Keypad Matrix Wiring
The TI-81 keypad works on a **Matrix**—a grid of rows and columns. Instead of one wire per button, we use a grid to scan for keypresses. 
* [Deep Dive: How Keypad Matrices Work](https://pcbiot.com/how-keypad-matrix-works/)

**Wiring Table (TI-81 Ribbon to STM32):**

> ### ⚠️ IMPORTANT — Physical Ribbon Mapping Not Yet Verified
>
> The table below shows the **STM32 GPIO assignments** used by the firmware (authoritative source: `App/HW/Keypad/keypad.h`). These are correct for the software.
>
> **What is missing:** The physical correspondence between the numbered pads/wires on your specific TI-81 PCB and the logical A-line/B-line names used here has **not yet been manually documented.** The original TI-81 ribbon connector pinout varies subtly between hardware revisions, and the exact wire-to-function mapping requires tracing with a multimeter on a real board.
>
> **This section will be updated with annotated photos** showing which physical ribbon wire connects to which STM32 GPIO header pin. Until then, use a multimeter in continuity mode to trace each ribbon wire to its key matrix row/column, then match it to the table below.
>
> If you have completed this mapping on your own hardware, please open an issue or PR with your findings — this is one of the most valuable contributions you can make to the project.

| Firmware Name | STM32 Pin | Function | Physical Ribbon Pin |
| :--- | :--- | :--- | :--- |
| **A1** | PE5 | Column 1 | *(to be verified — see note above)* |
| **A2** | PE4 | Column 2 | *(to be verified)* |
| **A3** | PE3 | Column 3 | *(to be verified)* |
| **A4** | PE2 | Column 4 | *(to be verified)* |
| **A5** | PB7 | Column 5 | *(to be verified)* |
| **A6** | PB4 | Column 6 | *(to be verified)* |
| **A7** | PB3 | Column 7 | *(to be verified)* |
| **B1** | PG9 | Row 1 | *(to be verified)* |
| **B2** | PD7 | Row 2 | *(to be verified)* |
| **B3** | PC11 | Row 3 | *(to be verified)* |
| **B4** | PC8 | Row 4 | *(to be verified)* |
| **B5** | PC3 | Row 5 | *(to be verified)* |
| **B6** | PA5 | Row 6 | *(to be verified)* |
| **B7** | PG2 | Row 7 | *(to be verified)* |
| **B8** | PG3 | Row 8 | *(to be verified)* |
| **ON** | PE6 | ON/Interrupt | *(to be verified)* |

---

## 3. Bill of Materials (BOM)

To build a Neo-81, you will need the following components:

| Component | Description | Source |
| :--- | :--- | :--- |
| **STM32F429I-DISC1** | Discovery kit with STM32F429ZI MCU and 2.4" LCD. | [ST Store](https://www.st.com/en/evaluation-tools/32f429idiscovery.html) |
| **TI-81 Calculator** | Donor for shell, keypad, and ribbon connector. | Second-hand (eBay/etc) |
| **Ribbon Connector** | 15-pin 1.25mm or 1.0mm pitch FFC/FPC connector (salvaged or new). | Salvage or Mouser/DigiKey |
| **Hookup Wire** | 28-30 AWG solid or stranded wire for matrix connections. | Any electronics supplier |
| **Power Source** | Micro-USB cable (for development) or 4xAAA batteries (original footprint). | Original shell |

---

## 4. Software Toolchain Setup
To compile the code and "flash" it onto the STM32, you need the right tools on your computer.

1.  **Install the ARM GCC toolchain:** `gcc-arm-none-eabi` 14.x or later.
    * macOS: `brew install --cask gcc-arm-embedded`
    * Linux: `sudo apt install gcc-arm-none-eabi`
2.  **Install CMake** (3.22 or later) and **Ninja** (optional but faster):
    * macOS: `brew install cmake ninja`
    * Linux: `sudo apt install cmake ninja-build`
3.  **Install OpenOCD** to flash the board:
    * macOS: `brew install open-ocd`
    * Linux: `sudo apt install openocd`
4.  **Install VSCode** and the **stm32-cube-clangd** extension for IntelliSense.

---

## 5. CubeMX Setup (First Time Only)

The STM32 HAL, CMSIS, and FreeRTOS vendor sources are not stored in the repository. You must generate them once with STM32CubeMX before your first build. If you cloned the repo and already have these directories populated, skip this section.

### Steps in CubeMX

1. **New Project → Board Selector → `STM32F429I-DISC1`** → click "Initialize All Peripherals with Default Mode"
2. **Middleware → FreeRTOS → CMSIS V1** — enable it. This is the only non-default peripheral to add.
   - In FreeRTOS settings also enable: `USE_IDLE_HOOK`, `USE_MALLOC_FAILED_HOOK` (these control generated stubs in `freertos.c` and must be set in the `.ioc` — they cannot be overridden from user code safely)
3. **Project Manager** → set Toolchain to **CMake**, set project name
4. **Generate Code** — this populates `Drivers/STM32F4xx_HAL_Driver/`, `Drivers/CMSIS/`, and `Middlewares/Third_Party/FreeRTOS/`

### Steps after generating

1. Copy the `App/` folder from the repo into the generated project root
2. In `CMakeLists.txt`, add the App sources (copy the `target_sources` and `target_include_directories` blocks from the repo's `CMakeLists.txt`)
3. Add `-u _printf_float` to `CMAKE_EXE_LINKER_FLAGS` in `CMakeLists.txt` (required for `%f`/`%g` in `snprintf` with `--specs=nano.specs`)
4. Paste the FreeRTOS USER CODE overrides into `Core/Inc/FreeRTOSConfig.h` — already present if you copied from the repo; see `/* USER CODE BEGIN Defines */`

> **After any future CubeMX regeneration**, re-apply the manual changes documented in `docs/TECHNICAL.md` "Build Configuration" — CubeMX resets several critical settings.

---

## 6. Building the Project
Once your software is ready, follow these steps to get the Neo-81 firmware running:

1.  **Clone the Repository:**
    ```bash
    git clone https://github.com/mndxc/STM32F429-TI81-Calculator.git
    ```
    * [New to Git? Start here.](https://docs.github.com/en/get-started/quickstart/hello-world)

2.  **Open in VSCode:**
    * Open the cloned folder in VSCode.
    * Install the **stm32-cube-clangd** extension for IntelliSense (disable the Microsoft C/C++ extension for this workspace if prompted).

3.  **Build:**

    The project includes a `CMakePresets.json` that configures the ARM toolchain automatically.

    **VSCode (recommended):** Use the CMake build button in the status bar — the stm32-cube-clangd extension sets up the toolchain automatically.

    **Command line** (ARM toolchain must be on PATH):
    ```bash
    export PATH="$HOME/Library/Application Support/stm32cube/bundles/gnu-tools-for-stm32/14.3.1+st.2/bin:$PATH"
    cmake --preset Debug
    cmake --build build/Debug
    ```

4.  **Flash:**
    * Connect your STM32F4-Discovery board via USB.
    * Use the **Run and Debug** panel in VSCode, or flash manually with OpenOCD:

    **macOS:**
    ```bash
    openocd \
      -f $(brew --prefix open-ocd)/share/openocd/scripts/board/stm32f429disc1.cfg \
      -c "program build/Debug/STM32F429-TI81-Calculator.elf verify reset exit"
    ```

    **Linux:**
    ```bash
    openocd \
      -f /usr/share/openocd/scripts/board/stm32f429disc1.cfg \
      -c "program build/Debug/STM32F429-TI81-Calculator.elf verify reset exit"
    ```

---

## 7. Host Tests

The project includes a 516-test host suite that runs on your development machine (no hardware required). It covers the expression tokenizer, shunting-yard evaluator, RPN engine, matrix operations, UTF-8 cursor logic, persistent storage round-trips, PRGM execution control flow, and handle_normal_mode dispatch.

Run these commands from the **repo root** (the directory containing `CMakeLists.txt`):

```bash
cmake -S App/Tests -B build-tests && cmake --build build-tests
ctest --test-dir build-tests   # runs all 5 suites (516 tests total)
```

Or run each executable individually:

```bash
./build-tests/test_calc_engine        # 169 tests
./build-tests/test_expr_util          # 96 tests
./build-tests/test_persist_roundtrip  # 52 tests
./build-tests/test_prgm_exec          # 95 tests
./build-tests/test_normal_mode        # 104 tests
```

All five executables exit `0` on a full pass (516 total). These tests run automatically on every push via CI — running them locally before opening a PR is strongly recommended.

---

## 8. Troubleshooting

* **Screen is white after flashing?** Do a full USB power cycle (unplug and replug). The ILI9341 in RGB interface mode does not always recover cleanly from an SWD reset alone.
* **Float values show as empty strings?** The linker flag `-u _printf_float` is required when using `--specs=nano.specs`. Check that `CMakeLists.txt` still contains it — it can be silently lost during a CMake refactor.
* **Keys not responding?** Verify that the ribbon cable is seated firmly. Use a multimeter in continuity mode to trace each ribbon wire to its key matrix row/column, then match it to the GPIO table in section 2 above.
* **GDB can't connect after using 2nd+ON (power-off screen)?** The firmware blocks on the keypad queue rather than entering true Stop mode on the discovery board prototype. Press ON to wake it, then reconnect GDB.
* **Build fails with "arm-none-eabi-gcc not found"?** The ARM toolchain must be on your PATH. If using the command-line build, run the `export PATH=...` step shown in section 5 first. If using VSCode, ensure the stm32-cube-clangd extension is installed — it manages the toolchain automatically.
* **CMake configure fails with `cube-cmake: not found`?** Check `.vscode/settings.json` — it overrides your system PATH via `cmake.environment.PATH`. This entry must include three directories: the ARM toolchain (`gnu-tools-for-stm32/14.3.1+st.2/bin`), the stm32cube-ide-core extension binaries, and the stm32cube-ide-build-cmake extension's `cube-cmake` binary (`resources/cube-cmake/darwin/aarch64`). If the build-cmake extension was updated, find the new version directory and update `.vscode/settings.json` to match.

---

[⬅ Back to Main README](../README.md)