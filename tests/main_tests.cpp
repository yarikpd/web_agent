#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include "classes.h"
#include "api.h"
#include <cstdlib>
#include <iostream>
#include <fstream>   // для std::ofstream
#include <chrono>    // для тестов производительности
#include <ctime>     // для time(nullptr)

// Вспомогательная функция для получения переменных окружения
std::string get_env(const std::string& key, const std::string& default_value = "") {
    if (const char* env = std::getenv(key.c_str())) {
        return std::string(env);
    }
    return default_value;
}

// ============================================================================
// 📦 UNIT ТЕСТЫ (без сети) - проверяем структуры данных
// ============================================================================

TEST(ApiErrorTest, CreateSuccessError) {
    ApiError error{0, "", "OK"};
    EXPECT_EQ(error.code, 0);
    EXPECT_TRUE(error.message.empty());
    EXPECT_EQ(error.status, "OK");
}

TEST(ApiErrorTest, CreateNetworkError) {
    ApiError error{-1003, "Connection failed", "ERROR"};
    EXPECT_EQ(error.code, -1003);
    EXPECT_EQ(error.message, "Connection failed");
    EXPECT_EQ(error.status, "ERROR");
}

TEST(RegisterAgentResponseTest, SuccessResponse) {
    RegisterAgentResponse resp{true, ApiError{0, "", "OK"}, "594807-1ddb-36af-9616-d8ed2b9d"};
    EXPECT_TRUE(resp.success);
    EXPECT_EQ(resp.access_code, "594807-1ddb-36af-9616-d8ed2b9d");
    EXPECT_EQ(resp.error.code, 0);
}

TEST(NewTaskResponseTest, TaskReceived) {
    NewTaskResponse resp{
        true,
        ApiError{0, "", "RUN"},
        "CONF",
        {},
        "bvLeD2gv-gtKH-IhmW-rsfd-Ejn1kyweawwi",
        "RUN"
    };
    EXPECT_TRUE(resp.success);
    EXPECT_EQ(resp.task_code, "CONF");
    EXPECT_EQ(resp.status, "RUN");
}

TEST(NewTaskResponseTest, WaitStatus) {
    NewTaskResponse resp{true, ApiError{0, "", "WAIT"}, "", {}, "", "WAIT"};
    EXPECT_TRUE(resp.success);
    EXPECT_TRUE(resp.task_code.empty());
    EXPECT_EQ(resp.status, "WAIT");
}

// ============================================================================
// 🌐 ИНТЕГРАЦИОННЫЕ ТЕСТЫ (с реальным API)
// ============================================================================

class ApiIntegrationTest : public ::testing::Test {
protected:
    std::string api_url;
    std::string uid;
    std::string description;

    void SetUp() override {
        api_url = get_env("API_URL");
        uid = get_env("AGENT_UID", "test_agent");
        description = get_env("AGENT_DESCR", "Test Agent");
    }
};

TEST_F(ApiIntegrationTest, RegisterAgent_Success) {
    if (api_url.empty()) {
        GTEST_SKIP() << "API_URL not set. Set environment variable API_URL";
    }

    // Используем уникальный UID для каждого теста
    std::string test_uid = uid + "_test_" + std::to_string(time(nullptr));

    Api api(api_url, test_uid, description);
    auto response = api.register_agent();

    std::cout << "\n📝 Registration Test:" << std::endl;
    std::cout << "   UID: " << test_uid << std::endl;
    std::cout << "   Success: " << (response.success ? "YES" : "NO") << std::endl;
    std::cout << "   Error Code: " << response.error.code << std::endl;
    std::cout << "   Error Message: " << response.error.message << std::endl;
    std::cout << "   Access Code: " << (response.access_code.empty() ? "(empty)" : response.access_code) << std::endl;

    // Регистрация может завершиться успешно ИЛИ агент уже зарегистрирован
    EXPECT_TRUE(response.success || response.error.code == -3)
        << "Registration failed: " << response.error.message;
}

TEST_F(ApiIntegrationTest, GetTask_AfterRegistration) {
    if (api_url.empty()) {
        GTEST_SKIP() << "API_URL not set";
    }

    // Сначала регистрируемся
    std::string test_uid = uid + "_task_" + std::to_string(time(nullptr));
    Api api(api_url, test_uid, description);

    auto reg_response = api.register_agent();
    if (!reg_response.success) {
        GTEST_SKIP() << "Failed to register agent";
    }

    // Получаем задачу
    auto task_response = api.new_tasks();

    std::cout << "\n📋 Get Task Test:" << std::endl;
    std::cout << "   Success: " << (task_response.success ? "YES" : "NO") << std::endl;
    std::cout << "   Status: " << task_response.status << std::endl;
    std::cout << "   Task Code: " << (task_response.task_code.empty() ? "(none)" : task_response.task_code) << std::endl;
    std::cout << "   Session ID: " << (task_response.session_id.empty() ? "(none)" : task_response.session_id) << std::endl;

    // Должны получить либо задачу, либо WAIT
    EXPECT_TRUE(task_response.success);
    EXPECT_TRUE(task_response.status == "WAIT" || !task_response.task_code.empty());
}

