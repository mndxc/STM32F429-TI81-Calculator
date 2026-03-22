# PRGM Command Reference

All commands supported by the PRGM execution engine (`prgm_exec.c`).
Commands are entered via the **CTL** and **I/O** sub-menus in the program editor,
or typed directly using the keypad.

---

## Control Flow (CTL menu)

### `If <expr>`

Evaluates `<expr>`. If the result is non-zero (true), the next line executes normally.
If the result is zero (false), two behaviors:

- **Single-line If** — the immediately following line is skipped.
- **Block If** (next line is `Then`) — execution skips forward to the matching `Else`
  (if present) or `End`.

```
If A>5
Disp "BIG"       ← skipped when A≤5
```

```
If A>5
Then
  Disp "BIG"
Else
  Disp "SMALL"
End
```

### `Then`

Marks the start of the true-branch body in a block `If`. Must appear on the line
immediately after `If <expr>`. Pushes a control-flow frame so the matching `End` is
recognized correctly.

### `Else`

Marks the start of the false-branch body. When encountered during normal execution
(i.e. the true branch was taken), execution jumps forward to the matching `End`.

### `End`

Closes a `Then`/`Else` block, a `While` loop, or a `For(` loop.

- **If block** — pops the `CF_IF` frame; execution continues after `End`.
- **While** — re-evaluates the `While` condition; loops back if still true, otherwise
  pops the frame and continues after `End`.
- **For(** — increments the loop variable by `step`, checks the limit, and either loops
  back or continues after `End`.

### `While <expr>`

Loops while `<expr>` evaluates to non-zero. On each pass, condition is re-checked at the
matching `End`. If the condition is false on first evaluation, execution skips to the line
after the matching `End` without entering the body.

```
1->I
While I<=5
  Disp I
  I+1->I
End
```

### `For(var,begin,end[,step])`

Counted loop. Sets `var` to `begin`, executes the body, increments `var` by `step` (default
1.0) at `End`, and repeats while the continuation condition holds:
- Positive step: repeats while `var ≤ end`
- Negative step: repeats while `var ≥ end`

If the initial value already fails the condition, the body is skipped entirely.
`step = 0` is a no-op guard (loop body never runs).

```
For(I,1,5)
  Disp I
End

For(I,10,1,-1)   ← counts down from 10 to 1
  Disp I
End
```

Arguments `begin`, `end`, and `step` are full expressions evaluated at loop entry.

### `Goto <lbl>`

Unconditional jump to the line immediately after `Lbl <lbl>`. If no matching label is
found, the program halts silently.

```
Goto LOOP
```

Label identifiers are arbitrary alphanumeric strings (e.g. `A`, `TOP`, `LOOP1`).

### `Lbl <lbl>`

Defines a jump target. Has no effect during normal sequential execution — it is a no-op
that `Goto` and `Menu(` use as a destination.

```
Lbl TOP
Disp "HELLO"
Goto TOP
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

### `Stop`

Terminates the program immediately. The executor returns to normal calculator mode.
Any remaining lines are not executed.

### `Return`

Returns from the current subroutine to the caller at the line after the `prgm` call.
If the call stack is empty (not inside a subroutine), `Return` behaves like `Stop`.

### `prgm<id>`

Calls another program as a subroutine. `<id>` is the slot identifier (1–9, 0, or A–Z).
Execution transfers to the first line of that program; when it ends (or executes `Return`),
execution resumes at the line after the `prgm` call in the calling program.

Call stack depth is 4 (maximum 4 nested subroutine calls).

```
prgmA          ← calls the program in slot A
prgm1          ← calls the program in slot 1
```

### `IS>(<var>,<val>)`

**Increment and Skip if greater.** Adds 1 to `var`, then skips the next line if
`var > val`.

```
IS>(I,5)
Disp "NOT DONE"   ← skipped once I exceeds 5
```

Equivalent to: `I+1->I : If I>val : (skip next line)`.
Useful for loop counters where you want to check the boundary after incrementing.

### `DS<(<var>,<val>)`

**Decrement and Skip if less.** Subtracts 1 from `var`, then skips the next line if
`var < val`.

```
DS<(I,1)
Disp "NOT DONE"   ← skipped once I drops below 1
```

---

## Input / Output (I/O menu)

### `Input <var>`

Clears the expression buffer, displays `<var>=?` in the history, and waits for the
user to type a value and press **ENTER**. The entered expression is evaluated and the
result stored in `<var>`.

```
Input A
Disp A*2
```

`<var>` must be a single letter A–Z.

### `Prompt <var>`

Identical to `Input <var>` in the current implementation. Displays `<var>=?` and waits
for the user to enter a value.

```
Prompt N
```

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

Note: `Disp` adds a result row to the scrolling history. Multiple `Disp` calls
accumulate in the history; `ClrHome` clears them.

### `ClrHome`

Clears all history rows from the display and resets the recall offset. The screen
appears blank until the next output or user input.

### `DispHome`

Switches the display to the calculator home screen (hiding any graph or menu overlay)
without clearing history content. Useful after a `DispGraph` to return to the text
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

### `Output(row,col,"str")`

Writes a string literal at a specific position on the home screen.

- `row` — 1–8 (display row, 1 = top)
- `col` — 1–N (character column, 1 = leftmost); columns before `col` are filled with spaces
- `"str"` — quoted string literal; the string is clipped to the available display width

```
Output(1,1,"HELLO")     ← writes HELLO at top-left
Output(4,5,"WORLD")     ← writes WORLD starting at column 5 of row 4
```

`row` and `col` are evaluated as full expressions, so variables are allowed:
`Output(R,C,"X")`.

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

## Interactive Menu

### `Menu("title","opt1",lbl1,"opt2",lbl2,...)`

Displays a full-screen menu overlay with up to 9 options. Execution pauses until the
user selects an option, then jumps to the matching `Lbl`.

**Arguments:**
- `"title"` — quoted string shown at the top of the menu
- `"opt1"`, `lbl1` — first option text (quoted) and its `Lbl` destination (unquoted)
- Additional `"optN"`, `lblN` pairs for each additional option (max 9)

```
Menu("CHOOSE","ADD",ADDL,"QUIT",ENDL)
Lbl ADDL
Disp A+B
Goto ENDL
Lbl ENDL
Stop
```

**Navigation during `Menu(`:**
- UP/DOWN — moves highlight
- Number keys 1–9 — direct selection
- ENTER — confirms highlighted option
- CLEAR — aborts program execution

The selected option causes a `Goto` to its label. If the label is not found, the program
halts silently.

---

## General Expression Lines

Any line that does not match a keyword is treated as an expression. It is evaluated using
the same engine as the main calculator. The result is stored in ANS but is **not** displayed
unless followed by `Disp ANS` (or the result is a matrix). Errors are silently ignored and
ANS is unchanged.

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
| Nested If/While/For depth | 8 |
| Subroutine call depth | 4 |
| Menu options | 9 |

---

## Storage

Programs are saved to FLASH sector 11 (`0x080E0000`, 128 KB) independently of calculator
variables. They persist across power cycles and are loaded on boot. Saving occurs when
the user presses **2nd+ON** (same as variable persistence).
