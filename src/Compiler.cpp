#include "pch.h"
#include "Compiler.h"
#include <iostream>

using namespace std;

void Compiler::print_usage_info (const char* exe_name)
{
	cout << "Nirvana IDL compiler.\n";
	IDL_FrontEnd::print_usage_info (exe_name);
//	cout << "Output options:\n"
//		"\t-d directory Directory for output files.\n";
}

bool Compiler::parse_command_line (CmdLine& args)
{
	if (IDL_FrontEnd::parse_command_line (args))
		return true;

	return false;
}

void Compiler::generate_code (const AST::Root& tree)
{
}
