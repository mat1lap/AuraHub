# MEMEX (repeat mistakes)

## userver static config / yaml-cpp: numeric scalars rejected

**Symptom:** Startup fails with messages like:
- `Error at path 'additionalProperties': Wrong type` while parsing schema YAML.
- `Value '4' ... must be integer` for `worker_threads`, `port`, or schema `minimum`/`maximum`.
- `Not representable as 'double'` / `'long unsigned int'` for values that look numeric in the YAML file.

**Cause:** With some toolchain / yaml-cpp builds, plain YAML scalars (e.g. `4`, `8080`, `minimum: 400`) are exposed to userver as **strings** (or otherwise not as strict int/double), so schema parsing and `As<std::size_t>()` / `As<uint16_t>()` / `As<double>()` on schema metadata fail.

**Established solution (AuraHub):**
1. After `FetchContent` populates userver v2.14, apply the bundled patch:
   `cmake/patches/userver_v2.14_aura_yaml_scalar_fixes.patch` from the repo root:
   `patch -p1 -d build/<dir>/_deps/userver-src < cmake/patches/userver_v2.14_aura_yaml_scalar_fixes.patch`
2. Rebuild `userver-universal`, `userver-core`, and `auracloud_userver`.

**Application code:** `components::Postgres` requires `TestsuiteSupport` and DNS client in the component list (see `server/src/userver/main.cpp`). Static config must include `testsuite-support: {}` and `dns-client` (see `server/configs/static_config.yaml`).

## Postgres Docker

**Symptom:** `docker compose` cannot reach `/var/run/docker.sock`.

**Cause:** Docker daemon not running or user not in `docker` group.

**Established solution:** `sudo systemctl start docker` (see project chat / LOCAL_DEV).

## userver Postgres: JSONB columns in SELECT

**Symptom:** Handler throws / HTTP 500; logs mention `jsonb` (oid 3802) and `doesn't have a parser` / `UnknownBufferCategory`.

**Cause:** `storages::postgres` row accessors expect C++ types that match DB types; raw `JSONB` is not always mapped the same as `TEXT`.

**Established solution:** In SQL, read JSONB as text and parse in code, e.g. `SELECT col::text ...` then `FromString(row["col"].As<std::string>())`. See `server/src/userver/handlers/settings.cpp`.

## SQLite approval queue: GUI vs MCP host must share one DB file

**Symptom:** Pending `edit_file` never appears in GUI while `auraclient_mcp_host` blocks; or tooling updates the wrong `aurahub.sqlite`.

**Cause:** `QStandardPaths::AppLocalDataLocation` depends on `QCoreApplication::organizationName` / `applicationName`. Different defaults ⇒ different directories.

**Established solution:** Set the same `QCoreApplication::setApplicationName("AuraHub")` and `setOrganizationName("AuraHub")` **before** constructing `QApplication` / `QCoreApplication` in every entrypoint (`main_gui.cpp`, `mcp_host_main.cpp`). Typical Linux path: `~/.local/share/AuraHub/AuraHub/aurahub.sqlite`.
