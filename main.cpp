#include <atomic>
#include <chrono>
#include <csignal>
#include <condition_variable>
#include <cstddef>
#include <iostream>
#include <map>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

#include "agent.h"
#include "api.h"
#include "logger.h"
#include "settings.h"

namespace {

std::atomic g_should_stop = false;

struct PendingTask {
    std::string task_code;
    std::map<std::string, std::string> options;
    std::string session_id;
};

struct ConsoleState {
    std::mutex mutex;
    std::vector<std::string> worker_statuses;
    std::size_t previous_line_length = 0;
};

void signal_handler(const int /*signal*/) {
    g_should_stop = true;
}

void init_console_encoding() {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

void render_worker_statuses(ConsoleState& console_state) {
    std::string line;
    for (std::size_t index = 0; index < console_state.worker_statuses.size(); ++index) {
        if (!line.empty()) {
            line += " | ";
        }
        line += "[" + std::to_string(index + 1) + "]: " + console_state.worker_statuses[index];
    }

    if (line.size() < console_state.previous_line_length) {
        line.append(console_state.previous_line_length - line.size(), ' ');
    }

    console_state.previous_line_length = line.size();
    std::cout << '\r' << line;
    std::cout << std::flush;
}

void redraw_worker_statuses(ConsoleState& console_state) {
    std::lock_guard<std::mutex> lock(console_state.mutex);
    render_worker_statuses(console_state);
}

void update_worker_status(ConsoleState& console_state, const int worker_index, const std::string& status) {
    std::lock_guard<std::mutex> lock(console_state.mutex);
    console_state.worker_statuses[static_cast<std::size_t>(worker_index - 1)] = status;
    render_worker_statuses(console_state);
}

void worker_loop(Api& api,
                 std::mutex& api_mutex,
                 std::queue<PendingTask>& task_queue,
                 std::mutex& task_mutex,
                 std::condition_variable& task_condition,
                 ConsoleState& console_state,
                 const int worker_index) {
    logger::log_message("Worker " + std::to_string(worker_index) + " started.");
    update_worker_status(console_state, worker_index, "Ожидает задачи");

    while (true) {
        PendingTask pending_task;
        {
            std::unique_lock<std::mutex> lock(task_mutex);
            task_condition.wait(lock, [&] {
                return g_should_stop || !task_queue.empty();
            });

            if (g_should_stop && task_queue.empty()) {
                break;
            }

            pending_task = std::move(task_queue.front());
            task_queue.pop();
        }

        task_condition.notify_all();
        update_worker_status(console_state, worker_index, "Работает над " + pending_task.task_code);

        const AgentCommandResponse agent_response = f(pending_task.task_code, pending_task.options);
        const int result_code = agent_response.success ? 0 : agent_response.error.code;
        const std::string result_message = agent_response.success
            ? agent_response.output
            : agent_response.error.message + (agent_response.output.empty() ? "" : "\n" + agent_response.output);

        TaskResultResponse result_response;
        {
            std::lock_guard<std::mutex> api_lock(api_mutex);
            result_response = api.send_task_result(
                result_code,
                result_message,
                pending_task.session_id,
                {}
            );
        }

        if (!result_response.success) {
            logger::log_message(
                "Worker " + std::to_string(worker_index) +
                " failed to send task result. Error: " + result_response.error.message
            );
        }

        update_worker_status(console_state, worker_index, "Ожидает задачи");
    }

    logger::log_message("Worker " + std::to_string(worker_index) + " stopped.");
    update_worker_status(console_state, worker_index, "Остановлен");
}

}  // namespace

int main() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    init_console_encoding();

