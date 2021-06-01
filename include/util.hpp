#pragma once

#include <any>
#include <string>
#include <sstream>
#include <filesystem>
#include <vector>
#include <result.hpp>
#include <magic_enum.hpp>

#include "common.hpp"

namespace Util {

using namespace std::string_literals;

bool directory_is_empty(const std::filesystem::path& path);
bool is_interactive();
bool y_or_n(const std::string_view message, bool no_default = true, bool default_value = false);
template <long unsigned int _Nm>
unsigned int choose(const std::array<std::string, _Nm>& choices, bool no_default = true, unsigned int default_value = 0);

void require_interactive();

template <long unsigned int _Nm>
unsigned int choose(const std::array<std::string, _Nm>& choices, bool no_default /* = true */, unsigned int default_value /* = 0 */) {
	require_interactive();
	do {
		if (!std::cin.good()) {
			return 0;
		}
		std::cout << "Please choose between the following options:" << std::endl;
		size_t i = 0;
		for (const std::string& choice : choices) {
			std::cout << " (" << i << ") " << choice << std::endl;
			i++;
		}
		if (no_default) {
			std::cout << "> ";
		} else {
			std::cout << '[' << default_value << "] ";
		}
		std::string answer_;
		std::getline(std::cin, answer_);
		if (answer_ == "" && !no_default) return default_value;
		unsigned long answer;
		try {
			answer = std::stoul(answer_);
		} catch (...) {
			std::cout << "Please enter a valid number." << std::endl;
			continue;
		}
		if (answer >= choices.size()) {
			std::cout << "Answer out of bounds." << std::endl;
			continue;
		}
		return answer;
	} while (true);
}

// template<typename K, typename V>
// std::optional<V&> map_get(std::map<K, V> kvs, K key) {
// 	try {
// 		return kvs.at(key);
// 	} catch (std::out_of_range e) {
// 		return {};
// 	}
// }

}
