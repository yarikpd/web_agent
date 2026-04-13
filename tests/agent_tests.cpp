#include <gtest/gtest.h>

#include "agent.h"

#include <filesystem>
#include <fstream>

namespace {

class ScopedEnvVar {
public:
    ScopedEnvVar(const char* key, std::string value) : key_(key) {
        const char* existing = std::getenv(key_);
        if (existing != nullptr) {
            had_previous_ = true;
            previous_value_ = existing;
        }
        setenv(key_, value.c_str(), 1);
    }

    ~ScopedEnvVar() {
        if (had_previous_) {
            setenv(key_, previous_value_.c_str(), 1);
        } else {
            unsetenv(key_);
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
}

TEST(AgentTest, FailsWhenJobsDirIsNotConfigured) {
    ScopedEnvVar jobs_dir("JOBS_DIR", "");
    const auto response = f("directory", {{"directory", "."}});

    EXPECT_FALSE(response.success);
    EXPECT_EQ(response.error.code, -2011);
    EXPECT_NE(response.error.message.find("JOBS_DIR"), std::string::npos);
}
