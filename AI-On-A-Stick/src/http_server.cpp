#include "http_server.h"
#include "hardware.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {
namespace fs = std::filesystem;

// #region debug-point upstream-connect
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
// #endregion debug-point upstream-connect

std::string trim(std::string value) {
    const auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](unsigned char ch) { return !is_space(ch); }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](unsigned char ch) { return !is_space(ch); }).base(),
                value.end());
    return value;
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

std::string json_escape(const std::string& value) {
    std::ostringstream stream;
    for (char ch : value) {
        switch (ch) {
            case '\\':
                stream << "\\\\";
                break;
            case '"':
                stream << "\\\"";
                break;
            case '\n':
                stream << "\\n";
                break;
            case '\r':
                stream << "\\r";
                break;
            case '\t':
                stream << "\\t";
                break;
            default:
                stream << ch;
                break;
        }
    }
    return stream.str();
}

std::string runtime_manifest_json(const LlamaRuntime& runtime) {
    const bool healthy = runtime.is_healthy();
    const bool ready = runtime.can_start();
    const bool running = runtime.is_running();
    std::string state = "stopped";
    if (!ready) {
        state = "needs bundle";
    } else if (healthy) {
        state = "live";
    } else if (running) {
        state = "loading";
    } else if (runtime.has_started() && runtime.last_exit_status() != 0) {
        state = "failed";
    }

    std::ostringstream json;
    json << "{"
         << "\"name\":\"" << json_escape(runtime.name()) << "\","
         << "\"base_url\":\"" << json_escape(runtime.base_url()) << "\","
         << "\"binary_path\":\"" << json_escape(runtime.binary_path()) << "\","
         << "\"model_path\":\"" << json_escape(runtime.model_path()) << "\","
         << "\"port\":" << runtime.port() << ","
         << "\"binary_available\":" << (runtime.binary_available() ? "true" : "false") << ","
         << "\"model_available\":" << (runtime.model_available() ? "true" : "false") << ","
         << "\"ready\":" << (ready ? "true" : "false") << ","
         << "\"running\":" << (running ? "true" : "false") << ","
         << "\"healthy\":" << (healthy ? "true" : "false") << ","
         << "\"state\":\"" << state << "\","
         << "\"context_size\":" << runtime.context_size() << ","
         << "\"parallel_slots\":" << runtime.parallel_slots() << ","
         << "\"gpu_layers\":" << runtime.gpu_layers() << ","
         << "\"startup_elapsed_ms\":" << runtime.startup_elapsed_ms() << ","
         << "\"ready_elapsed_ms\":" << runtime.ready_elapsed_ms() << ","
         << "\"last_exit_status\":" << runtime.last_exit_status() << "}";
    return json.str();
}

std::string json_unescape(const std::string& value) {
    std::string output;
    output.reserve(value.size());

    for (std::size_t index = 0; index < value.size(); ++index) {
        const char ch = value[index];
        if (ch != '\\' || index + 1 >= value.size()) {
            output.push_back(ch);
            continue;
        }

        const char next = value[++index];
        switch (next) {
            case 'n':
                output.push_back('\n');
                break;
            case 'r':
                output.push_back('\r');
                break;
            case 't':
                output.push_back('\t');
                break;
            case '\\':
                output.push_back('\\');
                break;
            case '"':
                output.push_back('"');
                break;
            default:
                output.push_back(next);
                break;
        }
    }

    return output;
}

std::size_t find_json_key(const std::string& body, const std::string& key, std::size_t start = 0) {
    return body.find("\"" + key + "\"", start);
}

std::string extract_json_string_field(const std::string& body, const std::string& key, std::size_t start = 0) {
    const auto key_pos = find_json_key(body, key, start);
    if (key_pos == std::string::npos) {
        return "";
    }

    const auto colon = body.find(':', key_pos);
    const auto quote = body.find('"', colon + 1);
    if (colon == std::string::npos || quote == std::string::npos) {
        return "";
    }

    std::string raw;
    bool escaping = false;
    for (std::size_t index = quote + 1; index < body.size(); ++index) {
        const char ch = body[index];
        if (!escaping && ch == '"') {
            return json_unescape(raw);
        }
        if (!escaping && ch == '\\') {
            escaping = true;
            raw.push_back(ch);
            continue;
        }
        escaping = false;
        raw.push_back(ch);
    }

    return "";
}

int extract_json_int_field(const std::string& body, const std::string& key, int fallback) {
    const auto key_pos = find_json_key(body, key);
    if (key_pos == std::string::npos) {
        return fallback;
    }

    const auto colon = body.find(':', key_pos);
    if (colon == std::string::npos) {
        return fallback;
    }

    std::size_t index = colon + 1;
    while (index < body.size() && std::isspace(static_cast<unsigned char>(body[index])) != 0) {
        ++index;
    }

    std::size_t end = index;
    while (end < body.size() && (std::isdigit(static_cast<unsigned char>(body[end])) != 0 || body[end] == '-')) {
        ++end;
    }

    if (end == index) {
        return fallback;
    }

    try {
        return std::stoi(body.substr(index, end - index));
    } catch (...) {
        return fallback;
    }
}

