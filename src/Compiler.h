#ifndef NIDL2CPP_COMPILER_H_
#define NIDL2CPP_COMPILER_H_

#include <IDL_FrontEnd.h>

class Compiler : public IDL_FrontEnd
{
public:
	Compiler () :
		IDL_FrontEnd (FLAG_DEPRECATE_ANONYMOUS_TYPES),
		servant_suffix_ ("_s"),
		proxy_suffix_ ("_p")
	{}

private:
	// Override print_usage_info for additional usage information.
	virtual void print_usage_info (const char* exe_name);

	// Override parse_command_line to parse own command line switches.
	virtual bool parse_command_line (CmdLine& args);

	// Override generate_code to build output from the AST.
	virtual void generate_code (const AST::Root& tree);

	std::filesystem::path out_file (const AST::Root& tree, const std::string& suffix, const std::filesystem::path& ext) const;

private:
	std::filesystem::path out_dir_;
	std::string client_suffix_;
	std::string servant_suffix_;
	std::string proxy_suffix_;
};

#endif
