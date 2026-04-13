#include "api.h"

#include <cpr/cpr.h>

#include <iostream>
#include <utility>

#include "logger.h"

std::string Api::extract_code_response(const nlohmann::json& json_response) {
    if (json_response.contains("code_responce") && json_response["code_responce"].is_string()) {
        return json_response["code_responce"].get<std::string>();
    }
    if (json_response.contains("code_response") && json_response["code_response"].is_string()) {
        return json_response["code_response"].get<std::string>();
    }
    return "";
}

std::string Api::extract_msg(const nlohmann::json& json_response) {
    if (json_response.contains("msg") && json_response["msg"].is_string()) {
        return json_response["msg"].get<std::string>();
    }
    if (json_response.contains("message") && json_response["message"].is_string()) {
        return json_response["message"].get<std::string>();
    }
    return "Unknown API error";
}

std::map<std::string, std::string> Api::extract_options(const nlohmann::json& json_response) {
    std::map<std::string, std::string> options;
    if (!json_response.contains("options")) {
        return options;
    }

    nlohmann::json options_json;
    if (json_response["options"].is_object()) {
        options_json = json_response["options"];
    } else if (json_response["options"].is_string()) {
        try {
            options_json = nlohmann::json::parse(json_response["options"].get<std::string>());
        } catch (...) {
            return options;
        }
    } else {
        return options;
    }

    if (!options_json.is_object()) {
        return options;
    }

    for (auto it = options_json.begin(); it != options_json.end(); ++it) {
        if (it.value().is_string()) {
            options[it.key()] = it.value().get<std::string>();
        } else {
            options[it.key()] = it.value().dump();
        }
    }

    return options;
}

int Api::parse_code_response(const std::string& code_response) {
    if (code_response.empty()) {
        return -1000;
    }
    try {
        return std::stoi(code_response);
    } catch (...) {
        return -1000;
    }
}

ApiError Api::make_error(const int code, std::string message, std::string status) {
    return ApiError{code, std::move(message), std::move(status)};
}

Api::Api(std::string api_url, std::string uid, std::string description)
    : api_url(std::move(api_url)), uid(std::move(uid)), description(std::move(description)) {}

void Api::set_access_code(std::string value) {
    access_code = std::move(value);
}

RegisterAgentResponse Api::register_agent() {
    if (!logger::log_message("RegisterAgent request started for UID: " + uid)) {
        std::cerr << "Failed to open logs.txt for writing." << std::endl;
        return RegisterAgentResponse{false, make_error(-1002, "Failed to open logs.txt for writing."), ""};
    }

    try {
        const nlohmann::json request_body = {
            {"UID", uid},
            {"descr", description}
        };

        cpr::Response response = cpr::Post(
            cpr::Url{ api_url + "/wa_reg"},
            cpr::Body(request_body.dump()),
            cpr::Header{{"Content-Type", "application/json"}}
        );

        if (response.status_code == 200) {
            const nlohmann::json json_response = nlohmann::json::parse(response.text);
            const std::string code_response = extract_code_response(json_response);
            const int code_response_int = parse_code_response(code_response);
            const std::string status = json_response.value("status", "");
            if (code_response_int == 0) {
                access_code = json_response.value("access_code", "");
                logger::log_message("Successfully registered agent with UID: " + uid);

                return RegisterAgentResponse{true, make_error(0, "", status), access_code};
            }
            const std::string message = extract_msg(json_response);
            logger::log_message(
                "Failed to register agent with UID: " + uid + "\n"
                "API responded with code_response: " + code_response + "\n"
                "Response message: " + message
            );

            return RegisterAgentResponse{false, make_error(code_response_int, message, status), ""};
        }

        logger::log_message(
            "Failed to register agent with UID: " + uid + "\n"
            "Status Code: " + std::to_string(response.status_code) + "\n"
            "Response Body: " + response.text
        );

        return RegisterAgentResponse{
            false,
            make_error(-1003, "Unexpected HTTP status code: " + std::to_string(response.status_code)),
            ""
        };
    } catch (std::exception& e) {
        logger::log_message(
            "Exception occurred while registering agent with UID: " + uid + "\n"
            "Exception message: " + std::string(e.what())
        );

        return RegisterAgentResponse{
            false,
            make_error(-1004, std::string("Exception occurred while registering agent: ") + e.what()),
            ""
        };
    }
}

