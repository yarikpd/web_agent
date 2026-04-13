#include "logger.h"

#include <chrono>
#include <condition_variable>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <queue>
#include <sstream>
#include <thread>

namespace logger {

namespace {

struct LogEntry {
    LogLevel level;
    std::string message;
};

std::mutex g_mutex;
std::condition_variable g_condition;
std::queue<LogEntry> g_messages;
std::thread g_worker;
bool g_running = false;
bool g_stopping = false;
bool g_show_logs_in_console = false;
std::string g_log_file_path = "logs.txt";
std::function<void()> g_console_redraw_callback;

std::string level_to_string(const LogLevel level) {
    switch (level) {
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warn:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
    }
    return "INFO";
}

std::string format_log_entry(const LogEntry& entry) {
    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_time{};
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&local_time, &now_time);
#else
    localtime_r(&now_time, &local_time);
#endif

    std::ostringstream formatted;
    formatted << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S")
              << ' '
              << level_to_string(entry.level)
              << ": "
              << entry.message;
    return formatted.str();
}

void write_log_entry(std::ofstream& logs, const LogEntry& entry) {
    const std::string formatted_entry = format_log_entry(entry);
    logs << formatted_entry << '\n';
    if (g_show_logs_in_console) {
        std::cerr << "\r\033[K" << formatted_entry << std::endl;
        if (g_console_redraw_callback) {
            g_console_redraw_callback();
        }
    }
}

void worker_loop() {
    std::ofstream logs(g_log_file_path, std::ios::app);
    if (!logs.is_open()) {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_running = false;
        g_stopping = true;
        return;
    }

    while (true) {
        std::unique_lock<std::mutex> lock(g_mutex);
        g_condition.wait(lock, [] { return g_stopping || !g_messages.empty(); });

        while (!g_messages.empty()) {
            const LogEntry message = g_messages.front();
            g_messages.pop();
            lock.unlock();
            write_log_entry(logs, message);
            logs.flush();
            lock.lock();
        }

        if (g_stopping) {
            break;
        }
    }
}

}  // namespace

bool start(const bool show_logs_in_console) {
    return start("logs.txt", show_logs_in_console);
}

bool start(const std::string& log_file_path, const bool show_logs_in_console) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_running) {
        return true;
    }

    g_stopping = false;
    g_running = true;
    g_show_logs_in_console = show_logs_in_console;
    g_log_file_path = log_file_path;
    g_worker = std::thread(worker_loop);
    return true;
}

void stop() {
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!g_running && !g_worker.joinable()) {
            return;
        }
        g_stopping = true;
    }

    g_condition.notify_all();

    if (g_worker.joinable()) {
        g_worker.join();
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    g_running = false;
    g_stopping = false;
    g_show_logs_in_console = false;
    g_log_file_path = "logs.txt";
    g_console_redraw_callback = nullptr;
}

bool log_message(const std::string& message, const LogLevel level) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_running) {
        std::ofstream logs(g_log_file_path, std::ios::app);
        if (!logs.is_open()) {
            return false;
        }

        write_log_entry(logs, LogEntry{level, message});
        return true;
    }

    g_messages.push(LogEntry{level, message});
    g_condition.notify_one();
    return true;
}

void set_console_redraw_callback(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_console_redraw_callback = std::move(callback);
}

}  // namespace logger
