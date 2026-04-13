#include "settings.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <vector>

#include <dotenv.h>

namespace fs = std::filesystem;

namespace {

std::string trim(std::string value) {
    const auto not_space = [](const unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string to_lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

}  // namespace

Settings::Settings() : dotenv_path_(find_dotenv_path()) {
    init_dotenv(dotenv_path_);
    api_url_ = read_env_value("API_URL");
    agent_uid_ = read_env_value("AGENT_UID");
    agent_description_ = read_env_value("AGENT_DESCR");
    access_code_ = read_env_value("ACCESS_CODE");
    jobs_dir_ = read_env_value("JOBS_DIR");
    poll_interval_seconds_ = read_env_value("POLL_INTERVAL_SECONDS");
    thread_count_ = read_env_value("THREAD_COUNT");
    show_logs_in_console_ = read_env_value("SHOW_LOGS_IN_CONSOLE");
}

std::string Settings::get_optional(const std::string& key, const std::string& default_value) const {
    if (key == "API_URL") {
        return api_url_.empty() ? default_value : api_url_;
    }
    if (key == "AGENT_UID") {
        return agent_uid_.empty() ? default_value : agent_uid_;
    }
    if (key == "AGENT_DESCR") {
        return agent_description_.empty() ? default_value : agent_description_;
    }
    if (key == "ACCESS_CODE") {
        return access_code_.empty() ? default_value : access_code_;
    }
    if (key == "JOBS_DIR") {
        return jobs_dir_.empty() ? default_value : jobs_dir_;
    }
    if (key == "POLL_INTERVAL_SECONDS") {
        return poll_interval_seconds_.empty() ? default_value : poll_interval_seconds_;
    }
    if (key == "THREAD_COUNT") {
        return thread_count_.empty() ? default_value : thread_count_;
    }
    if (key == "SHOW_LOGS_IN_CONSOLE") {
        return show_logs_in_console_.empty() ? default_value : show_logs_in_console_;
    }
    return default_value;
}

std::string Settings::get_required(const std::string& key) const {
    const std::string value = get_optional(key);
    if (value.empty()) {
        throw std::runtime_error("Environment variable " + key + " is not set.");
    }
    return value;
}

int Settings::get_required_positive_int(const std::string& key) const {
    const std::string raw_value = get_required(key);

    try {
        const int value = std::stoi(raw_value);
        if (value <= 0) {
            throw std::runtime_error("Environment variable " + key + " must be greater than 0.");
        }
        return value;
    } catch (const std::invalid_argument&) {
        throw std::runtime_error("Environment variable " + key + " must be an integer.");
    } catch (const std::out_of_range&) {
        throw std::runtime_error("Environment variable " + key + " is out of range.");
    }
}

std::string Settings::api_url() const {
    return get_required("API_URL");
}

std::string Settings::agent_uid() const {
    return get_required("AGENT_UID");
}

std::string Settings::agent_description() const {
    return get_required("AGENT_DESCR");
}

std::string Settings::access_code() const {
    return access_code_;
}

std::string Settings::jobs_dir() const {
    return get_required("JOBS_DIR");
}

int Settings::poll_interval_seconds() const {
    return get_required_positive_int("POLL_INTERVAL_SECONDS");
}

int Settings::thread_count() const {
    const std::string raw_value = get_optional("THREAD_COUNT", "5");

    try {
        const int value = std::stoi(raw_value);
        if (value < 2) {
            throw std::runtime_error("Environment variable THREAD_COUNT must be greater than or equal to 2.");
        }
        return value;
    } catch (const std::invalid_argument&) {
        throw std::runtime_error("Environment variable THREAD_COUNT must be an integer.");
    } catch (const std::out_of_range&) {
        throw std::runtime_error("Environment variable THREAD_COUNT is out of range.");
    }
}

bool Settings::show_logs_in_console() const {
    const std::string raw_value = to_lowercase(trim(get_optional("SHOW_LOGS_IN_CONSOLE", "false")));
    if (raw_value == "true" || raw_value == "1") {
        return true;
    }
    if (raw_value == "false" || raw_value == "0") {
        return false;
    }

    throw std::runtime_error("Environment variable SHOW_LOGS_IN_CONSOLE must be boolean.");
}

void Settings::save_access_code(const std::string& access_code) {
    std::vector<std::string> lines;
    bool access_code_updated = false;

    if (fs::exists(dotenv_path_)) {
        std::ifstream input(dotenv_path_);
        if (!input.is_open()) {
            throw std::runtime_error("Failed to open .env for reading.");
        }

        std::string line;
        while (std::getline(input, line)) {
            if (line.rfind("ACCESS_CODE=", 0) == 0) {
                lines.emplace_back("ACCESS_CODE=" + access_code);
                access_code_updated = true;
            } else {
                lines.push_back(line);
            }
        }
    }

    if (!access_code_updated) {
        lines.emplace_back("ACCESS_CODE=" + access_code);
    }

    std::ofstream output(dotenv_path_, std::ios::trunc);
    if (!output.is_open()) {
        throw std::runtime_error("Failed to open .env for writing.");
    }

    for (size_t i = 0; i < lines.size(); ++i) {
        output << lines[i];
        if (i + 1 < lines.size()) {
            output << '\n';
        }
    }

    access_code_ = access_code;
}

const fs::path& Settings::dotenv_path() const {
    return dotenv_path_;
}

fs::path Settings::find_dotenv_path() {
    fs::path current = fs::current_path();

    while (true) {
        const fs::path dotenv_path = current / ".env";
        if (fs::exists(dotenv_path)) {
            return dotenv_path;
        }

        if (current == current.parent_path()) {
            return fs::current_path() / ".env";
        }

        current = current.parent_path();
    }
}

void Settings::init_dotenv(const fs::path& dotenv_path) {
    if (fs::exists(dotenv_path)) {
        dotenv::init(dotenv_path.string().c_str());
    }
}

std::string Settings::read_env_value(const std::string& key) {
    const char* value = std::getenv(key.c_str());
    return value == nullptr ? "" : std::string(value);
}
