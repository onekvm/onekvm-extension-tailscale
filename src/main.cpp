#include <json-c/json.h>
#include "events.hpp"

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

struct Settings {
  std::string auth_key;
  std::string control_url;
  bool accept_routes = false;
  bool shields_up = false;
  int port = 41641;
};

struct AuthorizationState { std::string state, url; std::int64_t expires_at = 0; };
AuthorizationState ReadAuthorization(bool connected);
void WriteOAuthState(const std::string &token, const std::string &state,
                     std::int64_t expires_at = 0, std::string_view deadline_url = {});
std::string ReadOAuthWanted();
void RemoveOAuthOutput();

std::string EnvironmentPath(const char *name, std::string_view fallback) {
  const char *value = std::getenv(name);
  return value != nullptr && *value != '\0' ? value : std::string(fallback);
}

std::string ConfigPath() {
  return EnvironmentPath("ONEKVM_EXTENSION_CONFIG",
                         "/run/onekvm/extensions/tailscale") +
         "/config.json";
}

std::string DataPath() {
  return EnvironmentPath("ONEKVM_EXTENSION_DATA",
                         "/data/onekvm/extensions/tailscale");
}

std::string SocketPath() {
  return EnvironmentPath("ONEKVM_EXTENSION_SOCKET",
                         "/run/onekvm/extensions/tailscale/sockets");
}

bool ReadFile(const std::string &path, bool allow_missing, std::string &data,
              std::string &error) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    if (allow_missing && errno == ENOENT)
      return true;
    error = "open " + path + ": " + std::strerror(errno);
    return false;
  }
  data.assign(std::istreambuf_iterator<char>(input),
              std::istreambuf_iterator<char>());
  if (data.size() > (1U << 20)) {
    error = path + " exceeds 1 MiB";
    return false;
  }
  return true;
}

json_object *ParseJSON(std::string_view input, std::string &error) {
  json_tokener *tokener = json_tokener_new_ex(32);
  if (tokener == nullptr) {
    error = "allocate JSON parser";
    return nullptr;
  }
  json_tokener_set_flags(tokener, JSON_TOKENER_STRICT);
  json_object *value = json_tokener_parse_ex(
      tokener, input.data(), static_cast<int>(input.size()));
  const auto status = json_tokener_get_error(tokener);
  std::size_t end = json_tokener_get_parse_end(tokener);
  json_tokener_free(tokener);
  while (end < input.size() &&
         std::isspace(static_cast<unsigned char>(input[end])))
    ++end;
  if (status != json_tokener_success || value == nullptr ||
      end != input.size()) {
    if (value != nullptr)
      json_object_put(value);
    error = "invalid JSON";
    return nullptr;
  }
  return value;
}

bool OptionalString(json_object *root, const char *name, std::string &output) {
  json_object *value = nullptr;
  if (!json_object_object_get_ex(root, name, &value))
    return true;
  if (!json_object_is_type(value, json_type_string))
    return false;
  output = json_object_get_string(value);
  return true;
}

bool OptionalBool(json_object *root, const char *name, bool &output) {
  json_object *value = nullptr;
  if (!json_object_object_get_ex(root, name, &value))
    return true;
  if (!json_object_is_type(value, json_type_boolean))
    return false;
  output = json_object_get_boolean(value) != 0;
  return true;
}

bool OptionalInt(json_object *root, const char *name, int &output) {
  json_object *value = nullptr;
  if (!json_object_object_get_ex(root, name, &value))
    return true;
  if (!json_object_is_type(value, json_type_int))
    return false;
  output = json_object_get_int(value);
  return true;
}

bool LoadSettings(bool allow_missing, Settings &settings, std::string &error) {
  std::string input;
  if (!ReadFile(ConfigPath(), allow_missing, input, error))
    return false;
  if (input.empty())
    return true;
  json_object *root = ParseJSON(input, error);
  if (root == nullptr)
    return false;
  const bool valid = json_object_is_type(root, json_type_object) &&
                     OptionalString(root, "auth_key", settings.auth_key) &&
                     OptionalString(root, "control_url", settings.control_url) &&
                     OptionalBool(root, "accept_routes", settings.accept_routes) &&
                     OptionalBool(root, "shields_up", settings.shields_up) &&
                     OptionalInt(root, "port", settings.port);
  json_object_put(root);
  if (!valid || settings.auth_key.size() > 4096 ||
      settings.control_url.size() > 2048 ||
      settings.port < 0 || settings.port > 65535) {
    error = "invalid Tailscale settings";
    return false;
  }
  return true;
}

