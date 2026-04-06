#include "agent.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <utility>

#include <dotenv.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {

constexpr int kAgentLogOpenError = -2001;
constexpr int kAgentJobFileNotFoundError = -2002;
constexpr int kAgentJobFileReadError = -2003;
constexpr int kAgentJobSchemaError = -2004;
constexpr int kAgentMissingArgumentError = -2005;
constexpr int kAgentUnknownArgumentError = -2006;
constexpr int kAgentArgumentTypeError = -2007;
constexpr int kAgentPlaceholderError = -2008;
constexpr int kAgentCommandStartError = -2009;
constexpr int kAgentCommandExitError = -2010;
constexpr int kAgentJobsDirConfigError = -2011;

AgentError make_error(const int code, std::string message) {
    return AgentError{code, std::move(message)};
}

AgentCommandResponse make_response(const bool success, const int code, std::string message, std::string output = "") {
    return AgentCommandResponse{success, make_error(code, std::move(message)), std::move(output)};
}

std::ofstream open_logs() {
    std::ofstream logs("logs.txt", std::ios::app);
    return logs;
}

void log_message(std::ofstream& logs, const std::string& message) {
    logs << "----------\n" << message << "\n";
}

void init_dotenv_from_current_or_parent_dirs() {
    fs::path current = fs::current_path();

    while (true) {
        const fs::path dotenv_path = current / ".env";
        if (fs::exists(dotenv_path)) {
            dotenv::init(dotenv_path.string().c_str());
            return;
        }

        if (current == current.parent_path()) {
            return;
        }

        current = current.parent_path();
    }
}

