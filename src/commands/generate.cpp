#include <iostream>
#include <string>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <unistd.h>
#include <stdexcept>
#include <filesystem>
#include <string_view>
#include <utility>
#include <map>

#include <toml++/toml.h>
#include <result.hpp>
#include <magic_enum.hpp>

#include "commands.hpp"
#include "util.hpp"
#include "common.hpp"
#include "constants.hpp"

#define INCBIN_PREFIX
#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#include <incbin.h>
INCBIN(makefile_stub_deps, "resources/Makefile.deps");
INCBIN(makefile_stub_nodeps, "resources/Makefile.nodeps");

namespace fs = std::filesystem;
using Util::y_or_n;

using namespace std::string_view_literals;

namespace {

std::optional<ProjectType::Kind> get_project_type(const std::string_view str) {
	if (str == "executable"sv) { return { ProjectType::EXECUTABLE }; }
	if (str == "shared"sv) { return { ProjectType::SHARED_LIB }; }
	if (str == "static"sv) { return { ProjectType::STATIC_LIB }; }
	return {};
}

std::optional<ProjectLanguage::Kind> get_project_language(const std::string_view str) {
	if (str == "c"sv) { return { ProjectLanguage::C }; }
	if (str == "c++"sv) { return { ProjectLanguage::CPP }; }
	return {};
}

static const std::array<std::pair<std::string, std::string>, 6> default_project_directories = {
	std::pair("objects", "obj"),
	std::pair("source", "src"),
	std::pair("include", "include"),
	std::pair("system_include", "sysinc"),
	std::pair("dist", "dist"),
	std::pair("dependencies", "deps"),
};

static const std::array<std::string, magic_enum::enum_count<ProjectLanguage::Kind>()> default_compiler_for_language = {
	/* [ProjectLanguage::C] = */ "gcc",
	/* [ProjectLanguage::CPP] = */ "g++",
};

static const std::array<std::string, magic_enum::enum_count<ProjectType::Kind>()> project_type_flags = {
	/* [ProjectType::EXECUTABLE] = */ "",
	/* [ProjectType::SHARED_LIB] = */ "-shared",
	/* [ProjectType::STATIC_LIB] = */ "-static",
};

static const std::array<std::string, magic_enum::enum_count<ProjectType::Kind>()> default_suffix_for_type = {
	/* [ProjectType::EXECUTABLE] = */ "",
	/* [ProjectType::SHARED_LIB] = */ ".so",
	/* [ProjectType::STATIC_LIB] = */ ".a",
};

static const std::array<std::string, magic_enum::enum_count<ProjectType::Kind>()> project_type_pic_flag = {
	/* [ProjectType::EXECUTABLE] = */ "-fPIE -pie",
	/* [ProjectType::SHARED_LIB] = */ "-fPIC",
	/* [ProjectType::STATIC_LIB] = */ "-fPIC",
};

static const std::array<std::string, magic_enum::enum_count<ProjectType::Kind>()> project_type_no_pic_flag = {
	/* [ProjectType::EXECUTABLE] = */ "-fno-PIE -no-pie",
	/* [ProjectType::SHARED_LIB] = */ "-fno-PIC",
	/* [ProjectType::STATIC_LIB] = */ "-fno-PIC",
};

Result<std::stringstream*, QakefileError> generate_makefile(const fs::path& project_file) {
	// assumed to exist and be accessible
	// throws on parse error
	const toml::table& qakefile = toml::parse_file(std::string_view(project_file.string()));
	const std::optional<unsigned int>& config_version = qakefile["_config_version"].value<unsigned int>();
	if (!config_version) { // empty optional
		return Err(
			"I’m here being all backwards-compatible, then you waltz in and ruin it…\n"
			"Config version not present in project configuration. Did you mess with the file?? (Current version is " CURRENT_CONFIG_VERSION_str ")"sv
		);
	}

	std::stringstream* makefile_builder = new std::stringstream();
	try {
	switch (*config_version) {
		case 0: {
			auto& m = *makefile_builder;

			const auto& project_type_ = get_project_type(qakefile["project"]["type"].value_or<std::string>("executable"));
			if (!project_type_) {
				throw "The linker won’t like this!\nUnknown project type. Options are executable, shared, or static."sv;
			}
			const ProjectType::Kind project_type = *project_type_;

			const auto& project_language_ = get_project_language(qakefile["project"]["language"].value_or<std::string>("c"));
			if (!project_language_) {
				throw "Je ne parle pas français…\nInvalid project language. I only know C and C++."sv;
			}
			const ProjectLanguage::Kind project_language = *project_language_;

			const std::optional<std::string>& project_standard_ = qakefile["build"]["standard"].value<std::string>();
			if (!project_standard_) {
				throw "You have no standards!\nStandard not present in project configuration, or is not a string. This is necessary to avoid ambiguity."sv;
			}
			const std::string& project_standard = *project_standard_;

			const std::optional<std::string>& project_target_ = qakefile["project"]["target"].value<std::string>();
			if (!project_target_) {
				throw "No target in sight!\nTarget not present in project configuration, or is not a string. This is necessary to know what to call the build result."sv;
			}
			const std::string& project_target = *project_target_;

			m << "TARGET := " << project_target << std::endl;

			m << "bin_suffix := "
				<< qakefile["build"]["binary_suffix"].value_or<std::string>(
					std::string(default_suffix_for_type.at(project_type))
				)
				<< std::endl << std::endl;

			for (const std::pair<std::string, std::string>& dir_default_pair : default_project_directories) {
				m << "DIR_" << dir_default_pair.first << " := "
					<< qakefile["directories"][dir_default_pair.first]
						.value_or<std::string>(std::string(dir_default_pair.second)) // make a copy
					<< std::endl;
			}
			m << std::endl;

			{ // scope to reduce memory usage
				const toml::array* project_libs = qakefile["dependencies"]["libraries"].as_array();
				m << "LIBS :=";
				if (project_libs) {
					for (const toml::node& lib : *project_libs) {
						if (lib.is_string()) {
							m << ' ' << *(lib.value<std::string>());
						}
					}
				}
				m << std::endl;
			}
			{
				const toml::array* project_pkgs = qakefile["dependencies"]["packages"].as_array();
				m << "PKGS :=";
				if (project_pkgs) {
					for (const toml::node& pkg : *project_pkgs) {
						if (pkg.is_string()) {
							m << ' ' << *(pkg.value<std::string>());
						}
					}
				}
				m << std::endl;
			}
			{
				const toml::array* project_include_dirs = qakefile["dependencies"]["include_dirs"].as_array();
				m << "INCDIRS :=";
				if (project_include_dirs) {
					for (const toml::node& include_dir : *project_include_dirs) {
						if (include_dir.is_string()) {
							m << ' ' << *(include_dir.value<std::string>());
						}
					}
				}
				m << std::endl << std::endl;
			}
			{
				std::string project_cc;
				project_cc = qakefile["build"]["cc"].value_or<std::string>(
					std::string(default_compiler_for_language[*magic_enum::enum_index(project_language)]) // make a copy
				);
				m << "CC := " << project_cc << std::endl << std::endl;
			}
			{
				const toml::array* project__flags = qakefile["build"]["_flags"].as_array();
				m << "CFLAGS_user :=";
				if (project__flags) {
					for (const toml::node& flag : *project__flags) {
						if (flag.is_string()) {
							m << ' ' << *(flag.value<std::string>());
						}
					}
				}
				m << std::endl;
			}
			{
				const bool project_optimize = qakefile["build"]["optimization"].value_or<bool>(true);
				m << "CFLAGS_optimize :=";
				if (project_optimize) {
					m << ' ' << "-O2";
				}
				m << std::endl;
			}

			m << "CFLAGS_standard := -std=" << project_standard << std::endl;

			{
				const bool project_pic = qakefile["build"]["pic"].value_or<bool>(true);
				m << "CFLAGS_pic := ";
				if (project_pic) {
					m << project_type_pic_flag[project_type];
				} else {
					m << project_type_no_pic_flag[project_type];
				}
				m << std::endl;
			}
			{
				const auto& project_warnings_node = qakefile["build"]["warnings"];
				m << "CFLAGS_warnings :=";
				if (project_warnings_node.type() != toml::node_type::none) {
					const toml::table* project_warnings = project_warnings_node.as_table();
					if (project_warnings) {
						for (const auto& warning : *project_warnings) {
							if (!warning.second.is_boolean()) { continue; }
							m << " -W";
							if (!(*(warning.second.value<bool>()))) {
								m << "no-";
							}
							m << warning.first;
						}
					} else { // exists but is not a table
						throw "Sure thing! Wait, what?\nWarnings must be a table. See the template for an example."sv;
					}
				} else {
					m << " -Wall -Wextra -Wno-error";
				}
				m << std::endl;
			}

			{
				const auto& project_features_node = qakefile["build"]["features"];
				m << "CFLAGS_features :=";
				if (project_features_node.type() != toml::node_type::none) {
					const toml::table* project_features = project_features_node.as_table();
					if (project_features) {
						for (const auto& feature : *project_features) {
							if (!feature.second.is_boolean()) continue;
							m << " -f";
							if (!(*(feature.second.value<bool>()))) {
								m << "no-";
							}
							m << feature.first;
						}
					} else { // exists but is not a table
						throw "Got it boss! Wait a minute…\nFeatures must be a table. See the template for an example."sv;
					}
				}
				m << std::endl;
			}

			m << "CFLAGS_project_type := " << project_type_flags[*magic_enum::enum_index(project_type)] << std::endl << std::endl;
			
			if (qakefile["build"]["use_dependencies"].value_or<bool>(true)) {
				m.write((char*)makefile_stub_deps_data, makefile_stub_deps_size);
			} else {
				m.write((char*)makefile_stub_nodeps_data, makefile_stub_nodeps_size);
			}
			break;
		} default: {
			throw "Possible time traveler detected!\nUnknown config version (current max is " CURRENT_CONFIG_VERSION_str "). This may be due to an old `qake` running on a newer project."sv;
		}
	}
	} catch (QakefileError e) {
		delete makefile_builder;
		return Err(e);
	} catch (...) {
		delete makefile_builder;
		throw;
	}

	return Ok(makefile_builder);
}

}

int Commands::command_generate(const struct GlobalOptions& options) {
	if (!fs::exists(options.project_file)) {
		std::cerr << '`' << options.project_file << "` does not exist." << std::endl;
		return 1;
	}
	const auto& result = generate_makefile(options.project_file);
	if (result) {
		const std::stringstream*const generated_makefile = result.unwrap();
		std::cout << generated_makefile->rdbuf();
		delete generated_makefile;
		return 0;
	} else {
		const QakefileError err = result.unwrapErr();
		std::cerr << err << std::endl;
		return 1;
	}
}