std::string Executable(std::string_view name) {
  if (name == "tailscale")
    return EnvironmentPath("ONEKVM_TAILSCALE_CLI", "/usr/bin/tailscale");
  if (name == "tailscaled")
    return EnvironmentPath("ONEKVM_TAILSCALED", "/usr/sbin/tailscaled");
  return std::string(name);
}

bool RuntimeReady(std::string &error) {
  if (access(Executable("tailscale").c_str(), X_OK) != 0) {
    error = "Tailscale CLI runtime is missing or not executable; reinstall the runtime package";
    return false;
  }
  if (access(Executable("tailscaled").c_str(), X_OK) != 0) {
    error = "Tailscale daemon runtime is missing or not executable; reinstall the runtime package";
    return false;
  }
  return true;
}

[[noreturn]] void Exec(std::vector<std::string> arguments) {
  events::ChildSignals();
  std::vector<char *> values;
  values.reserve(arguments.size() + 1);
  for (std::string &argument : arguments)
    values.push_back(argument.data());
  values.push_back(nullptr);
  execv(values.front(), values.data());
  std::cerr << "onekvm-tailscale-controller: exec " << arguments.front()
            << ": " << std::strerror(errno) << '\n';
  std::_Exit(127);
}

int RunCommandCapture(std::vector<std::string> arguments, std::string &output) {
  int pipe_fds[2];
  if (pipe(pipe_fds) != 0)
    return -1;
  const pid_t child = fork();
  if (child < 0) {
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    return -1;
  }
  if (child == 0) {
    close(pipe_fds[0]);
    (void)dup2(pipe_fds[1], STDOUT_FILENO);
    (void)dup2(pipe_fds[1], STDERR_FILENO);
    close(pipe_fds[1]);
    Exec(std::move(arguments));
  }
  close(pipe_fds[1]);
  output.clear();
  char buffer[4096];
  while (output.size() <= (1U << 20)) {
    const ssize_t count = read(pipe_fds[0], buffer, sizeof(buffer));
    if (count > 0) {
      output.append(buffer, static_cast<std::size_t>(count));
      continue;
    }
    if (count < 0 && errno == EINTR)
      continue;
    break;
  }
  close(pipe_fds[0]);
  int status = 0;
  while (waitpid(child, &status, 0) < 0) {
    if (errno != EINTR)
      return -1;
  }
  if (output.size() > (1U << 20))
    return -1;
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

int RunCommandAndLinger(std::vector<std::string> arguments,
                        std::string &error) {
  const pid_t child = fork();
  if (child < 0) {
    error = "start OAuth command: " + std::string(std::strerror(errno));
    return 1;
  }
  if (child == 0)
    Exec(std::move(arguments));
  int status = 0;
  while (waitpid(child, &status, 0) < 0) {
    if (errno != EINTR) {
      error = "wait for OAuth command: " + std::string(std::strerror(errno));
      return 1;
    }
  }
  const int result = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
  if (result != 0)
    error = "Tailscale browser login failed";
  return result;
}

std::vector<std::string>
CLI(const std::string &socket,
    std::initializer_list<std::string> arguments) {
  std::vector<std::string> command{Executable("tailscale"),
                                   "--socket=" + socket};
  command.insert(command.end(), arguments.begin(), arguments.end());
  return command;
}

bool WaitForSocket(const std::string &socket, std::string &error) {
  return events::WaitForSocket(socket, error);
}

std::vector<std::string> UpArguments(const Settings &settings,
                                     const std::string &socket,
                                     std::string_view timeout, bool json) {
  std::vector<std::string> arguments{
      Executable("tailscale"), "--socket=" + socket, "up",
      "--timeout=" + std::string(timeout), "--reset", "--accept-dns=false",
      "--netfilter-mode=off",
      "--accept-routes=" +
          std::string(settings.accept_routes ? "true" : "false"),
      "--shields-up=" + std::string(settings.shields_up ? "true" : "false")};
  if (json)
    arguments.push_back("--json");
  if (!settings.control_url.empty())
    arguments.push_back("--login-server=" + settings.control_url);
  return arguments;
}

std::string ObjectString(json_object *object, const char *name) {
  json_object *value = nullptr;
  if (object != nullptr && json_object_object_get_ex(object, name, &value) &&
      json_object_is_type(value, json_type_string))
    return json_object_get_string(value);
  return {};
}

std::string OAuthOutputURL() {
  std::string output;
  std::string ignored;
  if (!ReadFile(DataPath() + "/oauth-output", true, output, ignored))
    return {};
  const std::string_view marker = "\"AuthURL\"";
  const std::size_t key = output.rfind(marker);
  if (key == std::string::npos)
    return {};
  const std::size_t colon = output.find(':', key + marker.size());
  const std::size_t quote =
      colon == std::string::npos ? colon : output.find('"', colon + 1);
  if (quote == std::string::npos)
    return {};
  json_tokener *tokener = json_tokener_new_ex(8);
  if (tokener == nullptr)
    return {};
  json_object *value = json_tokener_parse_ex(
      tokener, output.data() + quote,
      static_cast<int>(output.size() - quote));
  const bool valid = value != nullptr &&
                     json_object_is_type(value, json_type_string);
  std::string url = valid ? json_object_get_string(value) : "";
  if (value != nullptr)
    json_object_put(value);
  json_tokener_free(tokener);
  if (url.rfind("https://", 0) != 0 && url.rfind("http://", 0) != 0)
    url.clear();
  return url;
}

void WriteOAuthState(const std::string &token, const std::string &state,
                     std::int64_t expires_at, std::string_view deadline_url) {
  const std::string path = DataPath() + "/oauth-state";
  const std::string temp = path + "." + std::to_string(getpid());
  {
    std::ofstream output(temp, std::ios::trunc);
    output << token << '\n' << state << '\n' << expires_at << '\n' << deadline_url << '\n';
    if (!output) return;
  }
  (void)chmod(temp.c_str(), 0600);
  (void)rename(temp.c_str(), path.c_str());
}

AuthorizationState ReadAuthorization(bool connected) {
  if (connected) return {"idle", ""};
  const std::string token = ReadOAuthWanted();
  if (token.empty()) return {"idle", ""};
  std::ifstream input(DataPath() + "/oauth-state");
  std::string owner, state;
  std::getline(input, owner);
  std::getline(input, state);
  if (owner != token) return {"starting", ""};
  if (state == "starting") {
    const std::string url = OAuthOutputURL();
    std::string expiry, deadline_url;
    std::getline(input, expiry);
    std::getline(input, deadline_url);
    char *end = nullptr;
    const auto parsed = std::strtoll(expiry.c_str(), &end, 10);
    const std::int64_t expires_at = !expiry.empty() && end && *end == '\0' && parsed > 0 && deadline_url == url ? parsed : 0;
    return url.empty() ? AuthorizationState{"starting", ""} : AuthorizationState{"pending", url, expires_at};
  }
  if (state == "connecting" || state == "failed" || state == "expired" || state == "timed-out") return {state, ""};
  return {"idle", ""};
}

std::string OAuthFailure() {
  std::string output, ignored;
  ReadFile(DataPath() + "/oauth-output", true, output, ignored);
  for (char &ch : output) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  // `up --timeout` bounds this local sign-in session. It is not the
  // control server's URL lifetime (which is not exposed by LocalAPI).
  if (output.find("expired_token") != std::string::npos ||
      output.find("code expired") != std::string::npos ||
      output.find("code has expired") != std::string::npos ||
      output.find("link expired") != std::string::npos ||
      output.find("authorization expired") != std::string::npos) return "expired";
  if (output.find("deadline exceeded") != std::string::npos ||
      output.find("timed out") != std::string::npos ||
      output.find("timeout waiting for tailscale service") != std::string::npos) return "timed-out";
  return "failed";
}

void NotifyScreen() {
  std::string ignored;
  RunCommandCapture({EnvironmentPath("ONEKVM_SCREEN_NOTIFY", "/usr/bin/busctl"),
    "--system", "--auto-start=no", "--timeout=1", "call", "org.onekvm.ScreenDisplay",
    "/org/onekvm/ScreenDisplay", "org.onekvm.ScreenDisplay", "Refresh"}, ignored);
}

void RemoveOAuthOutput() {
  std::error_code ignored;
  std::filesystem::remove(DataPath() + "/oauth-output", ignored);
}

void PrintJSON(json_object *response) {
  std::cout << json_object_to_json_string_ext(response, JSON_C_TO_STRING_PLAIN)
            << '\n';
  json_object_put(response);
}

void PrintStatus() {
  const std::string socket = SocketPath() + "/tailscaled.sock";
  std::string output;
  const int result = RunCommandCapture(CLI(socket, {"status", "--json"}), output);
  std::string parse_error;
  json_object *status = result == 0 ? ParseJSON(output, parse_error) : nullptr;
  if (status != nullptr && !json_object_is_type(status, json_type_object)) {
    json_object_put(status);
    status = nullptr;
  }

  std::string backend_state = ObjectString(status, "BackendState");
  json_object *addresses = json_object_new_array();
  std::string hostname;
  bool logged_in = backend_state == "Running";
  json_object *self = nullptr;
  if (status != nullptr && json_object_object_get_ex(status, "Self", &self) &&
      json_object_is_type(self, json_type_object)) {
    hostname = ObjectString(self, "HostName");
    json_object *tailscale_ips = nullptr;
    if (json_object_object_get_ex(self, "TailscaleIPs", &tailscale_ips) &&
        json_object_is_type(tailscale_ips, json_type_array)) {
      const std::size_t count = json_object_array_length(tailscale_ips);
      for (std::size_t index = 0; index < count; ++index) {
        json_object *address = json_object_array_get_idx(tailscale_ips, index);
        if (json_object_is_type(address, json_type_string)) {
          // Expired node keys retain their old addresses in NeedsLogin.
          // They must still be allowed to display a fresh enrollment link.
          logged_in = backend_state != "NeedsLogin";
          json_object_array_add(
              addresses,
              json_object_new_string(json_object_get_string(address)));
        }
      }
    }
  }

  auto authorization = ReadAuthorization(logged_in);
  if (authorization.state == "pending" && backend_state != "NeedsLogin")
    authorization = {backend_state == "Starting" ? "connecting" : "idle", ""};
  const std::string &auth_url = authorization.url;
  if (logged_in) RemoveOAuthOutput();

  json_object *response = json_object_new_object();
  json_object_object_add(response, "authWindowSeconds", json_object_new_int(events::AuthWindowSeconds));
  json_object_object_add(response, "authExpiresAt", json_object_new_int64(authorization.expires_at));
  json_object_object_add(response, "online",
                         json_object_new_boolean(status != nullptr));
  json_object_object_add(response, "loggedIn",
                         json_object_new_boolean(logged_in));
  json_object_object_add(response, "backendState",
                         json_object_new_string(backend_state.empty()
                                                    ? "Offline"
                                                    : backend_state.c_str()));
  json_object_object_add(response, "authURL",
                         json_object_new_string(auth_url.c_str()));
  json_object_object_add(response, "authState",
                         json_object_new_string(authorization.state.c_str()));
  json_object_object_add(response, "hostname",
                         json_object_new_string(hostname.c_str()));
  json_object_object_add(response, "addresses", addresses);
  if (status != nullptr)
    json_object_put(status);
  PrintJSON(response);
}

bool WriteSecret(const std::string &path, const std::string &secret,
                 std::string &error) {
  const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
                      0600);
  if (fd < 0) {
    error = "open secret file: " + std::string(std::strerror(errno));
    return false;
  }
  std::string_view remaining(secret);
  while (!remaining.empty()) {
    const ssize_t written = write(fd, remaining.data(), remaining.size());
    if (written < 0 && errno == EINTR)
      continue;
    if (written <= 0) {
      error = "write secret file: " + std::string(std::strerror(errno));
      close(fd);
      return false;
    }
    remaining.remove_prefix(static_cast<std::size_t>(written));
  }
  if (close(fd) != 0) {
    error = "close secret file: " + std::string(std::strerror(errno));
    return false;
  }
  return true;
}

