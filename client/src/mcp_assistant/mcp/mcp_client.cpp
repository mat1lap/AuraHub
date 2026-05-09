#include "mcp_assistant/mcp/mcp_client.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>

namespace aura::mcp_assistant::mcp {

McpClient::McpClient(IMcpTransport* transport, QObject* parent)
    : QObject(parent), transport_(transport) {
  reconnect_timer_.setSingleShot(true);
  connect(&reconnect_timer_, &QTimer::timeout, this, &McpClient::OnReconnectTimer);

  stream_timer_.setInterval(18);
  connect(&stream_timer_, &QTimer::timeout, this, &McpClient::OnStreamChunkTimer);

  if (transport_ != nullptr) {
    connect(transport_, &IMcpTransport::Connected, this, &McpClient::OnTransportConnected);
    connect(transport_, &IMcpTransport::Disconnected, this, &McpClient::OnTransportDisconnected);
    connect(transport_, &IMcpTransport::JsonObjectReceived, this,
            &McpClient::OnTransportJson);
    connect(transport_, &IMcpTransport::TransportError, this, &McpClient::OnTransportError);
  }
}

void McpClient::SetTransport(IMcpTransport* transport) {
  if (transport_ == transport) {
    return;
  }
  if (transport_ != nullptr) {
    disconnect(transport_, nullptr, this, nullptr);
  }
  transport_ = transport;
  if (transport_ != nullptr) {
    connect(transport_, &IMcpTransport::Connected, this, &McpClient::OnTransportConnected);
    connect(transport_, &IMcpTransport::Disconnected, this, &McpClient::OnTransportDisconnected);
    connect(transport_, &IMcpTransport::JsonObjectReceived, this,
            &McpClient::OnTransportJson);
    connect(transport_, &IMcpTransport::TransportError, this, &McpClient::OnTransportError);
  }
}

void McpClient::ConfigureReconnect(bool enabled, int initial_delay_ms, int max_delay_ms) {
  reconnect_enabled_ = enabled;
  reconnect_current_ms_ = initial_delay_ms;
  reconnect_delay_ms_ = initial_delay_ms;
  reconnect_max_ms_ = max_delay_ms;
}

bool McpClient::IsConnected() const {
  return transport_ != nullptr && transport_->IsConnected();
}

void McpClient::Connect() {
  if (transport_ == nullptr) {
    emit TransportMessage(QStringLiteral("No MCP transport configured."));
    emit ConnectionStateChanged(false);
    return;
  }
  const bool ok = transport_->Connect();
  emit ConnectionStateChanged(ok && transport_->IsConnected());
}

void McpClient::Disconnect() {
  reconnect_timer_.stop();
  if (transport_ != nullptr) {
    transport_->Disconnect();
  }
  emit ConnectionStateChanged(false);
}

void McpClient::CancelInflight() {
  stream_timer_.stop();
  stream_buffer_.clear();
  stream_cursor_ = 0;
  pending_methods_.clear();
  emit StreamingFinished();
}

void McpClient::SendRequest(QString method, QJsonObject params) {
  if (transport_ == nullptr || !transport_->IsConnected()) {
    emit TransportMessage(QStringLiteral("Cannot send: transport offline."));
    return;
  }
  const int id = next_jsonrpc_id_++;
  pending_methods_.insert(id, method);

  QJsonObject req;
  req.insert(QStringLiteral("jsonrpc"), QStringLiteral("2.0"));
  req.insert(QStringLiteral("id"), id);
  req.insert(QStringLiteral("method"), std::move(method));
  req.insert(QStringLiteral("params"), std::move(params));
  transport_->SendJsonObject(req);
}

void McpClient::EmitDemoAssistantSequence(QString user_text) {
  Q_UNUSED(user_text);
  CancelInflight();
  stream_buffer_ =
      QStringLiteral("Proposed patch for `parser.cpp`:\n\n")
      + QStringLiteral("```cpp\n// ...\n```\n\n")
      + QStringLiteral("Awaiting your approval before applying changes.");
  stream_cursor_ = 0;
  stream_timer_.start();
}

void McpClient::OnTransportConnected() {
  ResetReconnectBackoff();
  emit ConnectionStateChanged(true);
}

void McpClient::OnTransportDisconnected(QString reason) {
  emit ConnectionStateChanged(false);
  emit TransportMessage(QStringLiteral("Disconnected: %1").arg(reason));
  if (reconnect_enabled_) {
    IncreaseReconnectBackoff();
    reconnect_timer_.start(reconnect_current_ms_);
    emit ScheduleReconnectIn(reconnect_current_ms_);
  }
}

void McpClient::OnTransportJson(const QJsonObject& object) { HandleIncomingObject(object); }

void McpClient::OnTransportError(QString message) {
  emit TransportMessage(message);
}

void McpClient::OnReconnectTimer() {
  if (!reconnect_enabled_ || transport_ == nullptr) {
    return;
  }
  Connect();
}

void McpClient::OnStreamChunkTimer() {
  if (stream_cursor_ >= stream_buffer_.size()) {
    stream_timer_.stop();
    emit AssistantMessageCompleted(stream_buffer_);
    emit StreamingFinished();
    emit TokenUsageEstimate(stream_buffer_.size() / 4, stream_buffer_.size() / 4);
    return;
  }
  const int next_break = stream_buffer_.indexOf(QChar(' '), stream_cursor_ + 6);
  const int chunk_end =
      (next_break > stream_cursor_) ? next_break : qMin(stream_cursor_ + 12, stream_buffer_.size());
  const QString chunk = stream_buffer_.mid(stream_cursor_, chunk_end - stream_cursor_);
  stream_cursor_ = chunk_end;
  emit AssistantDelta(chunk);
}

void McpClient::HandleIncomingObject(const QJsonObject& object) {
  if (object.contains(QStringLiteral("method"))) {
    const QString method = object.value(QStringLiteral("method")).toString();
    const QJsonObject params = object.value(QStringLiteral("params")).toObject();
    emit ToolCallReceived(method, params);
    return;
  }

  if (object.contains(QStringLiteral("result")) || object.contains(QStringLiteral("error"))) {
    const QJsonValue id_val = object.value(QStringLiteral("id"));
    int id = -1;
    if (id_val.isDouble()) {
      id = id_val.toInt();
    } else if (id_val.isString()) {
      id = id_val.toString().toInt();
    }
    if (object.contains(QStringLiteral("error"))) {
      const QJsonObject err = object.value(QStringLiteral("error")).toObject();
      emit JsonRpcError(err.value(QStringLiteral("code")).toInt(),
                        err.value(QStringLiteral("message")).toString(), id_val);
      if (id >= 0) {
        pending_methods_.remove(id);
      }
      return;
    }

    const QJsonObject result = object.value(QStringLiteral("result")).toObject();
    Q_UNUSED(result);
    if (id >= 0) {
      pending_methods_.remove(id);
    }
    emit AssistantMessageCompleted(QString::fromUtf8(
        QJsonDocument(object).toJson(QJsonDocument::Compact)));
    emit TokenUsageEstimate(0, 0);
  }
}

void McpClient::ResetReconnectBackoff() { reconnect_current_ms_ = reconnect_delay_ms_; }

void McpClient::IncreaseReconnectBackoff() {
  reconnect_current_ms_ = qMin(reconnect_current_ms_ * 2, reconnect_max_ms_);
}

}  // namespace aura::mcp_assistant::mcp
