#ifndef NIDL2CPP_COMPILER_H_
#define NIDL2CPP_COMPILER_H_

#include <IDL_FrontEnd.h>

class Compiler : public IDL_FrontEnd
{
private:
	// Override print_usage_info for additional usage information.
	virtual void print_usage_info (const char* exe_name);

	// Override parse_command_line to parse own command line switches.
	virtual bool parse_command_line (CmdLine& args);

	// Override generate_code to build output from the AST.
	virtual void generate_code (const AST::Root& tree);
};

#endif
