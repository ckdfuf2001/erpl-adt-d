#include <erpl_adt/mcp/destination_tools.hpp>
#include <nlohmann/json.hpp>

namespace erpl_adt {
namespace {

ToolResult OkData(const nlohmann::json& d) {
  ToolResult r;
  r.is_error = false;
  r.content = nlohmann::json::array({{{"type", "text"}, {"text", d.dump()}}});
  r.structured = d;
  return r;
}

ToolResult ParamErr(const std::string& m) {
  ToolResult r;
  r.is_error = true;
  r.content = nlohmann::json::array({{{"type", "text"}, {"text", m}}});
  return r;
}

nlohmann::json Redacted(const Destination& d) {
  nlohmann::json e;
  e["name"] = d.name;
  e["host"] = d.host;
  e["port"] = d.port;
  e["user"] = d.user;
  e["client"] = d.client;
  if (!d.language.empty()) e["language"] = d.language;
  e["https"] = d.use_https;
  e["insecure"] = d.insecure;
  e["timeout"] = d.timeout_sec;
  e["has_password"] = !d.password.empty();
  if (!d.password_env.empty()) e["password_env"] = d.password_env;
  return e;
}

}  // namespace

void RegisterDestinationTools(ToolRegistry& registry, DestinationManager& manager) {
  registry.Register(
      "adt_list_destinations",
      "List configured SAP destinations (passwords redacted). Pass one of these names as destination to every adt_*/bw_* tool.",
      {{"type", "object"}, {"properties", nlohmann::json::object()}, {"required", nlohmann::json::array()}},
      [&manager](const nlohmann::json&) {
        nlohmann::json j = nlohmann::json::array();
        for (const auto& d : manager.List()) j.push_back(Redacted(d));
        return OkData(j);
      });

  registry.Register(
      "adt_add_destination",
      "Add or update a SAP destination in the destinations file (upsert). Persists immediately so later calls can use it via destination.",
      {{"type", "object"},
       {"properties",
        {{"name", {{"type", "string"}, {"description", "Destination name (e.g. DEV, QAS, PRD)"}}},
         {"host", {{"type", "string"}, {"description", "SAP hostname"}}},
         {"port", {{"type", "integer"}, {"description", "SAP port"}}},
         {"user", {{"type", "string"}, {"description", "SAP username"}}},
         {"password", {{"type", "string"}, {"description", "SAP password (stored plaintext; prefer password_env)"}}},
         {"password_env", {{"type", "string"}, {"description", "Env var holding the password"}}},
         {"client", {{"type", "string"}, {"description", "SAP client (default 001)"}}},
         {"language", {{"type", "string"}, {"description", "Logon language (EN/DE)"}}},
         {"https", {{"type", "boolean"}, {"description", "Use HTTPS"}}},
         {"insecure", {{"type", "boolean"}, {"description", "Skip TLS verification"}}},
         {"timeout", {{"type", "integer"}, {"description", "Read timeout seconds"}}}}},
       {"required", {"name", "host", "user"}}},
      [&manager](const nlohmann::json& p) -> ToolResult {
        if (!p.contains("name") || !p["name"].is_string() || p["name"].get<std::string>().empty())
          return ParamErr("Missing required parameter: name");
        if (!p.contains("host") || !p["host"].is_string() || p["host"].get<std::string>().empty())
          return ParamErr("Missing required parameter: host");
        if (!p.contains("user") || !p["user"].is_string() || p["user"].get<std::string>().empty())
          return ParamErr("Missing required parameter: user");
        Destination d;
        d.name = p["name"].get<std::string>();
        d.host = p["host"].get<std::string>();
        d.user = p["user"].get<std::string>();
        if (p.contains("port") && p["port"].is_number_integer()) d.port = (uint16_t)p["port"].get<int>();
        if (p.contains("password") && p["password"].is_string()) d.password = p["password"].get<std::string>();
        if (p.contains("password_env") && p["password_env"].is_string()) d.password_env = p["password_env"].get<std::string>();
        if (p.contains("client") && p["client"].is_string()) d.client = p["client"].get<std::string>();
        if (p.contains("language") && p["language"].is_string()) d.language = p["language"].get<std::string>();
        if (p.contains("https") && p["https"].is_boolean()) d.use_https = p["https"].get<bool>();
        if (p.contains("insecure") && p["insecure"].is_boolean()) d.insecure = p["insecure"].get<bool>();
        if (p.contains("timeout") && p["timeout"].is_number_integer()) d.timeout_sec = p["timeout"].get<int>();
        auto r = manager.AddOrUpdate(d);
        if (r.IsErr()) return ParamErr(r.Error().message);
        return OkData(Redacted(d));
      });

  registry.Register(
      "adt_remove_destination",
      "Remove a SAP destination from the destinations file.",
      {{"type", "object"},
       {"properties", {{"name", {{"type", "string"}, {"description", "Destination name"}}}}},
       {"required", {"name"}}},
      [&manager](const nlohmann::json& p) -> ToolResult {
        if (!p.contains("name") || !p["name"].is_string() || p["name"].get<std::string>().empty())
          return ParamErr("Missing required parameter: name");
        auto r = manager.Remove(p["name"].get<std::string>());
        if (r.IsErr()) return ParamErr(r.Error().message);
        return OkData({{"removed", true}});
      });
}

}  // namespace erpl_adt
