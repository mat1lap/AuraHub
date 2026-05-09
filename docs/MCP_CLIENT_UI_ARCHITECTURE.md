# AuraHub MCP Client — Production GUI Architecture (Qt6 / C++20)

**Audience:** senior engineers implementing a desktop **AI assistant + MCP** client with **chat UX**, **human approval for edits**, **checkpoints**, and **rollback**.

**Branch:** `feature/mcp-client-ui`  
**Stack:** Qt 6.x, C++20, CMake 3.22+, optional KDE Frameworks for syntax highlighting.

---

## 0. Relationship to existing AuraHub code

Today AuraHub already contains:

- **AuraMCP** (`mcp/`) — JSON-RPC router, stdio transport contract.
- **Local Time Machine** (`client/` + SQLite) — snapshots before file edits; approval queue for `edit_file`.

The **new MCP Client GUI** described here is a **product-grade shell** around those ideas:

- **Chat** becomes the primary UX (Cursor / VS Code Chat-like).
- **Approvals + checkpoints + rollback** become first-class **conversation messages** and **sidebar history**.

Integration strategy:

1. **Phase 1:** UI + models + mock MCP service (deterministic replay).
2. **Phase 2:** Wire `McpClient` to **stdio transport** compatible with existing `auraclient_mcp_host` / AuraMCP protocol.
3. **Phase 3:** Unify **checkpoint storage** with existing SQLite snapshot tables or migrate to **git-native** checkpoints when workspace is a git repo.

---

## 1. Architectural style: MVVM (Qt-flavored) + Clean boundaries

### Why MVVM (not “classic MVP” for this UI)

| Concern | MVP (AuraHub demo today) | MVVM (recommended here) |
|--------|---------------------------|---------------------------|
| Chat list with roles, delegates, filtering | Presenter grows huge | **ViewModel** exposes `QAbstractListModel` + roles |
| Multiple surfaces (chat, diff, checkpoints) | View talks to one Presenter | **Multiple ViewModels** + coordinators |
| Qt data binding | Manual glue | Models + roles map cleanly to QML later if needed |

**Definition for this project:**

- **Model (domain):** pure C++ types (`ChatSession`, `Checkpoint`, `PendingChange`, `McpEnvelope`) — no Qt in core types if feasible; Qt only at edges.
- **ViewModel:** `QAbstractListModel` subclasses, `QObject` glue, exposes commands (`submitPrompt`, `approveChange`, `rollback`).
- **View:** `QMainWindow`, `QListView`, delegates, widgets — **no business rules**.

**Coordinator / Application Service:** `AssistantSessionController` wires ViewModels to services (`McpClient`, `CheckpointManager`, `WorkspaceState`).

This avoids **god objects** (`MainWindow` doing MCP + git + chat).

---

## 2. Project layout (CMake + directories)

Recommended repository layout (fits AuraHub `client/`):

```text
client/
  CMakeLists.txt                       # adds target aurahub_mcp_assistant when Qt6 found
  include/mcp_assistant/
    app/application.h
    session/assistant_session_controller.h
    chat/chat_messages_model.h
    chat/chat_delegate.h
    chat/chat_message.h                # domain value types + QVariant roles
    checkpoint/checkpoint_list_model.h
    checkpoint/checkpoint_manager.h
    workspace/workspace_root.h
    diff/diff_view_model.h
    diff/diff_preview_widget.h
    mcp/mcp_client.h
    mcp/imcp_transport.h
    mcp/stdio_mcp_transport.h
    mcp/websocket_mcp_transport.h      # optional / future
    services/approval_flow_coordinator.h
  src/mcp_assistant/
    app/application.cpp
    session/assistant_session_controller.cpp
    chat/chat_messages_model.cpp
    chat/chat_delegate.cpp
    checkpoint/checkpoint_list_model.cpp
    checkpoint/checkpoint_manager.cpp
    diff/diff_preview_widget.cpp
    mcp/mcp_client.cpp
    mcp/stdio_mcp_transport.cpp
    ui/main_window.cpp
    ui/main_window.h
    main.cpp
resources/
  mcp_assistant.qrc                    # icons, dark palette JSON
```