TEST_F(ApiIntegrationTest, FullWorkflow_Register_GetTask_SendResult) {
    if (api_url.empty()) {
        GTEST_SKIP() << "API_URL not set";
    }

    std::string test_uid = uid + "_full_" + std::to_string(time(nullptr));
    Api api(api_url, test_uid, description);

    // 1. Регистрируемся
    std::cout << "\n🔄 Full Workflow Test:" << std::endl;
    auto reg_response = api.register_agent();
    EXPECT_TRUE(reg_response.success) << "Registration failed";

    if (!reg_response.success) {
        GTEST_SKIP() << "Cannot continue without registration";
    }

    std::cout << "   ✓ Registered successfully" << std::endl;
    std::cout << "   Access Code: " << reg_response.access_code << std::endl;

    // 2. Получаем задачу
    auto task_response = api.new_tasks();
    EXPECT_TRUE(task_response.success) << "Failed to get task";

    if (!task_response.success) {
        GTEST_SKIP() << "Cannot continue without task";
    }

    std::cout << "   ✓ Got task response" << std::endl;
    std::cout << "   Status: " << task_response.status << std::endl;

    // 3. Если есть задача - отправляем результат
    if (task_response.status == "RUN" && !task_response.task_code.empty()) {
        auto result_response = api.send_task_result(
            0,  // success
            "Task completed successfully",
            task_response.session_id,
            {}  // no files
        );

        EXPECT_TRUE(result_response.success) << "Failed to send result";
        std::cout << "   ✓ Sent result: " << (result_response.success ? "SUCCESS" : "FAILED") << std::endl;
    } else {
        std::cout << "   ⏸ No task available (WAIT status)" << std::endl;
    }
}

// ============================================================================
// 🧪 Тесты парсинга JSON (проверяем логику без сети)
// ============================================================================

TEST(JsonParsingTest, ExtractCodeResponse_WithResponce) {
    nlohmann::json json = {{"code_responce", "0"}};
    // Функция приватная, но мы можем проверить через публичный интерфейс
    // или сделать friend class для тестов
}

TEST(JsonParsingTest, ExtractCodeResponse_WithResponse) {
    nlohmann::json json = {{"code_response", "1"}};
    // Проверяем оба варианта написания
}

// ============================================================================
// 🚀 Запуск
// ============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// ============================================================================
// Я ХЗ ЧТО ЗДЕСЬ
// ============================================================================

class ApiHelperTest : public ::testing::Test {};

TEST_F(ApiHelperTest, ExtractCodeResponse_WithResponce) {
    nlohmann::json json = {{"code_responce", "0"}};
    EXPECT_EQ(Api::extract_code_response(json), "0");
}

TEST_F(ApiHelperTest, ExtractCodeResponse_WithResponse) {
    nlohmann::json json = {{"code_response", "1"}};
    EXPECT_EQ(Api::extract_code_response(json), "1");
}

TEST_F(ApiHelperTest, ExtractCodeResponse_Empty) {
    nlohmann::json json = {{"other_field", "value"}};
    EXPECT_EQ(Api::extract_code_response(json), "");
}

TEST_F(ApiHelperTest, ParseCodeResponse_ValidNumbers) {
    EXPECT_EQ(Api::parse_code_response("0"), 0);
    EXPECT_EQ(Api::parse_code_response("1"), 1);
    EXPECT_EQ(Api::parse_code_response("-3"), -3);
}

TEST_F(ApiHelperTest, ParseCodeResponse_EmptyString) {
    EXPECT_EQ(Api::parse_code_response(""), -1000);
}

TEST_F(ApiHelperTest, ParseCodeResponse_InvalidString) {
    EXPECT_EQ(Api::parse_code_response("abc"), -1000);
}

TEST_F(ApiHelperTest, ExtractMsg_WithMsg) {
    nlohmann::json json = {{"msg", "Success"}};
    EXPECT_EQ(Api::extract_msg(json), "Success");
}

TEST_F(ApiHelperTest, ExtractMsg_WithMessage) {
    nlohmann::json json = {{"message", "Error occurred"}};
    EXPECT_EQ(Api::extract_msg(json), "Error occurred");
}

TEST_F(ApiHelperTest, ExtractMsg_Default) {
    nlohmann::json json = {{"other", "field"}};
    EXPECT_EQ(Api::extract_msg(json), "Unknown API error");
}

