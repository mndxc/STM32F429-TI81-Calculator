# PRGM Command Reference

All commands supported by the PRGM execution engine (`prgm_exec.c`).
Commands are entered via the **CTL** and **I/O** sub-menus in the program editor,
or typed directly using the keypad.

---

## Control Flow (CTL menu)

### `If <expr>`

Evaluates `<expr>`. If the result is non-zero (true), the next line executes normally.
If the result is zero (false), the immediately following line is skipped (**single-line If only**).

```
If A>5
Disp "BIG"       ← skipped when A≤5
```

Block `If` (with `Then`/`Else`/`End`) is not supported.

### `Goto <lbl>`

Unconditional jump to the line immediately after `Lbl <lbl>`. If no matching label is
found, the program halts silently.

```
Goto A
```

Label identifier is a **single character** (A–Z, 0–9).

### `Lbl <lbl>`

Defines a jump target. Has no effect during normal sequential execution — it is a no-op
that `Goto` uses as a destination.

```
Lbl A
Disp "HELLO"
Goto A
```

Label identifier is a **single character** (A–Z, 0–9).

### `IS>(<var>,<val>)`

**Increment and Skip if greater.** Adds 1 to `var`, then skips the next line if
`var > val`.

```
IS>(I,5)
Disp "NOT DONE"   ← skipped once I exceeds 5
```

Equivalent to: `I+1->I : If I>val : (skip next line)`.

### `DS<(<var>,<val>)`

**Decrement and Skip if less.** Subtracts 1 from `var`, then skips the next line if
`var < val`.

```
DS<(I,1)
Disp "NOT DONE"   ← skipped once I drops below 1
```

### `Pause`

Halts execution and waits for the user to press **ENTER**. No variable is stored.
Useful for displaying a value before continuing.

```
Disp "RESULT="
Disp ANS
Pause
ClrHome
```

### `End`

Recognized as a keyword (no syntax error), but is a **no-op** in the current
implementation. Block structures (`If/Then/Else/End`, `While`, `For(`) are not
supported; `End` is retained for source compatibility only.

### `Stop`

Terminates the program immediately. The executor returns to normal calculator mode
and displays `Done`. Any remaining lines are not executed.

### `prgm<name>`

Calls another program as a subroutine. `<name>` is the program's name or slot
identifier (1–9, 0, A–Z). Execution transfers to the first line of that program;
when it ends, execution resumes at the line after the `prgm` call.

Call stack depth is 4 (maximum 4 nested subroutine calls).

```
prgmA          ← calls the program named A (or in slot A)
prgm1          ← calls the program in slot 1
```

`prgm<name>` tokens are inserted by selecting a program in the **EXEC** tab of the
PRGM menu. In the main calculator, entering `prgmNAME` and pressing **ENTER**
runs the program and shows `Done`.

---

## Input / Output (I/O menu)

### `Disp <arg>`

Displays a value in the history area.

- **String literal** — `Disp "text"` — displays the string between the quotes.
- **Expression** — `Disp <expr>` — evaluates the expression and displays the formatted
  result. The result is also stored in ANS.

```
Disp "HELLO"
Disp 2+2          ← displays 4, ANS=4
Disp A*B
```

Multiple `Disp` calls accumulate in the history; `ClrHome` clears them.

### `Input <var>`

Displays `?` and waits for the user to type a value and press **ENTER**. The entered
expression is evaluated and the result stored in `<var>`.

```
Input A
Disp A*2
```

`<var>` must be a single letter A–Z.

### `DispHome`

Switches the display to the calculator home screen (hiding any graph or menu overlay)
without clearing history content. Useful after `DispGraph` to return to the text
output view.

### `DispGraph`

Renders the current graph equations and switches the display to the full-screen graph
canvas. Execution continues on the next line immediately (non-blocking).

```
1->Xmin
10->Xmax
DispGraph
Pause
DispHome
```

### `ClrHome`

Clears all history rows from the display and resets the recall offset. The screen
appears blank until the next output or user input.

---

## Assignment

### `<expr>-><var>`

Evaluates `<expr>` and stores the result in variable `<var>` (A–Z). Also updates ANS.
The `->` arrow is entered via the **STO→** key.

```
5->A
A*2->B
sin(45)->X
```

---

## General Expression Lines

Any line that does not match a keyword is treated as an expression. It is evaluated using
the same engine as the main calculator. The result is stored in ANS but is **not** displayed
unless followed by `Disp ANS`. Errors are silently ignored and ANS is unchanged.

```
A+B*C           ← result stored in ANS, not shown
Disp ANS        ← show it
```

---

## Limits

| Resource | Limit |
|---|---|
| Program slots | 37 (1–9, 0, A–Z) |
| Program name length | 8 characters |
| Program body size | 512 bytes |
| Lines per program | 64 |
| Characters per line | 47 (+ null) |
| Subroutine call depth | 4 |

---

## Storage

Programs are saved to FLASH sector 11 (`0x080E0000`, 128 KB) independently of calculator
variables. They persist across power cycles and are loaded on boot. Saving occurs when
the user presses **2nd+ON** (same as variable persistence).
