# PCB

## Pre-push Checklist (Avoiding Conflicts)

Always pull the latest changes from the remote before starting work and before pushing.

### 1. Before starting work
```bash
git pull origin main
```

### 2. Before pushing
```bash
# 1) Review changes — make sure no build artifacts (Debug/, *.o, *.elf, etc.) are included
git status

# 2) Commit
git add <files>
git commit -m "Short summary of changes"

# 3) Sync with the remote once more right before pushing
git pull --rebase origin main

# 4) Push if there are no conflicts
git push origin main
```

### 3. If a conflict occurs
```bash
# Check which files are in conflict
git status

# Open each file, resolve the <<<<<<< / ======= / >>>>>>> sections, then
git add <resolved files>
git rebase --continue

# If things get messy, go back to the state before the rebase
git rebase --abort
```

### Notes
- Do not use `git push --force` — it can erase other people's commits.
- `.ioc` files (CubeMX configuration) are hard to merge by hand. Tell the team before editing one so two people don't change it at the same time.
- When regenerating code with CubeMX, only write code between `USER CODE BEGIN` / `USER CODE END`. Anything outside those blocks is deleted on regeneration.
- Build and IDE files such as `Debug/`, `Release/`, and `.settings/` are listed in `.gitignore` — do not commit them.