**CMake target:** `aurahub_mcp_assistant` (executable), links:

- `Qt6::Widgets`, `Qt6::Concurrent` (or `Qt6::Network` if WS), optional `KF6::SyntaxHighlighting`.

---

## 3. Full architecture (layers)

```text
┌───────────────────────────────────────────────────────────────┐
│                         Views (Qt Widgets)                     │
│ MainWindow, ChatListView, CheckpointSidebar, DiffDock/Panel    │
└───────────────▲───────────────────────────────────────────────┘
                │ signals/slots, model reset/layoutChanged
┌───────────────┴───────────────────────────────────────────────┐
│                      ViewModels (QObject)                      │
│ ChatMessagesModel, CheckpointListModel, DiffViewModel            │
└───────────────▲───────────────────────────────────────────────┘
                │
┌───────────────┴───────────────────────────────────────────────┐
│               AssistantSessionController                       │
│ - orchestrates user intent                                     │
│ - owns session state machine                                   │
└───────▲───────────────────────┬───────────────────────────────┘
        │                       │
┌───────┴──────────┐   ┌────────┴────────────┐
│    McpClient      │   │ CheckpointManager   │
│ (async IO + parse)│   │ (git/fs/sqlite)    │
└───────▲──────────┘   └────────▲────────────┘
        │                       │
┌───────┴───────────────────────┴───────────────────────────────┐
│ IMcpTransport (stdio / websocket), Filesystem/Git adapters       │
└─────────────────────────────────────────────────────────────────┘
```

---

## 4. Chat UI: QListView + QAbstractListModel + **custom delegate** (recommended)

### Choice

**Use `QListView` + `QAbstractListModel` + `QStyledItemDelegate` (subclass)** for the chat stream.

**Why not a vertical `QScrollArea` full of widgets per message?**

- **Performance:** long transcripts → hundreds/thousands of bubbles; QListView **virtualizes** painting.
- **Consistency:** selection, keyboard nav, accessibility bridges easier.
- **Theming:** delegate paints bubbles + code chips; optional overlay widgets via **IndexWidget** only for rare interactive rows (approval rows).

**Why not QML ListView for v1?**

- You asked for Widgets-first desktop; QML adds separate styling pipeline. MVVM models are reusable if you later add `qtquick` UI.

### Message type hierarchy (domain + roles)

**Domain types** (non-QObject; stored in model internal vector):

```cpp
// chat/chat_message.h (conceptual)
enum class ChatAuthor { User, Agent, System };
enum class ChatSurfaceKind { Bubble, ApprovalCard, CheckpointCard, ToolStatus };

struct ChatMessageId {
  qint64 value{};
};

struct ChatMessage {
  ChatMessageId id{};
  qint64 created_epoch_ms{};
  ChatAuthor author{ChatAuthor::Agent};
  ChatSurfaceKind kind{ChatSurfaceKind::Bubble};
  QString markdown_body;            // UTF-16 in Qt
  QString short_title;              // one-line for checkpoints
  std::optional<PendingChangeId> pending_change; // links approval workflow
  std::optional<CheckpointId> checkpoint_ref;
};
```

**Model exposes roles:**

```cpp
namespace ChatRoles {
inline constexpr int BodyMarkdownRole = Qt::UserRole + 1;
inline constexpr int AuthorRole      = Qt::UserRole + 2;
inline constexpr int SurfaceKindRole   = Qt::UserRole + 3;
inline constexpr int CreatedMsRole     = Qt::UserRole + 4;
inline constexpr int PendingChangeRole = Qt::UserRole + 5;
inline constexpr int CheckpointRole    = Qt::UserRole + 6;
}
```

### Markdown rendering strategy (production realistic)

Options:

