#pragma once
#include <erpl_adt/adt/adt_session.hpp>
#include <erpl_adt/core/result.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <map>

namespace erpl_adt {

// Single SAP destination entry. Mirrors the global CLI flags.
struct Destination {
  std::string name;
  std::string host = "localhost";
  uint16_t port = 50000;
  std::string user = "DEVELOPER";
  std::string password;
  std::string password_env;
  std::string client = "001";
  std::string language;
  bool use_https = false;
  bool insecure = false;
  int timeout_sec = 600;
};

// Thread-safe store + lazy AdtSession cache.
// MCP tools resolve the session per call via Get(name), so concurrent
// clients using different destinations never mix.
class DestinationManager {
 public:
  explicit DestinationManager(std::string file_path = "");
  DestinationManager(const DestinationManager&) = delete;
  DestinationManager& operator=(const DestinationManager&) = delete;

  // Load JSON file. Missing file is OK (starts empty).
  Result<void, Error> Load();
  // Persist current list to file. No-op when file_path is empty.
  Result<void, Error> Save() const;

  void SetFilePath(const std::string& p) { file_path_ = p; }
  const std::string& FilePath() const { return file_path_; }

  // Register the startup default (flags/env/.adt.creds) as "default".
  void AddDefault(Destination d);

  std::vector<Destination> List() const;
  bool Has(const std::string& name) const;

  // Lazy-create and cache AdtSession. Returns error when unknown.
  Result<IAdtSession*, Error> Get(const std::string& name);

  // Upsert + persist. Validates name/host/user/client.
  Result<void, Error> AddOrUpdate(const Destination& d);
  Result<void, Error> Remove(const std::string& name);

  // ${VAR} / $VAR expansion for password fields.
  static std::string ExpandEnv(const std::string& s);

 private:
  Result<std::shared_ptr<AdtSession>, Error> CreateSession(const Destination& d) const;

  mutable std::mutex mu_;
  std::string file_path_;
  std::map<std::string, Destination> dests_;
  std::map<std::string, std::shared_ptr<AdtSession>> sessions_;
};

}  // namespace erpl_adt
