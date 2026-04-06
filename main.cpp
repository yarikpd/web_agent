#include <dotenv.h>
#include <iostream>

#include "agent.h"

int main() {
    // TODO: in the start of main we log it to logs.txt that the program has started, and in the end of main we log that the program has ended.

    auto [success, error, output] = f("directory", {{"directory", ".."}});

    std::cout << success << std::endl;

    if (success) {
        std::cout << output << std::endl;
    } else {
        std::cout << error.code << std::endl;
        std::cout << error.message << std::endl;
    }

    return 0;
}
