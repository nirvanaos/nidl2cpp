/*
* Nirvana IDL to C++ compiler.
*
* This is a part of the Nirvana project.
*
* Author: Igor Popov
*
* Copyright (c) 2021 Igor Popov.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation; either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*
* Send comments and/or bug reports to:
*  popov.nirvana@gmail.com
*/
#include "pch.h"
#include "Compiler.h"
#include "Client.h"
#include "Servant.h"
#include "Proxy.h"
#include <iostream>

using namespace std;
using namespace std::filesystem;
using namespace AST;

void Compiler::print_usage_info (const char* exe_name)
{
	cout << "Nirvana IDL compiler.\n";
	IDL_FrontEnd::print_usage_info (exe_name);
	cout << "Output options:\n"
		"\t-out directory Directory for output files.\n"
		"\t-out_h directory Directory for output *.h files.\n"
		"\t-out_cpp directory Directory for output *.cpp files.\n"
		"\t-no_client Do not generate the client code.\n"
		"\t-client Generate the client code only.\n"
		"\t-no_server Do not generate the server code.\n"
		"\t-server Generate the server code only.\n"
		"\t-no_proxy Do not generate the proxy code.\n"
		"\t-proxy Generate the proxy code only.\n"
		"\t-client_suffix suffix The suffix for client file names. Default is empty.\n"
		"\t-server_suffix suffix The suffix for server file name. Default is _s.\n"
		"\t-proxy_suffix suffix The suffix for proxy file name. Default is _p.\n";
}

const char* Compiler::option (const char* arg, const char* opt)
{
	++arg;
	size_t cc = strlen (opt);
	if (!strncmp (arg, opt, cc))
		return arg + cc;
	else
		return nullptr;
}

bool Compiler::parse_command_line (CmdLine& args)
{
	if (IDL_FrontEnd::parse_command_line (args))
		return true;

	const char* arg = nullptr;
	if ((arg = option (args.arg (), "out_h")))
		out_h_ = args.parameter (arg);
	else if ((arg = option (args.arg (), "out_cpp")))
		out_cpp_ = args.parameter (arg);
	else if ((arg = option (args.arg (), "out")))
		out_h_ = out_cpp_ = args.parameter (arg);
	else if ((arg = option (args.arg (), "no_client")))
		client_ = false;
	else if ((arg = option (args.arg (), "no_server")))
		server_ = false;
	else if ((arg = option (args.arg (), "no_proxy")))
		proxy_ = false;
	else if ((arg = option (args.arg (), "client"))) {
		server_ = false;
		proxy_ = false;
	} else if ((arg = option (args.arg (), "server"))) {
		client_ = false;
		proxy_ = false;
	} else if ((arg = option (args.arg (), "proxy"))) {
		client_ = false;
		server_ = false;
	} else if ((arg = option (args.arg (), "client_suffix")))
		client_suffix_ = args.parameter (arg);
	else if ((arg = option (args.arg (), "server_suffix")))
		servant_suffix_ = args.parameter (arg);
	else if ((arg = option (args.arg (), "proxy_suffix")))
		proxy_suffix_ = args.parameter (arg);

	if (arg) {
		args.next ();
		return true;
	} else
		return false;
}

path Compiler::out_file (const Root& tree, const path& dir, const string& suffix, const path& ext) const
{
	path name (tree.file ().stem ().string () + suffix);
	name.replace_extension (ext);
	if (!dir.empty ())
		return dir / name;
	else
		return tree.file ().parent_path () / name;
}

void Compiler::generate_code (const Root& tree)
{
	path client_h = out_file (tree, out_h_, client_suffix_, "h");
	if (client_) {
		path client_cpp = out_file (tree, out_cpp_, client_suffix_, "cpp");
		Client client (client_h, client_cpp, tree, client_suffix_);
		tree.visit (client);
	}
	path servant_h = out_file (tree, out_h_, servant_suffix_, "h");
	if (server_) {
		Servant servant (servant_h, client_h, tree, servant_suffix_);
		tree.visit (servant);
	}
	if (proxy_) {
		Proxy proxy (out_file (tree, out_cpp_, proxy_suffix_, "cpp"), servant_h, tree);
		tree.visit (proxy);
	}
}
