# Display Stability & Clock Optimization

This document outlines the final configuration and rationale for the display clock settings discovered during the iterative hardware testing process for the STM32F429-TI81-Calculator.

## Final Solution

To resolve the "flowing noise" and "jiggling" artifacts observed on the prototype hardware, the display pixel clock has been optimized for the best balance between interference rejection and visual stability.

- **Target Frequency**: **5.5 MHz** (approx. **60 Hz** refresh rate).
- **Rationale**:
    - **16.67 MHz (Original)**: Caused significant "flowing noise" due to high-frequency interference on the breadboard/prototype wiring.
    - **4.0 MHz**: Eliminated noise but introduced visible "jiggling" (refresh flicker).
    - **5.5 MHz**: Industry-standard refresh rate; flicker-free and noise-free on the target dark backgrounds.

## Hardened Architecture

The clock configuration has been "hardened" to ensure these stable settings are preserved even if the project is regenerated from CubeMX boilerplate:

1.  **Initial Boot**: The primary clock configuration is applied at the very top of `App_DefaultTask_Run` in `App/Src/app_init.c`. Since this is a custom file, CubeMX will not overwrite it. This ensures the correct clock is set before the display is ever enabled.
2.  **Wake-from-Sleep**: The same 5.5 MHz logic is applied in `Power_EnterStop` in `App/Src/app_init.c` to ensure a clean wake sequence.
3.  **Silent Boot**: The `LTDC` controller is explicitly disabled at the end of `MX_LTDC_Init` (in `main.c`) within a protected `USER CODE` block, keeping the display silent until the application logic takes over.

## Configuration Parameters

For reference, the **5.5 MHz** clock corresponds to these PLLSAI parameters:
- **PLLSAIN**: 176
- **PLLSAIR**: 4
- **DivR**: 8 (`RCC_PLLSAIDIVR_8`)

Recommended `.ioc` settings for a permanent fix in CubeMX:
- **N**: 176, **R**: 4, **LTDC Divider**: /8

---
*Last Updated: 2026-03-20*
