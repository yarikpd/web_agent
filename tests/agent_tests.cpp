#include <gtest/gtest.h>

#include "agent.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace {

void set_env_var(const char* key, const std::string& value) {
#if defined(_WIN32)
    _putenv_s(key, value.c_str());
#else
    setenv(key, value.c_str(), 1);
#endif
}

void unset_env_var(const char* key) {
#if defined(_WIN32)
    _putenv_s(key, "");
#else
    unsetenv(key);
#endif
}

class ScopedEnvVar {
public:
    ScopedEnvVar(const char* key, std::string value) : key_(key) {
        const char* existing = std::getenv(key_);
        if (existing != nullptr) {
            had_previous_ = true;
            previous_value_ = existing;
        }
        set_env_var(key_, value);
    }

    ~ScopedEnvVar() {
        if (had_previous_) {
            set_env_var(key_, previous_value_);
        } else {
            unset_env_var(key_);
        }
    }

private:
    const char* key_;
    bool had_previous_ = false;
    std::string previous_value_;
};

std::string jobs_dir_path() {
    return (std::filesystem::current_path().parent_path() / "jobs").string();
}

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

}  // namespace

TEST(AgentTest, RejectsMissingRequiredArgument) {
    ScopedEnvVar jobs_dir("JOBS_DIR", jobs_dir_path());
    const auto response = f("directory", {});

    EXPECT_FALSE(response.success);
    EXPECT_EQ(response.error.code, -2005);
    EXPECT_NE(response.error.message.find("directory"), std::string::npos);
}

TEST(AgentTest, RejectsUnknownArgument) {
    ScopedEnvVar jobs_dir("JOBS_DIR", jobs_dir_path());
    const auto response = f("directory", {{"directory", "."}, {"extra", "value"}});

    EXPECT_FALSE(response.success);
    EXPECT_EQ(response.error.code, -2006);
    EXPECT_NE(response.error.message.find("extra"), std::string::npos);
}

TEST(AgentTest, RejectsArgumentWithWrongType) {
    ScopedEnvVar jobs_dir("JOBS_DIR", jobs_dir_path());
    const std::filesystem::path temp_job = std::filesystem::path(jobs_dir_path()) / "typed_validation_test.json";
    std::filesystem::create_directories(temp_job.parent_path());
    std::ofstream file(temp_job);
    file << R"({
  "name": "typed_validation_test",
  "command": {
    "linux": "echo {count}",
    "macos": "echo {count}",
    "windows": "echo {count}"
  },
  "parameters": {
    "count": {
      "required": true,
      "type": "int"
    }
  }
})";
    file.close();

    const auto response = f("typed_validation_test", {{"count", "abc"}});

    EXPECT_FALSE(response.success);
    EXPECT_EQ(response.error.code, -2007);
    EXPECT_NE(response.error.message.find("count"), std::string::npos);

    std::filesystem::remove(temp_job);
}

TEST(AgentTest, ExecutesJobWithValidArguments) {
    ScopedEnvVar jobs_dir("JOBS_DIR", jobs_dir_path());
    const auto response = f("directory", {{"directory", "."}});

    EXPECT_TRUE(response.success) << response.error.message;
    EXPECT_EQ(response.error.code, 0);
    EXPECT_FALSE(response.output.empty());
    EXPECT_FALSE(response.restart_required);
}

TEST(AgentTest, ReturnsFilesWhenJobEnablesReturnFiles) {
    ScopedEnvVar jobs_dir("JOBS_DIR", jobs_dir_path());
    const std::filesystem::path temp_job = std::filesystem::path(jobs_dir_path()) / "return_files_test.json";
    const std::filesystem::path output_file = std::filesystem::current_path() / "return_files_test_output.txt";

    std::ofstream file(temp_job);
    file << R"({
  "name": "return_files_test",
  "command": {
    "linux": "printf return-file > {path}",
    "macos": "printf return-file > {path}",
    "windows": "cmd /C echo return-file > {path}"
  },
  "return_files": true,
  "parameters": {
    "path": {
      "required": true,
      "type": "string"
    }
  }
})";
    file.close();

    const auto response = f("return_files_test", {{"path", output_file.string()}});

    EXPECT_TRUE(response.success) << response.error.message;
    ASSERT_EQ(response.files.size(), 1);
    EXPECT_EQ(std::filesystem::path(response.files[0]), output_file.lexically_normal());

    std::filesystem::remove(output_file);
    std::filesystem::remove(temp_job);
}

