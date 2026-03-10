#pragma once
#include <string>

#include "classes.h"

class Api {
    std::string access_code;
    std::string api_url;
    std::string uid;
    std::string description;

public:
    Api(std::string api_url, std::string uid, std::string description);

    bool register_agent();
    NewTaskResponse new_tasks();
};
