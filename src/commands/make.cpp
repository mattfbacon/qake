#include <iostream>
#include <pstream.h>
#include <cstdlib>

#include "commands.hpp"

int Commands::command_make(const struct GlobalOptions& options) {
	if (system("which make >/dev/null 2>&1") != 0) {
		std::cerr << "`make` is required to build the project." << std::endl;
		return 1;
	}

	redi::opstream make("make " + options.make_options + " -f -");
	std::streambuf* oldCout = std::cout.rdbuf();
	std::cout.rdbuf(make.rdbuf()); // pipe stdout to make

	// piped to the opstream
	const unsigned int result = Commands::command_generate(options);

	// not only does this ensure that the caller continues to work correctly,
	// it also makes sure destructors close the proper streambuf.
	std::cout.rdbuf(oldCout);
	return result;
}
