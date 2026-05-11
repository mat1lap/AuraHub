#pragma once

#include "imcp_transport.h"

#include <QAbstractSocket>
#include <QUrl>

QT_FORWARD_DECLARE_CLASS(QWebSocket)

namespace aura::mcp_assistant::mcp {

/// Experimental: JSON text frames over WebSocket (extensibility / remote MCP bridges).
class WebSocketMcpTransport final : public IMcpTransport {
  Q_OBJECT

 public:
  explicit WebSocketMcpTransport(QObject* parent = nullptr);
  ~WebSocketMcpTransport() override;

  void SetServerUrl(QUrl url);

  bool Connect() override;
  void Disconnect() override;
  [[nodiscard]] bool IsConnected() const override;
  void SendJsonObject(const QJsonObject& object) override;

 private slots:
  void OnConnected();
  void OnDisconnected();
  void OnTextMessageReceived(QString message);
  void OnError(QAbstractSocket::SocketError error);

 private:
  QUrl url_;
  QWebSocket* socket_{nullptr};
};

}  // namespace aura::mcp_assistant::mcp
