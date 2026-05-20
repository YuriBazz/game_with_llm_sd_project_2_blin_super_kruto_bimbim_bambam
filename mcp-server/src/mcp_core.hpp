#pragma once

#include <nlohmann/json.hpp>
#include <functional>
#include <unordered_map>
#include <iostream>
#include <string>

using json = nlohmann::json;

namespace mcp {

struct JsonRpcMessage {
    std::string jsonrpc = "2.0";
    std::string id;
    std::string method;
    json params;
    json result;
    json error;
    bool is_request = false;
    bool is_response = false;
    bool is_notification = false;
};

struct Tool {
    std::string name;
    std::string description;
    json input_schema;
    std::function<json(const json&)> handler;
};

class McpServer {
private:
    std::unordered_map<std::string, Tool> tools_;
    std::string server_name_;
    std::string server_version_;

public:
    McpServer(const std::string& name, const std::string& version)
        : server_name_(name), server_version_(version) {}

    void register_tool(const std::string& name, const std::string& description,
                       const json& schema, std::function<json(const json&)> handler) {
        tools_[name] = {name, description, schema, handler};
    }

    std::string process_message(const std::string& input) {
        json req;
        try {
            req = json::parse(input);
        } catch (...) {
            return make_error_response("null", -32700, "Parse error").dump();
        }

        std::string method = req.value("method", "");
        std::string id = req.value("id", "null");

        if (method == "initialize") {
            return make_initialize_response(id).dump();
        }

        if (method == "tools/list") {
            return make_tools_list_response(id).dump();
        }

        if (method == "tools/call") {
            json params = req.value("params", json::object());
            std::string tool_name = params.value("name", "");
            json arguments = params.value("arguments", json::object());

            auto it = tools_.find(tool_name);
            if (it == tools_.end()) {
                return make_error_response(id, -32601, "Tool not found: " + tool_name).dump();
            }

            try {
                json result = it->second.handler(arguments);
                return make_tool_response(id, result).dump();
            } catch (const std::exception& e) {
                return make_error_response(id, -32000, e.what()).dump();
            }
        }

        return make_error_response(id, -32601, "Method not found: " + method).dump();
    }

private:
    json make_initialize_response(const std::string& id) {
        return {
            {"jsonrpc", "2.0"},
            {"id", id},
            {"result", {
                {"protocolVersion", "2024-11-05"},
                {"capabilities", {
                    {"tools", json::object()}
                }},
                {"serverInfo", {
                    {"name", server_name_},
                    {"version", server_version_}
                }}
            }}
        };
    }

    json make_tools_list_response(const std::string& id) {
        json tools_array = json::array();
        for (const auto& [name, tool] : tools_) {
            tools_array.push_back({
                {"name", tool.name},
                {"description", tool.description},
                {"inputSchema", tool.input_schema}
            });
        }
        return {
            {"jsonrpc", "2.0"},
            {"id", id},
            {"result", {{"tools", tools_array}}}
        };
    }

    json make_tool_response(const std::string& id, const json& content) {
        return {
            {"jsonrpc", "2.0"},
            {"id", id},
            {"result", {
                {"content", {
                    {{"type", "text"}, {"text", content.dump()}}
                }}
            }}
        };
    }

    json make_error_response(const std::string& id, int code, const std::string& message) {
        return {
            {"jsonrpc", "2.0"},
            {"id", id},
            {"error", {
                {"code", code},
                {"message", message}
            }}
        };
    }
};

} // namespace mcp
