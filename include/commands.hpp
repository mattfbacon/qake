#pragma once

#include <filesystem>

#include "common.hpp"

namespace Commands {
using command_t = int(const struct GlobalOptions&);

command_t command_init;
command_t command_generate;
command_t command_make;
}
