#include <erpl_adt/mcp/destination_manager.hpp>
#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>

namespace erpl_adt {

namespace {
Error CfgErr(const std::string& m) { return Error{"DestinationManager", "", std::nullopt, m, std::nullopt}; }
}  // namespace

DestinationManager::DestinationManager(std::string file_path) : file_path_(std::move(file_path)) {}

std::string DestinationManager::ExpandEnv(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size();) {
    if (s[i] == '$' && i + 1 < s.size() && s[i + 1] == '{') {
      auto e = s.find('}', i + 2);
      if (e == std::string::npos) { out += s.substr(i); break; }
      auto var = s.substr(i + 2, e - (i + 2));
      const char* v = std::getenv(var.c_str());
      if (v) out += v;
      i = e + 1;
    } else if (s[i] == '$' && i + 1 < s.size() && (isalnum((unsigned char)s[i+1]) || s[i+1] == '_')) {
      size_t j = i + 1;
      while (j < s.size() && (isalnum((unsigned char)s[j]) || s[j] == '_')) ++j;
      auto var = s.substr(i + 1, j - (i + 1));
      const char* v = std::getenv(var.c_str());
      if (v) out += v;
      i = j;
    } else {
      out += s[i++];
    }
  }
  return out;
}

Result<void, Error> DestinationManager::Load() {
  std::lock_guard<std::mutex> lk(mu_);
  if (file_path_.empty()) return Result<void, Error>::Ok();
  std::ifstream ifs(file_path_);
  if (!ifs) return Result<void, Error>::Ok();  // missing file is OK
  nlohmann::json j;
  try {
    j = nlohmann::json::parse(ifs);
  } catch (const std::exception& e) {
    return Result<void, Error>::Err(CfgErr(std::string("parse destinations file: ") + e.what()));
  }
  nlohmann::json arr = j;
  if (j.is_object() && j.contains("destinations")) arr = j["destinations"];
  if (!arr.is_array()) return Result<void, Error>::Err(CfgErr("destinations file must be an array or {destinations:[...]}"));
  for (const auto& e : arr) {
    if (!e.is_object() || !e.contains("name") || !e["name"].is_string()) continue;
    Destination d;
    d.name = e["name"].get<std::string>();
    if (d.name.empty()) continue;
    d.host = e.value("host", "localhost");
    d.port = static_cast<uint16_t>(e.value("port", 50000));
    d.user = e.value("user", "DEVELOPER");
    d.password = e.value("password", "");
    d.password_env = e.value("password_env", "");
    d.client = e.value("client", "001");
    d.language = e.value("language", "");
    d.use_https = e.value("https", e.value("use_https", false));
    d.insecure = e.value("insecure", e.value("disable_tls_verify", false));
    d.timeout_sec = e.value("timeout", e.value("timeout_sec", 600));
    dests_[d.name] = d;
  }
  return Result<void, Error>::Ok();
}

Result<void, Error> DestinationManager::Save() const {
  std::lock_guard<std::mutex> lk(mu_);
  if (file_path_.empty()) return Result<void, Error>::Ok();
  nlohmann::json arr = nlohmann::json::array();
  for (const auto& kv : dests_) {
    const auto& d = kv.second;
    nlohmann::json e;
    e["name"] = d.name;
    e["host"] = d.host;
    e["port"] = d.port;
    e["user"] = d.user;
    if (!d.password.empty()) e["password"] = d.password;
    if (!d.password_env.empty()) e["password_env"] = d.password_env;
    e["client"] = d.client;
    if (!d.language.empty()) e["language"] = d.language;
    e["https"] = d.use_https;
    e["insecure"] = d.insecure;
    e["timeout"] = d.timeout_sec;
    arr.push_back(e);
  }
  nlohmann::json j;
  j["destinations"] = arr;
  std::ofstream ofs(file_path_);
  if (!ofs) return Result<void, Error>::Err(CfgErr("cannot write destinations file: " + file_path_));
  ofs << j.dump(2);
  return Result<void, Error>::Ok();
}

