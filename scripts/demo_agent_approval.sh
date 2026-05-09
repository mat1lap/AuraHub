#!/usr/bin/env bash
# Полный walkthrough: Postgres → AuraCloud → GUI → эмуляция агента (HTTP + MCP edit_file).
# Использование: из корня репозитория: ./scripts/demo_agent_approval.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

CLIENT_BUILD="${AURAHUB_CLIENT_BUILD:-${REPO_ROOT}/build_client}"
SERVER_BUILD="${AURAHUB_SERVER_BUILD:-${REPO_ROOT}/build_userver2}"

GUI="${CLIENT_BUILD}/client/auraclient_gui"
MCP_HOST="${CLIENT_BUILD}/client/auraclient_mcp_host"
SERVER="${SERVER_BUILD}/server/auracloud_userver"
CONFIG="${REPO_ROOT}/server/configs/static_config.yaml"
COMPOSE="${REPO_ROOT}/server/dev/docker-compose.yml"

for bin in "$GUI" "$MCP_HOST" "$SERVER"; do
  if [[ ! -x "$bin" ]]; then
    echo "Не найден исполняемый файл: $bin" >&2
    echo "Соберите клиент и сервер, например:" >&2
    echo "  cmake -S \"${REPO_ROOT}\" -B \"${CLIENT_BUILD}\" -DAURAHUB_BUILD_SERVER=OFF -DAURAHUB_BUILD_TESTS=OFF" >&2
    echo "  cmake --build \"${CLIENT_BUILD}\" --target auraclient_gui auraclient_mcp_host" >&2
    echo "  cmake -S \"${REPO_ROOT}\" -B \"${SERVER_BUILD}\" -DAURAHUB_BUILD_USERVER=ON -DAURAHUB_BUILD_CLIENT=OFF -DAURAHUB_BUILD_TESTS=OFF" >&2
    echo "  cmake --build \"${SERVER_BUILD}\" --target auracloud_userver" >&2
    exit 1
  fi
done

export AURAHUB_PG_DSN="${AURAHUB_PG_DSN:-postgresql://aurahub:aurahub@localhost:15432/aurahub}"

cleanup() {
  if [[ -n "${SERVER_PID:-}" ]] && kill -0 "$SERVER_PID" 2>/dev/null; then
    kill "$SERVER_PID" 2>/dev/null || true
    wait "$SERVER_PID" 2>/dev/null || true
  fi
  if [[ -n "${GUI_PID:-}" ]] && kill -0 "$GUI_PID" 2>/dev/null; then
    kill "$GUI_PID" 2>/dev/null || true
  fi
}
trap cleanup EXIT

echo "=== 1) Клиент (GUI) ==="
if [[ -n "${DISPLAY:-}" || -n "${WAYLAND_DISPLAY:-}" ]]; then
  "$GUI" &
  GUI_PID=$!
  echo "Запущен auraclient_gui (PID $GUI_PID)."
else
  echo "DISPLAY/WAYLAND не задан — окно GUI не запускаю. Откройте клиент вручную на машине с графикой:"
  echo "  \"$GUI\""
fi

echo ""
echo "=== 2) Сервер (AuraCloud) + Postgres ==="
if command -v docker >/dev/null 2>&1; then
  docker compose -f "$COMPOSE" up -d
  echo "Postgres (docker compose) поднят."
else
  echo "Docker не найден — убедитесь, что Postgres доступен по AURAHUB_PG_DSN."
fi

"$SERVER" -c "$CONFIG" &
SERVER_PID=$!
echo "Запущен auracloud_userver (PID $SERVER_PID), ожидание порта 8080..."
for _ in $(seq 1 80); do
  code="$(curl -s -o /dev/null -w '%{http_code}' --connect-timeout 1 "http://127.0.0.1:8080/" || true)"
  if [[ -n "$code" && "$code" != "000" ]]; then
    break
  fi
  sleep 0.25
done

echo ""
echo "=== 3) Эмуляция агента (облако): регистрация, логин, заглушка LLM ==="
curl -sS -X POST "http://127.0.0.1:8080/v1/auth/register" \
  -H "Content-Type: application/json" \
  -d '{"email":"demo_agent@walkthrough.local","password":"DemoAgent2026"}' || true
echo ""
curl -sS -X POST "http://127.0.0.1:8080/v1/auth/login" \
  -H "Content-Type: application/json" \
  -d '{"email":"demo_agent@walkthrough.local","password":"DemoAgent2026"}'
echo ""
curl -sS -X POST "http://127.0.0.1:8080/v1/llm/proxy" \
  -H "Content-Type: application/json" \
  -d '{"provider":"stub","messages":[{"role":"user","content":"hello from emulated agent"}]}'
echo ""

echo ""
echo "=== 4–5) Эмуляция агента (локальный MCP): edit_file — смотрите окно клиента ==="
echo "В очереди одобрения появится запрос. Нажмите «Одобрить запись» или «Отклонить»."
echo "Пока не решите, эта команда будет ждать (до таймаута одобрения)."
echo ""

DEMO_FILE="${TMPDIR:-/tmp}/aura_agent_demo_$$.txt"
rm -f "$DEMO_FILE"

PAYLOAD=$(printf '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"edit_file","arguments":{"path":"%s","content":"written after approval\\n"}}}' "$DEMO_FILE")
printf '%s\n' "$PAYLOAD" | "$MCP_HOST"
echo ""
echo "Ответ MCP выше. Файл демо (если одобрили):"
if [[ -f "$DEMO_FILE" ]]; then
  cat "$DEMO_FILE"
else
  echo "(файл не создан — вероятно отклонено или ошибка)"
fi

echo ""
echo "Готово. Остановка фоновых процессов при выходе из скрипта."
