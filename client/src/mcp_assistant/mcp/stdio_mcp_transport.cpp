#include "mcp_assistant/mcp/stdio_mcp_transport.h"

#include <QJsonDocument>
#include <QJsonParseError>

namespace aura::mcp_assistant::mcp {

StdioMcpTransport::StdioMcpTransport(QObject* parent) : IMcpTransport(parent) {
  connect(&process_, &QProcess::readyReadStandardOutput, this,
          &StdioMcpTransport::OnReadyReadStdout);
  connect(&process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          &StdioMcpTransport::OnProcessFinished);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  connect(&process_, &QProcess::errorOccurred, this, &StdioMcpTransport::OnProcessError);
#else
  connect(&process_, QOverload<QProcess::ProcessError>::of(&QProcess::error), this,
          &StdioMcpTransport::OnProcessError);
#endif
}

void StdioMcpTransport::SetCommand(QString program, QStringList arguments,
                                     QString working_directory) {
  program_ = std::move(program);
  arguments_ = std::move(arguments);
  working_directory_ = std::move(working_directory);
}

bool StdioMcpTransport::Connect() {
  if (program_.isEmpty()) {
    emit TransportError(QStringLiteral("MCP stdio program is not configured."));
    return false;
  }
  if (process_.state() != QProcess::NotRunning) {
    return connected_;
  }

  process_.setProgram(program_);
  process_.setArguments(arguments_);
  process_.setWorkingDirectory(working_directory_);
  process_.setProcessChannelMode(QProcess::ForwardedErrorChannel);

  process_.start();
  if (!process_.waitForStarted(5000)) {
    emit TransportError(QStringLiteral("Failed to start MCP process: %1").arg(program_));
    connected_ = false;
    return false;
  }

  connected_ = true;
  emit Connected();
  return true;
}

void StdioMcpTransport::Disconnect() {
  if (process_.state() != QProcess::NotRunning) {
    process_.kill();
    process_.waitForFinished(3000);
  }
  read_buffer_.clear();
  connected_ = false;
  emit Disconnected(QStringLiteral("disconnected"));
}

bool StdioMcpTransport::IsConnected() const {
  return connected_ && process_.state() == QProcess::Running;
}

void StdioMcpTransport::SendJsonObject(const QJsonObject& object) {
  if (!IsConnected()) {
    emit TransportError(QStringLiteral("stdio transport is not connected."));
    return;
  }
  QJsonDocument doc(object);
  QByteArray line = doc.toJson(QJsonDocument::Compact);
  line.append('\n');
  process_.write(line);
}

void StdioMcpTransport::OnReadyReadStdout() {
  read_buffer_.append(process_.readAllStandardOutput());
  AppendAndDispatchLines(&read_buffer_);
}

void StdioMcpTransport::AppendAndDispatchLines(QByteArray* buffer) {
  while (true) {
    const int nl = buffer->indexOf('\n');
    if (nl < 0) {
      break;
    }
    QByteArray line = buffer->left(nl);
    buffer->remove(0, nl + 1);
    line = line.trimmed();
    if (line.isEmpty()) {
      continue;
    }
    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(line, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
      emit TransportError(QStringLiteral("JSON parse error on MCP line: %1").arg(err.errorString()));
      continue;
    }
    emit JsonObjectReceived(doc.object());
  }
}

void StdioMcpTransport::OnProcessFinished(int exit_code, QProcess::ExitStatus status) {
  Q_UNUSED(status);
  connected_ = false;
  emit Disconnected(QStringLiteral("process exited (%1)").arg(exit_code));
}

void StdioMcpTransport::OnProcessError(QProcess::ProcessError error) {
  emit TransportError(QStringLiteral("QProcess error: %1").arg(static_cast<int>(error)));
}

}  // namespace aura::mcp_assistant::mcp
