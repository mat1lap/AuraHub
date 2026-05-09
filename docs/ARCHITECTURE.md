# AuraHub Architecture

## Scope

AuraHub is a cross-platform desktop GUI for local AI agents (a visual alternative to CLI agents). Agents analyze code, run shell commands, and edit files via MCP (Model Context Protocol).

Killer feature: a **local Time Machine** (rollback engine) that traces all AI actions and can **atomically undo** any filesystem changes.

This document is the executable version of the architectural brief (ТЗ). It defines strict boundaries, responsibilities, and data contracts.

## Components & Responsibilities

### AuraClient (`/client`)
- **Role**: “Hands and Eyes”. UI + local execution.
- **Tech**: C++20, Qt 6 Widgets, Qt SQL (SQLite).
- **Main storage**: SQLite (local cache + rollback journal).
- **Responsibilities**:
  - UI rendering and user interactions.
  - Running tools locally (shell, read/write files) via AuraMCP.
  - Recording **every AI action** into local DB.
  - Persisting snapshots/diffs **before** file modifications.
  - Performing local rollback/undo.
  - Communicating with server via REST (`NetworkGateway`), never directly to PostgreSQL.

### AuraCloud (`/server`)
- **Role**: “Brain and Safe”. Infrastructure & secrets.
- **Tech**: C++ (userver), PostgreSQL.
- **Main storage**: PostgreSQL.
- **Responsibilities**:
  - Authentication (registration/login), issuing JWT.
  - Storing password hashes (bcrypt / SHA-3 as policy).
  - Secure LLM API proxying (server holds provider API keys; client never stores them).
  - Syncing user settings (system prompts library, MCP presets, telemetry if needed).

### AuraMCP (`/mcp`)
- **Role**: Local MCP implementation used by the client.
- **Constraints (strict)**:
  - **No exceptions in runtime**: errors must be returned as `ToolResult{isError=true}`.
  - **Lock-free hot path**: no `std::mutex` in execute/read phase; state is immutable after init.
  - **Parsing**: use `jsonrpc::JsonRpcParser` with `std::variant` + `std::visit`. Avoid raw `nlohmann::json` parsing in router.
  - **Transport**: `StdioTransport::start()` must be **blocking** (main thread loop) to prevent early process exit.

## Client Architecture (MVP)

AuraClient is **strict MVP**.

### View
- Qt Widgets, “dumb” UI.
- Uses `QAbstractTableModel` / `QSortFilterProxyModel` for action history and logs.
- No business logic, no DB access, no networking.
- Emits signals / forwards user intent to Presenter.

### Presenter
- Coordinates View and Model.
- Examples:
  - User clicks “Rollback” → Presenter reads selected action id → calls Model rollback → updates view with result.
  - User starts an agent run → Presenter invokes Model execution and streams log entries to View.

### Model
Contains the application logic and all side-effects.
- **`McpEngine`**: runs AuraMCP tools, reads local resources, packages context.
- **`LocalDbService` (SQLite)**: action journal + file snapshots/diffs.
- **`NetworkGateway`**: REST client to AuraCloud (auth/sync/llm proxy).

## Time Machine (Rollback Engine)

### Invariants
- Every tool call that **mutates the filesystem** MUST:
  - snapshot the pre-image (blob or diff) **before** writing.
  - write an `ActionLog` entry.
  - link snapshots to the action id.
- Rollback is defined per action id and must be **atomic** from the user perspective:
  - either the original content is restored, or nothing is changed.

### Sequence: edit_file + rollback
1. Agent decides to modify `main.cpp` via MCP tool `edit_file`.
2. **Pre-hook snapshot**: Model reads current content and stores it in SQLite (`FileSnapshot` blob or diff).
3. **ActionLog**: insert (`tool="edit_file"`, `target="main.cpp"`, status transitions).
4. Apply modification to filesystem.
5. On UI rollback:
   - Presenter calls `Model.rollback(action_id)`.
   - Model fetches snapshot and restores file content.
   - Model marks action as `ROLLED_BACK` in `ActionLog`.

## Data Contracts

### SQLite (client)
Minimal baseline tables (migration-managed):
- `action_log`
  - `id` (INTEGER PK)
  - `ts_utc` (INTEGER) unix epoch ms
  - `tool` (TEXT) e.g. `edit_file`, `run_shell`
  - `target` (TEXT) e.g. path, command
  - `status` (TEXT) enum: `STARTED`, `SUCCESS`, `FAILED`, `ROLLED_BACK`
  - `error` (TEXT nullable)
- `file_snapshot`
  - `id` (INTEGER PK)
  - `action_id` (INTEGER FK → `action_log.id`)
  - `path` (TEXT)
  - `kind` (TEXT) enum: `FULL_BLOB` (baseline), `DIFF` (future)
  - `content` (BLOB) pre-image bytes
  - `content_hash` (TEXT) optional integrity

Notes:
- Start with `FULL_BLOB` for correctness; add diffs later as optimization.
- Storing `content_hash` allows validation before restore.

### PostgreSQL (server)
Baseline conceptual schema:
- `users`:
  - `id` UUID PK
  - `email` unique
  - `password_hash`
  - `created_at`
- `user_settings`:
  - `user_id` FK
  - `system_prompts` JSONB
  - `mcp_presets` JSONB
  - `updated_at`

## REST API (client ↔ server)

All payloads are JSON.

Baseline endpoints:
- `POST /v1/auth/register`
- `POST /v1/auth/login` → returns JWT
- `GET /v1/settings` (auth)
- `PUT /v1/settings` (auth)
- `POST /v1/llm/proxy` (auth) → server injects provider key

## Testing
- Unit tests for Model and parsers using GoogleTest (`/tests`).
- Server tests follow userver testing approach (to be added when server skeleton exists).

