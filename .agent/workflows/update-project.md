---
description: follow the project update procedure to sync docs after changes
---
Follow the instructions in [PROJECT_UPDATE_PROCEDURE.md](../../docs/PROJECT_UPDATE_PROCEDURE.md) to ensure all project documentation and tracking files are in sync after your changes.

// turbo
1. Read the session diff to identify touched files:
```bash
git log --oneline --since="session start date"
git diff HEAD~1 HEAD --stat
```

2. Update `CLAUDE.md`, `docs/QUALITY_TRACKER.md`, `README.md`, and other docs based on the Tiered Update Rules (Tier 1-4) in [PROJECT_UPDATE_PROCEDURE.md](../../docs/PROJECT_UPDATE_PROCEDURE.md).

3. Verify completion using the "Full Project Update Checklist" in the procedure document.
