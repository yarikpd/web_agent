#pragma once

#include <map>
#include <string>

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#define OS_NAME "windows"
#define POPEN _popen
#define PCLOSE _pclose

#elif defined(__APPLE__) && defined(__MACH__)
#define OS_NAME "macos"
#define POPEN popen
#define PCLOSE pclose

#elif defined(__linux__) || defined(__linux)
#define OS_NAME "linux"
#define POPEN popen
#define PCLOSE pclose

#else
#define OS_NAME "Unknown"
#define POPEN popen
#define PCLOSE pclose
#endif

struct AgentError {
    int code;
    std::string message;
};

struct AgentCommandResponse {
    bool success;
    AgentError error;
    std::string output;
};

AgentCommandResponse execCommand(const std::string &cmd);

AgentCommandResponse f(const std::string &name, const std::map<std::string, std::string>& args);

void replacePlaceholder(std::string &command,
                        const std::string &placeholder,
                        const std::string &value);
