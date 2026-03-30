#include "agent.h"

#include <iostream>
#include <array>
#include <stdexcept>
#include <fstream>
#include <nlohmann/json.hpp>
#include <utility>
using json = nlohmann::json;


std::string execCommand(const std::string &cmd) {
    std::array<char, 128> buffer{};
    std::string result;

    FILE *pipe = POPEN(cmd.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("Couldn't run the command");
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

    PCLOSE(pipe);
    return result;
}


std::string f(const std::string &name, const std::map<std::string, std::string>& args) {
    std::string res = "Nothing";
    std::ifstream file("../jobs/" + name + ".json");
    json data = json::parse(file);

    std::string command = data["command"][OS_NAME];
    for (const auto&[key, value]: args) {
        replacePlaceholder(command, "{" + key + "}", value);
    }

    return execCommand(command);
}


void replacePlaceholder(std::string &command,
                        const std::string &placeholder,
                        const std::string &value) {

    //TODO проверить все аргументы правильности и их значения
    const size_t pos = command.find(placeholder);
    command.replace(pos, placeholder.length(), value);
}