std::string extract_json_object_field(const std::string& body, const std::string& key) {
    const auto key_pos = find_json_key(body, key);
    if (key_pos == std::string::npos) {
        return "{}";
    }

    const auto colon = body.find(':', key_pos);
    const auto open = body.find('{', colon + 1);
    if (colon == std::string::npos || open == std::string::npos) {
        return "{}";
    }

    int depth = 0;
    bool in_string = false;
    bool escaping = false;
    for (std::size_t index = open; index < body.size(); ++index) {
        const char ch = body[index];
        if (in_string) {
            if (!escaping && ch == '"') {
                in_string = false;
            }
            escaping = (!escaping && ch == '\\');
            continue;
        }

        if (ch == '"') {
            in_string = true;
            continue;
        }
        if (ch == '{') {
            ++depth;
        } else if (ch == '}') {
            --depth;
            if (depth == 0) {
                return body.substr(open, index - open + 1);
            }
        }
    }

    return "{}";
}

bool path_starts_with(const fs::path& candidate, const fs::path& base) {
    auto base_it = base.begin();
    auto candidate_it = candidate.begin();
    for (; base_it != base.end(); ++base_it, ++candidate_it) {
        if (candidate_it == candidate.end() || *candidate_it != *base_it) {
            return false;
        }
    }
    return true;
}

void close_socket(int fd) {
#if defined(_WIN32)
    closesocket(fd);
#else
    close(fd);
#endif
}

bool send_all(int fd, const char* data, std::size_t size) {
    std::size_t sent = 0;
    while (sent < size) {
        const auto bytes = send(fd, data + sent, static_cast<int>(size - sent), 0);
        if (bytes <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(bytes);
    }
    return true;
}

bool send_all(int fd, const std::string& data) {
    return send_all(fd, data.data(), data.size());
}

bool send_chunk(int fd, const char* data, std::size_t size) {
    std::ostringstream chunk_header;
    chunk_header << std::hex << size << "\r\n";
    if (!send_all(fd, chunk_header.str())) {
        return false;
    }
    if (size > 0 && !send_all(fd, data, size)) {
        return false;
    }
    return send_all(fd, "\r\n", 2);
}

bool send_chunk(int fd, const std::string& data) {
    return send_chunk(fd, data.data(), data.size());
}

int connect_upstream_socket(const LlamaRuntime& runtime) {
    const int upstream_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (upstream_fd < 0) {
        // #region debug-point upstream-connect
        report_debug_event("upstream.connect.socket_failed", "runtime=" + runtime.name() + " base_url=" + runtime.base_url());
        // #endregion debug-point upstream-connect
        return -1;
    }

    sockaddr_in upstream{};
    upstream.sin_family = AF_INET;
    upstream.sin_port = htons(static_cast<std::uint16_t>(runtime.port()));
    if (inet_pton(AF_INET, runtime.host().c_str(), &upstream.sin_addr) <= 0) {
        // #region debug-point upstream-connect
        report_debug_event("upstream.connect.invalid_host", "runtime=" + runtime.name() + " host=" + runtime.host());
        // #endregion debug-point upstream-connect
        close_socket(upstream_fd);
        return -1;
    }

    if (::connect(upstream_fd, reinterpret_cast<sockaddr*>(&upstream), sizeof(upstream)) < 0) {
        // #region debug-point upstream-connect
        std::ostringstream payload;
        payload << "runtime=" << runtime.name() << " base_url=" << runtime.base_url()
                << " binary_available=" << (runtime.binary_available() ? "true" : "false")
                << " model_available=" << (runtime.model_available() ? "true" : "false")
                << " running=" << (runtime.is_running() ? "true" : "false");
        report_debug_event("upstream.connect.refused", payload.str());
        // #endregion debug-point upstream-connect
        close_socket(upstream_fd);
        return -1;
    }

    return upstream_fd;
}

std::string status_text(int status_code) {
    switch (status_code) {
        case 200:
            return "OK";
        case 404:
            return "Not Found";
        case 502:
            return "Bad Gateway";
        default:
            return "Error";
    }
}

bool response_has_success_status(const std::string& response_header) {
    const auto first_break = response_header.find("\r\n");
    const std::string status_line =
        first_break == std::string::npos ? response_header : response_header.substr(0, first_break);
    return status_line.find(" 200 ") != std::string::npos;
}

}  // namespace

