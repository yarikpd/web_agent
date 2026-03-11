#pragma once
#include <map>
#include <string>

struct ApiError {
    // Unified code: 0 for success, API-negative codes for remote errors, -100x for local errors.
    int code;
    // Human-readable error description. Empty on successful response.
    std::string message;
    // Optional status from server (for example RUN, WAIT, ERROR).
    std::string status;
};

struct RegisterAgentResponse {
    // True when registration request completed successfully.
    bool success;
    // Unified error payload for programmatic handling.
    ApiError error;
    // Access code received from API after successful registration.
    std::string access_code;
};

struct NewTaskResponse {
    // True when request processing succeeded (task received or no task available).
    bool success;
    // Unified error payload for programmatic handling.
    ApiError error;
    // Task identifier returned by API when a task is available.
    std::string task_code;
    // Additional task parameters from API.
    std::map<std::string, std::string> options;
    // Task execution session identifier.
    std::string session_id;
    // Task state returned by API (for example RUN or WAIT).
    std::string status;
};

struct TaskResultResponse {
    // True when API accepted result upload.
    bool success{};
    // Unified error payload for programmatic handling.
    ApiError error;
};
