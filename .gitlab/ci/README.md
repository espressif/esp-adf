# GitLab CI Configuration Guide

Two files control what gets built and tested:

| File | Purpose |
|------|---------|
| `.gitlab/ci/ci-config.yml` | IDF versions, supported boards, trigger rules |
| `tools/ci/apps.yaml` | Which apps to build/test, on which chip+board, for which IDF range |

---

## `ci-config.yml`

### `LATEST_IDF_VERSION` (required)
Used as `${LATEST_IDF_VERSION}` in `apps.yaml` board ranges and by `generated_triggers`.  
Must be kept in sync with the latest entry in `IDF_VERSIONS`.

```yaml
LATEST_IDF_VERSION: v5.5.3
```

### `IDF_VERSIONS`
Defines which IDF versions are active and which Docker images to use for each.

```yaml
IDF_VERSIONS:
  - v5.5.3:
      BUILD_IMAGE: "$CI_DOCKER_REGISTRY/esp-env-v5.5:3"
      TEST_IMAGE:  "$CI_DOCKER_REGISTRY/target-test-env-v5.5:2"
```

### `SUPPORT_BOARDS`
Registers all board names used in `apps.yaml`.  
`chip` must match the chip keys in `apps.yaml`; `board_name` is an alias used by the board manager.

```yaml
SUPPORT_BOARDS:
  - ESP32_S3_KORVO2_V3:
      chip: "esp32s3"
      board_name: "esp32_s3_korvo2_v3"
      idf_version: ">=4.4,<=${LATEST_IDF_VERSION}"
```

### `PRIORITY_LIST`

> **Important**: `PRIORITY_LIST` is a **top-level key**, at the same level as `variables` — not nested inside it.
> Putting it inside `variables` will cause it to be ignored silently.

Defines the order in which `trigger_rules` condition types are evaluated.  
The first condition type that matches any rule wins; remaining types are skipped.

Available condition types:

| Type | Meaning |
|------|---------|
| `commit_tag` | Match against commit message tags (e.g. `[test-all]`) |
| `changes` | Match against changed file paths (regex) |

```yaml
# Correct: top-level, same indent as `variables`
PRIORITY_LIST:
  - commit_tag   # checked first
  - changes      # checked only if no commit_tag rule matched

variables:
  LATEST_IDF_VERSION: v5.5.3
  ...
```

### `trigger_rules`

Each rule has a `name`, one or more condition fields, and a `trigger_versions` value.  
The first rule whose condition matches the current pipeline is used; the rest are skipped.

**`trigger_versions` values:**

| Value | IDF versions triggered |
|-------|----------------------|
| `all` | Every version listed in `IDF_VERSIONS` |
| `latest` | Only `LATEST_IDF_VERSION` |
| `lts` | All versions whose tag does **not** start with `release/` |
| `stable` | The two most recent `lts` versions |

**Condition fields (one or more per rule):**

| Field | Matches when… |
|-------|---------------|
| `changes` | Any changed file path matches the regex |
| `commit_tag` | The commit message contains the regex (e.g. `\[test-all\]`) |

```yaml
trigger_rules:
  - name: "Core components changed"
    changes:
      - "^components/"
    trigger_versions: all          # build all IDF versions

  - name: "Docs only"
    changes:
      - "^docs/"
      - "\\.md$"
    trigger_versions: latest       # build latest IDF version only

  - name: "Commit tag: test-all"
    commit_tag:
      - "\\[test-all\\]"
    trigger_versions: all
```

---

## `apps.yaml`

Defines apps under two sections: `EXAMPLES` and `TEST_APPS`.

Each entry:

```yaml
EXAMPLES:
  - app_dir: adf_examples/ai_agent/coze_ws_app             # path from repo root
    board:
      esp32:                                               # chip key — must exist in SUPPORT_BOARDS
        - ESP_LYRAT_MINI_V1_1: v5.5-${LATEST_IDF_VERSION}  # board: idf_range
      esp32s3:
        - ESP32_S3_KORVO2_V3: v5.5-${LATEST_IDF_VERSION}
      esp32p4:
        - ESP32_P4_FUNCTION_EV: v5.5-${LATEST_IDF_VERSION}
```

**`idf_range` formats:**

| Format | Meaning |
|--------|---------|
| `v5.5-${LATEST_IDF_VERSION}` | from v5.5 to latest |
| `>=4.4,<=${LATEST_IDF_VERSION}` | semver range |
| _(bare board string)_ | applies to all IDF versions |

> Board names must match entries in `SUPPORT_BOARDS`.  
> `${LATEST_IDF_VERSION}` is resolved from `ci-config.yml`, not redefined in `apps.yaml`.