    try {
        Settings settings;
        logger::start(settings.log_file_path(), settings.show_logs_in_console());
        logger::log_message("Program started.");
        const std::string api_url = settings.api_url();
        const std::string agent_uid = settings.agent_uid();
        const std::string agent_description = settings.agent_description();
        const std::string saved_access_code = settings.access_code();
        const int poll_interval_seconds = settings.poll_interval_seconds();
        const int total_thread_count = settings.thread_count();
        const int worker_thread_count = total_thread_count - 1;
        const std::size_t max_queue_size = static_cast<std::size_t>(worker_thread_count);

        Api api(api_url, agent_uid, agent_description);
        if (!saved_access_code.empty()) {
            api.set_access_code(saved_access_code);
            logger::log_message("Using access code from .env.");
        } else {
            const RegisterAgentResponse register_response = api.register_agent();
            if (!register_response.success) {
                logger::log_message("Agent registration failed. Error: " + register_response.error.message, logger::LogLevel::Error);
                return 1;
            }

            settings.save_access_code(register_response.access_code);
            api.set_access_code(register_response.access_code);
            logger::log_message("Agent registration completed successfully. Access code saved to .env.");
        }

        logger::log_message(
            "Starting thread pool. Total threads: " + std::to_string(total_thread_count) +
            ", worker threads: " + std::to_string(worker_thread_count) +
            ", logger threads: 1"
        );

        std::mutex api_mutex;
        std::queue<PendingTask> task_queue;
        std::mutex task_mutex;
        std::condition_variable task_condition;
        ConsoleState console_state;
        console_state.worker_statuses.assign(static_cast<std::size_t>(worker_thread_count), "Запускается");
        logger::set_console_redraw_callback([&console_state]() {
            redraw_worker_statuses(console_state);
        });
        {
            std::lock_guard<std::mutex> lock(console_state.mutex);
            render_worker_statuses(console_state);
        }
        std::vector<std::thread> workers;
        workers.reserve(static_cast<size_t>(worker_thread_count));
        for (int worker_index = 0; worker_index < worker_thread_count; ++worker_index) {
            workers.emplace_back(
                worker_loop,
                std::ref(api),
                std::ref(api_mutex),
                std::ref(task_queue),
                std::ref(task_mutex),
                std::ref(task_condition),
                std::ref(console_state),
                worker_index + 1
            );
        }

        while (!g_should_stop) {
            {
                std::unique_lock<std::mutex> lock(task_mutex);
                task_condition.wait(lock, [&] {
                    return g_should_stop || task_queue.size() < max_queue_size;
                });
            }

            if (g_should_stop) {
                break;
            }

            NewTaskResponse task_response;
            {
                std::lock_guard<std::mutex> api_lock(api_mutex);
                task_response = api.new_tasks();
            }

            if (!task_response.success) {
                logger::log_message("Failed to fetch a new task. Error: " + task_response.error.message, logger::LogLevel::Error);
                std::this_thread::sleep_for(std::chrono::seconds(poll_interval_seconds));
                continue;
            }

            if (task_response.status == "WAIT" || task_response.task_code.empty()) {
                std::this_thread::sleep_for(std::chrono::seconds(poll_interval_seconds));
                continue;
            }

            {
                std::lock_guard<std::mutex> lock(task_mutex);
                task_queue.push(PendingTask{
                    task_response.task_code,
                    task_response.options,
                    task_response.session_id
                });
            }

            logger::log_message(
                "Task queued. task_code: " + task_response.task_code +
                ", session_id: " + task_response.session_id
            );
            task_condition.notify_one();
        }

        task_condition.notify_all();

        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }

        {
            std::lock_guard<std::mutex> lock(console_state.mutex);
            std::cout << '\r';
            std::cout << std::string(console_state.previous_line_length, ' ');
            std::cout << "\r" << std::endl;
        }
        logger::set_console_redraw_callback(nullptr);
    } catch (const std::exception& e) {
        logger::log_message(std::string("Program terminated with error: ") + e.what(), logger::LogLevel::Error);
        logger::log_message("Program ended.");
        logger::stop();
        return 1;
    }

    logger::log_message("Program ended.");
    logger::stop();
    return 0;
}
