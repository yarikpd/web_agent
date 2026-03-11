#pragma once
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "classes.h"

class Api {
    std::string access_code;
    std::string api_url;
    std::string uid;
    std::string description;

    static std::string extract_code_response(const nlohmann::json& json_response);
    static std::string extract_msg(const nlohmann::json& json_response);
    static std::map<std::string, std::string> extract_options(const nlohmann::json& json_response);
    static int parse_code_response(const std::string& code_response);
    static ApiError make_error(int code, std::string message, std::string status = "");

public:
    /**
     * Creates API client for web-agent backend.
     * @param api_url Base API URL.
     * @param uid Agent identifier used in requests.
     * @param description Agent description sent to backend.
     */
    Api(std::string api_url, std::string uid, std::string description);

    /**
     * Sends registration request to `/wa_reg` and stores received `access_code`.
     * @return `RegisterAgentResponse` with unified error information and received `access_code`.
     */
    RegisterAgentResponse register_agent();

    /**
     * Requests a new task from `/wa_task`.
     * @return `NewTaskResponse` with task data when `code_responce == 1`,
     *         `WAIT` status when `code_responce == 0`, or unified error data on failure.
     */
    NewTaskResponse new_tasks();

    /**
     * Uploads task execution result to `/wa_result` as `multipart/form-data`.
     * @param result_code Result code where `0` means success and negative values mean errors.
     * @param result_json JSON string with task execution payload.
     * @param files Paths to files that will be sent as `file1`, `file2`, and so on.
     * @return `TaskResultResponse` with unified error information.
     */
    [[nodiscard]] TaskResultResponse send_task_result(int result_code, const std::string& result_json, const std::vector<std::string>& files) const;
};