int RunDaemon(std::string &error);
int Apply(std::string &error);
int RunOAuth(std::string &error);

std::string OAuthWantedPath() { return DataPath() + "/oauth-wanted"; }

std::string ReadOAuthWanted() {
  std::string data;
  std::string ignored;
  if (!ReadFile(OAuthWantedPath(), true, data, ignored))
    return {};
  while (!data.empty() &&
         std::isspace(static_cast<unsigned char>(data.back())))
    data.pop_back();
  return data;
}

void RemoveOAuthWanted() {
  std::error_code ignored;
  std::filesystem::remove(OAuthWantedPath(), ignored);
  RemoveOAuthOutput();
}

int WaitPid(pid_t pid) {
  int status = 0;
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR)
      return -1;
  }
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

pid_t Spawn(int (*fn)(std::string &), std::string &error) {
  const pid_t pid = fork();
  if (pid < 0) {
    error = std::string("fork: ") + std::strerror(errno);
    return -1;
  }
  if (pid == 0) {
    events::ChildSignals();
    (void)setpgid(0, 0);
    std::string child_error;
    const int result = fn(child_error);
    if (result != 0 && !child_error.empty())
      std::cerr << "onekvm-tailscale-controller: " << child_error << '\n';
    std::_Exit(result == 0 ? 0 : 1);
  }
  (void)setpgid(pid, pid);
  return pid;
}