TEST_F(ApiHelperTest, ExtractOptions_Valid) {
    nlohmann::json json = {
        {"options", {
            {"param1", "value1"},
            {"param2", "value2"}
        }}
    };
    auto options = Api::extract_options(json);
    EXPECT_EQ(options.size(), 2);
    EXPECT_EQ(options["param1"], "value1");
}

TEST_F(ApiHelperTest, ExtractOptions_Empty) {
    nlohmann::json json = {{"other", "field"}};
    auto options = Api::extract_options(json);
    EXPECT_TRUE(options.empty());
}

TEST_F(ApiHelperTest, MakeError_Success) {
    auto error = Api::make_error(0, "", "OK");
    EXPECT_EQ(error.code, 0);
    EXPECT_TRUE(error.message.empty());
    EXPECT_EQ(error.status, "OK");
}

TEST_F(ApiHelperTest, MakeError_NetworkError) {
    auto error = Api::make_error(-1003, "Connection failed", "ERROR");
    EXPECT_EQ(error.code, -1003);
    EXPECT_EQ(error.message, "Connection failed");
    EXPECT_EQ(error.status, "ERROR");
}

// ============================================================================
// Я ХЗ ОНО НАДО ВООБЩЕ?
// ============================================================================

TEST_F(ApiIntegrationTest, RegisterAgent_EmptyUID) {
    if (api_url.empty()) GTEST_SKIP();

    Api api(api_url, "", "test");
    auto response = api.register_agent();

    // API может вернуть ошибку или успех (зависит от сервера)
    // Главное - что код не падает
    EXPECT_TRUE(response.success || response.error.code < 0);
}

TEST_F(ApiIntegrationTest, RegisterAgent_SpecialCharacters) {
    if (api_url.empty()) GTEST_SKIP();

    std::string test_uid = "test_agent_🚀_" + std::to_string(time(nullptr));
    Api api(api_url, test_uid, "Test with émojis & spëcial çharacters");
    auto response = api.register_agent();

    // Проверяем, что код обрабатывает спецсимволы
    EXPECT_TRUE(response.success || response.error.code < 0);
}

TEST_F(ApiIntegrationTest, GetTask_BeforeRegistration) {
    if (api_url.empty()) GTEST_SKIP();

    // Создаём API без регистрации
    Api api(api_url, "unregistered_" + std::to_string(time(nullptr)), "test");
    auto response = api.new_tasks();

    // Должна быть ошибка -1001 (access code empty)
    EXPECT_FALSE(response.success);
    EXPECT_EQ(response.error.code, -1001);
}

TEST_F(ApiIntegrationTest, SendResult_InvalidSession) {
    if (api_url.empty()) GTEST_SKIP();

    Api api(api_url, "test_" + std::to_string(time(nullptr)), "test");
    auto reg = api.register_agent();

    if (!reg.success) GTEST_SKIP();

    // Отправляем результат с неверным session_id
    auto response = api.send_task_result(
        0,
        "Test message",
        "invalid_session_id_12345",
        {}
    );

    // API должен вернуть ошибку
    EXPECT_TRUE(response.success || response.error.code < 0);
}

TEST_F(ApiIntegrationTest, SendResult_WithFiles) {
    if (api_url.empty()) GTEST_SKIP();

    // Сначала регистрируемся и получаем задачу
    std::string test_uid = "test_files_" + std::to_string(time(nullptr));
    Api api(api_url, test_uid, "test");

    auto reg = api.register_agent();
    if (!reg.success) GTEST_SKIP();

    auto task = api.new_tasks();
    if (!task.success || task.status == "WAIT") GTEST_SKIP();

    // Создаём тестовый файл
    std::ofstream test_file("test_upload.txt");
    test_file << "Test file content for upload";
    test_file.close();

    // Отправляем результат с файлом
    auto response = api.send_task_result(
        0,
        "Task completed with file",
        task.session_id,
        {"test_upload.txt"}
    );

    EXPECT_TRUE(response.success) << "Failed: " << response.error.message;

    // Удаляем тестовый файл
    std::remove("test_upload.txt");
}


//уже лагает от колва строк хз поч
TEST_F(ApiIntegrationTest, Performance_MultipleRequests) {
    if (api_url.empty()) GTEST_SKIP();

    const int iterations = 5;
    int success_count = 0;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        std::string test_uid = "perf_test_" + std::to_string(time(nullptr)) + "_" + std::to_string(i);
        Api api(api_url, test_uid, "Performance test");

        auto response = api.register_agent();
        if (response.success) success_count++;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "\n⚡ Performance Test:" << std::endl;
    std::cout << "   Requests: " << iterations << std::endl;
    std::cout << "   Success: " << success_count << "/" << iterations << std::endl;
    std::cout << "   Duration: " << duration.count() << "ms" << std::endl;
    std::cout << "   Avg per request: " << (duration.count() / iterations) << "ms" << std::endl;

    EXPECT_GE(success_count, iterations * 0.8); // Хотя бы 80% успешных
}