void DestinationManager::AddDefault(Destination d) {
  std::lock_guard<std::mutex> lk(mu_);
  if (d.name.empty()) d.name = "default";
  if (!dests_.count(d.name)) dests_[d.name] = d;
}

std::vector<Destination> DestinationManager::List() const {
  std::lock_guard<std::mutex> lk(mu_);
  std::vector<Destination> out;
  for (const auto& kv : dests_) out.push_back(kv.second);
  return out;
}

bool DestinationManager::Has(const std::string& name) const {
  std::lock_guard<std::mutex> lk(mu_);
  return dests_.count(name) > 0;
}

Result<std::shared_ptr<AdtSession>, Error> DestinationManager::CreateSession(const Destination& d) const {
  std::string pw = d.password;
  if (!d.password_env.empty()) {
    const char* v = std::getenv(d.password_env.c_str());
    if (v) pw = v;
  }
  pw = ExpandEnv(pw);
  auto cli = SapClient::Create(d.client);
  if (cli.IsErr()) return Result<std::shared_ptr<AdtSession>, Error>::Err(CfgErr("invalid client for destination " + d.name + ": " + cli.Error()));
  AdtSessionOptions opts;
  if (!d.language.empty()) {
    auto lang = SapLanguage::Create(d.language);
    if (lang.IsErr()) return Result<std::shared_ptr<AdtSession>, Error>::Err(CfgErr(lang.Error()));
    opts.language = std::move(lang).Value();
  }
  if (d.timeout_sec > 0) opts.read_timeout = std::chrono::seconds(d.timeout_sec);
  if (d.insecure) opts.disable_tls_verify = true;
  auto s = std::make_shared<AdtSession>(d.host, d.port, d.use_https, d.user, pw, std::move(cli).Value(), opts);
  return Result<std::shared_ptr<AdtSession>, Error>::Ok(s);
}

Result<IAdtSession*, Error> DestinationManager::Get(const std::string& name) {
  std::lock_guard<std::mutex> lk(mu_);
  auto it = dests_.find(name);
  if (it == dests_.end()) {
    std::string known;
    for (const auto& kv : dests_) { if (!known.empty()) known += ","; known += kv.first; }
    return Result<IAdtSession*, Error>::Err(CfgErr("unknown destination: " + name + (known.empty() ? "" : " (known: " + known + ")")));
  }
  auto si = sessions_.find(name);
  if (si != sessions_.end()) return Result<IAdtSession*, Error>::Ok(si->second.get());
  auto r = CreateSession(it->second);
  if (r.IsErr()) return Result<IAdtSession*, Error>::Err(r.Error());
  sessions_[name] = std::move(r).Value();
  return Result<IAdtSession*, Error>::Ok(sessions_[name].get());
}

Result<void, Error> DestinationManager::AddOrUpdate(const Destination& d) {
  if (d.name.empty()) return Result<void, Error>::Err(CfgErr("destination name is required"));
  if (d.host.empty()) return Result<void, Error>::Err(CfgErr("host is required"));
  if (d.user.empty()) return Result<void, Error>::Err(CfgErr("user is required"));
  auto cli = SapClient::Create(d.client);
  if (cli.IsErr()) return Result<void, Error>::Err(CfgErr("invalid client: " + cli.Error()));
  {
    std::lock_guard<std::mutex> lk(mu_);
    dests_[d.name] = d;
    sessions_.erase(d.name);  // drop cached session so next Get() reconnects
  }
  return Save();
}

Result<void, Error> DestinationManager::Remove(const std::string& name) {
  {
    std::lock_guard<std::mutex> lk(mu_);
    if (!dests_.count(name)) return Result<void, Error>::Err(CfgErr("unknown destination: " + name));
    dests_.erase(name);
    sessions_.erase(name);
  }
  return Save();
}

}  // namespace erpl_adt