TEST(AgentTest, DoesNotReturnFilesWhenJobDisablesReturnFiles) {
    ScopedEnvVar jobs_dir("JOBS_DIR", jobs_dir_path());
    const std::filesystem::path temp_job = std::filesystem::path(jobs_dir_path()) / "no_return_files_test.json";
    const std::filesystem::path output_file = std::filesystem::current_path() / "no_return_files_test_output.txt";

    std::ofstream file(temp_job);
    file << R"({
  "name": "no_return_files_test",
  "command": {
    "linux": "printf no-return-file > {path}",
    "macos": "printf no-return-file > {path}",
    "windows": "cmd /C echo no-return-file > {path}"
  },
  "return_files": false,
  "parameters": {
    "path": {
      "required": true,
      "type": "string"
    }
  }
})";
    file.close();

    const auto response = f("no_return_files_test", {{"path", output_file.string()}});

    EXPECT_TRUE(response.success) << response.error.message;
    EXPECT_TRUE(response.files.empty());

    std::filesystem::remove(output_file);
    std::filesystem::remove(temp_job);
}

TEST(AgentTest, SupportsOptionalPlaceholders) {
    ScopedEnvVar jobs_dir("JOBS_DIR", jobs_dir_path());
    const std::filesystem::path temp_job = std::filesystem::path(jobs_dir_path()) / "optional_placeholder_test.json";

    std::ofstream file(temp_job);
    file << R"({
  "name": "optional_placeholder_test",
  "command": {
    "linux": "printf '{required}{optional}'",
    "macos": "printf '{required}{optional}'",
    "windows": "cmd /C echo {required}{optional}"
  },
  "parameters": {
    "required": {
      "required": true,
      "type": "string"
    },
    "optional": {
      "required": false,
      "type": "string"
    }
  }
})";
    file.close();

    const auto response = f("optional_placeholder_test", {{"required", "value"}});

    EXPECT_TRUE(response.success) << response.error.message;
    EXPECT_NE(response.output.find("value"), std::string::npos);
    EXPECT_EQ(response.output.find("{optional}"), std::string::npos);

    std::filesystem::remove(temp_job);
}

TEST(AgentTest, MarksRestartRequiredWhenJobEnablesRestartAfter) {
    ScopedEnvVar jobs_dir("JOBS_DIR", jobs_dir_path());
    const std::filesystem::path temp_job = std::filesystem::path(jobs_dir_path()) / "restart_after_test.json";

    std::ofstream file(temp_job);
    file << R"({
  "name": "restart_after_test",
  "command": {
    "linux": "printf restart",
    "macos": "printf restart",
    "windows": "cmd /C echo restart"
  },
  "restart_after": true,
  "parameters": {}
})";
    file.close();

    const auto response = f("restart_after_test", {});

    EXPECT_TRUE(response.success) << response.error.message;
    EXPECT_TRUE(response.restart_required);

    std::filesystem::remove(temp_job);
}

TEST(AgentTest, ConfJobUpdatesEnvAndRequestsRestart) {
    ScopedEnvVar jobs_dir("JOBS_DIR", jobs_dir_path());
    const std::filesystem::path env_path = std::filesystem::current_path() / ".env";
    const bool had_env = std::filesystem::exists(env_path);
    const std::string previous_env = had_env ? read_text_file(env_path) : "";

    const auto response = f("CONF", {
        {"AGENT_UID", "conf_test_uid"},
        {"THREAD_COUNT", "7"}
    });

    EXPECT_TRUE(response.success) << response.error.message;
    EXPECT_TRUE(response.restart_required);

    const std::string updated_env = read_text_file(env_path);
    EXPECT_NE(updated_env.find("AGENT_UID=conf_test_uid"), std::string::npos);
    EXPECT_NE(updated_env.find("THREAD_COUNT=7"), std::string::npos);

    if (had_env) {
        std::ofstream output(env_path, std::ios::trunc);
        output << previous_env;
    } else {
        std::filesystem::remove(env_path);
    }
}

TEST(AgentTest, FailsWhenJobsDirIsNotConfigured) {
    ScopedEnvVar jobs_dir("JOBS_DIR", "");
    const auto response = f("directory", {{"directory", "."}});

    EXPECT_FALSE(response.success);
    EXPECT_EQ(response.error.code, -2011);
    EXPECT_NE(response.error.message.find("JOBS_DIR"), std::string::npos);
}