HttpServer::HttpServer(AppConfig config, MachineProfile machine, Database& database, LlamaRuntime& general_runtime,
                       LlamaRuntime& coder_runtime)
    : config_(std::move(config)),
      machine_(std::move(machine)),
      database_(database),
      general_runtime_(general_runtime),
      coder_runtime_(coder_runtime) {}

void HttpServer::run() {
#if defined(_WIN32)
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif

    const int server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        throw std::runtime_error("Unable to create HTTP server socket");
    }

    int reuse = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(static_cast<std::uint16_t>(config_.app_port));

    if (::bind(server_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        close_socket(server_fd);
        throw std::runtime_error("Unable to bind HTTP server port");
    }

    if (::listen(server_fd, 16) < 0) {
        close_socket(server_fd);
        throw std::runtime_error("Unable to listen on HTTP server socket");
    }

    while (true) {
        const int client_fd = ::accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) {
            continue;
        }
        std::thread(&HttpServer::handle_client, this, client_fd).detach();
    }
}

void HttpServer::handle_client(int client_fd) const {
    std::string request;
    char buffer[4096];
    while (true) {
        const auto bytes = recv(client_fd, buffer, sizeof(buffer), 0);
        if (bytes <= 0) {
            break;
        }
        request.append(buffer, static_cast<std::size_t>(bytes));
        if (request.find("\r\n\r\n") != std::string::npos) {
            break;
        }
    }

    if (request.empty()) {
        close_socket(client_fd);
        return;
    }

    const auto header_end = request.find("\r\n\r\n");
    const std::string head = request.substr(0, header_end);

    std::size_t content_length = 0;
    const auto content_header = head.find("Content-Length:");
    if (content_header != std::string::npos) {
        const auto line_end = head.find("\r\n", content_header);
        const auto raw = head.substr(content_header + 15, line_end - (content_header + 15));
        content_length = static_cast<std::size_t>(std::stoul(raw));
    }

    std::string body = request.substr(header_end + 4);
    while (body.size() < content_length) {
        const auto bytes = recv(client_fd, buffer, sizeof(buffer), 0);
        if (bytes <= 0) {
            break;
        }
        body.append(buffer, static_cast<std::size_t>(bytes));
    }

    std::istringstream request_stream(head);
    std::string method;
    std::string path;
    std::string version;
    request_stream >> method >> path >> version;
    // #region debug-point B:request-entry
    if (path == "/api/chat/stream" || path == "/api/chat" || path == "/api/agent") {
        std::ostringstream payload;
        payload << "method=" << method << " path=" << path << " body_bytes=" << body.size();
        report_debug_event("http.request.entry", payload.str());
    }
    // #endregion

    std::string content_type = "application/json";
    int status_code = 200;
    if (handle_stream_request(client_fd, method, path, body)) {
        close_socket(client_fd);
        return;
    }
    const std::string response_body = route_request(method, path, body, content_type, status_code);

    std::ostringstream response;
    response << "HTTP/1.1 " << status_code << " " << status_text(status_code) << "\r\n";
    response << "Content-Type: " << content_type << "\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Content-Length: " << response_body.size() << "\r\n";
    response << "Connection: close\r\n\r\n";
    response << response_body;

    const std::string payload = response.str();
    send(client_fd, payload.c_str(), static_cast<int>(payload.size()), 0);
    close_socket(client_fd);
}

