#include "agent.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "logger.h"
#include "settings.h"

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
constexpr int kReturnFileWaitSeconds = 3;

AgentError make_error(const int code, std::string message) {
    return AgentError{code, std::move(message)};
}

AgentCommandResponse make_response(const bool success,
                                   const int code,
                                   std::string message,
                                   std::string output = "",
                                   std::vector<std::string> files = {},
                                   const bool restart_required = false) {
    return AgentCommandResponse{
        success,
        make_error(code, std::move(message)),
        std::move(output),
        std::move(files),
        restart_required
    };
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

std::string get_return_type(const json& data) {
    if (!data.contains("return") || !data["return"].is_object()) {
        return "string";
    }

    if (!data["return"].contains("type") || !data["return"]["type"].is_string()) {
        return "string";
    }

    return data["return"]["type"].get<std::string>();
}

AgentCommandResponse validate_return_files_schema(const std::string& job_name, const json& data) {
    if (data.contains("return_files") && !data["return_files"].is_boolean()) {
        return make_response(false, kAgentJobSchemaError, "Job '" + job_name + "' has non-boolean 'return_files' value.");
    }

    return make_response(true, 0, "");
}

AgentCommandResponse validate_restart_after_schema(const std::string& job_name, const json& data) {
    if (data.contains("restart_after") && !data["restart_after"].is_boolean()) {
        return make_response(false, kAgentJobSchemaError, "Job '" + job_name + "' has non-boolean 'restart_after' value.");
    }

    return make_response(true, 0, "");
}

bool should_return_files(const json& data) {
    return data.value("return_files", false);
}

bool should_restart_after(const json& data) {
    return data.value("restart_after", false);
}

std::vector<std::string> split_command_line_tokens(const std::string& value) {
    std::vector<std::string> tokens;
    std::string current;
    bool in_single_quotes = false;
    bool in_double_quotes = false;

    for (const char ch : value) {
        if (ch == '\'' && !in_double_quotes) {
            in_single_quotes = !in_single_quotes;
            continue;
        }
        if (ch == '"' && !in_single_quotes) {
            in_double_quotes = !in_double_quotes;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(ch)) && !in_single_quotes && !in_double_quotes) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }

        current += ch;
    }

    if (!current.empty()) {
        tokens.push_back(current);
    }

    return tokens;
}

std::string strip_path_token(std::string token) {
    token = trim(std::move(token));
    while (!token.empty() && (token.front() == '"' || token.front() == '\'')) {
        token.erase(token.begin());
    }
    while (!token.empty() && (token.back() == '"' || token.back() == '\'' ||
                              token.back() == ',' || token.back() == ';')) {
        token.pop_back();
    }
    return token;
}

bool has_path_signal(const std::string& token) {
    if (token.empty()) {
        return false;
    }
    if (token.rfind("http://", 0) == 0 || token.rfind("https://", 0) == 0) {
        return false;
    }
    if (token[0] == '-' || token[0] == '>' || token[0] == '<' || token[0] == '|') {
        return false;
    }

    const fs::path path(token);
    if (fs::exists(path)) {
        return true;
    }
    if (token.find('/') != std::string::npos || token.find('\\') != std::string::npos) {
        return true;
    }

    const std::string filename = path.filename().string();
    const auto dot_pos = filename.find_last_of('.');
    return dot_pos != std::string::npos && dot_pos > 0 && dot_pos + 1 < filename.size();
}

fs::path normalize_return_file_path(const std::string& token) {
    fs::path path(token);
    if (path.is_relative()) {
        path = fs::current_path() / path;
    }
    return path.lexically_normal();
}

void add_return_file_candidate(const std::string& raw_token,
                               std::set<std::string>& unique_paths,
                               std::vector<std::string>& candidates) {
    const std::string token = strip_path_token(raw_token);
    if (!has_path_signal(token)) {
        return;
    }

    const std::string normalized = normalize_return_file_path(token).string();
    if (unique_paths.insert(normalized).second) {
        candidates.push_back(normalized);
    }
}