NewTaskResponse Api::new_tasks() {
    if (access_code.empty()) {
        std::cerr << "Access code is empty. Please register the agent first." << std::endl;
        return NewTaskResponse{false, make_error(-1001, "Access code is empty. Please register the agent first."), "", {}, "", ""};
    }

    if (!logger::log_message("NewTask request started for UID: " + uid)) {
        std::cerr << "Failed to open logs.txt for writing." << std::endl;
        return NewTaskResponse{false, make_error(-1002, "Failed to open logs.txt for writing."), "", {}, "", ""};
    }

    try {
        nlohmann::json request_body = {
            {"UID", uid},
            {"descr", description},
            {"access_code", access_code}
        };

        cpr::Response response = cpr::Post(
            cpr::Url{ api_url + "/wa_task"},
            cpr::Body(request_body.dump()),
            cpr::Header{{"Content-Type", "application/json"}}
        );

        if (response.status_code == 200) {
            nlohmann::json json_response = nlohmann::json::parse(response.text);
            const std::string code_response = extract_code_response(json_response);
            const int code_response_int = parse_code_response(code_response);
            const std::string status = json_response.value("status", "");

            if (code_response_int == 1) {
                logger::log_message("Successfully got new task for agent with UID: " + uid);

                return NewTaskResponse{
                    true,
                    make_error(0, "", status),
                    json_response.value("task_code", ""),
                    extract_options(json_response),
                    json_response.value("session_id", ""),
                    status
                };
            }

            if (code_response_int == 0) {
                logger::log_message("No task available for agent with UID: " + uid);
                return NewTaskResponse{true, make_error(0, "", status.empty() ? "WAIT" : status), "", {}, "", status.empty() ? "WAIT" : status};
            }

            const std::string message = extract_msg(json_response);
            logger::log_message(
                "Failed to get new tasks for agent with UID: " + uid + "\n"
                "API responded with code_response: " + code_response + "\n"
                "Response message: " + message
            );
            return NewTaskResponse{false, make_error(code_response_int, message, status), "", {}, "", status};
        }

        logger::log_message(
            "Failed to get new tasks for agent with UID: " + uid + "\n"
            "Status Code: " + std::to_string(response.status_code) + "\n"
            "Response Body: " + response.text
        );
        return NewTaskResponse{false, make_error(-1003, "Unexpected HTTP status code: " + std::to_string(response.status_code)), "", {}, "", ""};
    } catch (std::exception& e) {
        logger::log_message(
            "Failed to get new tasks for agent with UID: " + uid + "\n"
            "Exception message: " + std::string(e.what())
        );

        return NewTaskResponse{false, make_error(-1004, std::string("Exception occurred while getting new tasks: ") + e.what()), "", {}, "", ""};
    }
}

TaskResultResponse Api::send_task_result(const int result_code, const std::string& message, const std::string& session_id, const std::vector<std::string>& files) const {
    if (access_code.empty()) {
        std::cerr << "Access code is empty. Please register the agent first." << std::endl;
        return TaskResultResponse{false, make_error(-1001, "Access code is empty. Please register the agent first.")};
    }

    if (!logger::log_message("TaskResult request started for UID: " + uid)) {
        std::cerr << "Failed to open logs.txt for writing." << std::endl;
        return TaskResultResponse{false, make_error(-1002, "Failed to open logs.txt for writing.")};
    }

    try {
        const nlohmann::json result_payload = {
            {"UID", uid},
            {"access_code", access_code},
            {"message", message},
            {"files", static_cast<int>(files.size())},
            {"session_id", session_id}
        };

        std::vector<cpr::Part> parts;
        parts.emplace_back("result_code", result_code);
        parts.emplace_back("result", result_payload.dump(), "application/json");
        for (size_t i = 0; i < files.size(); ++i) {
            const std::string field_name = "file" + std::to_string(i + 1);
            parts.emplace_back(field_name, cpr::Files{cpr::File{files[i]}});
        }

        cpr::Response response = cpr::Post(
            cpr::Url{ api_url + "/wa_result"},
            cpr::Multipart{parts}
        );

        if (response.status_code == 200) {
            nlohmann::json json_response = nlohmann::json::parse(response.text);
            const std::string code_response = extract_code_response(json_response);
            const int code_response_int = parse_code_response(code_response);
            const std::string status = json_response.value("status", "");
            if (code_response_int == 0) {
                logger::log_message(
                    "Successfully sent task result for agent with UID: " + uid + "\n"
                    "result_code: " + std::to_string(result_code) + ", files: " + std::to_string(files.size())
                );
                return TaskResultResponse{true, make_error(0, "", status)};
            }

            const std::string response_message = extract_msg(json_response);
            logger::log_message(
                "Failed to send task result for agent with UID: " + uid + "\n"
                "result_code: " + std::to_string(result_code) + ", files: " + std::to_string(files.size()) + "\n"
                "API responded with code_response: " + code_response + "\n"
                "Response message: " + response_message
            );
            return TaskResultResponse{false, make_error(code_response_int, response_message, status)};
        }

        logger::log_message(
            "Failed to send task result for agent with UID: " + uid + "\n"
            "result_code: " + std::to_string(result_code) + ", files: " + std::to_string(files.size()) + "\n"
            "Status Code: " + std::to_string(response.status_code) + "\n"
            "Response Body: " + response.text
        );
        return TaskResultResponse{false, make_error(-1003, "Unexpected HTTP status code: " + std::to_string(response.status_code))};
    } catch (std::exception& e) {
        logger::log_message(
            "Exception while sending task result for agent with UID: " + uid + "\n"
            "result_code: " + std::to_string(result_code) + ", files: " + std::to_string(files.size()) + "\n"
            "Exception message: " + std::string(e.what())
        );
        return TaskResultResponse{false, make_error(-1004, std::string("Exception occurred while sending task result: ") + e.what())};
    }
}
