#include "agent.h"

#include <iostream>
#include <array>
#include <stdexcept>

#ifdef _WIN32
    #define POPEN _popen
    #define PCLOSE _pclose
#else
    #define POPEN popen
    #define PCLOSE pclose
#endif

std::string execCommand(const std::string& cmd) {
    std::array<char, 128> buffer{};
    std::string result;

    FILE* pipe = POPEN(cmd.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("Не удалось запустить команду");
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

    PCLOSE(pipe);
    return result;
}