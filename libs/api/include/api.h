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
    Api(std::string api_url, std::string uid, std::string description);

    bool register_agent();
    NewTaskResponse new_tasks();
    [[nodiscard]] TaskResultResponse send_task_result(int result_code, const std::string& result_json, const std::vector<std::string>& files) const;
};
