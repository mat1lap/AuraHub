#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>

namespace aura::mcp_assistant::mcp {

/// Abstract framing for JSON-RPC style MCP messages (one JSON object per logical send).
class IMcpTransport : public QObject {
  Q_OBJECT

 public:
  explicit IMcpTransport(QObject* parent = nullptr) : QObject(parent) {}
  IMcpTransport(const IMcpTransport&) = delete;
  IMcpTransport& operator=(const IMcpTransport&) = delete;
  IMcpTransport(IMcpTransport&&) = delete;
  IMcpTransport& operator=(IMcpTransport&&) = delete;
  ~IMcpTransport() override = default;

  virtual bool Connect() = 0;
  virtual void Disconnect() = 0;
  [[nodiscard]] virtual bool IsConnected() const = 0;

  virtual void SendJsonObject(const QJsonObject& object) = 0;

 signals:
  void Connected();
  void Disconnected(QString reason);
  void JsonObjectReceived(const QJsonObject& object);
  void TransportError(QString message);
};

}  // namespace aura::mcp_assistant::mcp
