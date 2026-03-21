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
    - Keep `CLAUDE.md` and `TECHNICAL.md` updated with any architectural changes.

## Feedback

If you have questions or need help, feel free to open a "Question" issue or reach out to the maintainers.
