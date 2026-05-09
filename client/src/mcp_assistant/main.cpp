#include "mcp_assistant/app/application.h"
#include "mcp_assistant/chat/chat_messages_model.h"
#include "mcp_assistant/checkpoint/checkpoint_list_model.h"
#include "mcp_assistant/checkpoint/checkpoint_manager.h"
#include "mcp_assistant/mcp/mcp_client.h"
#include "mcp_assistant/mcp/stdio_mcp_transport.h"
#include "mcp_assistant/services/assistant_session_controller.h"
#include "mcp_assistant/views/main_window.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QStringList>

namespace {

QStringList SplitArgs(const QString& raw) {
  return raw.split(QLatin1Char(' '), Qt::SkipEmptyParts);
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication::setApplicationName(QStringLiteral("AuraHub"));
  QCoreApplication::setOrganizationName(QStringLiteral("AuraHub"));

  QApplication app(argc, argv);

  aura::mcp_assistant::app::ApplyAssistantTheme(&app);

  auto* chat_model = new aura::mcp_assistant::chat::ChatMessagesModel();
  auto* checkpoint_model = new aura::mcp_assistant::checkpoint::CheckpointListModel();
  auto* checkpoint_manager = new aura::mcp_assistant::checkpoint::CheckpointManager();

  auto* stdio_transport = new aura::mcp_assistant::mcp::StdioMcpTransport();
  const QString mcp_program = qEnvironmentVariable("AURA_MCP_PROGRAM");
  const QString mcp_args_env = qEnvironmentVariable("AURA_MCP_ARGS");
  stdio_transport->SetCommand(mcp_program, SplitArgs(mcp_args_env), QDir::currentPath());

  auto* mcp_client = new aura::mcp_assistant::mcp::McpClient(stdio_transport);
  mcp_client->ConfigureReconnect(true, 1000, 30000);

  auto* controller = new aura::mcp_assistant::services::AssistantSessionController(
      chat_model, checkpoint_model, checkpoint_manager, mcp_client);
  controller->SetWorkspaceRoot(QDir::currentPath());

  chat_model->AppendSystemMessage(
      QStringLiteral(
          "MCP Assistant — offline demo unless connected.\n"
          "Test agent (stdio): set AURA_MCP_PROGRAM to "
          "build/mcp/aurahub_test_mcp_agent (built target). "
          "Workspace for file edits = current directory; optional AURA_TEST_AGENT_FILE."));

  aura::mcp_assistant::views::MainWindow window;
  window.SetChatModel(chat_model);
  window.SetCheckpointModel(checkpoint_model);
  window.SetSessionController(controller);

  window.show();
  return app.exec();
}
