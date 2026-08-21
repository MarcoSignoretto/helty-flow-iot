# AGENTS.md — Project Conventions for Helty Flow IoT

## Versioning Convention

**Use [Semantic Versioning](https://semver.org/) (MAJOR.MINOR.PATCH) for all version bumps.**

| Type | Meaning | When to bump |
|------|---------|--------------|
| MAJOR | Breaking change | Protocol changes, API incompatibility, config format changes |
| MINOR | New feature (backward-compatible) | New modes, new sensors, new capabilities |
| PATCH | Bug fix / reliability improvement | QoS changes, connection fixes, error handling |

### Version Fields

| Location | Field | Example |
|----------|-------|---------|
| `helty-nodemcu-firmware/helty-nodemcu-firmware.ino` | Comment header at line 1 | `// Version: 1.0.1` |
| `helty_flow/manifest.json` | `"version"` | `"1.0.1"` |
| `helty_flow/config_flow.py` | `VERSION = 1` | Increment on config breaking changes |

### Bumping Rules

1. **Firmware and HA integration versions should match** — if the firmware is 1.1.0, the integration should also be 1.1.0. This makes it clear which firmware version works with which integration version.

2. **Config version (`config_flow.py`)** only changes when the config entry format changes in a breaking way (e.g., adding a required field that wasn't there before). Internal protocol changes (like QoS) do NOT require a config version bump.

3. **Always bump in the same commit as the code change** — never commit code with a stale version.

4. **Tag releases on GitHub** — after merge, create a git tag: `git tag -a v1.0.1 -m "Release 1.0.1"` and push.

### Example Commits

```bash
# Bug fix
git commit -m "fix: use MQTT QoS 1 for reliable command delivery
Bump versions: firmware 1.0.0 → 1.0.1, integration 1.0.0 → 1.0.1"

# New feature
git commit -m "feat: add alarm acknowledgment command
Bump versions: firmware 1.0.1 → 1.1.0, integration 1.0.1 → 1.1.0"

# Breaking change
git commit -m "feat: rewrite MQTT topic structure
Bump versions: firmware 1.1.0 → 2.0.0, integration 1.1.0 → 2.0.0, config 1 → 2"
```

## Pull Request Rules

**Marco must review ALL pull requests before any merge. Never auto-merge without explicit approval.**

## Code Style

### Firmware (Arduino/C++)
- Use `//` single-line comments for inline notes
- Use `/* block */` for section headers
- 2-space indentation
- `camelCase` for variables, `UPPER_SNAKE_CASE` for constants/macros
- Include a comment at the top of new functions describing purpose

### HA Integration (Python)
- Follow Home Assistant integration conventions
- Use `async_*` prefix for async functions
- Log with `_LOGGER` (not `print`)
- Include docstrings for all public methods

## Commit Message Format

Conventional Commits:

```
type(scope): short description

Longer explanation if needed. Wrap at 72 characters.
```

Types: `feat`, `fix`, `refactor`, `docs`, `test`, `ci`, `chore`, `perf`

When bumping versions, include the version change in the commit message:

```
fix: use MQTT QoS 1 for reliable command delivery
Bump versions: firmware 1.0.0 → 1.0.1, integration 1.0.0 → 1.0.1
```
