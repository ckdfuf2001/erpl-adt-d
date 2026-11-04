#pragma once

#include <erpl_adt/adt/i_adt_session.hpp>
#include <erpl_adt/mcp/destination_manager.hpp>
#include <erpl_adt/mcp/tool_registry.hpp>

namespace erpl_adt {

// Per-call destination variant (production MCP server): every tool requires a
// destination parameter and resolves its own session per call, so concurrent
// clients with different destinations never mix.
void RegisterAdtTools(ToolRegistry& registry, DestinationManager& manager);

// Single-session variant (unit tests, embedders): binds every tool directly to
// the given session, no destination parameter (upstream behavior).
void RegisterAdtTools(ToolRegistry& registry, IAdtSession& session);

} // namespace erpl_adt
