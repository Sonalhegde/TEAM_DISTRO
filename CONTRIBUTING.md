# Contributing Guidelines

This document gives the rules to change code and documentation in this repository.

## Branch Rules

- Keep the `main` branch stable at all times.
- Create a new branch for each feature or bug fix:
  - Format: `feature/short-description` or `fix/short-description`.
- Merge changes into `main` with a pull request.

## Commit Message Rules

- Use the Conventional Commits specification.
- Use lowercase prefixes:
  - `feat:` for new features.
  - `fix:` for bug fixes.
  - `docs:` for documentation updates.
  - `refactor:` for code restructuring without behavior change.
  - `test:` for test additions or updates.
  - `chore:` for maintenance tasks.
- Keep the first line under 72 characters.

## Code Quality Rules

- Do not add spaces to file names or folder names.
- Do not commit generated build files or temporary binary files.
- Run all simulation tests before you open a pull request:
  ```bash
  node tests/simulation/run-tests.js
  ```
- Make sure that all 16 test cases pass.
