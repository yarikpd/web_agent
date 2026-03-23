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


void tets();

std::string execCommand(const std::string &cmd);

std::string f(const std::string &name, std::map<std::string, std::string> args);

std::string directory(std::map<std::string, std::string> args);

std::string create_file(std::map<std::string, std::string> args);

bool replacePlaceholder(std::string &command,
                        const std::string &placeholder,
                        const std::string &value);
