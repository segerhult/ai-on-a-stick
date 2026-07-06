#include "llama_runtime.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif

namespace {
namespace fs = std::filesystem;

std::int64_t now_ms() {
    using clock = std::chrono::steady_clock;
    return std::chrono::duration_cast<std::chrono::milliseconds>(clock::now().time_since_epoch()).count();
}

#if defined(_WIN32)
std::string quote_arg(const std::string& value) {
    std::string escaped = "\"";
    for (const char ch : value) {
        if (ch == '"') {
            escaped += "\\\"";
        } else {
            escaped.push_back(ch);
        }
    }
    escaped += "\"";
    return escaped;
}
#endif

void close_runtime_socket(int fd) {
#if defined(_WIN32)
    closesocket(fd);
#else
    close(fd);
#endif
}

bool probe_runtime_health(const std::string& host, int port) {
#if defined(_WIN32)
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        return false;
    }
#endif

    const int socket_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
#if defined(_WIN32)
        WSACleanup();
#endif
        return false;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<std::uint16_t>(port));
    if (inet_pton(AF_INET, host.c_str(), &address.sin_addr) <= 0) {
        close_runtime_socket(socket_fd);
#if defined(_WIN32)
        WSACleanup();
#endif
        return false;
    }

    if (::connect(socket_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        close_runtime_socket(socket_fd);
#if defined(_WIN32)
        WSACleanup();
#endif
        return false;
    }

    const std::string request =
        "GET /health HTTP/1.1\r\nHost: " + host + ":" + std::to_string(port) + "\r\nConnection: close\r\n\r\n";
    const int sent = send(socket_fd, request.c_str(), static_cast<int>(request.size()), 0);
    if (sent <= 0) {
        close_runtime_socket(socket_fd);
#if defined(_WIN32)
        WSACleanup();
#endif
        return false;
    }

    char buffer[256];
    const int received = recv(socket_fd, buffer, sizeof(buffer) - 1, 0);
    close_runtime_socket(socket_fd);
#if defined(_WIN32)
    WSACleanup();
#endif
    if (received <= 0) {
        return false;
    }

    buffer[received] = '\0';
    return std::string(buffer).find(" 200 ") != std::string::npos;
}

// #region debug-point runtime-launch
void report_debug_event(const std::string& point, const std::string& payload) {
    const char* debug_server_url = std::getenv("DEBUG_SERVER_URL");
    const char* debug_session_id = std::getenv("DEBUG_SESSION_ID");
    if (debug_server_url == nullptr || debug_session_id == nullptr) {
        return;
    }

    std::ostringstream command;
    command << "curl --silent --show-error --max-time 1 "
            << "-H 'Content-Type: application/json' "
            << "-X POST "
            << "-d '{"
            << "\"sessionId\":\"" << debug_session_id << "\","
            << "\"point\":\"" << point << "\","
            << "\"payload\":\"";

    for (const char ch : payload) {
        if (ch == '\\' || ch == '"' || ch == '\'') {
            command << '\\';
        }
        if (ch == '\n' || ch == '\r') {
            command << ' ';
        } else {
            command << ch;
        }
    }

    command << "\"}' "
            << "'" << debug_server_url << "' >/dev/null 2>&1";
    std::system(command.str().c_str());
}
// #endregion debug-point runtime-launch
}  // namespace

LlamaRuntime::LlamaRuntime(RuntimeTarget target) : target_(std::move(target)) {}

LlamaRuntime::~LlamaRuntime() {
    stop();
}

bool LlamaRuntime::can_start() const {
    return binary_available() && model_available();
}