| Approach | Pros | Cons |
|---------|------|------|
| **QTextDocument + subset Markdown parser** | Control, offline | Need fenced code rules |
| **QMarkdown** (third-party) | Faster time-to-market | Dependency audit |
| **WebEngine** | Full MD | Heavy, security surface |

**Recommendation:** Phase 1 render as **plain QTextDocument** with a tiny Markdown subset (headings, bold, code fences). Phase 2 integrate **cmark** or **MD4C** C library via CMake `FetchContent`.

**Code highlighting inside fences:**

- Best engineering tradeoff for Widgets: **KF Syntax Highlighting** (`KSyntaxHighlighting::SyntaxHighlighter` on `QTextDocument`).
- If KDE deps unacceptable: ship **Kate syntax definitions** subset + simple highlighter.

---

## 5. Main window regions (widgets)

```text
┌───────────────┬───────────────────────────────────────────────┐
│ Checkpoints   │ Chat stream                                    │
│ ( QListView ) │ ( QListView + ChatDelegate )                   │
│               │                                                │
│ [Rollback]    │                                                │
└───────────────┴───────────────────────────────────────────────┘
│ Prompt multi-line (QPlainTextEdit or QTextEdit) [ Send ] [Stop]│
│ Status bar: connection • cwd • branch • tokens • errors        │
└────────────────────────────────────────────────────────────────┘
```

**Keyboard:**

- `Enter` sends (guard: not IME composing).
- `Shift+Enter` inserts newline — implement in `eventFilter` on prompt widget.

---

## 6. MCP integration

### Protocol reality check

Most MCP deployments today are **JSON-RPC over stdio** (line-delimited NDJSON not universal; AuraMCP reads lines — align transports).

### Class diagram (core)

```text
IMcpTransport
 ├─ StdioMcpTransport (QProcess + QIODevice)
 └─ WebSocketMcpTransport (QTcpSocket upgrade / QWebSocket)

McpClient
 ├─ sendRequest(JsonRpcRequest)
 ├─ notifications stream (optional)
 └─ reconnect policy
```

### IMcpTransport interface

```cpp
// mcp/imcp_transport.h
#pragma once
#include <QObject>
#include <QJsonObject>

class IMcpTransport : public QObject {
  Q_OBJECT
 public:
  explicit IMcpTransport(QObject* parent = nullptr) : QObject(parent) {}

  virtual bool connectToServer() = 0;
  virtual void disconnectFromServer() = 0;
  virtual bool isConnected() const = 0;

  // Writes one JSON-RPC message (already framed per transport)
  virtual void sendJson(const QJsonObject& json) = 0;

 signals:
  void connected();
  void disconnected(QString reason);
  void jsonReceived(QJsonObject obj);
  void transportError(QString message);
};
```

### Stdio transport sketch

```cpp
// mcp/stdio_mcp_transport.h
#pragma once
#include "imcp_transport.h"
#include <QProcess>

class StdioMcpTransport final : public IMcpTransport {
  Q_OBJECT
 public:
  explicit StdioMcpTransport(QObject* parent = nullptr);

  void setCommand(QString program, QStringList arguments, QString workingDirectory);

  bool connectToServer() override;
  void disconnectFromServer() override;
  bool isConnected() const override;
  void sendJson(const QJsonObject& json) override;

 private slots:
  void onReadyReadStdout();
  void onProcessFinished(int exit_code, QProcess::ExitStatus status);

 private:
  QProcess process_;
  QByteArray read_buffer_;
};
```

**Line framing:** append bytes to buffer; split on `\n`; each line `QJsonDocument::fromJson`.

### McpClient responsibilities

- Serialize IDs (`jsonrpc`, `id`).
- Maintain **request table** `QHash<int, PendingCall>` for correlated responses.
- **Tool calls:** translate `tools/call` results into domain events (`ToolCallFinished`).
- **Streaming:** If server supports NDJSON partial lines, parse incremental; else simulate streaming by chunking assistant text at UI layer.

