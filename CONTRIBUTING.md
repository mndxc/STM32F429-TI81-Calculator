# Contributing to Neo-81

Thank you for your interest in contributing to the Neo-81 project! This guide will help you get started with reporting issues, suggesting features, and submitting code.

## Code of Conduct

By participating in this project, you agree to abide by our [Code of Conduct](CODE_OF_CONDUCT.md).

## How Can I Contribute?

### Reporting Bugs

If you find a bug, please search existing issues to see if it has already been reported. If not, open a new issue using the **Bug Report** template. Please include:
- Hardware version (e.g., STM32F429I-DISC1).
- Wiring setup (if relevant).
- Steps to reproduce the issue.
- Expected vs. actual behavior.

### Suggesting Enhancements

We welcome ideas for new features! Please use the **Feature Request** template when opening a new issue.

### Pull Requests

1. **Fork the repository** and create your branch from `main`.
2. **Setup your environment:** Follow the instructions in [GETTING_STARTED.md](docs/GETTING_STARTED.md).
3. **Write code:** Ensure your code follows our style guidelines (see below).
4. **Test your changes:** Verify your changes on hardware or the simulator if applicable.
5. **Submit a Pull Request:** Use the PR template and provide a clear description of your changes.

## Development Guidelines

### Build System

We use CMake and the GCC ARM toolchain. See [docs/GETTING_STARTED.md](docs/GETTING_STARTED.md) for detailed setup instructions.

### Code Style

To keep the codebase consistent, please follow these conventions:

- **Naming Conventions:**
    - **Modules:** Use consistent prefixes (e.g., `Calc_*`, `Graph_*`, `Persist_*`, `Keypad_*`).
    - **Public Functions:** `Prefix_FunctionName` (CamelCase).
    - **Internal/Static Functions:** `snake_case` or `PascalCase`.
    - **Variables:** `snake_case` for local and static variables.
    - **Constants & Macros:** `SCREAMING_SNAKE_CASE`.
    - **Types:** `PascalCase_t` (e.g., `CalcResult_t`).
- **Formatting:**
    - Use 4 spaces for indentation.
    - Keep functions small and focused (ideally under 100 lines).
    - Use `const` for immutable data to save RAM and improve safety.
- **Documentation:**
    - Use Doxygen-style comments for public header functions.
    - Keep `CLAUDE.md`, `TECHNICAL.md`, and `docs/ARCHITECTURE.md` updated with any architectural changes.
    - **Sync Procedure:** after any significant session or feature completion, follow the update rules in [docs/MAINTENANCE_STANDARDS.md](docs/MAINTENANCE_STANDARDS.md) to ensure all tracking files and public numbers are in sync.

## Complexity Management

**The codebase must not grow in complexity faster than it is simplified.** After every commit, rate the complexity delta:

- `neutral` — no net change (e.g. pure refactor, test addition, config change)
- `increase` — new logic, new state, new abstractions, or a file grew significantly
- `decrease` — logic removed, files split, dead code deleted, abstractions simplified

If a commit is rated `increase`, add a follow-up item to the `Next session priorities` list in `CLAUDE.md` describing exactly how to pay down the debt introduced. Name the file, the function, and the technique. Do not leave an `increase` commit without a follow-up plan.

**What counts as a complexity increase:**
- A file grows by more than ~100 lines without a corresponding extraction or removal elsewhere
- A new module is added without a clear, bounded responsibility
- New global or shared state is introduced
- A function exceeds ~80–100 lines
- A workaround or special-case is added rather than fixing the underlying model

---

## Where to File Work Items

When adding a to-do item, place it in the correct location — never duplicate it in both files.

| Item type | Where it goes |
|---|---|
| **Feature work** — new calculator behaviour, UI improvements | `Next session priorities` in `CLAUDE.md` |
| **Bug fix** — incorrect behaviour, crashes, display glitches | `Next session priorities` in `CLAUDE.md` |
| **Complexity debt** — introduced by a commit that needs paying down | `Next session priorities` in `CLAUDE.md` (tag `[complexity]`) |
| **Code quality** — compiler warnings, CI gates, refactoring, static analysis | `Next session priorities` in `CLAUDE.md` (tag `[refactor]`) |
| **Testing** — new test coverage, test infrastructure, coverage targets | `Next session priorities` in `CLAUDE.md` (tag `[testing]`) |
| **Contributor/open-source docs** — architecture diagrams, guides, onboarding | `Next session priorities` in `CLAUDE.md` (tag `[docs]`) |

**Rule of thumb:** all work items go in `CLAUDE.md` "Next session priorities". `docs/MAINTENANCE_STANDARDS.md` defines process standards — read it before starting significant work.

---

## Feedback

If you have questions or need help, feel free to open a "Question" issue or reach out to the maintainers.