bool HttpServer::handle_stream_request(int client_fd, const std::string& method, const std::string& path,
                                       const std::string& body) const {
    if (!(method == "POST" && path == "/api/chat/stream")) {
        return false;
    }

    LlamaRuntime& runtime = resolve_runtime_for_request(body, body);
    // #region debug-point C:stream-start
    report_debug_event("http.stream.begin", "runtime=" + runtime.name() + " body_bytes=" + std::to_string(body.size()));
    // #endregion
    if (!ensure_runtime_available(runtime)) {
        // #region debug-point C:stream-runtime-fail
        report_debug_event("http.stream.runtime_unavailable", "runtime=" + runtime.name());
        // #endregion
        const std::string error_body = "{\"error\":\"Unable to start llama-server\"}";
        std::ostringstream response;
        response << "HTTP/1.1 502 Bad Gateway\r\n";
        response << "Content-Type: application/json\r\n";
        response << "Access-Control-Allow-Origin: *\r\n";
        response << "Content-Length: " << error_body.size() << "\r\n";
        response << "Connection: close\r\n\r\n";
        response << error_body;
        send_all(client_fd, response.str());
        return true;
    }
    const int upstream_fd = connect_upstream_socket(runtime);
    if (upstream_fd < 0) {
        // #region debug-point C:stream-connect-fail
        report_debug_event("http.stream.connect_failed", "runtime=" + runtime.name());
        // #endregion
        const std::string error_body = "{\"error\":\"Unable to reach llama-server\"}";
        std::ostringstream response;
        response << "HTTP/1.1 502 Bad Gateway\r\n";
        response << "Content-Type: application/json\r\n";
        response << "Access-Control-Allow-Origin: *\r\n";
        response << "Content-Length: " << error_body.size() << "\r\n";
        response << "Connection: close\r\n\r\n";
        response << error_body;
        send_all(client_fd, response.str());
        return true;
    }

    const std::string stream_body = ensure_stream_enabled(body);
    std::ostringstream request;
    request << "POST /v1/chat/completions HTTP/1.1\r\n";
    request << "Host: " << runtime.host() << ":" << runtime.port() << "\r\n";
    request << "Content-Type: application/json\r\n";
    request << "Content-Length: " << stream_body.size() << "\r\n";
    request << "Connection: close\r\n\r\n";
    request << stream_body;

    if (!send_all(upstream_fd, request.str())) {
        // #region debug-point C:stream-send-fail
        report_debug_event("http.stream.upstream_send_failed", "runtime=" + runtime.name());
        // #endregion
        close_socket(upstream_fd);
        return true;
    }
    // #region debug-point C:stream-sent
    report_debug_event("http.stream.upstream_sent", "runtime=" + runtime.name() + " payload_bytes=" +
                                                       std::to_string(stream_body.size()));
    // #endregion

    std::string upstream_response;
    char buffer[4096];
    while (upstream_response.find("\r\n\r\n") == std::string::npos) {
        const auto bytes = recv(upstream_fd, buffer, sizeof(buffer), 0);
        if (bytes <= 0) {
            // #region debug-point C:stream-header-fail
            report_debug_event("http.stream.header_recv_failed", "runtime=" + runtime.name());
            // #endregion
            close_socket(upstream_fd);
            return true;
        }
        upstream_response.append(buffer, static_cast<std::size_t>(bytes));
    }

    const auto header_end = upstream_response.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        const std::string error_body = "{\"error\":\"Invalid upstream response\"}";
        std::ostringstream response;
        response << "HTTP/1.1 502 Bad Gateway\r\n";
        response << "Content-Type: application/json\r\n";
        response << "Access-Control-Allow-Origin: *\r\n";
        response << "Content-Length: " << error_body.size() << "\r\n";
        response << "Connection: close\r\n\r\n";
        response << error_body;
        send_all(client_fd, response.str());
        close_socket(upstream_fd);
        return true;
    }

    const std::string header = upstream_response.substr(0, header_end);
    // #region debug-point C:stream-header-ok
    report_debug_event("http.stream.header_received", "runtime=" + runtime.name());
    // #endregion
    if (!response_has_success_status(header)) {
        const std::string upstream_body = upstream_response.substr(header_end + 4);
        std::string error_body = upstream_body;
        if (error_body.empty()) {
            error_body = "{\"error\":\"Upstream streaming request failed\"}";
        }

        std::ostringstream response;
        response << "HTTP/1.1 502 Bad Gateway\r\n";
        response << "Content-Type: application/json\r\n";
        response << "Access-Control-Allow-Origin: *\r\n";
        response << "Content-Length: " << error_body.size() << "\r\n";
        response << "Connection: close\r\n\r\n";
        response << error_body;
        send_all(client_fd, response.str());
        close_socket(upstream_fd);
        return true;
    }

    std::ostringstream response;
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: text/event-stream; charset=utf-8\r\n";
    response << "Cache-Control: no-cache\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Connection: close\r\n";
    response << "Transfer-Encoding: chunked\r\n\r\n";
    if (!send_all(client_fd, response.str())) {
        // #region debug-point C:stream-client-header-fail
        report_debug_event("http.stream.client_header_failed", "runtime=" + runtime.name());
        // #endregion
        close_socket(upstream_fd);
        return true;
    }

    const std::string initial_body = upstream_response.substr(header_end + 4);
    if (!initial_body.empty() && !send_chunk(client_fd, initial_body)) {
        close_socket(upstream_fd);
        return true;
    }

    std::size_t streamed_bytes = initial_body.size();
    while (true) {
        const auto bytes = recv(upstream_fd, buffer, sizeof(buffer), 0);
        if (bytes <= 0) {
            break;
        }
        streamed_bytes += static_cast<std::size_t>(bytes);
        if (!send_chunk(client_fd, buffer, static_cast<std::size_t>(bytes))) {
            // #region debug-point C:stream-client-chunk-fail
            report_debug_event("http.stream.client_chunk_failed", "runtime=" + runtime.name() + " streamed_bytes=" +
                                                                       std::to_string(streamed_bytes));
            // #endregion
            break;
        }
    }

    send_all(client_fd, "0\r\n\r\n");
    // #region debug-point C:stream-finish
    report_debug_event("http.stream.finish", "runtime=" + runtime.name() + " streamed_bytes=" +
                                                  std::to_string(streamed_bytes));
    // #endregion
    close_socket(upstream_fd);
    return true;
}

