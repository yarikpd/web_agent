#pragma once

#include <filesystem>
#include <string>

class Settings {
public:
    Settings();

    [[nodiscard]] std::string get_optional(const std::string& key, const std::string& default_value = "") const;
    [[nodiscard]] std::string get_required(const std::string& key) const;
    [[nodiscard]] int get_required_positive_int(const std::string& key) const;

    [[nodiscard]] std::string api_url() const;
    [[nodiscard]] std::string agent_uid() const;
    [[nodiscard]] std::string agent_description() const;
    [[nodiscard]] std::string access_code() const;
    [[nodiscard]] std::string jobs_dir() const;
    [[nodiscard]] int poll_interval_seconds() const;
    [[nodiscard]] int thread_count() const;
    [[nodiscard]] bool show_logs_in_console() const;
    [[nodiscard]] std::string log_file_path() const;

    void save_access_code(const std::string& access_code);
    [[nodiscard]] const std::filesystem::path& dotenv_path() const;

private:
    std::filesystem::path dotenv_path_;
    std::string api_url_;
    std::string agent_uid_;
    std::string agent_description_;
    std::string access_code_;
    std::string jobs_dir_;
    std::string poll_interval_seconds_;
    std::string thread_count_;
    std::string show_logs_in_console_;
    std::string log_file_path_;

    static std::filesystem::path find_dotenv_path();
    static void init_dotenv(const std::filesystem::path& dotenv_path);
    static std::string read_env_value(const std::string& key);
};
