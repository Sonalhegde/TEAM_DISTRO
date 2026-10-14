# Repository Setup and Version Control Guide

This guide provides step-by-step instructions for cloning, updating, and contributing to the **TEAM DISTRO** repository. It is written for students and contributors using Git on Windows, Linux, or macOS.

---

## 1. First-Time Git Configuration

Configure your Git username and email address before making any commits. Git attaches this identity to your commit metadata.

### Steps
1. Open your terminal (PowerShell, Command Prompt, or bash).
2. Set your global user name:
   ```bash
   git config --global user.name "Your Full Name"
   ```
3. Set your global email address (use your GitHub-associated email):
   ```bash
   git config --global user.email "your.email@example.com"
   ```
4. Configure default pull reconciliation to avoid unwanted merge commits:
   ```bash
   git config --global pull.rebase false
   ```

### Verify it worked
Run:
```bash
git config --list --show-origin
```
Confirm that `user.name` and `user.email` appear with your configured values.

---

## 2. Cloning the Repository

Download a complete local clone of the repository, including git history and subdirectories.

### Steps
1. Navigate to your desired development directory:
   ```bash
   cd ~/projects
   ```
2. Clone the repository over HTTPS:
   ```bash
   git clone https://github.com/Sonalhegde/TEAM_DISTRO.git
   ```
3. Change directory into the repository root:
   ```bash
   cd TEAM_DISTRO
   ```

### Verify it worked
Run:
```bash
git status
```
Output should state:
```text
On branch main
Your branch is up to date with 'origin/main'.
nothing to commit, working tree clean
```

---

## 3. Pulling and Updating Code

Synchronize your local workspace with the latest updates from the remote repository.

### Steps
1. Fetch latest remote branches and refs:
   ```bash
   git fetch origin
   ```
2. Pull and merge updates into your active branch:
   ```bash
   git pull origin main
   ```

### Verify it worked
Run:
```bash
git log -n 1 --oneline
```
Confirm that the latest commit hash and message match the top commit on GitHub.

---

## 4. Branching and Feature Work

Always create an isolated branch when testing modifications or developing features.

### Steps
1. Ensure your base branch is up to date:
   ```bash
   git checkout main
   git pull origin main
   ```
2. Create and switch to a new descriptive feature branch:
   ```bash
   git checkout -b feature/calibration-update
   ```
3. Make your code or documentation changes in the workspace.

### Verify it worked
Run:
```bash
git branch
```
Confirm that an asterisk `*` marks your new branch name (e.g. `* feature/calibration-update`).

---

## 5. Staging, Committing, and Pushing Changes

Save changes to git history and push them to the remote GitHub server.

### Steps
1. Check changed and untracked files:
   ```bash
   git status
   ```
2. Stage modified files for commit:
   ```bash
   git add firmware/main/main.cpp docs/SETUP_ESP32.md
   ```
   *(Or stage all tracked modifications using `git add -u`)*
3. Commit the staged changes with a concise imperative message:
   ```bash
   git commit -m "docs: add ESP-IDF setup instructions and calibration guide"
   ```
4. Push your branch to GitHub:
   ```bash
   git push -u origin feature/calibration-update
   ```

### Verify it worked
Run:
```bash
git status
```
Output must show `nothing to commit, working tree clean`. Open GitHub in your browser to confirm the branch exists remotely.

---

## 6. Resolving Pull Conflicts with Local Changes

If you have local modifications that conflict with incoming changes during `git pull`, follow these steps to resolve safely without losing work.

### Scenario A: You have uncommitted local changes
1. Temporarily stash your local changes:
   ```bash
   git stash
   ```
2. Pull the latest commits from the remote repository:
   ```bash
   git pull origin main
   ```
3. Re-apply your stashed changes:
   ```bash
   git stash pop
   ```
4. If merge conflicts occur inside specific files, Git flags them with conflict markers (`<<<<<<<`, `=======`, `>>>>>>>`). Open the indicated files in VS Code, choose the appropriate changes, save, and stage:
   ```bash
   git add <resolved-file>
   ```

### Scenario B: You committed locally and remote has conflicting commits
1. Pull from remote:
   ```bash
   git pull origin main
   ```
2. Git reports `CONFLICT (content): Merge conflict in <filename>`.
3. Open `<filename>` in your editor and resolve the conflict.
4. Mark the conflict resolved by staging the file:
   ```bash
   git add <filename>
   ```
5. Complete the merge commit:
   ```bash
   git commit -m "merge: resolve conflict with origin/main"
   ```
6. Push the resolved tree:
   ```bash
   git push origin main
   ```

### Verify it worked
Run:
```bash
git status
```
The tree must be clean with no unresolved conflict markers in any file.
