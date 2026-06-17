#pragma once

#include <nlohmann/json.hpp>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

using json = nlohmann::json;

class McpClient {
private:
    pid_t mcp_pid_ = -1;
    int write_fd_ = -1;
    int read_fd_ = -1;
    int request_id_ = 0;

    void close_fds() {
        if (write_fd_ >= 0) close(write_fd_);
        if (read_fd_ >= 0) close(read_fd_);
        write_fd_ = read_fd_ = -1;
    }

    std::string read_line() {
        std::string line;
        char ch = '\0';
        while (read(read_fd_, &ch, 1) == 1) {
            if (ch == '\n') break;
            line.push_back(ch);
        }
        return line;
    }

    json send_request(const std::string& method, const json& params = json::object()) {
        const std::string id = std::to_string(++request_id_);
        json req = {
            {"jsonrpc", "2.0"},
            {"id", id},
            {"method", method},
            {"params", params}
        };

        const std::string payload = req.dump() + "\n";
        if (write(write_fd_, payload.c_str(), payload.size()) < 0) {
            throw std::runtime_error("Failed to write to MCP server");
        }

        const std::string response_line = read_line();
        if (response_line.empty()) {
            throw std::runtime_error("Empty response from MCP server");
        }

        json response = json::parse(response_line);
        if (response.contains("error")) {
            throw std::runtime_error(response["error"].value("message", "MCP error"));
        }
        return response["result"];
    }

    json extract_tool_content(const json& result) {
        if (!result.contains("content") || !result["content"].is_array() || result["content"].empty()) {
            return result;
        }
        const json& first = result["content"][0];
        if (first.contains("text")) {
            return json::parse(first["text"].get<std::string>());
        }
        return result;
    }

public:
    explicit McpClient(const std::string& server_path) {
        int to_mcp[2];
        int from_mcp[2];
        if (pipe(to_mcp) != 0 || pipe(from_mcp) != 0) {
            throw std::runtime_error("Failed to create pipes for MCP server");
        }

        mcp_pid_ = fork();
        if (mcp_pid_ < 0) {
            throw std::runtime_error("Failed to fork MCP server");
        }

        if (mcp_pid_ == 0) {
            dup2(to_mcp[0], STDIN_FILENO);
            dup2(from_mcp[1], STDOUT_FILENO);
            close(to_mcp[0]);
            close(to_mcp[1]);
            close(from_mcp[0]);
            close(from_mcp[1]);
            execl(server_path.c_str(), server_path.c_str(), nullptr);
            std::cerr << "Failed to exec MCP server: " << server_path << std::endl;
            _exit(1);
        }

        close(to_mcp[0]);
        close(from_mcp[1]);
        write_fd_ = to_mcp[1];
        read_fd_ = from_mcp[0];

        send_request("initialize", json::object());
        std::cout << "Connected to MCP server: " << server_path << std::endl;
    }

    ~McpClient() {
        close_fds();
        if (mcp_pid_ > 0) {
            kill(mcp_pid_, SIGTERM);
            waitpid(mcp_pid_, nullptr, 0);
        }
    }

    std::vector<json> list_tools() {
        json result = send_request("tools/list");
        std::vector<json> tools;
        for (const auto& tool : result["tools"]) {
            tools.push_back(tool);
        }
        return tools;
    }

    json call_tool(const std::string& name, const json& arguments = json::object()) {
        json result = send_request("tools/call", {
            {"name", name},
            {"arguments", arguments}
        });
        return extract_tool_content(result);
    }

    static std::unique_ptr<McpClient> from_env() {
        const char* path = std::getenv("MCP_SERVER_PATH");
        if (!path) {
            path = "/usr/local/bin/mcp_server";
        }
        return std::make_unique<McpClient>(path);
    }
};
