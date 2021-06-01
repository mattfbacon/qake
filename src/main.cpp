#include <string>
#include <sstream>

#include <tclap/CmdLine.h>
#include <tclap/ArgGroup.h>

#include <magic_enum.hpp>

#include "constants.hpp"
#include "common.hpp"
#include "commands.hpp"

using namespace Commands;

Command::Kind get_command(
	const bool init,
	const bool generate,
	const bool clean,
	const bool build
) noexcept {
	if (init) return Command::INIT;
	if (generate) return Command::GENERATE;
	if (clean) return Command::CLEAN;
	if (build) return Command::BUILD;
	return Command::BUILD; // default
}

int main (int argc, char* argv[]) {
	TCLAP::CmdLine cmdline("Qake, a C/C++ project generator and high-level interface to GNU Make", ' ', VERSION);
	cmdline.setExceptionHandling(true);

	TCLAP::EitherOf command(cmdline);
	TCLAP::SwitchArg init("I", "init", "Initialize a new project in this directory. Requires an interactive terminal.", command);
	TCLAP::SwitchArg generate("g", "generate", "Generate a Makefile based on the Qakefile. "
		"This command is only necessary if you would like to have the intermediary Makefile for whatever reason; "
		"for example, to provide a Makefile for people that don't have Qake.",
		command);

	TCLAP::SwitchArg force("f", "force", "Do not ask for confirmation when performing possibly destructive actions.", cmdline);

	TCLAP::ValueArg<std::string> project("p", "project", "Specify the project file.", false, "Qakefile.toml", "path", cmdline);

	TCLAP::UnlabeledMultiArg<std::string> make_options("make_options", "Options to pass directly to `make`, for example `--always-make`. "
		"These are used to run `make` commands as well, like `build`.",
		false, "list of args", cmdline);

	cmdline.parse(argc, argv); // handles exiting on parse failure

	std::stringstream make_options_str;
	for (const auto& option : make_options.getValue()) { make_options_str << option << ' '; }

	const struct GlobalOptions options = {
		.force = force.getValue(),
		.project_file = project.getValue(),
		.invoked_as = std::string(argv[0]),
		.make_options = make_options_str.str(),
	};

	if (init.getValue()) { return Commands::command_init(options); }
	if (generate.getValue()) { return Commands::command_generate(options); }
	return Commands::command_make(options);
}