int Connect(std::string &error) {
  if (!RuntimeReady(error)) return 1;
  std::error_code filesystem_error;
  std::filesystem::create_directories(DataPath(), filesystem_error);
  if (filesystem_error) {
    error = "create data directory: " + filesystem_error.message();
    return 1;
  }
  events::AuthorizationLock lock(DataPath());
  if (lock.fd < 0) { error = "lock authorization request"; return 1; }
  const auto authorization = ReadAuthorization(false);
  if (authorization.state == "starting" || authorization.state == "pending" || authorization.state == "connecting") {
    json_object *response = json_object_new_object();
    json_object_object_add(response, "started", json_object_new_boolean(0));
    json_object_object_add(response, "alreadyPending", json_object_new_boolean(1));
    PrintJSON(response);
    return 0;
  }
  const std::string pending = OAuthWantedPath() + "." + std::to_string(getpid());
  std::ofstream output(pending, std::ios::trunc);
  if (!output) {
    error = "write oauth request";
    return 1;
  }
  output << std::chrono::steady_clock::now().time_since_epoch().count() << '\n';
  output.close();
  if (!output || rename(pending.c_str(), OAuthWantedPath().c_str()) != 0) {
    error = "publish oauth request";
    return 1;
  }
  json_object *response = json_object_new_object();
  json_object_object_add(response, "started", json_object_new_boolean(1));
  PrintJSON(response);
  return 0;
}

