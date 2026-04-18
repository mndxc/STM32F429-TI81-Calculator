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

**Wiring Table (TI-81 → STM32):**

> **PCB revision note:** The TI-81 was produced in multiple PCB revisions with different internal trace layouts. The wiring documented below applies to **PCB Revision b** (believed to be the most common revision). If your donor board has a different layout, use a multimeter in continuity mode to trace each keypad pad and match it to the firmware names in the table. If you complete a mapping for another revision, please open an issue or PR — it is one of the most valuable contributions you can make.

**Method (Rev b):** The original keypad controller IC is removed from the TI-81 PCB. Individual wires are soldered to the IC pad footprint and run to a Dupont connector harness that plugs into the STM32F429I-DISC1 GPIO headers.

---

**Step 1 — Identify your keypad traces**

The image below shows the back of the Rev b PCB with each keypad switch colour-coded to its column and row line.

![TI-81 Rev b PCB matrix trace map](TI-81bPCB_Mapped2.png)

---

**Step 2 — Locate the IC pad footprint**

After removing the controller IC, the pad footprint becomes the solder points for your wire harness. The diagram below shows which pad numbers carry each matrix signal. Columns land on pins 59–65; rows on pins 70–77; ON button on pin 5.

![IC pad footprint pin assignments](IC_Matrix_pins.png)

---

**Step 3 — Wire IC pads to STM32 GPIO**

> *(Photo of completed Rev b wiring harness — pending. Will be added once wiring is complete.)*

Use the table below to connect each IC pad to the correct STM32 GPIO pin:

| Firmware Name | STM32 Pin | Function | Rev b IC Pad |
| :--- | :--- | :--- | :--- |
| **A1** | PE5 | Column 1 | IC pin 60 |
| **A2** | PE4 | Column 2 | IC pin 61 |
| **A3** | PE3 | Column 3 | IC pin 62 |
| **A4** | PE2 | Column 4 | IC pin 63 |
| **A5** | PB7 | Column 5 | IC pin 64 |
| **A6** | PB4 | Column 6 | IC pin 65 |
| **A7** | PB3 | Column 7 | IC pin 59 |
| **B1** | PG9 | Row 1 | IC pin 77 |
| **B2** | PD7 | Row 2 | IC pin 76 |
| **B3** | PC11 | Row 3 | IC pin 75 |
| **B4** | PC8 | Row 4 | IC pin 74 |
| **B5** | PC3 | Row 5 | IC pin 73 |
| **B6** | PA5 | Row 6 | IC pin 72 |
| **B7** | PG2 | Row 7 | IC pin 71 |
| **B8** | PG3 | Row 8 | IC pin 70 |
| **ON** | PE6 | ON/Interrupt | IC pin 5 |

![Updated documentation of a newer revision PCB from TI-81](TI-81bPCB_Mapped2.png)

![Updated documentation of a newer revision PCB from TI-81](IC_Matrix_pins.png)

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

The project includes a host test suite that runs on your development machine (no hardware required). See [docs/TESTING.md](TESTING.md) for the full suite list and current assertion counts.

Run these commands from the **repo root** (the directory containing `CMakeLists.txt`):

```bash
cmake -S App/Tests -B build-tests && cmake --build build-tests
ctest --test-dir build-tests
```

All suites must exit `0`. These tests run automatically on every push via CI — running them locally before opening a PR is strongly recommended.

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