```cpp
// mcp/mcp_client.h (signals)
signals:
  void assistantDelta(QString chunk);           // UI typing effect
  void assistantCompleted(QString full_text);
  void toolCallRequested(QString tool, QJsonObject args);
  void toolCallProgress(QString message);
  void connectionStateChanged(bool ok);
```

### Async pattern

- **Never block GUI thread.** Use `QProcess` async signals.
- Heavy parsing → `QtConcurrent::run` **only if profiling proves need**; start simple on GUI thread for JSON sizes typical of MCP.

### Reconnect logic

State machine:

```
Disconnected → Connecting → Connected → Degraded (retry backoff)
```

Expose:

```cpp
void McpClient::scheduleReconnect();
```

Use `QTimer` exponential backoff (cap 30s), jitter.

---

## 7. Approval workflow + diff preview

### Sequence

```text
User prompt → MCP agent proposes patch → PendingChange created
   → Chat inserts ApprovalMessage row
   → DiffPreviewPanel binds to PendingChange
User Accept → CheckpointManager.createCheckpoint(...)
           → apply patch (or invoke tool confirmation)
User Reject → discard pending artifacts
```

### Coordinator

```cpp
class ApprovalFlowCoordinator : public QObject {
  Q_OBJECT
 public:
  explicit ApprovalFlowCoordinator(McpClient* mcp,
                                   CheckpointManager* cp,
                                   QObject* parent = nullptr);

 public slots:
  void beginPendingChange(PendingChange draft);
  void acceptPending(PendingChangeId id);
  void rejectPending(PendingChangeId id);

 signals:
  void pendingChangeUpdated(PendingChangeId id);
};
```

### Diff preview widget

**Recommendation:** Two-pane diff:

- Left: `QPlainTextEdit` read-only (before)
- Right: `QPlainTextEdit` read-only (after)
- Sync scroll: connect `verticalScrollBar()->valueChanged` both ways **with guard flag** to avoid feedback loops.

For multi-file: `QTreeWidget` file list + stacked diff pages.

**KSyntaxHighlighting** applies separately to each side.

---

## 8. Checkpoint system — storage strategy

### Options tradeoffs

| Strategy | Pros | Cons |
|---------|------|------|
| **Git commits / refs** (`refs/aura/checkpoint/<id>`) | Exact, compact, familiar rollback | Requires git repo; conflict UX |
| **Filesystem tarball snapshots** | Works anywhere | Large, slow |
| **Patch series / reverse patches** | Storage cheap | Complex merges |
| **SQLite blobs** (AuraHub today) | Nice metadata | Not ideal for huge trees |

### Recommendation (production)

**Hybrid:**

1. If workspace root has `.git` → **git-based checkpoint**:
   - Create **git stash** or **WIP commit** on branch `aura/checkpoints`.
   - Store pointer: commit SHA + optional stash ref.
2. Else → **filesystem snapshot** using **hardlink tree copy** (Linux `cp -al`) or `rsync --link-dest` for speed.

Checkpoint record:

```cpp
struct Checkpoint {
  CheckpointId id{};
  qint64 created_epoch_ms{};
  QString title;
  QString user_prompt_excerpt;
  StorageHandle storage; // variant: GitSha / FsSnapshotPath
};
```

This aligns with Cursor-like UX (git-aware).

---

## 9. Rollback flow

1. User clicks rollback in sidebar **or** inline checkpoint card.
2. `QMessageBox::question` confirmation.
3. `CheckpointManager::rollbackAsync(CheckpointId)` runs in worker thread via `QtConcurrent::run`:

   - Git mode: `git reset --hard <sha>` **or** safer `git checkout <sha> -- .` depending policy.
   - FS mode: restore snapshot directory.

4. Emit `rollbackFinished(bool, QString error)` → Chat inserts `SystemMessage`.

**Safety:**

- Never delete checkpoints automatically.
- Keep **dry-run** mode in dev builds (`git status --porcelain` must be empty or warn).

---

## 10. Signals / slots map (essential interactions)

