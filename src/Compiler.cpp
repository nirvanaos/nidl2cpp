#include "pch.h"
#include "Compiler.h"
#include "Client.h"
#include <iostream>

using namespace std;
using namespace std::filesystem;

void Compiler::print_usage_info (const char* exe_name)
{
	cout << "Nirvana IDL compiler.\n";
	IDL_FrontEnd::print_usage_info (exe_name);
	cout << "Output options:\n"
		"\t-d directory Directory for output files.\n";
}

bool Compiler::parse_command_line (CmdLine& args)
{
	if (IDL_FrontEnd::parse_command_line (args))
		return true;

	if (args.arg () [1] == 'd') {
		out_dir_ = args.parameter (args.arg () + 2);
		assert (!out_dir_.empty ());
		args.next ();
		return true;
	}
	return false;
}

path Compiler::out_file (const AST::Root& tree, const string& suffix, const path& ext) const
{
	path name (tree.file ().stem ().string () + suffix);
	name.replace_extension (ext);
	if (!out_dir_.empty ())
		name = out_dir_ / name;
	else
		name = tree.file ().parent_path () / name;
	return name;
}

void Compiler::generate_code (const AST::Root& tree)
{
	Client client (out_file (tree, client_suffix_, ".h"), out_file (tree, client_suffix_, ".cpp"), tree);
	tree.visit (client);
}
