#include <fstream>

#include "move_consumer.h"

bool verify_move_consumer(
    const mental1104::file::MoveResult& result, const std::string& target) {
    std::ifstream input(target, std::ios::binary);
    std::string text;
    input >> text;
#if defined(_WIN32)
    const bool status =
        result.status == mental1104::file::MoveStatus::DurabilityUncertain;
#else
    const bool status =
        result.status == mental1104::file::MoveStatus::Completed &&
        !result.error;
#endif
    return status && result.source_removed && text == "hello";
}
