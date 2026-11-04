#pragma once

#include <erpl_adt/mcp/destination_manager.hpp>
#include <erpl_adt/mcp/tool_registry.hpp>

namespace erpl_adt {

// Register all ADT operation tools. Each tool requires a destination
// parameter and resolves its own session per call, so concurrent
// clients with different destinations never mix.
void RegisterAdtTools(ToolRegistry& registry, DestinationManager& manager);

} // namespace erpl_adt
