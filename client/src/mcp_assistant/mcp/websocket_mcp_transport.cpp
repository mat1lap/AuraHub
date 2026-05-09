#include "mcp_assistant/mcp/websocket_mcp_transport.h"

#include <QAbstractSocket>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QtWebSockets/QWebSocket>

namespace aura::mcp_assistant::mcp {

WebSocketMcpTransport::WebSocketMcpTransport(QObject* parent) : IMcpTransport(parent) {
  socket_ = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
  connect(socket_, &QWebSocket::connected, this, &WebSocketMcpTransport::OnConnected);
  connect(socket_, &QWebSocket::disconnected, this, &WebSocketMcpTransport::OnDisconnected);
  connect(socket_, &QWebSocket::textMessageReceived, this,
          &WebSocketMcpTransport::OnTextMessageReceived);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  connect(socket_, &QWebSocket::errorOccurred, this, &WebSocketMcpTransport::OnError);
#else
  connect(socket_, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this,
          &WebSocketMcpTransport::OnError);
#endif
}

WebSocketMcpTransport::~WebSocketMcpTransport() = default;

void WebSocketMcpTransport::SetServerUrl(QUrl url) { url_ = std::move(url); }

bool WebSocketMcpTransport::Connect() {
  if (!url_.isValid()) {
    emit TransportError(QStringLiteral("WebSocket URL is invalid."));
    return false;
  }
  socket_->open(url_);
  return true;
}

void WebSocketMcpTransport::Disconnect() { socket_->close(); }

bool WebSocketMcpTransport::IsConnected() const {
  return socket_->state() == QAbstractSocket::ConnectedState;
}

void WebSocketMcpTransport::SendJsonObject(const QJsonObject& object) {
  if (!IsConnected()) {
    emit TransportError(QStringLiteral("WebSocket is not connected."));
    return;
  }
  QJsonDocument doc(object);
  socket_->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

void WebSocketMcpTransport::OnConnected() { emit Connected(); }

void WebSocketMcpTransport::OnDisconnected() {
  emit Disconnected(QStringLiteral("websocket closed"));
}

void WebSocketMcpTransport::OnTextMessageReceived(QString message) {
  QJsonParseError err{};
  QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &err);
  if (err.error != QJsonParseError::NoError || !doc.isObject()) {
    emit TransportError(QStringLiteral("WebSocket JSON parse error: %1").arg(err.errorString()));
    return;
  }
  emit JsonObjectReceived(doc.object());
}

void WebSocketMcpTransport::OnError(QAbstractSocket::SocketError error) {
  emit TransportError(
      QStringLiteral("WebSocket error: %1").arg(static_cast<int>(error)));
}

}  // namespace aura::mcp_assistant::mcp
