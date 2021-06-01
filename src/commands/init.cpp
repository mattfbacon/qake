#include <iostream>
#include <filesystem>
#include <regex>
#include <fstream>

#include <toml++/toml.h>

#include "commands.hpp"
#include "common.hpp"
#include "util.hpp"
#include "constants.hpp"

#include <magic_enum.hpp>

#define INCBIN_PREFIX
#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#include <incbin.h>
INCBIN(config_template, "resources/Qakefile.template.toml");

namespace fs = std::filesystem;

using Util::y_or_n;
using Util::choose;
using Util::is_interactive;
using Util::directory_is_empty;

int copy_template(const fs::path& project_file) {
	std::ofstream f(project_file);
	f.write((const char*)config_template_data, config_template_size);
	return 0;
}

static const std::array<std::string, magic_enum::enum_count<ProjectType::Kind>()> project_types = {
	"Executable",
	"Shared Library",
	"Static Library",
};

static const std::array<std::string, magic_enum::enum_count<ProjectType::Kind>()> project_type_to_string = {
	"executable",
	"shared",
	"static",
};

static const std::array<std::string, magic_enum::enum_count<ProjectLanguage::Kind>()> language_type_to_string = {
	"c",
	"c++",
};

static const std::array<std::string, magic_enum::enum_count<ProjectLanguage::Kind>()> project_languages = {
	"C",
	"C++"
};

static const std::regex c_standard_regex(R"regex(((c|gnu)(\+\+)?\d{2}|iso9899:\d{4}))regex");

int init_questions(const fs::path& project_file) {
	std::string project_name;
	do {
		std::cout << "Project name: ";
		std::getline(std::cin, project_name);
		if (project_name != "") break;
		std::cout << "Project name must be non-empty. Keep in mind that this determines the name of the result file of the build process." << std::endl;
	} while (true);
	const ProjectType::Kind project_type = *magic_enum::enum_cast<ProjectType::Kind>(choose<>(project_types, false, 0));
	const ProjectLanguage::Kind project_language = *magic_enum::enum_cast<ProjectLanguage::Kind>(choose<>(project_languages, true));
	std::string project_c_standard;
	do {
		std::cout << "C/C++ standard [gnu(++)17]: ";
		std::getline(std::cin, project_c_standard);
		if (project_c_standard == "") project_c_standard = project_language == ProjectLanguage::C ? "gnu17" : "gnu++17";
		if (regex_match(project_c_standard, c_standard_regex)) {
			break;
		} else {
			std::cout << "Warning: C/C++ standard doesn’t match general format." << std::endl;
			if(y_or_n("Retype?", false, false)) {
				continue;
			} else {
				break;
			}
		}
	} while (true);
	const toml::table data{{
		{ "_config_version", CURRENT_CONFIG_VERSION },
		{ "project", toml::table{{
			{ "target", project_name },
			{ "type", project_type_to_string[magic_enum::enum_integer(project_type)] },
			{ "language", language_type_to_string[magic_enum::enum_integer(project_language)] },
		}} },
		{ "build", toml::table{{
			{ "standard", project_c_standard },
		}} },
	}};
	std::ofstream f(project_file, std::ofstream::trunc | std::ofstream::out);
	f << data << std::endl;
	return 0;
}

static const std::array<std::string, 3> init_options = {
	"Cancel",
	"Allow you to edit the template manually",
	"Answer a series of questions about your project",
};

int Commands::command_init(const struct GlobalOptions& options) {
	if (!is_interactive()) {
		std::cerr << "This command is meant to be run interactively. Consider generating your own Qakefile.toml." << std::endl;
		if (!options.force) return 1;
	}
	if(!directory_is_empty(fs::current_path()) && !options.force) {
		if (!y_or_n("Directory is not empty. Overwrite existing files? (n cancels)", false, false)) {
			std::cout << "Okay, canceling" << std::endl;
			return 0;
		}
	}
	// we are running interactively, and the directory is either empty or will be overwritten
	const unsigned int choice = choose<>(init_options, false, 0);
	switch (choice) {
		case 0:
			std::cout << "Okay, canceling" << std::endl;
			return 0;
		case 1: {
			const int result = copy_template(options.project_file);
			if (result != 0) { return result; }
			break;
		} case 2: {
			const int result = init_questions(options.project_file);
			if (result != 0) { return result; }
			break;
		} default: // never
			return 0;
	}
	// NOTE: this assumes that init_questions does not allow renaming of the directories
	for (auto& directory : std::array<std::string, 3>{ "src", "include", "sysinc", }) {
		std::filesystem::create_directory(directory);
	}
	std::cout << "Please review the created `Qakefile.toml`. " \
		"Note that if you change directory names you will need to rename those directories." << std::endl;
	return 0;
}
