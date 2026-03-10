#include "api.h"

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include <utility>
#include <fstream>
#include <iostream>

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
            nlohmann::json json_response = nlohmann::json::parse(response.text);
            if (json_response["code_responce"] == "0") {
                access_code = json_response["access_code"];
                logs << "----------\n" << "Successfully registered agent with UID: " << uid << "\n";

                return true;
            }
            logs << "----------\n" << "Failed to register agent with UID: " << uid << "\n"
                 << "API responded with code_response: " << json_response["code_responce"] << "\n"
                 << "Response message: " << json_response["msg"] << "\n";

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
        return NewTaskResponse{false, "Access code is empty. Please register the agent first.", "", {}, ""};
    }

    std::ofstream logs;
    logs.open("logs.txt", std::ios::app);
    if (!logs.is_open()) {
        std::cerr << "Failed to open logs.txt for writing." << std::endl;
        return NewTaskResponse{false, "Failed to open logs.txt for writing.", "", {}, ""};
    }

    try {
        cpr::Response response = cpr::Post(
            cpr::Url{ api_url + "/wa_task"},
            cpr::Body(
                R"({"uid": ")" + uid + R"(", "descr": ")" + description + R"(", "access_code": ")" + access_code + "\"}"
            )
        );

        // TODO: finish it
    } catch (std::exception& e) {
        logs << "Failed to get new tasks for agent with UID: " << uid << "\n"
             << "Exception message: " << e.what() << "\n";

        return NewTaskResponse{false, "Exception occurred while getting new tasks", "", {}, ""};
    }
}