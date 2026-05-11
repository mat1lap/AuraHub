#pragma once

#include <Qt>

namespace aura::mcp_assistant::models {

/// QAbstractItemModel roles for chat QListView + delegate.
namespace ChatRoles {
inline constexpr int KindRole = Qt::UserRole + 1;
inline constexpr int BodyRole = Qt::UserRole + 2;
inline constexpr int CreatedMsRole = Qt::UserRole + 3;
inline constexpr int CheckpointIdRole = Qt::UserRole + 4;
inline constexpr int CheckpointTitleRole = Qt::UserRole + 5;
inline constexpr int PendingChangeIdRole = Qt::UserRole + 6;
inline constexpr int MessageIdRole = Qt::UserRole + 7;
}  // namespace ChatRoles

}  // namespace aura::mcp_assistant::models
