# Сценарий: клиент + сервер + «агент» + одобрение в GUI

Ниже — ровно тот поток, который вы описали. **Одобрение правок файла** сейчас реализовано в **локальном** контуре (SQLite + `auraclient_mcp_host` + `auraclient_gui`). **AuraCloud** (порт 8080) в этом MVP — отдельная цепочка: регистрация, настройки, заглушка LLM; запросы на правку файла через облако пока **не** приходят в очередь одобрения клиента автоматически.

## Автоматизация

Из корня репозитория (после сборки `build_client` и `build_userver2`):

```bash
chmod +x scripts/demo_agent_approval.sh
./scripts/demo_agent_approval.sh
```

Скрипт поднимает Postgres (Docker), сервер userver, GUI (если есть дисплей), выводит команды «облака» и затем блокирующий вызов MCP — в этот момент в окне клиента нужно нажать **Одобрить** или **Отклонить**.

## Ручной сценарий (пять шагов)

### 1. Запустили клиент

Десктопное окно с очередью одобрения и журналом:

```bash
./build_client/client/auraclient_gui
```

База локальных данных (типичный Linux-путь):  
`~/.local/share/AuraHub/AuraHub/aurahub.sqlite`  
(имя приложения задано в коде как `AuraHub`, чтобы GUI и `auraclient_mcp_host` открывали **один и тот же** файл.)

### 2. Запустили сервер (AuraCloud)

PostgreSQL (если ещё не поднят):

```bash
docker compose -f server/dev/docker-compose.yml up -d
export AURAHUB_PG_DSN="postgresql://aurahub:aurahub@localhost:15432/aurahub"
./build_userver2/server/auracloud_userver -c server/configs/static_config.yaml
```

Слушает `http://127.0.0.1:8080`.

### 3. Эмулируем работу агента

**А) «Облако»** — как будто агент синхронизирует аккаунт и дергает заглушку LLM:

```bash
curl -sS -X POST "http://127.0.0.1:8080/v1/auth/register" \
  -H "Content-Type: application/json" \
  -d '{"email":"walkthrough@demo.local","password":"DemoWalk2026"}'
curl -sS -X POST "http://127.0.0.1:8080/v1/auth/login" \
  -H "Content-Type: application/json" \
  -d '{"email":"walkthrough@demo.local","password":"DemoWalk2026"}'
curl -sS -X POST "http://127.0.0.1:8080/v1/llm/proxy" \
  -H "Content-Type: application/json" \
  -d '{"provider":"stub","messages":[{"role":"user","content":"ping"}]}'
```

**Б) «Локальный MCP»** — как будто агент вызывает инструмент `edit_file` (это и попадает в очередь одобрения):

```bash
printf '%s\n' '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"edit_file","arguments":{"path":"/tmp/aura_agent_demo.txt","content":"hello after approve\n"}}}' \
  | ./build_client/client/auraclient_mcp_host
```

Пока команда «висит», переходите к шагу 4.

### 4. В клиенте видим намерение агента и жмём одобрение или отказ

В блоке **«Очередь одобрения правок файлов»** появится строка с путём и превью **Было / Будет**.

- **Одобрить запись** — MCP завершится ответом с `"content":"ok"`, файл появится на диске.
- **Отклонить** — MCP получит JSON-RPC `error`, файл не меняется.

### 5. Обрабатываем результат и продолжаем работу

- В терминале с `auraclient_mcp_host` появится одна строка JSON — успех или ошибка.
- В **журнале действий** клиента после одобрения появится строка `edit_file` (и при откате можно вернуть файл из снимка).
- Можно снова выполнить шаг 3Б с другим содержимым — снова появится запрос на одобрение.

## Что дальше по продукту

Связка «ответ LLM на сервере → постановка `edit_file` в очередь клиента» потребует синхронизации AuraCloud ↔ AuraClient (например, задания на правку по сессии пользователя). Сейчас это два параллельных демо-слоя в одном сценарии.
