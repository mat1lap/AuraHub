# Databases (Draft Schemas)

This is a draft of the minimal schema needed to make the Time Machine and cloud sync work end-to-end.

## Client: SQLite (Qt SQL)

### Goals
- Persist an **append-only action log** for every agent/tool operation.
- Store pre-image snapshots for file mutations (baseline: full blob).
- Support rollback by `action_id`.

### Baseline tables (DDL sketch)

```sql
CREATE TABLE IF NOT EXISTS action_log (
  id           INTEGER PRIMARY KEY AUTOINCREMENT,
  ts_utc_ms    INTEGER NOT NULL,
  tool         TEXT    NOT NULL,
  target       TEXT    NOT NULL,
  status       TEXT    NOT NULL, -- STARTED|SUCCESS|FAILED|ROLLED_BACK
  error        TEXT
);

CREATE TABLE IF NOT EXISTS file_snapshot (
  id            INTEGER PRIMARY KEY AUTOINCREMENT,
  action_id     INTEGER NOT NULL,
  path          TEXT    NOT NULL,
  kind          TEXT    NOT NULL, -- FULL_BLOB|DIFF
  content       BLOB    NOT NULL, -- pre-image
  content_hash  TEXT,
  FOREIGN KEY(action_id) REFERENCES action_log(id)
);

CREATE INDEX IF NOT EXISTS idx_file_snapshot_action_id
  ON file_snapshot(action_id);
```

### Rollback algorithm (data-level)
- `SELECT * FROM file_snapshot WHERE action_id = ?` → restore `content` to `path`
- `UPDATE action_log SET status='ROLLED_BACK' WHERE id=?`

## Server: PostgreSQL (userver storages::postgres)

### Goals
- Store users and secure password hashes.
- Store per-user settings (system prompts, MCP presets) for sync across devices.

### Baseline tables (DDL sketch)

```sql
CREATE TABLE IF NOT EXISTS users (
  id             UUID PRIMARY KEY,
  email          TEXT NOT NULL UNIQUE,
  password_hash  TEXT NOT NULL,
  created_at     TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS user_settings (
  user_id        UUID PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
  system_prompts JSONB NOT NULL DEFAULT '[]'::jsonb,
  mcp_presets    JSONB NOT NULL DEFAULT '{}'::jsonb,
  updated_at     TIMESTAMPTZ NOT NULL DEFAULT now()
);
```

