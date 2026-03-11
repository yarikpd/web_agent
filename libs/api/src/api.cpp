#include "api.h"

#include <cpr/cpr.h>

#include <utility>
#include <fstream>
#include <iostream>

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
    if (!json_response.contains("options") || !json_response["options"].is_object()) {
        return options;
    }

    for (auto it = json_response["options"].begin(); it != json_response["options"].end(); ++it) {
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

bool Api::register_agent() {
    std::ofstream logs;
    logs.open("logs.txt", std::ios::app);
    if (!logs.is_open()) {
        std::cerr << "Failed to open logs.txt for writing." << std::endl;
        return false;
    }

    try {
        cpr::Response response = cpr::Post(
            cpr::Url{ api_url + "/wa_reg"},
            cpr::Body(
                R"({"uid": ")" + uid + R"(", "descr": ")" + description + "\"}"
            )
        );

        if (response.status_code == 200) {
            const nlohmann::json json_response = nlohmann::json::parse(response.text);
            const std::string code_response = extract_code_response(json_response);
            if (code_response == "0") {
                access_code = json_response.value("access_code", "");
                logs << "----------\n" << "Successfully registered agent with UID: " << uid << "\n";

                return true;
            }
            logs << "----------\n" << "Failed to register agent with UID: " << uid << "\n"
                 << "API responded with code_response: " << code_response << "\n"
                 << "Response message: " << extract_msg(json_response) << "\n";

            return false;
        }

        logs << "----------\n" << "Failed to register agent with UID: " << uid << "\n"
             << "Status Code: " << response.status_code << "\n"
             << "Response Body: " << response.text << "\n";

        return false;
    } catch (std::exception& e) {
        logs << "----------\n" << "Exception occurred while registering agent with UID: " << uid << "\n"
             << "Exception message: " << e.what() << "\n";

        return false;
    }
}

NewTaskResponse Api::new_tasks() {
    if (access_code.empty()) {
        std::cerr << "Access code is empty. Please register the agent first." << std::endl;
        return NewTaskResponse{false, make_error(-1001, "Access code is empty. Please register the agent first."), "", {}, "", ""};
    }

    std::ofstream logs;
    logs.open("logs.txt", std::ios::app);
    if (!logs.is_open()) {
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
            cpr::Body(request_body.dump())
        );

        if (response.status_code == 200) {
            nlohmann::json json_response = nlohmann::json::parse(response.text);
            const std::string code_response = extract_code_response(json_response);
            const int code_response_int = parse_code_response(code_response);
            const std::string status = json_response.value("status", "");

            if (code_response_int == 1) {
                logs << "----------\n" << "Successfully got new task for agent with UID: " << uid << "\n";
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
                logs << "----------\n" << "No task available for agent with UID: " << uid << "\n";
                return NewTaskResponse{true, make_error(0, "", status.empty() ? "WAIT" : status), "", {}, "", status.empty() ? "WAIT" : status};
            }

            const std::string message = extract_msg(json_response);
            logs << "----------\n" << "Failed to get new tasks for agent with UID: " << uid << "\n"
                 << "API responded with code_response: " << code_response << "\n"
                 << "Response message: " << message << "\n";
            return NewTaskResponse{false, make_error(code_response_int, message, status), "", {}, "", status};
        }

        logs << "----------\n" << "Failed to get new tasks for agent with UID: " << uid << "\n"
             << "Status Code: " << response.status_code << "\n"
             << "Response Body: " << response.text << "\n";
        return NewTaskResponse{false, make_error(-1003, "Unexpected HTTP status code: " + std::to_string(response.status_code)), "", {}, "", ""};
    } catch (std::exception& e) {
        logs << "----------\n" << "Failed to get new tasks for agent with UID: " << uid << "\n"
             << "Exception message: " << e.what() << "\n";

        return NewTaskResponse{false, make_error(-1004, std::string("Exception occurred while getting new tasks: ") + e.what()), "", {}, "", ""};
    }
}

TaskResultResponse Api::send_task_result(const int result_code, const std::string& result_json, const std::vector<std::string>& files) const {
    if (access_code.empty()) {
        std::cerr << "Access code is empty. Please register the agent first." << std::endl;
        return TaskResultResponse{false, make_error(-1001, "Access code is empty. Please register the agent first.")};
    }

    std::ofstream logs;
    logs.open("logs.txt", std::ios::app);
    if (!logs.is_open()) {
        std::cerr << "Failed to open logs.txt for writing." << std::endl;
        return TaskResultResponse{false, make_error(-1002, "Failed to open logs.txt for writing.")};
    }

    try {
        std::vector<cpr::Part> parts;
        parts.emplace_back("result_code", result_code);
        parts.emplace_back("result", result_json, "application/json");
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
                logs << "----------\n" << "Successfully sent task result for agent with UID: " << uid << "\n"
                     << "result_code: " << result_code << ", files: " << files.size() << "\n";
                return TaskResultResponse{true, make_error(0, "", status)};
            }

            const std::string message = extract_msg(json_response);
            logs << "----------\n" << "Failed to send task result for agent with UID: " << uid << "\n"
                 << "result_code: " << result_code << ", files: " << files.size() << "\n"
                 << "API responded with code_response: " << code_response << "\n"
                 << "Response message: " << message << "\n";
            return TaskResultResponse{false, make_error(code_response_int, message, status)};
        }

        logs << "----------\n" << "Failed to send task result for agent with UID: " << uid << "\n"
             << "result_code: " << result_code << ", files: " << files.size() << "\n"
             << "Status Code: " << response.status_code << "\n"
             << "Response Body: " << response.text << "\n";
        return TaskResultResponse{false, make_error(-1003, "Unexpected HTTP status code: " + std::to_string(response.status_code))};
    } catch (std::exception& e) {
        logs << "----------\n" << "Exception while sending task result for agent with UID: " << uid << "\n"
             << "result_code: " << result_code << ", files: " << files.size() << "\n"
             << "Exception message: " << e.what() << "\n";
        return TaskResultResponse{false, make_error(-1004, std::string("Exception occurred while sending task result: ") + e.what())};
    }
}
