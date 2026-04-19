# Security Policy

## Scope

Neo-81 is an open-source embedded firmware project targeting the STM32F429I-DISC1 development board. It runs as a standalone calculator on personal hardware and has no network connectivity, cloud services, or server-side components.

Security issues most relevant to this project:

- **Memory safety** — buffer overflows, out-of-bounds writes, stack corruption in the expression parser, tokenizer, or executor.
- **Input handling** — malformed expressions or program sequences that cause crashes, undefined behaviour, or incorrect results.
- **FLASH handling** — defects in `persist.c` that could corrupt the persist block or, in the worst case, erase firmware sectors (see the `FLASH_SECTOR_10` / `FLASH_SECTOR_7` distinction documented in `CLAUDE.md`).
- **RTOS safety** — race conditions, deadlocks, or mutex misuse that could corrupt display state or hang the device.

Issues that are **out of scope** (no network attack surface exists):
- Network or remote code execution.
- Authentication or authorisation bypasses.
- Supply-chain attacks on dependencies (ST HAL, FreeRTOS, LVGL are vendored; changes are reviewed on update).

## Supported Versions

Only the latest commit on `main` is actively maintained. There are no versioned release branches at this time.

## Reporting a Vulnerability

If you discover a security-relevant defect, please report it privately before opening a public issue.

Open a [GitHub private vulnerability report](https://github.com/mndxc/STM32F429-TI81-Calculator/security/advisories/new) (requires a GitHub account).

Please include:
- A description of the defect and which component is affected.
- Steps to reproduce (input sequence, expression, or program listing).
- Observed vs. expected behaviour.
- If applicable, whether you can trigger the issue from user-supplied input only (no physical access required).

## Response

- Acknowledgement within **7 days**.
- Assessment and triage within **30 days**.
- Fix and public disclosure coordinated with the reporter.

There is no bug bounty programme at this time.

## Security-Relevant Design Decisions

The following architectural choices are intentional and documented:

| Decision | Rationale |
|---|---|
| FLASH persist uses sector 10 (0x080C0000), never sectors 0–7 | Sectors 0–7 overlap the firmware image; erasing them causes a HardFault boot loop |
| Expression buffer is fixed-size; no heap allocation in the parser | Eliminates a class of heap-corruption bugs in a resource-constrained environment |
| LVGL calls gated behind `xLVGL_Mutex` | Prevents concurrent display corruption from CalcCoreTask and DefaultTask |
| Program executor uses a bounded step counter | Prevents infinite loops from hanging the RTOS |