int RunNamed(std::string_view command, const char *key, bool value,
             std::string &error) {
  events::AuthorizationLock lock(DataPath());
  if (lock.fd < 0) { error = "lock authorization request"; return 1; }
  RemoveOAuthWanted();
  const std::string socket = SocketPath() + "/tailscaled.sock";
  if (!WaitForSocket(socket, error))
    return 1;
  std::string output;
  const int result =
      RunCommandCapture(CLI(socket, {std::string(command)}), output);
  if (result != 0) {
    error = output.empty() ? "tailscale " + std::string(command) + " failed"
                           : output;
    return 1;
  }
  json_object *response = json_object_new_object();
  json_object_object_add(response, key, json_object_new_boolean(value));
  PrintJSON(response);
  return 0;
}

void StopChild(pid_t &pid) {
  if (pid > 0) {
    (void)kill(-pid, SIGTERM);
    (void)WaitPid(pid);
    pid = -1;
  }
}

int Service(std::string &error) {
  if (!RuntimeReady(error)) return 78; // EX_CONFIG: repair missing packages.
  events::Watch watch(DataPath(), SocketPath());
  events::AuthorizationDeadline deadline;
  if (!watch.valid() || deadline.fd < 0) { error = "create service event subscriptions"; return 1; }
  std::string oauth_token;
  {
    events::AuthorizationLock lock(DataPath());
    if (lock.fd < 0) { error = "lock authorization recovery"; return 1; }
    oauth_token = ReadOAuthWanted();
    const auto previous = ReadAuthorization(false);
    // A new supervisor cannot resume an old login process or its monotonic
    // deadline. Retire interrupted requests; never replay terminal requests.
    if (previous.state == "starting" || previous.state == "pending" || previous.state == "connecting")
      WriteOAuthState(oauth_token, "failed");
    RemoveOAuthOutput();
  }
  pid_t daemon = Spawn(RunDaemon, error);
  if (daemon < 0) return 1;
  pid_t apply = Spawn(Apply, error);
  pid_t oauth = -1;
  std::string authorization_url;
  events::Stream stream;
  for (;;) {
    const std::string wanted = ReadOAuthWanted();
    if (wanted != oauth_token) {
      deadline.clear();
      authorization_url.clear();
      StopChild(oauth);
      RemoveOAuthOutput();
      oauth_token = wanted;
      if (!wanted.empty()) {
        WriteOAuthState(wanted, "starting");
        oauth = Spawn(RunOAuth, error);
        if (oauth < 0) WriteOAuthState(wanted, "failed");
      }
      NotifyScreen();
    }
    stream.open(SocketPath() + "/tailscaled.sock", "GET /localapi/v0/watch-ipn-bus?mask=2 HTTP/1.0\r\nHost: local-tailscaled.sock\r\n\r\n");
    pollfd fds[] = {{watch.files, POLLIN, 0}, {watch.signals, POLLIN, 0}, {stream.fd, POLLIN, 0}, {deadline.fd, POLLIN, 0}};
    if (poll(fds, 4, stream.timeout()) < 0) {
      if (errno == EINTR) continue;
      error = "wait for service events";
      StopChild(oauth); StopChild(apply); StopChild(daemon);
      return 1;
    }
    bool changed = watch.drainFiles();
    if (watch.stopping()) {
      StopChild(oauth); StopChild(apply); StopChild(daemon);
      RemoveOAuthOutput();
      return 0;
    }
    int status = 0;
    for (pid_t ended; (ended = waitpid(-1, &status, WNOHANG)) > 0;) {
      if (ended == daemon) {
        daemon = -1;
        StopChild(oauth); StopChild(apply);
        error = "daemon exited";
        return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : 1;
      }
      if (ended == apply) apply = -1;
      if (ended == oauth) {
        oauth = -1;
        const auto auth = ReadAuthorization(false);
        if (auth.state == "starting" || auth.state == "pending") WriteOAuthState(oauth_token, "failed");
        RemoveOAuthOutput();
        changed = true;
      }
    }
    if (fds[2].revents) changed = stream.read() || changed;
    // Bind the one-shot window to the current request and URL, never to an App view.
    const auto auth = ReadAuthorization(false);
    if (oauth > 0 && !oauth_token.empty() && ReadOAuthWanted() == oauth_token && auth.state == "pending") {
      if (authorization_url != auth.url) {
        authorization_url = auth.url;
        if (!deadline.arm()) {
          error = "arm authorization expiry event";
          StopChild(oauth); StopChild(apply); StopChild(daemon);
          RemoveOAuthOutput();
          return 1;
        }
        WriteOAuthState(oauth_token, "starting", events::AuthorizationDeadline::expiresAt(), authorization_url);
        changed = true;
      }
      if (deadline.expired()) {
        StopChild(oauth);
        WriteOAuthState(oauth_token, "expired");
        RemoveOAuthOutput();
        deadline.clear();
        authorization_url.clear();
        changed = true;
      }
    } else if (!authorization_url.empty()) {
      deadline.clear();
      authorization_url.clear();
    }
    if (changed) NotifyScreen();
  }
}