std::string HttpServer::build_status_json() const {
    const HardwareProfile live_hardware = detect_hardware(machine_);
    std::ostringstream json;
    json << "{"
         << "\"machine_id\":\"" << json_escape(machine_.id) << "\","
         << "\"hostname\":\"" << json_escape(machine_.hostname) << "\","
         << "\"operating_system\":\"" << json_escape(machine_.operating_system) << "\","
         << "\"architecture\":\"" << json_escape(machine_.architecture) << "\","
         << "\"runtime_profile\":\"" << json_escape(config_.runtime_profile_name) << "\","
         << "\"gpu_backend\":\"" << json_escape(config_.detected_gpu_backend) << "\","
         << "\"logical_cores\":" << config_.detected_logical_cores << ","
         << "\"memory_gb\":" << config_.detected_memory_gb << ","
         << "\"memory_used_gb\":" << live_hardware.memory_used_gb << ","
         << "\"memory_available_gb\":" << live_hardware.memory_available_gb << ","
         << "\"runtimes\":{"
         << "\"general\":" << runtime_manifest_json(general_runtime_) << ","
         << "\"coder\":" << runtime_manifest_json(coder_runtime_)
         << "},"
         << "\"indexed_files\":" << database_.file_count() << "}";
    return json.str();
}

std::string HttpServer::build_files_json() const {
    const auto files = database_.recent_files(25);
    std::ostringstream json;
    json << "[";
    for (std::size_t index = 0; index < files.size(); ++index) {
        const auto& file = files[index];
        if (index > 0) {
            json << ",";
        }
        json << "{"
             << "\"path\":\"" << json_escape(file.path) << "\","
             << "\"machine_id\":\"" << json_escape(file.machine_id) << "\","
             << "\"size_bytes\":" << file.size << ","
             << "\"modified_at\":" << file.modified_at << "}";
    }
    json << "]";
    return json.str();
}

std::string HttpServer::build_agent_actions_json() const {
    return R"json([
  {"name":"get_status","description":"Get runtime, machine, and llama.cpp status.","args":{}},
  {"name":"list_recent_files","description":"List recently indexed files from the graph database.","args":{"limit":"integer up to 50"}},
  {"name":"read_text_file","description":"Read a UTF-8 text file from an allowed path.","args":{"path":"string","max_chars":"integer up to 12000"}},
  {"name":"list_directory","description":"List files and folders in an allowed directory.","args":{"path":"string","limit":"integer up to 100"}},
  {"name":"rescan_paths","description":"Rescan the configured host file paths and refresh the portable index.","args":{}}
])json";
}

LlamaRuntime& HttpServer::resolve_runtime_for_request(const std::string& request_body,
                                                      const std::string& prompt_hint) const {
    const std::string explicit_target = lowercase(extract_json_string_field(request_body, "target"));
    const std::string explicit_mode = lowercase(extract_json_string_field(request_body, "mode"));

    if ((explicit_target == "coder" || explicit_mode == "coder") && coder_runtime_.can_start()) {
        return coder_runtime_;
    }
    if (explicit_target == "general" || explicit_mode == "general") {
        return general_runtime_;
    }
    if (prompt_prefers_coder(prompt_hint) && coder_runtime_.can_start()) {
        return coder_runtime_;
    }
    return general_runtime_;
}

