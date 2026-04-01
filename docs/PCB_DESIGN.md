# PCB Design Notes

**Status: paused — resuming after software is complete.**

Custom PCB targets the STM32H7B0VBT6 as the main MCU (upgrade from the STM32F429 discovery board used for development).

---

## Bill of Materials

All ICs verified available on JLCPCB:

| IC | Purpose |
|----|---------|
| STM32H7B0VBT6 | Main MCU LQFP100 |
| W25Q128JV | 16MB OctoSPI NOR flash — firmware XIP + user data (variables, programs, settings) |
| RT9471 | LiPo charger with power path management — I²C programmable, 3A, USB OTG boost, WQFN-24L 4×4 |
| RT8059 | 3.3V main buck (R_upper=100K, R_lower=22.1K 1%) |
| RT4812 | 5V boost — **DNF Rev1; reserved for Rev2 when Raspberry Pi Zero 2 W is integrated** |
| TPD4E05U06DQAR | USB ESD protection (TI, 4-channel, SOT-563) |

---

## Power Architecture

> See also: [docs/POWER_MANAGEMENT.md](POWER_MANAGEMENT.md) — firmware Stop mode sleep/wake sequence (SDRAM self-refresh, PLLSAI restore, RTOS integration) targeting this hardware.

```
USB ──► RT9471 (SYS rail) ──► RT8059 (buck) ──► 3.3V system rail
         ▲        │
        BAT ──────┘  (RT9471 power path selects best source automatically)
```

- **RT9471 SYS rail** is a managed output — not a raw battery passthrough. Guaranteed minimum 3.5V (VSYS_MIN) while any power source is available. When USB is connected, SYS is regulated from VBUS and the system runs normally even with a fully depleted battery.
- **RT8059** bucks the SYS rail down to 3.3V. Minimum headroom at worst case (SYS = 3.5V, VOUT = 3.3V) is 200mV — tight but workable. At 300mA load the dropout is ~100mV, well within margin.
- **Low-battery threshold:** set the ADC monitor to flag at ~3.6V (battery terminal, not SYS) to ensure graceful shutdown before SYS hits its 3.5V floor.
- **RT8059 feedback resistors:** R_upper = 100kΩ, R_lower = 22.1kΩ 1% → 3.3V output. Optionally bias to 3.28V for a few mV of extra dropout margin.
- **RT9471 I²C:** SCL/SDA to STM32 with 10kΩ pull-ups to 3.3V. Default power-on settings are safe without firmware init, but configure AICR to 1.5A on boot for faster charging. CE pin pulled low to enable charging; INT pin to STM32 GPIO for fault notification.
- **RT9471 package:** WQFN-24L 4×4 with exposed thermal pad — must be soldered to solid GND copper pour. Requires reflow; not hand-solderable.
- **TPD4E05U06DQAR:** placed on USB D+/D− lines as close to the connector as possible, before the RT9471 D+/D− pins. SOT-563 package.
- **RT4812 (DNF Rev1):** footprint placed but unpopulated. Reserved for Rev2 when a Raspberry Pi Zero 2 W is added — the Pi requires a 5V rail the RT4812 will provide by boosting from the LiPo.

---

## External Flash (W25Q128JV)

Single 16MB device on OCTOSPI1 serves both firmware XIP and user data storage.

**Proposed partition layout:**
- `0x000000 – 0x0FFFFF` (first 1MB): firmware image (XIP, never written at runtime)
- `0x100000 – 0xFFFFFF` (remaining 15MB): user data — variables, programs, settings

**Write-freeze gotcha (critical):** W25Q128JV is single-bank NOR. Erasing or writing any sector while OCTOSPI1 is in memory-mapped (XIP) mode stalls the AHB bus and freezes execution. Any erase/write routine touching the user data partition **must** run from RAM:
- Declare with `__attribute__((section(".RamFunc")))`
- Switch OCTOSPI1 to indirect mode before erase/write; switch back to memory-mapped mode before returning
- Keep the routine short — execution is stalled during the switch

**Simpler alternative:** only write flash on `2nd+ON` power-off gesture (same pattern as STM32F429 persistent storage). Limits erase/write to once per session; makes the RAM-execution window short and predictable.

Flash wear negligible at this scale — effectively unlimited for calculator use.

---

## VBAT Supply

No coin cell. Route LiPo BAT pin → BAT54 Schottky diode (SOD-323) → STM32 VBAT. The diode prevents back-feed into the RT9471 power path. VBAT draws ~1–5μA; a 1000mAh LiPo powering only VBAT would last decades. RTC survives soft power-off as long as the LiPo is installed. If LiPo is removed the RTC resets — acceptable since no timekeeping feature is planned.

---

## Battery Monitoring

Voltage divider R1=100K / R2=82K 1% into STM32 ADC; max ADC voltage at 4.2V LiPo = 1.89V (safe for 3.3V VDDA reference).
