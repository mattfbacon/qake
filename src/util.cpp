#include <iostream>
#include <algorithm>
#include <unistd.h>
#include <stdexcept>
#include <filesystem>
#include <string>
#include <string_view>
#include <fstream>
#include <sstream>
#include <result.hpp>
#include <utility>
#include <map>
#include <toml++/toml.h>

#include "util.hpp"
#include "common.hpp"
#include "constants.hpp"

namespace fs = std::filesystem;

namespace Util {

using namespace std::string_view_literals;

std::string generate_prompt(bool no_default, bool default_value) {
	if (no_default) { return "[y/n]"; }
	if (default_value) {
		return "[Y/n]";
	} else {
		return "[y/N]";
	}
}

bool directory_is_empty(const std::filesystem::path& path) {
	// this is kind of ugly but I can't find a better way...
	for (auto& _ __attribute__((unused)) : fs::directory_iterator(path)) {
		return false;
	}
	return true;
}

bool is_interactive() {
	return isatty(fileno(stdout));
}

void require_interactive() {
	if (!is_interactive()) {
		throw std::runtime_error("User interaction was required but qake is not running interactively.");
	}
}

bool y_or_n(const std::string_view message /* = "" */, bool no_default /* = true */, bool default_value /* = false */) {
	require_interactive();
	do {
		if (message != "") {
			std::cout << message << " ";
		}
		std::cout << generate_prompt(no_default, default_value) << " ";
		std::string answer;
		std::getline(std::cin, answer);
		std::transform(answer.begin(), answer.end(), answer.begin(), ::tolower);
		if (answer == "y") return true;
		if (answer == "n") return false;
		if (!no_default && answer == "") return default_value;
		std::cout << "Please answer with y or n" << std::endl;
	} while (true);
}

}
