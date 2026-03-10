#pragma once
#include <map>
#include <string>

struct NewTaskResponse {
    bool success;
    std::string error;
    std::string task_code;
    std::map<std::string, std::string> options;
    std::string session_id;
};