int RunDaemon(std::string &error) {
  Settings settings;
  if (!LoadSettings(false, settings, error))
    return 1;
  const std::string data = DataPath();
  const std::string state_directory = data + "/tailscaled";
  const std::string state_file = state_directory + "/tailscaled.state";
  const std::string legacy_state_file = data + "/tailscaled.state";
  const std::string sockets = SocketPath();
  std::error_code filesystem_error;
  std::filesystem::create_directories(data, filesystem_error);
  if (filesystem_error) {
    error = "create data directory: " + filesystem_error.message();
    return 1;
  }
  if (chmod(data.c_str(), 0750) != 0) {
    error = "set data directory permissions: " +
            std::string(std::strerror(errno));
    return 1;
  }
  std::filesystem::create_directories(state_directory, filesystem_error);
  if (filesystem_error) {
    error = "create tailscaled state directory: " + filesystem_error.message();
    return 1;
  }
  if (chmod(state_directory.c_str(), 0700) != 0) {
    error = "set tailscaled state directory permissions: " +
            std::string(std::strerror(errno));
    return 1;
  }
  if (std::filesystem::exists(legacy_state_file, filesystem_error) &&
      !filesystem_error &&
      !std::filesystem::exists(state_file, filesystem_error) &&
      !filesystem_error) {
    std::filesystem::rename(legacy_state_file, state_file, filesystem_error);
    if (filesystem_error) {
      error = "migrate tailscaled state: " + filesystem_error.message();
      return 1;
    }
  }
  if (filesystem_error) {
    error = "inspect tailscaled state: " + filesystem_error.message();
    return 1;
  }
  std::vector<std::string> arguments{
      Executable("tailscaled"), "--state=" + state_file,
      "--statedir=" + state_directory,
      "--socket=" + sockets + "/tailscaled.sock",
      "--tun=tailscale0", "--port=" + std::to_string(settings.port)};
  Exec(std::move(arguments));
}

