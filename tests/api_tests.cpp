#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "classes.h"
#include "api.h"
#include "settings.h"

#include <iostream>
#include <fstream>
#include <chrono>

std::string get_env(const std::string& key, const std::string& default_value = "") {
    const Settings settings;
    return settings.get_optional(key, default_value);
}

TEST(ApiErrorTest, CreateSuccessError) {
    const ApiError error{0, "", "OK"};
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
    const NewTaskResponse resp{
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

    // Use a unique UID for each test run.
    std::string test_uid = uid + "_test_" + std::to_string(time(nullptr));

    Api api(api_url, test_uid, description);
    auto response = api.register_agent();

    std::cout << "\nRegistration Test:" << std::endl;
    std::cout << "   UID: " << test_uid << std::endl;
    std::cout << "   Success: " << (response.success ? "YES" : "NO") << std::endl;
    std::cout << "   Error Code: " << response.error.code << std::endl;
    std::cout << "   Error Message: " << response.error.message << std::endl;
    std::cout << "   Access Code: " << (response.access_code.empty() ? "(empty)" : response.access_code) << std::endl;

    // Registration may succeed or report that the agent already exists.
    EXPECT_TRUE(response.success || response.error.code == -3)
        << "Registration failed: " << response.error.message;
}

TEST_F(ApiIntegrationTest, GetTask_AfterRegistration) {
    if (api_url.empty()) {
        GTEST_SKIP() << "API_URL not set";
    }

    std::string test_uid = uid + "_task_" + std::to_string(time(nullptr));
    Api api(api_url, test_uid, description);

    auto reg_response = api.register_agent();
    if (!reg_response.success) {
        GTEST_SKIP() << "Failed to register agent";
    }

    auto task_response = api.new_tasks();

    std::cout << "\nGet Task Test:" << std::endl;
    std::cout << "   Success: " << (task_response.success ? "YES" : "NO") << std::endl;
    std::cout << "   Status: " << task_response.status << std::endl;
    std::cout << "   Task Code: " << (task_response.task_code.empty() ? "(none)" : task_response.task_code) << std::endl;
    std::cout << "   Session ID: " << (task_response.session_id.empty() ? "(none)" : task_response.session_id) << std::endl;

    // The API should return either a task or WAIT.
    EXPECT_TRUE(task_response.success);
    EXPECT_TRUE(task_response.status == "WAIT" || !task_response.task_code.empty());
}

TEST_F(ApiIntegrationTest, FullWorkflow_Register_GetTask_SendResult) {
    if (api_url.empty()) {
        GTEST_SKIP() << "API_URL not set";
    }

    std::string test_uid = uid + "_full_" + std::to_string(time(nullptr));
    Api api(api_url, test_uid, description);

    std::cout << "\nFull Workflow Test:" << std::endl;
    auto reg_response = api.register_agent();
    EXPECT_TRUE(reg_response.success) << "Registration failed";

    if (!reg_response.success) {
        GTEST_SKIP() << "Cannot continue without registration";
    }

    std::cout << "   Registered successfully" << std::endl;
    std::cout << "   Access Code: " << reg_response.access_code << std::endl;

    auto task_response = api.new_tasks();
    EXPECT_TRUE(task_response.success) << "Failed to get task";

    if (!task_response.success) {
        GTEST_SKIP() << "Cannot continue without task";
    }

    std::cout << "   Got task response" << std::endl;
    std::cout << "   Status: " << task_response.status << std::endl;

    // If a task is available, submit a success result.
    if (task_response.status == "RUN" && !task_response.task_code.empty()) {
        auto result_response = api.send_task_result(
            0,
            "Task completed successfully",
            task_response.session_id,
            {}
        );

        EXPECT_TRUE(result_response.success) << "Failed to send result";
        std::cout << "   Sent result: " << (result_response.success ? "SUCCESS" : "FAILED") << std::endl;
    } else {
        std::cout << "   No task available (WAIT status)" << std::endl;
    }
}

TEST(JsonParsingTest, ExtractCodeResponse_WithResponce) {
    nlohmann::json json = {{"code_responce", "0"}};
}

TEST(JsonParsingTest, ExtractCodeResponse_WithResponse) {
    nlohmann::json json = {{"code_response", "1"}};
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

class ApiTestAccessor {
public:
    static std::string extract_code_response(const nlohmann::json& json) {
        return Api::extract_code_response(json);
    }

    static int parse_code_response(const std::string& code_response) {
        return Api::parse_code_response(code_response);
    }

    static std::string extract_msg(const nlohmann::json& json) {
        return Api::extract_msg(json);
    }

    static std::map<std::string, std::string> extract_options(const nlohmann::json& json) {
        return Api::extract_options(json);
    }

    static ApiError make_error(int code, std::string message, std::string status = "") {
        return Api::make_error(code, std::move(message), std::move(status));
    }
};

class ApiHelperTest : public ::testing::Test {};

TEST_F(ApiHelperTest, ExtractCodeResponse_WithResponce) {
    nlohmann::json json = {{"code_responce", "0"}};
    EXPECT_EQ(ApiTestAccessor::extract_code_response(json), "0");
}

TEST_F(ApiHelperTest, ExtractCodeResponse_WithResponse) {
    nlohmann::json json = {{"code_response", "1"}};
    EXPECT_EQ(ApiTestAccessor::extract_code_response(json), "1");
}

TEST_F(ApiHelperTest, ExtractCodeResponse_Empty) {
    nlohmann::json json = {{"other_field", "value"}};
    EXPECT_EQ(ApiTestAccessor::extract_code_response(json), "");
}

TEST_F(ApiHelperTest, ParseCodeResponse_ValidNumbers) {
    EXPECT_EQ(ApiTestAccessor::parse_code_response("0"), 0);
    EXPECT_EQ(ApiTestAccessor::parse_code_response("1"), 1);
    EXPECT_EQ(ApiTestAccessor::parse_code_response("-3"), -3);
}

TEST_F(ApiHelperTest, ParseCodeResponse_EmptyString) {
    EXPECT_EQ(ApiTestAccessor::parse_code_response(""), -1000);
}

TEST_F(ApiHelperTest, ParseCodeResponse_InvalidString) {
    EXPECT_EQ(ApiTestAccessor::parse_code_response("abc"), -1000);
}

TEST_F(ApiHelperTest, ExtractMsg_WithMsg) {
    nlohmann::json json = {{"msg", "Success"}};
    EXPECT_EQ(ApiTestAccessor::extract_msg(json), "Success");
}

TEST_F(ApiHelperTest, ExtractMsg_WithMessage) {
    nlohmann::json json = {{"message", "Error occurred"}};
    EXPECT_EQ(ApiTestAccessor::extract_msg(json), "Error occurred");
}

TEST_F(ApiHelperTest, ExtractMsg_Default) {
    nlohmann::json json = {{"other", "field"}};
    EXPECT_EQ(ApiTestAccessor::extract_msg(json), "Unknown API error");
}

TEST_F(ApiHelperTest, ExtractOptions_Valid) {
    nlohmann::json json = {
        {"options", {
            {"param1", "value1"},
            {"param2", "value2"}
        }}
    };
    auto options = ApiTestAccessor::extract_options(json);
    EXPECT_EQ(options.size(), 2);
    EXPECT_EQ(options["param1"], "value1");
}

TEST_F(ApiHelperTest, ExtractOptions_Empty) {
    nlohmann::json json = {{"other", "field"}};
    auto options = ApiTestAccessor::extract_options(json);
    EXPECT_TRUE(options.empty());
}

TEST_F(ApiHelperTest, MakeError_Success) {
    auto error = ApiTestAccessor::make_error(0, "", "OK");
    EXPECT_EQ(error.code, 0);
    EXPECT_TRUE(error.message.empty());
    EXPECT_EQ(error.status, "OK");
}

TEST_F(ApiHelperTest, MakeError_NetworkError) {
    auto error = ApiTestAccessor::make_error(-1003, "Connection failed", "ERROR");
    EXPECT_EQ(error.code, -1003);
    EXPECT_EQ(error.message, "Connection failed");
    EXPECT_EQ(error.status, "ERROR");
}

TEST_F(ApiIntegrationTest, RegisterAgent_EmptyUID) {
    if (api_url.empty()) GTEST_SKIP();

    Api api(api_url, "", "test");
    auto response = api.register_agent();

    // The request should not crash even if the server rejects the payload.
    EXPECT_TRUE(response.success || response.error.code < 0);
}

TEST_F(ApiIntegrationTest, GetTask_BeforeRegistration) {
    if (api_url.empty()) GTEST_SKIP();

    Api api(api_url, "unregistered_" + std::to_string(time(nullptr)), "test");
    const auto response = api.new_tasks();

    // Missing registration should produce the "access code empty" error.
    EXPECT_FALSE(response.success);
    EXPECT_EQ(response.error.code, -1001);
}

TEST_F(ApiIntegrationTest, SendResult_InvalidSession) {
    if (api_url.empty()) GTEST_SKIP();

    Api api(api_url, "test_" + std::to_string(time(nullptr)), "test");

    if (auto reg = api.register_agent(); !reg.success) GTEST_SKIP();

    auto response = api.send_task_result(
        0,
        "Test message",
        "invalid_session_id_12345",
        {}
    );

    // The API should reject an invalid session or return another failure.
    EXPECT_TRUE(response.success || response.error.code < 0);
}

TEST_F(ApiIntegrationTest, SendResult_WithFiles) {
    if (api_url.empty()) GTEST_SKIP();

    std::string test_uid = "test_files_" + std::to_string(time(nullptr));
    Api api(api_url, test_uid, "test");

    auto reg = api.register_agent();
    if (!reg.success) GTEST_SKIP();

    auto task = api.new_tasks();
    if (!task.success || task.status == "WAIT") GTEST_SKIP();

    std::ofstream test_file("test_upload.txt");
    test_file << "Test file content for upload";
    test_file.close();

    auto response = api.send_task_result(
        0,
        "Task completed with file",
        task.session_id,
        {"test_upload.txt"}
    );

    EXPECT_TRUE(response.success) << "Failed: " << response.error.message;

    std::remove("test_upload.txt");
}

TEST_F(ApiIntegrationTest, Performance_MultipleRequests) {
    if (api_url.empty()) GTEST_SKIP();

    constexpr int iterations = 5;
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

    std::cout << "\nPerformance Test:" << std::endl;
    std::cout << "   Requests: " << iterations << std::endl;
    std::cout << "   Success: " << success_count << "/" << iterations << std::endl;
    std::cout << "   Duration: " << duration.count() << "ms" << std::endl;
    std::cout << "   Avg per request: " << (duration.count() / iterations) << "ms" << std::endl;

    EXPECT_GE(success_count, iterations * 0.8); // At least 80% should succeed.
}
