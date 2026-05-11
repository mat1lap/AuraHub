#pragma once

#include "imcp_transport.h"

#include <QByteArray>
#include <QProcess>
#include <QStringList>

namespace aura::mcp_assistant::mcp {

/// JSON-RPC lines over subprocess stdin/stdout (common MCP deployment).
class StdioMcpTransport final : public IMcpTransport {
  Q_OBJECT

 public:
  explicit StdioMcpTransport(QObject* parent = nullptr);

  void SetCommand(QString program, QStringList arguments, QString working_directory);

  bool Connect() override;
  void Disconnect() override;
  [[nodiscard]] bool IsConnected() const override;
  void SendJsonObject(const QJsonObject& object) override;

 private slots:
  void OnReadyReadStdout();
  void OnProcessFinished(int exit_code, QProcess::ExitStatus status);
  void OnProcessError(QProcess::ProcessError error);

 private:
  void AppendAndDispatchLines(QByteArray* buffer);

  QProcess process_;
  QByteArray read_buffer_;
  QString program_;
  QStringList arguments_;
  QString working_directory_;
  bool connected_{false};
};

}  // namespace aura::mcp_assistant::mcp