int Apply(std::string &error) {
  Settings settings;
  if (!LoadSettings(false, settings, error))
    return 1;
  const std::string data = DataPath();
  const std::string socket = SocketPath() + "/tailscaled.sock";
  if (!WaitForSocket(socket, error))
    return 1;
  std::vector<std::string> arguments =
      UpArguments(settings, socket, "20s", false);
  if (!settings.auth_key.empty()) {
    const std::string secret_path = data + "/auth-key";
    if (!WriteSecret(secret_path, settings.auth_key, error))
      return 1;
    arguments.push_back("--auth-key=file:" + secret_path);
  } else {
    std::string output;
    const int status_result =
        RunCommandCapture(CLI(socket, {"status", "--json"}), output);
    std::string parse_error;
    json_object *status =
        status_result == 0 ? ParseJSON(output, parse_error) : nullptr;
    const std::string backend_state = ObjectString(status, "BackendState");
    if (status != nullptr)
      json_object_put(status);
    // Do not accidentally start an interactive login from a lifecycle action.
    // Browser login is owned by the separate oauth instance below.
    if (backend_state != "Running")
      return 0;
  }
  Exec(std::move(arguments));
}

int RunOAuth(std::string &error) {
  const std::string token = ReadOAuthWanted();
  WriteOAuthState(token, "starting");
  Settings settings;
  if (!LoadSettings(false, settings, error))
    return 1;
  const std::string data = DataPath();
  const std::string socket = SocketPath() + "/tailscaled.sock";
  if (!WaitForSocket(socket, error))
    return 1;
  std::error_code filesystem_error;
  std::filesystem::create_directories(data, filesystem_error);
  if (filesystem_error) {
    error = "create data directory: " + filesystem_error.message();
    return 1;
  }
  const int output_fd =
      open((data + "/oauth-output").c_str(),
           O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
  if (output_fd < 0) {
    error = "open OAuth output: " + std::string(std::strerror(errno));
    return 1;
  }
  if (dup2(output_fd, STDOUT_FILENO) < 0 ||
      dup2(output_fd, STDERR_FILENO) < 0) {
    error = "redirect OAuth output: " + std::string(std::strerror(errno));
    close(output_fd);
    return 1;
  }
  close(output_fd);
  std::vector<std::string> arguments =
      UpArguments(settings, socket, "10m", true);
  const int result = RunCommandAndLinger(std::move(arguments), error);
  WriteOAuthState(token, result == 0 ? "idle" : OAuthFailure());
  RemoveOAuthOutput();
  return result;
}

} // namespace

int main(int argc, char **argv) {
  std::string error;
  int result = 1;
  const std::string_view command = argc >= 2 ? argv[1] : "--service";
  if (argc > 2) {
    error = "usage: onekvm-tailscale-controller "
            "[--service|status|connect|disconnect|logout|--health|--run|--apply|--oauth]";
  } else if (command == "--service") {
    result = Service(error);
  } else if (command == "status") {
    PrintStatus();
    result = 0;
  } else if (command == "connect") {
    result = Connect(error);
  } else if (command == "disconnect") {
    result = RunNamed("down", "connected", false, error);
  } else if (command == "logout") {
    result = RunNamed("logout", "loggedOut", true, error);
  } else if (command == "--health") {
    Settings settings;
    result = LoadSettings(true, settings, error) && RuntimeReady(error) ? 0 : 1;
  } else if (command == "--run") {
    result = RunDaemon(error);
  } else if (command == "--apply") {
    result = Apply(error);
  } else if (command == "--oauth") {
    result = RunOAuth(error);
  } else {
    error = "usage: onekvm-tailscale-controller "
            "[--service|status|connect|disconnect|logout|--health|--run|--apply|--oauth]";
  }
  if (result != 0)
    std::cerr << "onekvm-tailscale-controller: " << error << '\n';
  return result;
}