std::vector<std::string> collect_return_file_candidates(const std::map<std::string, std::string>& args) {
    std::set<std::string> unique_paths;
    std::vector<std::string> candidates;

    for (const auto& [key, value] : args) {
        if (key == "program") {
            continue;
        }

        const std::vector<std::string> tokens = split_command_line_tokens(value);
        if (tokens.empty()) {
            add_return_file_candidate(value, unique_paths, candidates);
        }

        for (const std::string& token : tokens) {
            add_return_file_candidate(token, unique_paths, candidates);

            const auto equals_pos = token.find('=');
            if (equals_pos != std::string::npos && equals_pos + 1 < token.size()) {
                add_return_file_candidate(token.substr(equals_pos + 1), unique_paths, candidates);
            }
        }
    }

    return candidates;
}

bool can_open_for_reading(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return file.is_open();
}

std::vector<std::string> wait_for_return_files(const std::vector<std::string>& candidates) {
    struct FileState {
        bool ready = false;
        bool seen = false;
        std::uintmax_t size = 0;
        fs::file_time_type modified{};
        int stable_observations = 0;
    };

    std::vector<FileState> states(candidates.size());
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(kReturnFileWaitSeconds);

    while (std::chrono::steady_clock::now() <= deadline) {
        bool all_ready_or_missing = true;

        for (std::size_t index = 0; index < candidates.size(); ++index) {
            if (states[index].ready) {
                continue;
            }

            const fs::path path(candidates[index]);
            std::error_code error;
            if (!fs::exists(path, error) || !fs::is_regular_file(path, error)) {
                all_ready_or_missing = false;
                continue;
            }

            const std::uintmax_t size = fs::file_size(path, error);
            if (error) {
                all_ready_or_missing = false;
                continue;
            }

            const fs::file_time_type modified = fs::last_write_time(path, error);
            if (error) {
                all_ready_or_missing = false;
                continue;
            }

            FileState& state = states[index];
            if (state.seen && state.size == size && state.modified == modified && can_open_for_reading(path)) {
                ++state.stable_observations;
            } else {
                state.seen = true;
                state.size = size;
                state.modified = modified;
                state.stable_observations = 0;
            }

            if (state.stable_observations >= 1) {
                state.ready = true;
            } else {
                all_ready_or_missing = false;
            }
        }

        if (all_ready_or_missing) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    std::vector<std::string> files;
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        std::error_code error;
        const fs::path path(candidates[index]);
        if ((states[index].ready || fs::exists(path, error)) && fs::is_regular_file(path, error)) {
            files.push_back(candidates[index]);
        }
    }

    return files;
}

void replace_placeholder_if_present(std::string& command,
                                    const std::string& placeholder,
                                    const std::string& value) {
    size_t pos = 0;
    while ((pos = command.find(placeholder, pos)) != std::string::npos) {
        command.replace(pos, placeholder.length(), value);
        pos += value.length();
    }
}

AgentCommandResponse load_job(const std::string& name, json& data) {
    std::string jobs_dir_value;
    if (const char* jobs_dir_env = std::getenv("JOBS_DIR"); jobs_dir_env != nullptr) {
        jobs_dir_value = jobs_dir_env;
    } else {
        try {
            const Settings settings;
            jobs_dir_value = settings.jobs_dir();
        } catch (const std::exception&) {
            return make_response(false, kAgentJobsDirConfigError, "Environment variable JOBS_DIR is not set.");
        }
    }

    if (jobs_dir_value.empty()) {
        return make_response(false, kAgentJobsDirConfigError, "Environment variable JOBS_DIR is not set.");
    }

    const fs::path job_path = fs::path(jobs_dir_value) / (name + ".json");

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
    if (!logger::log_message("Starting command execution.\nCommand: " + cmd)) {
        return make_response(false, kAgentLogOpenError, "Failed to open logs.txt for writing.");
    }

    std::array<char, 128> buffer{};
    std::string result;
    const std::string command_with_stderr = cmd + " 2>&1";

    FILE *pipe = POPEN(command_with_stderr.c_str(), "r");
    if (!pipe) {
        logger::log_message("Failed to start command.\nCommand: " + cmd);
        return make_response(false, kAgentCommandStartError, "Couldn't run the command: " + cmd);
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result += buffer.data();
    }

    const int exit_code = PCLOSE(pipe);
    if (exit_code != 0) {
        logger::log_message("Command finished with non-zero exit code.\nCommand: " + cmd + "\nExit code: " + std::to_string(exit_code) + "\nOutput:\n" + result);
        return make_response(false, kAgentCommandExitError, "Command finished with exit code " + std::to_string(exit_code) + ".", result);
    }

    logger::log_message("Command finished successfully.\nCommand: " + cmd + "\nOutput:\n" + result);
    return make_response(true, 0, "", result);
}