std::string trim(std::string value) {
    const auto not_space = [](const unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

bool is_integer(const std::string& value) {
    if (value.empty()) {
        return false;
    }

    size_t index = 0;
    if (value[0] == '+' || value[0] == '-') {
        if (value.size() == 1) {
            return false;
        }
        index = 1;
    }

    for (; index < value.size(); ++index) {
        if (!std::isdigit(static_cast<unsigned char>(value[index]))) {
            return false;
        }
    }

    return true;
}

bool is_float_number(const std::string& value) {
    if (value.empty()) {
        return false;
    }

    try {
        size_t pos = 0;
        std::stod(value, &pos);
        return pos == value.size();
    } catch (...) {
        return false;
    }
}

bool is_bool(const std::string& value) {
    const std::string normalized = trim(value);
    return normalized == "true" || normalized == "false" || normalized == "1" || normalized == "0";
}

AgentError validate_argument_type(const std::string& arg_name, const std::string& declared_type, const std::string& value) {
    if (declared_type == "string") {
        return make_error(0, "");
    }
    if (declared_type == "int" || declared_type == "integer") {
        if (is_integer(value)) {
            return make_error(0, "");
        }
        return make_error(kAgentArgumentTypeError, "Argument '" + arg_name + "' must have type '" + declared_type + "', got value '" + value + "'.");
    }
    if (declared_type == "float" || declared_type == "double" || declared_type == "number") {
        if (is_float_number(value)) {
            return make_error(0, "");
        }
        return make_error(kAgentArgumentTypeError, "Argument '" + arg_name + "' must have type '" + declared_type + "', got value '" + value + "'.");
    }
    if (declared_type == "bool" || declared_type == "boolean") {
        if (is_bool(value)) {
            return make_error(0, "");
        }
        return make_error(kAgentArgumentTypeError, "Argument '" + arg_name + "' must have type '" + declared_type + "', got value '" + value + "'.");
    }

    return make_error(kAgentJobSchemaError, "Job parameter '" + arg_name + "' has unsupported type '" + declared_type + "'.");
}

AgentCommandResponse validate_args(const std::string& job_name, const json& data, const std::map<std::string, std::string>& args) {
    if (!data.contains("parameters") || !data["parameters"].is_object()) {
        return make_response(false, kAgentJobSchemaError, "Job '" + job_name + "' does not contain a valid 'parameters' object.");
    }

    const json& parameters = data["parameters"];
    for (auto it = parameters.begin(); it != parameters.end(); ++it) {
        if (!it.value().is_object()) {
            return make_response(false, kAgentJobSchemaError, "Parameter '" + it.key() + "' in job '" + job_name + "' must be a JSON object.");
        }

        const bool required = it.value().value("required", false);
        if (required && args.find(it.key()) == args.end()) {
            return make_response(false, kAgentMissingArgumentError, "Missing required argument '" + it.key() + "' for job '" + job_name + "'.");
        }

        const auto arg_it = args.find(it.key());
        if (arg_it != args.end()) {
            if (!it.value().contains("type") || !it.value()["type"].is_string()) {
                return make_response(false, kAgentJobSchemaError, "Parameter '" + it.key() + "' in job '" + job_name + "' does not define a valid type.");
            }

            const auto type_error = validate_argument_type(it.key(), it.value()["type"].get<std::string>(), arg_it->second);
            if (type_error.code != 0) {
                return make_response(false, type_error.code, type_error.message);
            }
        }
    }

    for (const auto& [key, value] : args) {
        (void)value;
        if (!parameters.contains(key)) {
            return make_response(false, kAgentUnknownArgumentError, "Unknown argument '" + key + "' for job '" + job_name + "'.");
        }
    }

    return make_response(true, 0, "");
}

AgentCommandResponse load_job(const std::string& name, json& data) {
    const char* jobs_dir_env = std::getenv("JOBS_DIR");
    if (jobs_dir_env == nullptr || std::string(jobs_dir_env).empty()) {
        init_dotenv_from_current_or_parent_dirs();
        jobs_dir_env = std::getenv("JOBS_DIR");
    }

    if (jobs_dir_env == nullptr || std::string(jobs_dir_env).empty()) {
        return make_response(false, kAgentJobsDirConfigError, "Environment variable JOBS_DIR is not set.");
    }

    const fs::path job_path = fs::path(jobs_dir_env) / (name + ".json");

    if (!fs::exists(job_path)) {
        return make_response(false, kAgentJobFileNotFoundError, "Job description '" + job_path.string() + "' was not found.");
    }

    std::ifstream file(job_path);
    if (!file.is_open()) {
        return make_response(false, kAgentJobFileReadError, "Failed to open job description '" + job_path.string() + "'.");
    }

    try {
        data = json::parse(file);
    } catch (const std::exception& e) {
        return make_response(false, kAgentJobFileReadError, "Failed to parse job description '" + job_path.string() + "': " + e.what());
    }

    return make_response(true, 0, "");
}

}  // namespace

AgentCommandResponse execCommand(const std::string &cmd) {
    std::ofstream logs = open_logs();
    if (!logs.is_open()) {
        return make_response(false, kAgentLogOpenError, "Failed to open logs.txt for writing.");
    }

    std::array<char, 128> buffer{};
    std::string result;
    const std::string command_with_stderr = cmd + " 2>&1";

    log_message(logs, "Starting command execution.\nCommand: " + cmd);

    FILE *pipe = POPEN(command_with_stderr.c_str(), "r");
    if (!pipe) {
        log_message(logs, "Failed to start command.\nCommand: " + cmd);
        return make_response(false, kAgentCommandStartError, "Couldn't run the command: " + cmd);
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result += buffer.data();
    }

    const int exit_code = PCLOSE(pipe);
    if (exit_code != 0) {
        log_message(logs, "Command finished with non-zero exit code.\nCommand: " + cmd + "\nExit code: " + std::to_string(exit_code) + "\nOutput:\n" + result);
        return make_response(false, kAgentCommandExitError, "Command finished with exit code " + std::to_string(exit_code) + ".", result);
    }

    log_message(logs, "Command finished successfully.\nCommand: " + cmd + "\nOutput:\n" + result);
    return make_response(true, 0, "", result);
}

AgentCommandResponse f(const std::string &name, const std::map<std::string, std::string>& args) {
    std::ofstream logs = open_logs();
    if (!logs.is_open()) {
        return make_response(false, kAgentLogOpenError, "Failed to open logs.txt for writing.");
    }

    log_message(logs, "Starting job execution.\nJob: " + name);

    json data;
    auto load_response = load_job(name, data);
    if (!load_response.success) {
        log_message(logs, "Failed to load job.\nJob: " + name + "\nError: " + load_response.error.message);
        return load_response;
    }

    if (!data.contains("command") || !data["command"].is_object()) {
        const auto response = make_response(false, kAgentJobSchemaError, "Job '" + name + "' does not contain a valid 'command' object.");
        log_message(logs, "Failed to execute job.\nJob: " + name + "\nError: " + response.error.message);
        return response;
    }

    if (!data["command"].contains(OS_NAME) || !data["command"][OS_NAME].is_string()) {
        const auto response = make_response(false, kAgentJobSchemaError, "Job '" + name + "' does not define a command for OS '" OS_NAME "'.");
        log_message(logs, "Failed to execute job.\nJob: " + name + "\nError: " + response.error.message);
        return response;
    }

    auto validation_response = validate_args(name, data, args);
    if (!validation_response.success) {
        log_message(logs, "Job argument validation failed.\nJob: " + name + "\nError: " + validation_response.error.message);
        return validation_response;
    }

    std::string command = data["command"][OS_NAME];
    for (const auto& [key, value] : args) {
        const std::string placeholder = "{" + key + "}";
        try {
            replacePlaceholder(command, placeholder, value);
        } catch (const std::exception& e) {
            const auto response = make_response(false, kAgentPlaceholderError, "Failed to prepare command for job '" + name + "': " + e.what());
            log_message(logs, "Failed to execute job.\nJob: " + name + "\nError: " + response.error.message);
            return response;
        }
    }

    log_message(logs, "Job prepared successfully.\nJob: " + name + "\nCommand: " + command);
    auto command_response = execCommand(command);
    if (!command_response.success) {
        log_message(logs, "Job execution failed.\nJob: " + name + "\nError: " + command_response.error.message);
        return command_response;
    }

    log_message(logs, "Job finished successfully.\nJob: " + name);
    return command_response;
}

void replacePlaceholder(std::string &command,
                        const std::string &placeholder,
                        const std::string &value) {
    size_t pos = 0;
    bool replaced = false;
    while ((pos = command.find(placeholder, pos)) != std::string::npos) {
        command.replace(pos, placeholder.length(), value);
        pos += value.length();
        replaced = true;
    }

    if (!replaced) {
        throw std::runtime_error("Placeholder '" + placeholder + "' was not found in the command template.");
    }
}