```text
PromptWidget::sendRequested(QString markdown)
   → AssistantSessionController::submitUserMessage(...)
       → ChatMessagesModel::appendUser(...)
       → McpClient::sendPrompt(...)

McpClient::toolCallRequested(...)
   → ApprovalFlowCoordinator::beginPendingChange(...)
       → ChatMessagesModel::appendApprovalCard(...)
       → DiffViewModel::loadPending(...)

DiffPreviewWidget::accepted()
   → ApprovalFlowCoordinator::acceptPending(...)
       → CheckpointManager::createCheckpoint(...)
       → ChatMessagesModel::appendCheckpointCard(...)

CheckpointSidebar::rollbackRequested(CheckpointId)
   → CheckpointManager::rollbackAsync(...)
       → ChatMessagesModel::appendSystem(...)
```

---

## 11. Modern UI / dark theme

Use **Qt Fusion + dark palette** + **stylesheet tokens**:

```cpp
// Pseudocode in Application::setupTheme()
QApplication::setStyle("Fusion");
QPalette dark = …;
qApp->setPalette(dark);
qApp->setStyleSheet(load(":/themes/dark.qss"));
```

Add:

- **Typing indicator:** extra row in model `SurfaceKind::TypingIndicator` or overlay widget at bottom of chat.
- **Cancel:** `McpClient::cancelInflight()` kills pending JSON-RPC / closes tool await (define semantics).
- **Token usage:** small QLabel in status bar; updated from MCP metadata if available else hidden.
- **Reconnect badge:** `QLabel` with stylesheet pill when `McpClient` state degraded.

---

## 12. MVP implementation milestones

### Milestone A — skeleton

- `MainWindow` layout。
- Empty models + stub `McpClient` returning canned assistant replies.

### Milestone B — chat quality

- Delegate painting bubbles + timestamps.
- Markdown subset.

### Milestone C — MCP stdio

- Connect to user-provided command line (`Settings`: program + args + cwd).

### Milestone D — approvals + diff

- Pending change entity + diff widget.

### Milestone E — checkpoints + rollback

- Git detection + checkpoint creation + rollback worker.

---

## 13. Thread safety rules

- **Models mutate only on GUI thread.**
- Background workers emit signals with results; controller applies to models via `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` if needed.
- Shared pointers: `std::shared_ptr<const SnapshotManifest>` immutable after publish.

---

## 14. Example: ChatMessagesModel skeleton

```cpp
// chat_messages_model.h
#pragma once
#include <QAbstractListModel>
#include <vector>
#include "chat_message.h"

class ChatMessagesModel final : public QAbstractListModel {
  Q_OBJECT
 public:
  explicit ChatMessagesModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;

  Q_INVOKABLE void appendUserMessage(QString markdown);
  Q_INVOKABLE void appendAgentMessage(QString markdown);
  Q_INVOKABLE void appendSystemMessage(QString text);
  Q_INVOKABLE void appendCheckpointMessage(const Checkpoint& cp);

 private:
  std::vector<ChatMessage> messages_;
};
```

---

## 15. Risk register (production)

- **Markdown XSS:** never use `QWebEngine` with untrusted HTML; sanitize links.
- **Git rollback destructive:** confirm dialogs; optional shadow branch.
- **Large repos:** checkpoint creation async with progress bar.

---

## 16. Summary choices

| Topic | Choice |
|------|--------|
| UI architecture | **MVVM** (Qt models + controllers) |
| Chat rendering | **QListView + model + delegate** |
| MCP IO | **Async QProcess stdio** first |
| Checkpoints | **Git-native if repo**, else FS snapshots |
| Diff / highlighting | **KF Syntax Highlighting** if allowed; else QTextDocument |
| Approvals | Coordinator + dedicated chat surface kinds |

---

**Next engineering step in repo:** add CMake target skeleton `aurahub_mcp_assistant` with empty `MainWindow` + `ChatMessagesModel` stub wired — implementation PR after this design freeze.
