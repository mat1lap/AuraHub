## Local development

### Build (all targets)

```bash
cmake -S . -B build2 -DCMAKE_BUILD_TYPE=Debug
cmake --build build2 -j
ctest --test-dir build2 --output-on-failure
```

### AuraCloud (userver + Postgres) local run

#### Prerequisites

- Docker (for local Postgres)
- `pkg-config`/`pkgconf` installed (userver build uses it)
- A working PostgreSQL client dev setup (`libpq` headers/libs) compatible with your distro

#### Start PostgreSQL (Docker)

From repo root:

```bash
docker compose -f server/dev/docker-compose.yml up -d
```

This starts Postgres on `localhost:15432` and initializes schema from:
- `server/postgres/init/001_schema.sql`

#### Run service

Set DSN and start the binary:

```bash
export AURAHUB_PG_DSN="postgresql://aurahub:aurahub@localhost:15432/aurahub"
cmake -S . -B build_userver -DCMAKE_BUILD_TYPE=Debug -DAURAHUB_BUILD_USERVER=ON
cmake --build build_userver --parallel 1
./build_userver/server/auracloud_userver -c server/configs/static_config.yaml
```

Server listens on `localhost:8080`.

#### API examples

- Register:

```bash
curl -sS -X POST "http://localhost:8080/v1/auth/register" \
  -H "Content-Type: application/json" \
  -d '{"email":"user@example.com","password":"pass123"}'
```

- Login:

```bash
curl -sS -X POST "http://localhost:8080/v1/auth/login" \
  -H "Content-Type: application/json" \
  -d '{"email":"user@example.com","password":"pass123"}'
```

- Settings (MVP auth via `X-Aura-User` header):

```bash
curl -sS "http://localhost:8080/v1/settings" -H "X-Aura-User: user@example.com"
```

```bash
curl -sS -X PUT "http://localhost:8080/v1/settings" \
  -H "Content-Type: application/json" \
  -H "X-Aura-User: user@example.com" \
  -d '{"system_prompts":"[]","mcp_presets":"{}"}'
```

- LLM proxy (stub):

```bash
curl -sS -X POST "http://localhost:8080/v1/llm/proxy" \
  -H "Content-Type: application/json" \
  -d '{"provider":"stub","prompt":"hello"}'
```

### Notes / current MVP limitations
- `auracloud_userver` currently uses a minimal token model (not JWT yet).
- `settings` endpoint uses `X-Aura-User` header as MVP auth; it will be replaced by JWT bearer auth.
- LLM proxy is a stub (echoes request).

### Full-stack walkthrough (GUI + server + emulated agent + approve/reject)

See [AGENT_APPROVAL_WALKTHROUGH.md](./AGENT_APPROVAL_WALKTHROUGH.md) and `scripts/demo_agent_approval.sh`.

