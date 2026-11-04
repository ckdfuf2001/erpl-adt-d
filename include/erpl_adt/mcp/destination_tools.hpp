#pragma once
#include <erpl_adt/mcp/destination_manager.hpp>
#include <erpl_adt/mcp/tool_registry.hpp>

namespace erpl_adt {

// adt_list_destinations / adt_add_destination / adt_remove_destination.
// Management tools need the manager itself, not a resolved session.
void RegisterDestinationTools(ToolRegistry& registry, DestinationManager& manager);

}