void LlamaRuntime::start() {
    std::lock_guard<std::mutex> lock(lifecycle_mutex_);
    if (process_thread_.joinable()) {
        process_thread_.join();
    }

    // #region debug-point runtime-launch
    if (!can_start()) {
        std::ostringstream payload;
        payload << "runtime=" << target_.name << " binary=" << target_.binary_path << " binary_available="
                << (binary_available() ? "true" : "false") << " model=" << target_.model_path << " model_available="
                << (model_available() ? "true" : "false");
        report_debug_event("runtime.start.blocked", payload.str());
        return;
    }

    if (running_.exchange(true)) {
        report_debug_event("runtime.start.skipped", "runtime=" + target_.name + " reason=already-running");
        return;
    }
    // #endregion debug-point runtime-launch

    started_at_ms_.store(now_ms());
    ready_at_ms_.store(0);
    last_exit_status_.store(0);
    child_pid_.store(0);

    const auto binary = target_.binary_path;
    const auto model = target_.model_path;
    const auto host = target_.host;
    const int port = target_.port;
    const int context_size = target_.context_size;
    const int parallel_slots = target_.parallel_slots;
    const int gpu_layers = target_.gpu_layers;

    process_thread_ = std::thread([=, this]() {
        // #region debug-point runtime-launch
        {
            std::ostringstream payload;
            payload << "runtime=" << target_.name << " binary=" << binary << " model=" << model << " host=" << host
                    << " port=" << port << " context=" << context_size << " parallel=" << parallel_slots
                    << " gpu_layers=" << gpu_layers;
            report_debug_event("runtime.start.spawn", payload.str());
        }
        // #endregion debug-point runtime-launch
#if defined(_WIN32)
        std::ostringstream command;
        command << quote_arg(binary) << " "
                << "-m " << quote_arg(model) << " "
                << "--host " << quote_arg(host) << " "
                << "--port " << port << " "
                << "-c " << context_size << " "
                << "--parallel " << parallel_slots << " "
                << "--threads-http " << parallel_slots << " ";

        if (gpu_layers > 0) {
            command << "-ngl " << gpu_layers << " ";
        }

        const int exit_code = std::system(command.str().c_str());
        last_exit_status_.store(exit_code);
#else
        std::vector<std::string> args = {
            binary,
            "-m",
            model,
            "--host",
            host,
            "--port",
            std::to_string(port),
            "-c",
            std::to_string(context_size),
            "--parallel",
            std::to_string(parallel_slots),
            "--threads-http",
            std::to_string(parallel_slots),
        };

        if (gpu_layers > 0) {
            args.push_back("-ngl");
            args.push_back(std::to_string(gpu_layers));
        }

        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);

        pid_t child_pid = 0;
        const int spawn_result = posix_spawn(&child_pid, binary.c_str(), nullptr, nullptr, argv.data(), environ);
        // #region debug-point runtime-launch
        {
            std::ostringstream payload;
            payload << "runtime=" << target_.name << " spawn_result=" << spawn_result << " child_pid=" << child_pid;
            report_debug_event("runtime.start.spawn_result", payload.str());
        }
        // #endregion debug-point runtime-launch
        if (spawn_result == 0) {
            child_pid_.store(static_cast<int>(child_pid));
            int wait_status = 0;
            waitpid(child_pid, &wait_status, 0);
            child_pid_.store(0);
            if (WIFEXITED(wait_status)) {
                last_exit_status_.store(WEXITSTATUS(wait_status));
            } else if (WIFSIGNALED(wait_status)) {
                last_exit_status_.store(128 + WTERMSIG(wait_status));
            } else {
                last_exit_status_.store(wait_status);
            }
            // #region debug-point runtime-launch
            {
                std::ostringstream payload;
                payload << "runtime=" << target_.name << " child_pid=" << child_pid << " wait_status=" << wait_status;
                report_debug_event("runtime.start.exit", payload.str());
            }
            // #endregion debug-point runtime-launch
        } else {
            last_exit_status_.store(spawn_result);
        }
#endif
        running_.store(false);
    });
}

void LlamaRuntime::stop() {
    std::lock_guard<std::mutex> lock(lifecycle_mutex_);

#if defined(_WIN32)
    running_.store(false);
#else
    const int child_pid = child_pid_.load();
    if (child_pid > 0) {
        kill(child_pid, SIGTERM);
        for (int attempt = 0; attempt < 20; ++attempt) {
            if (kill(child_pid, 0) != 0) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (kill(child_pid, 0) == 0) {
            kill(child_pid, SIGKILL);
        }
    }
#endif

    if (process_thread_.joinable()) {
        process_thread_.join();
    }

    child_pid_.store(0);
    running_.store(false);
}

bool LlamaRuntime::is_running() const { return running_.load(); }

bool LlamaRuntime::is_healthy() const {
    if (!probe_runtime_health(target_.host, target_.port)) {
        return false;
    }

    std::int64_t expected = 0;
    ready_at_ms_.compare_exchange_strong(expected, now_ms());
    return true;
}

bool LlamaRuntime::has_started() const { return started_at_ms_.load() > 0; }

std::string LlamaRuntime::base_url() const {
    return "http://" + target_.host + ":" + std::to_string(target_.port);
}

const std::string& LlamaRuntime::name() const { return target_.name; }

const std::string& LlamaRuntime::model_path() const { return target_.model_path; }

const std::string& LlamaRuntime::binary_path() const { return target_.binary_path; }

const std::string& LlamaRuntime::host() const { return target_.host; }

int LlamaRuntime::port() const { return target_.port; }

int LlamaRuntime::context_size() const { return target_.context_size; }

int LlamaRuntime::parallel_slots() const { return target_.parallel_slots; }

int LlamaRuntime::gpu_layers() const { return target_.gpu_layers; }

bool LlamaRuntime::model_available() const {
    return !target_.model_path.empty() && fs::exists(target_.model_path);
}

bool LlamaRuntime::binary_available() const {
    return !target_.binary_path.empty() && fs::exists(target_.binary_path);
}

std::int64_t LlamaRuntime::startup_elapsed_ms() const {
    const std::int64_t started = started_at_ms_.load();
    if (started <= 0) {
        return 0;
    }

    const std::int64_t ready = ready_at_ms_.load();
    const std::int64_t end = ready > 0 ? ready : now_ms();
    return end > started ? end - started : 0;
}

std::int64_t LlamaRuntime::ready_elapsed_ms() const {
    const std::int64_t started = started_at_ms_.load();
    const std::int64_t ready = ready_at_ms_.load();
    if (started <= 0 || ready <= 0 || ready <= started) {
        return -1;
    }
    return ready - started;
}

int LlamaRuntime::last_exit_status() const { return last_exit_status_.load(); }