bool HttpServer::ensure_runtime_available(LlamaRuntime& runtime) const {
    std::lock_guard<std::mutex> lock(runtime_switch_mutex_);

    LlamaRuntime& other_runtime = (&runtime == &general_runtime_) ? coder_runtime_ : general_runtime_;
    if (other_runtime.is_running() || other_runtime.is_healthy()) {
        other_runtime.stop();
    }

    if (!runtime.is_running() && !runtime.is_healthy()) {
        runtime.start();
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(45);
    while (std::chrono::steady_clock::now() < deadline) {
        if (runtime.is_healthy()) {
            return true;
        }
        if (!runtime.is_running() && runtime.last_exit_status() != 0) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    return runtime.is_healthy();
}

bool HttpServer::prompt_prefers_coder(const std::string& prompt_hint) const {
    const std::string prompt = lowercase(prompt_hint);
    static const std::vector<std::string> keywords = {"code",      "bug",      "refactor", "function",
                                                      "class",     "compile",  "stack",    "trace",
                                                      "typescript","javascript","python",   "c++",
                                                      "rust",      "sql",      "debug",    "program"};
    for (const auto& keyword : keywords) {
        if (prompt.find(keyword) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::string HttpServer::proxy_chat_request(const std::string& request_body, LlamaRuntime& runtime) const {
    // #region debug-point D:proxy-start
    report_debug_event("http.proxy.begin", "runtime=" + runtime.name() + " body_bytes=" + std::to_string(request_body.size()));
    // #endregion
    if (!ensure_runtime_available(runtime)) {
        // #region debug-point D:proxy-runtime-fail
        report_debug_event("http.proxy.runtime_unavailable", "runtime=" + runtime.name());
        // #endregion
        return "{\"error\":\"Unable to start llama-server\"}";
    }

    const int upstream_fd = connect_upstream_socket(runtime);
    if (upstream_fd < 0) {
        // #region debug-point D:proxy-connect-fail
        report_debug_event("http.proxy.connect_failed", "runtime=" + runtime.name());
        // #endregion
        return "{\"error\":\"Unable to reach llama-server\"}";
    }

    std::ostringstream request;
    request << "POST /v1/chat/completions HTTP/1.1\r\n";
    request << "Host: " << runtime.host() << ":" << runtime.port() << "\r\n";
    request << "Content-Type: application/json\r\n";
    request << "Content-Length: " << request_body.size() << "\r\n";
    request << "Connection: close\r\n\r\n";
    request << request_body;

    const std::string payload = request.str();
    send(upstream_fd, payload.c_str(), static_cast<int>(payload.size()), 0);
    // #region debug-point D:proxy-sent
    report_debug_event("http.proxy.upstream_sent", "runtime=" + runtime.name() + " payload_bytes=" +
                                                    std::to_string(payload.size()));
    // #endregion

    std::string response;
    char buffer[4096];
    while (true) {
        const auto bytes = recv(upstream_fd, buffer, sizeof(buffer), 0);
        if (bytes <= 0) {
            break;
        }
        response.append(buffer, static_cast<std::size_t>(bytes));
    }

    close_socket(upstream_fd);

    const auto body_start = response.find("\r\n\r\n");
    if (body_start == std::string::npos) {
        // #region debug-point D:proxy-invalid
        report_debug_event("http.proxy.invalid_response", "runtime=" + runtime.name() + " response_bytes=" +
                                                            std::to_string(response.size()));
        // #endregion
        return "{\"error\":\"Invalid upstream response\"}";
    }
    // #region debug-point D:proxy-finish
    report_debug_event("http.proxy.finish", "runtime=" + runtime.name() + " response_bytes=" +
                                             std::to_string(response.size()));
    // #endregion
    return response.substr(body_start + 4);
}

std::string HttpServer::ensure_stream_enabled(const std::string& request_body) const {
    const auto stream_key = find_json_key(request_body, "stream");
    if (stream_key == std::string::npos) {
        const auto close = request_body.rfind('}');
        if (close == std::string::npos) {
            return request_body;
        }
        return request_body.substr(0, close) + ",\"stream\":true" + request_body.substr(close);
    }

    const auto colon = request_body.find(':', stream_key);
    if (colon == std::string::npos) {
        return request_body;
    }

    std::size_t value_start = colon + 1;
    while (value_start < request_body.size() &&
           std::isspace(static_cast<unsigned char>(request_body[value_start])) != 0) {
        ++value_start;
    }

    std::size_t value_end = value_start;
    while (value_end < request_body.size() &&
           std::isalpha(static_cast<unsigned char>(request_body[value_end])) != 0) {
        ++value_end;
    }

    std::string updated = request_body;
    updated.replace(value_start, value_end - value_start, "true");
    return updated;
}

std::string HttpServer::build_agent_model_request(const std::string& user_prompt, const std::string& scratchpad,
                                                  const std::string& runtime_name) const {
    const std::string system_prompt =
        "You are AI-On-A-Stick operating in agent mode. "
        "You may either answer directly or choose exactly one action at a time. "
        "Return JSON only with one of these shapes: "
        "{\"type\":\"answer\",\"content\":\"final reply for the user\"} "
        "or "
        "{\"type\":\"action\",\"name\":\"action_name\",\"args\":{...}}. "
        "Available actions: " +
        build_agent_actions_json();

    const std::string user_message =
        "Runtime target:\n" + runtime_name + "\n\nUser request:\n" + user_prompt +
        "\n\nCurrent machine status and previous tool work:\n" + scratchpad +
        "\n\nIf you already have enough information, answer. Otherwise choose one action.";

    std::ostringstream request;
    request << "{"
            << "\"messages\":["
            << "{\"role\":\"system\",\"content\":\"" << json_escape(system_prompt) << "\"},"
            << "{\"role\":\"user\",\"content\":\"" << json_escape(user_message) << "\"}"
            << "],"
            << "\"stream\":false"
            << "}";
    return request.str();
}

std::string HttpServer::extract_prompt_from_agent_request(const std::string& request_body) const {
    return extract_json_string_field(request_body, "prompt");
}

std::string HttpServer::extract_model_content(const std::string& model_response) const {
    const auto content_pos = model_response.rfind("\"content\"");
    if (content_pos == std::string::npos) {
        return "";
    }
    return extract_json_string_field(model_response, "content", content_pos);
}

HttpServer::AgentDecision HttpServer::parse_agent_decision(const std::string& model_content) const {
    AgentDecision decision;
    const auto open = model_content.find('{');
    const auto close = model_content.rfind('}');
    if (open == std::string::npos || close == std::string::npos || close <= open) {
        decision.valid = true;
        decision.type = "answer";
        decision.content = trim(model_content);
        return decision;
    }

    const std::string candidate = model_content.substr(open, close - open + 1);
    const std::string type = extract_json_string_field(candidate, "type");
    if (type == "answer") {
        decision.valid = true;
        decision.type = type;
        decision.content = extract_json_string_field(candidate, "content");
        if (decision.content.empty()) {
            decision.content = trim(model_content);
        }
        return decision;
    }

    if (type == "action") {
        decision.valid = true;
        decision.type = type;
        decision.name = extract_json_string_field(candidate, "name");
        decision.args_json = extract_json_object_field(candidate, "args");
        return decision;
    }

    decision.valid = true;
    decision.type = "answer";
    decision.content = trim(model_content);
    return decision;
}

bool HttpServer::is_allowed_host_path(const std::string& path) const {
    if (path.empty()) {
        return false;
    }

    std::error_code error;
    const fs::path candidate = fs::weakly_canonical(fs::path(path), error);
    if (error) {
        return false;
    }

    const fs::path usb_root = fs::weakly_canonical(fs::path(config_.usb_root), error);
    if (!error && path_starts_with(candidate, usb_root)) {
        return true;
    }

    for (const auto& root : config_.scan_paths) {
        error.clear();
        const fs::path allowed_root = fs::weakly_canonical(fs::path(root), error);
        if (!error && path_starts_with(candidate, allowed_root)) {
            return true;
        }
    }

    return false;
}

std::string HttpServer::execute_agent_action(const std::string& action_name, const std::string& args_json) const {
    if (action_name == "get_status") {
        return build_status_json();
    }

    if (action_name == "list_recent_files") {
        const int limit = std::clamp(extract_json_int_field(args_json, "limit", 10), 1, 50);
        const auto files = database_.recent_files(limit);
        std::ostringstream json;
        json << "{"
             << "\"action\":\"list_recent_files\","
             << "\"files\":[";
        for (std::size_t index = 0; index < files.size(); ++index) {
            if (index > 0) {
                json << ",";
            }
            const auto& file = files[index];
            json << "{"
                 << "\"path\":\"" << json_escape(file.path) << "\","
                 << "\"machine_id\":\"" << json_escape(file.machine_id) << "\","
                 << "\"size_bytes\":" << file.size << ","
                 << "\"modified_at\":" << file.modified_at << "}";
        }
        json << "]}";
        return json.str();
    }

    if (action_name == "rescan_paths") {
        database_.index_scan_paths(machine_, config_.scan_paths);
        std::ostringstream json;
        json << "{"
             << "\"action\":\"rescan_paths\","
             << "\"indexed_files\":" << database_.file_count() << ","
             << "\"scan_path_count\":" << config_.scan_paths.size() << "}";
        return json.str();
    }

    if (action_name == "read_text_file") {
        const std::string path = extract_json_string_field(args_json, "path");
        const int max_chars = std::clamp(extract_json_int_field(args_json, "max_chars", 4000), 256, 12000);
        if (!is_allowed_host_path(path)) {
            return "{\"error\":\"Path is not allowed for agent reads\"}";
        }

        std::ifstream input(path, std::ios::binary);
        if (!input) {
            return "{\"error\":\"Unable to open file\"}";
        }

        std::ostringstream buffer;
        buffer << input.rdbuf();
        std::string content = buffer.str();
        const bool truncated = static_cast<int>(content.size()) > max_chars;
        if (truncated) {
            content.resize(static_cast<std::size_t>(max_chars));
        }

        std::ostringstream json;
        json << "{"
             << "\"action\":\"read_text_file\","
             << "\"path\":\"" << json_escape(path) << "\","
             << "\"truncated\":" << (truncated ? "true" : "false") << ","
             << "\"content\":\"" << json_escape(content) << "\"}";
        return json.str();
    }

    if (action_name == "list_directory") {
        const std::string path = extract_json_string_field(args_json, "path");
        const int limit = std::clamp(extract_json_int_field(args_json, "limit", 25), 1, 100);
        if (!is_allowed_host_path(path)) {
            return "{\"error\":\"Path is not allowed for agent directory listing\"}";
        }

        const fs::path root(path);
        if (!fs::exists(root) || !fs::is_directory(root)) {
            return "{\"error\":\"Directory does not exist\"}";
        }

        std::ostringstream json;
        json << "{"
             << "\"action\":\"list_directory\","
             << "\"path\":\"" << json_escape(path) << "\","
             << "\"entries\":[";

        int count = 0;
        for (const auto& entry : fs::directory_iterator(root)) {
            if (count >= limit) {
                break;
            }
            if (count > 0) {
                json << ",";
            }
            json << "{"
                 << "\"name\":\"" << json_escape(entry.path().filename().string()) << "\","
                 << "\"path\":\"" << json_escape(entry.path().string()) << "\","
                 << "\"is_directory\":" << (entry.is_directory() ? "true" : "false") << "}";
            ++count;
        }
        json << "]}";
        return json.str();
    }

    return "{\"error\":\"Unknown agent action\"}";
}

std::string HttpServer::run_agent_request(const std::string& request_body) const {
    const std::string prompt = extract_prompt_from_agent_request(request_body);
    if (prompt.empty()) {
        return "{\"error\":\"Missing agent prompt\"}";
    }

    LlamaRuntime& runtime = resolve_runtime_for_request(request_body, prompt);
    std::string scratchpad = "Initial status:\n" + build_status_json();
    std::vector<std::string> action_log;

    for (int step = 0; step < 4; ++step) {
        const std::string model_request = build_agent_model_request(prompt, scratchpad, runtime.name());
        const std::string model_response = proxy_chat_request(model_request, runtime);
        if (model_response.find("\"error\"") != std::string::npos) {
            return model_response;
        }

        const std::string model_content = extract_model_content(model_response);
        const AgentDecision decision = parse_agent_decision(model_content);
        if (!decision.valid) {
            return "{\"error\":\"Invalid agent response\"}";
        }

        if (decision.type == "answer") {
            std::ostringstream json;
            json << "{"
                 << "\"reply\":\"" << json_escape(decision.content) << "\","
                 << "\"actions\":[";
            for (std::size_t index = 0; index < action_log.size(); ++index) {
                if (index > 0) {
                    json << ",";
                }
                json << "\"" << json_escape(action_log[index]) << "\"";
            }
            json << "]}";
            return json.str();
        }

        if (decision.type == "action") {
            const std::string result = execute_agent_action(decision.name, decision.args_json);
            action_log.push_back(runtime.name() + ":" + decision.name);
            scratchpad += "\n\nAction " + std::to_string(step + 1) + ": " + decision.name + "\nResult:\n" + result;
            continue;
        }
    }

    return "{\"error\":\"Agent reached max steps without final answer\"}";
}

std::string HttpServer::serve_static_file(const std::string& relative_path, std::string& content_type) const {
    std::string clean_path = relative_path;
    if (clean_path == "/" || clean_path.empty()) {
        clean_path = "/index.html";
    }

    const fs::path file_path = fs::path(config_.ui_root) / clean_path.substr(1);
    if (!fs::exists(file_path) || fs::is_directory(file_path)) {
        content_type = "text/plain";
        return "Not found";
    }

    if (file_path.extension() == ".html") {
        content_type = "text/html; charset=utf-8";
    } else if (file_path.extension() == ".css") {
        content_type = "text/css; charset=utf-8";
    } else if (file_path.extension() == ".js") {
        content_type = "application/javascript; charset=utf-8";
    } else {
        content_type = "application/octet-stream";
    }

    std::ifstream input(file_path, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string HttpServer::route_request(const std::string& method, const std::string& path, const std::string& body,
                                      std::string& content_type, int& status_code) const {
    if (method == "GET" && path == "/api/status") {
        content_type = "application/json";
        return build_status_json();
    }

    if (method == "GET" && path == "/api/files") {
        content_type = "application/json";
        return build_files_json();
    }

    if (method == "GET" && path == "/api/agent/actions") {
        content_type = "application/json";
        return build_agent_actions_json();
    }

    if (method == "POST" && path == "/api/chat") {
        content_type = "application/json";
        LlamaRuntime& runtime = resolve_runtime_for_request(body, body);
        const std::string response = proxy_chat_request(body, runtime);
        if (response.find("\"error\"") != std::string::npos) {
            status_code = 502;
        }
        return response;
    }

    if (method == "POST" && path == "/api/agent") {
        content_type = "application/json";
        const std::string response = run_agent_request(body);
        if (response.find("\"error\"") != std::string::npos) {
            status_code = 502;
        }
        return response;
    }

    if (method == "GET") {
        return serve_static_file(path, content_type);
    }

    status_code = 404;
    content_type = "application/json";
    return "{\"error\":\"Route not found\"}";
}
