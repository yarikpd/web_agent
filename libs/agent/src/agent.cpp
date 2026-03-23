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


std::string f(const std::string &name, std::map<std::string, std::string> args) {
    std::string res = "Nothing";

    if (name == "create_file") {
        create_file(std::move(args));
    } else if (name == "directory") {
        directory(args);
    } else {
        std::cout << "There is no such command";
        return "Error";
    }

    return res;
}

std::string directory(std::map<std::string, std::string> args) {
    std::ifstream f("jobs/directory.json");
    json data = json::parse(f);
    std::string command = data["command"][OS_NAME];

    if (!replacePlaceholder(command, "{directory}", args["directory"])) {
        return "Error";
    }

    return execCommand(command);
}

std::string create_file(std::map<std::string, std::string> args) {
    std::ifstream f("jobs/create_file.json");
    json data = json::parse(f);
    std::string command = data["command"][OS_NAME];

    if (!replacePlaceholder(command, "{directory}", args["directory"])) return "Error";
    if (!replacePlaceholder(command, "{filename}", args["filename"])) return "Error";


    return execCommand(command);
}

bool replacePlaceholder(std::string &command,
                        const std::string &placeholder,
                        const std::string &value) {
    if (command.empty()) {
        std::cerr << "Error: the command is empty" << std::endl;
        return false;
    }

    if (placeholder.empty()) {
        std::cerr << "Error: the placeholder is empty" << std::endl;
        return false;
    }

    if (value.empty()) {
        std::cerr << "Error: the value is empty" << std::endl;
        return false;
    }

    size_t pos = command.find(placeholder);
    if (pos == std::string::npos) {
        std::cerr << "Warning: placeholder '" << placeholder << "' not found" << std::endl;
        return false;
    }

    command.replace(pos, placeholder.length(), value);
    return true;
}

void tets() {
    std::cout << "!";
}
