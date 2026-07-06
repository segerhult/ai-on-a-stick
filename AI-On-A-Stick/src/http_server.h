#pragma once

#include <mutex>
#include <string>

#include "config.h"
#include "database.h"
#include "llama_runtime.h"
#include "machine.h"

class HttpServer {
  public:
    HttpServer(AppConfig config, MachineProfile machine, Database& database, LlamaRuntime& general_runtime,
               LlamaRuntime& coder_runtime);
    void run();

  private:
    struct AgentDecision {
        bool valid = false;
        std::string type;
        std::string name;
        std::string content;
        std::string args_json;
    };

    void handle_client(int client_fd) const;
    bool handle_stream_request(int client_fd, const std::string& method, const std::string& path,
                               const std::string& body) const;
    std::string build_status_json() const;
    std::string build_files_json() const;
    std::string build_agent_actions_json() const;
    std::string proxy_chat_request(const std::string& request_body, LlamaRuntime& runtime) const;
    bool ensure_runtime_available(LlamaRuntime& runtime) const;
    std::string ensure_stream_enabled(const std::string& request_body) const;
    std::string run_agent_request(const std::string& request_body) const;
    std::string build_agent_model_request(const std::string& user_prompt, const std::string& scratchpad,
                                          const std::string& runtime_name) const;
    std::string extract_prompt_from_agent_request(const std::string& request_body) const;
    std::string extract_model_content(const std::string& model_response) const;
    AgentDecision parse_agent_decision(const std::string& model_content) const;
    std::string execute_agent_action(const std::string& action_name, const std::string& args_json) const;
    LlamaRuntime& resolve_runtime_for_request(const std::string& request_body, const std::string& prompt_hint) const;
    bool prompt_prefers_coder(const std::string& prompt_hint) const;
    bool is_allowed_host_path(const std::string& path) const;
    std::string serve_static_file(const std::string& relative_path, std::string& content_type) const;
    std::string route_request(const std::string& method, const std::string& path, const std::string& body,
                              std::string& content_type, int& status_code) const;

    AppConfig config_;
    MachineProfile machine_;
    Database& database_;
    LlamaRuntime& general_runtime_;
    LlamaRuntime& coder_runtime_;
    mutable std::mutex runtime_switch_mutex_;
};
