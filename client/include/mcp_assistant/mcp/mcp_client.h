#pragma once

#include "imcp_transport.h"

#include <QHash>
#include <QJsonObject>
#include <QTimer>

namespace aura::mcp_assistant::mcp {

/// JSON-RPC 2.0 oriented client with reconnect policy and streaming helpers.
class McpClient final : public QObject {
  Q_OBJECT

 public:
  explicit McpClient(IMcpTransport* transport, QObject* parent = nullptr);

  void SetTransport(IMcpTransport* transport);

  void ConfigureReconnect(bool enabled, int initial_delay_ms, int max_delay_ms);

  [[nodiscard]] bool IsConnected() const;

 public slots:
  void Connect();
  void Disconnect();
  void CancelInflight();

  /// Sends a minimal JSON-RPC request; id is managed internally.
  void SendRequest(QString method, QJsonObject params);

  /// Simulated agent reply for UX plumbing when no server is attached.
  void EmitDemoAssistantSequence(QString user_text);

 signals:
  void ConnectionStateChanged(bool connected);
  void AssistantDelta(QString chunk);
  void AssistantMessageCompleted(QString full_text);
  /// Emitted after `messages/prompt` with the JSON **result** object (diff / flags).
  void PromptResult(QJsonObject result);
  void ToolCallReceived(QString tool_name, QJsonObject arguments);
  void StreamingFinished();
  void JsonRpcError(int code, QString message, QJsonValue id);
  void TransportMessage(QString info);
  void TokenUsageEstimate(int prompt_tokens, int completion_tokens);
  void ScheduleReconnectIn(int milliseconds);

 private slots:
  void OnTransportConnected();
  void OnTransportDisconnected(QString reason);
  void OnTransportJson(const QJsonObject& object);
  void OnTransportError(QString message);
  void OnReconnectTimer();
  void OnStreamChunkTimer();

 private:
  void HandleIncomingObject(const QJsonObject& object);
  void ResetReconnectBackoff();
  void IncreaseReconnectBackoff();

  IMcpTransport* transport_{nullptr};
  bool reconnect_enabled_{true};
  int reconnect_delay_ms_{1000};
  int reconnect_max_ms_{30000};
  int reconnect_current_ms_{1000};
  QTimer reconnect_timer_;

  int next_jsonrpc_id_{1};
  QHash<int, QString> pending_methods_;

  /// Demo streaming buffer.
  QString stream_buffer_;
  QTimer stream_timer_;
  int stream_cursor_{0};
};

}  // namespace aura::mcp_assistant::mcp