AgentCommandResponse f(const std::string &name, const std::map<std::string, std::string>& args) {
    if (!logger::log_message("Starting job execution.\nJob: " + name)) {
        return make_response(false, kAgentLogOpenError, "Failed to open logs.txt for writing.");
    }

    json data;
    auto load_response = load_job(name, data);
    if (!load_response.success) {
        logger::log_message("Failed to load job.\nJob: " + name + "\nError: " + load_response.error.message);
        return load_response;
    }

    if (!data.contains("command") || !data["command"].is_object()) {
        const auto response = make_response(false, kAgentJobSchemaError, "Job '" + name + "' does not contain a valid 'command' object.");
        logger::log_message("Failed to execute job.\nJob: " + name + "\nError: " + response.error.message);
        return response;
    }

    if (!data["command"].contains(OS_NAME) || !data["command"][OS_NAME].is_string()) {
        const auto response = make_response(false, kAgentJobSchemaError, "Job '" + name + "' does not define a command for OS '" OS_NAME "'.");
        logger::log_message("Failed to execute job.\nJob: " + name + "\nError: " + response.error.message);
        return response;
    }

    auto return_files_schema_response = validate_return_files_schema(name, data);
    if (!return_files_schema_response.success) {
        logger::log_message("Job schema validation failed.\nJob: " + name + "\nError: " + return_files_schema_response.error.message);
        return return_files_schema_response;
    }

    auto restart_after_schema_response = validate_restart_after_schema(name, data);
    if (!restart_after_schema_response.success) {
        logger::log_message("Job schema validation failed.\nJob: " + name + "\nError: " + restart_after_schema_response.error.message);
        return restart_after_schema_response;
    }

    auto validation_response = validate_args(name, data, args);
    if (!validation_response.success) {
        logger::log_message("Job argument validation failed.\nJob: " + name + "\nError: " + validation_response.error.message);
        return validation_response;
    }

    const bool return_files_enabled = should_return_files(data);
    const bool restart_after_enabled = should_restart_after(data);
    const std::vector<std::string> return_file_candidates = return_files_enabled
        ? collect_return_file_candidates(args)
        : std::vector<std::string>{};

    std::string command = data["command"][OS_NAME];
    for (const auto& [key, value] : args) {
        const std::string placeholder = "{" + key + "}";
        try {
            replacePlaceholder(command, placeholder, value);
        } catch (const std::exception& e) {
            const auto response = make_response(false, kAgentPlaceholderError, "Failed to prepare command for job '" + name + "': " + e.what());
            logger::log_message("Failed to execute job.\nJob: " + name + "\nError: " + response.error.message);
            return response;
        }
    }
    for (auto it = data["parameters"].begin(); it != data["parameters"].end(); ++it) {
        if (args.find(it.key()) == args.end() && !it.value().value("required", false)) {
            replace_placeholder_if_present(command, "{" + it.key() + "}", "");
        }
    }

    logger::log_message("Job prepared successfully.\nJob: " + name + "\nCommand: " + command);
    auto command_response = execCommand(command);
    command_response.restart_required = command_response.success && restart_after_enabled;
    if (return_files_enabled) {
        command_response.files = wait_for_return_files(return_file_candidates);
        logger::log_message(
            "Return files collected.\nJob: " + name +
            "\nCandidates: " + std::to_string(return_file_candidates.size()) +
            "\nFiles: " + std::to_string(command_response.files.size())
        );
    }

    if (!command_response.success) {
        logger::log_message("Job execution failed.\nJob: " + name + "\nError: " + command_response.error.message);
        return command_response;
    }

    if (get_return_type(data) != "string") {
        command_response.output = "success";
    }

    logger::log_message("Job finished successfully.\nJob: " + name);
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
