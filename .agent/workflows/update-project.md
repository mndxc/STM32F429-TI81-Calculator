---
description: follow the project update procedure to sync docs after changes
---
Follow the Update Rules and Full Update Checklist in [docs/MAINTENANCE_STANDARDS.md](../../docs/MAINTENANCE_STANDARDS.md) to ensure all project documentation and tracking files are in sync after your changes.

// turbo
1. Read the session diff to identify touched files:
```bash
git log --oneline --since="session start date"
git diff HEAD~1 HEAD --stat
```

2. Update `CLAUDE.md`, `README.md`, and other docs per the Update Rules in `MAINTENANCE_STANDARDS.md`. Only touch `docs/MAINTENANCE_STANDARDS.md` itself when a scorecard dimension rating changes. Record resolved items in `docs/PROJECT_HISTORY.md`.

3. Verify completion using the "Full Update Checklist" in `MAINTENANCE_STANDARDS.md`.
