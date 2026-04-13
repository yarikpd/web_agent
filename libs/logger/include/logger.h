#pragma once

#include <functional>
#include <string>

namespace logger {

enum class LogLevel {
    Info,
    Warn,
    Error
};

bool start(bool show_logs_in_console = false);
void stop();
bool log_message(const std::string& message, LogLevel level = LogLevel::Info);
void set_console_redraw_callback(std::function<void()> callback);

}  // namespace logger
