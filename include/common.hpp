#pragma once

#include <filesystem>
#include <optional>
#include <string>

struct Command {
	enum Kind : unsigned char {
		INIT = 0,
		GENERATE = 1,
		CLEAN = 2,
		BUILD = 3,
	};
};

struct GlobalOptions {
	const bool force;
	const std::filesystem::path& project_file;
	const std::string& invoked_as;
	const std::string& make_options;
};

struct ProjectType {
	enum Kind : unsigned char {
		EXECUTABLE = 0,
		SHARED_LIB = 1,
		STATIC_LIB = 2,
	};
};

struct ProjectLanguage {
	enum Kind : unsigned char {
		C = 0,
		CPP = 1,
	};
};

using QakefileError = std::string_view;
