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
#include <AST/Builder.h>
#include <iostream>

using std::filesystem::path;
using namespace AST;

void Compiler::print_usage_info (const char* exe_name)
{
	std::cout << "Nirvana IDL compiler.\n";
	IDL_FrontEnd::print_usage_info (exe_name);
	std::cout << "Output options:\n"
		"\t-out <directory>        Directory for output files.\n"
		"\t-out_h <directory>      Directory for output *.h files.\n"
		"\t-out_cpp <directory>    Directory for output *.cpp files.\n"
		"\t-client                 Generate the client code only.\n"
		"\t-no_client              Do not generate the client code.\n"
		"\t-no_client_cpp          Generate only header for client code.\n"
		"\t-server                 Generate the server code only.\n"
		"\t-no_server              Do not generate the server code.\n"
		"\t-proxy                  Generate the proxy code only.\n"
		"\t-no_proxy               Do not generate the proxy code.\n"
		"\t-client_suffix <suffix> The suffix for client file names. Default is empty.\n"
		"\t-server_suffix <suffix> The suffix for server file name. Default is _s.\n"
		"\t-proxy_suffix <suffix>  The suffix for proxy file name. Default is _p.\n"
		"\t-legacy                 Generate code compatible with C++ language mapping 1.3.\n"
		"\t                        To enable the compatibility define macro LEGACY_CORBA_CPP.\n"
		"\t-no_servant             Do not generate servant implementations.\n"
		"\t-inc_cpp <file>         Add additional include file to each .cpp file\n";
}

const char* Compiler::option (const char* arg, const char* opt)
{
	++arg;
	size_t cc = strlen (opt);
	if (std::equal (arg, arg + cc + 1, opt))
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
		out_h = args.parameter (arg);
	else if ((arg = option (args.arg (), "out_cpp")))
		out_cpp = args.parameter (arg);
	else if ((arg = option (args.arg (), "out")))
		out_h = out_cpp = args.parameter (arg);
	else if ((arg = option (args.arg (), "no_client")))
		client = false;
	else if ((arg = option (args.arg (), "no_client_cpp")))
		no_client_cpp = true;
	else if ((arg = option (args.arg (), "no_server")))
		server = false;
	else if ((arg = option (args.arg (), "no_proxy")))
		proxy = false;
	else if ((arg = option (args.arg (), "client"))) {
		server = false;
		proxy = false;
	} else if ((arg = option (args.arg (), "server"))) {
		client = false;
		proxy = false;
	} else if ((arg = option (args.arg (), "proxy"))) {
		client = false;
		server = false;
	} else if ((arg = option (args.arg (), "client_suffix")))
		client_suffix = args.parameter (arg);
	else if ((arg = option (args.arg (), "server_suffix")))
		servant_suffix = args.parameter (arg);
	else if ((arg = option (args.arg (), "proxy_suffix")))
		proxy_suffix = args.parameter (arg);
	else if ((arg = option (args.arg (), "legacy")))
		legacy = true;
	else if ((arg = option (args.arg (), "no_servant")))
		no_servant = true;
	else if ((arg = option (args.arg (), "inc_cpp")))
		inc_cpp = args.parameter (arg);

	if (arg) {
		args.next ();
		return true;
	} else
		return false;
}

path Compiler::out_file (const Root& tree, const path& dir, const std::string& suffix, const path& ext) const
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
	path client_h = out_file (tree, out_h, client_suffix, "h");
	if (client) {
		path client_cpp;
		if (!no_client_cpp)
			client_cpp = out_file (tree, out_cpp, client_suffix, "cpp");
		Client client (*this, tree, client_h, client_cpp);
		tree.visit (client);
	}
	path servant_h = out_file (tree, out_h, servant_suffix, "h");
	if (server) {
		Servant servant (*this, tree, servant_h, client_h);
		tree.visit (servant);
	}
	if (proxy) {
		Proxy proxy (*this, tree, out_file (tree, out_cpp, proxy_suffix, "cpp"), servant_h);
		tree.visit (proxy);
	}
}

void Compiler::file_begin (const std::filesystem::path& file, Builder& builder)
{
	ami_map_.clear ();
	ami_handler_map_.clear ();
}

void Compiler::interface_end (const Interface& itf, Builder& builder)
{
	if (async_supported (itf)) {

		Location loc = builder.location ();
		SimpleDeclarator ami_return_val (AMI_RETURN_VAL, loc);

		AMI_Objects ami_objects;

		AMI_Bases async_bases;
		for (const auto b : itf.bases ()) {
			auto f = ami_map_.find (b);
			if (f != ami_map_.end ())
				async_bases.push_back (&*f);
		}

		{ // Make Poller
			SimpleDeclarator ami_timeout (AMI_TIMEOUT, loc);

			builder.valuetype_begin (SimpleDeclarator (make_ami_id (itf, AMI_POLLER), loc), ValueType::Modifier::ABSTRACT);

			ScopedNames bases;
			for (const auto b : async_bases) {
				bases.push_front (b->second.poller->scoped_name ());
			}
			if (bases.empty ()) {
				ScopedName base (loc, true, { "Messaging", "Poller" });
				bases.push_front (std::move (base));
			}
			builder.valuetype_bases (false, bases);

			for (auto item : itf) {
				switch (item->kind ()) {
				case Item::Kind::OPERATION: {
					const Operation& op = static_cast <const Operation&> (*item);
					builder.operation_begin (false, Type (), SimpleDeclarator (op.name (), loc));

					builder.parameter (Parameter::Attribute::IN, Type (BasicType::ULONG), ami_timeout);
					if (op.tkind () != Type::Kind::VOID)
						builder.parameter (Parameter::Attribute::OUT, Type (op), ami_return_val);

					for (auto par : op) {
						if (par->attribute () != Parameter::Attribute::IN)
							builder.parameter (Parameter::Attribute::OUT, Type (*par), SimpleDeclarator (par->name (), loc));
					}

					builder.raises (poller_raises (loc, op.raises ()));
					builder.operation_end ();
				} break;

				case Item::Kind::ATTRIBUTE: {
					const Attribute& att = static_cast <const Attribute&> (*item);

					builder.operation_begin (false, Type (), SimpleDeclarator ("get_" + att.name (), loc));
					builder.parameter (Parameter::Attribute::IN, Type (BasicType::ULONG), ami_timeout);
					builder.parameter (Parameter::Attribute::OUT, Type (att), ami_return_val);
					builder.raises (poller_raises (loc, att.getraises ()));
					builder.operation_end ();

					if (!att.readonly ()) {
						builder.operation_begin (false, Type (), SimpleDeclarator ("set_" + att.name (), loc));
						builder.parameter (Parameter::Attribute::IN, Type (BasicType::ULONG), ami_timeout);
						builder.raises (poller_raises (loc, att.setraises ()));
						builder.operation_end ();
					}
				} break;
				}
			}

			ami_objects.poller = static_cast <const ValueType*> (builder.cur_parent ());
			builder.valuetype_end ();
		}

		{ // Make Handler

			const Type exception_holder = builder.lookup_type (ScopedName (loc, true, { "Messaging", "ExceptionHolder" }));
			SimpleDeclarator excep_holder ("excep_holder", loc);

			builder.interface_begin (SimpleDeclarator (make_ami_id (itf, AMI_HANDLER), loc), InterfaceKind::UNCONSTRAINED);

			ScopedNames bases;
			for (const auto b : async_bases) {
				bases.push_front (b->second.handler->scoped_name ());
			}
			if (bases.empty ()) {
				ScopedName base (loc, true, { "Messaging", "ReplyHandler" });
				bases.push_front (std::move (base));
			}
			builder.interface_bases (bases);

			for (auto item : itf) {
				switch (item->kind ()) {
				case Item::Kind::OPERATION: {
					const Operation& op = static_cast <const Operation&> (*item);

					builder.operation_begin (false, Type (), SimpleDeclarator (op.name (), loc));

					if (op.tkind () != Type::Kind::VOID)
						builder.parameter (Parameter::Attribute::IN, Type (op), ami_return_val);

					for (auto par : op) {
						if (par->attribute () != Parameter::Attribute::IN)
							builder.parameter (Parameter::Attribute::IN, Type (*par), SimpleDeclarator (par->name (), loc));
					}

					builder.operation_end ();

					builder.operation_begin (false, Type (), SimpleDeclarator (op.name () + AMI_EXCEP, loc));
					builder.parameter (Parameter::Attribute::IN, Type (exception_holder), excep_holder);
					builder.operation_end ();

				} break;

				case Item::Kind::ATTRIBUTE: {
					const Attribute& att = static_cast <const Attribute&> (*item);

					builder.operation_begin (false, Type (), SimpleDeclarator ("get_" + att.name (), loc));
					builder.parameter (Parameter::Attribute::IN, Type (att), ami_return_val);
					builder.operation_end ();

					builder.operation_begin (false, Type (), SimpleDeclarator ("get_" + att.name () + AMI_EXCEP, loc));
					builder.parameter (Parameter::Attribute::IN, Type (exception_holder), excep_holder);
					builder.operation_end ();

					if (!att.readonly ()) {
						builder.operation_begin (false, Type (), SimpleDeclarator ("set_" + att.name (), loc));
						builder.operation_end ();

						builder.operation_begin (false, Type (), SimpleDeclarator ("set_" + att.name () + AMI_EXCEP, loc));
						builder.parameter (Parameter::Attribute::IN, Type (exception_holder), excep_holder);
						builder.operation_end ();
					}
				} break;
				}
			}

			ami_objects.handler = static_cast <const Interface*> (builder.cur_parent ());
			builder.interface_end ();
		}

		ami_map_.emplace (&itf, ami_objects);
		ami_handler_map_.emplace (ami_objects.handler, &itf);
	}
}

bool Compiler::async_supported (const Interface& itf)
{
	return CodeGenBase::async_supported (itf);
}

Identifier Compiler::make_ami_id (const Interface& itf, const char* suffix)
{
	std::string id = "AMI_" + itf.name () + suffix;
	const Symbols* scope = itf.parent_scope ();
	assert (scope);
	while (scope && scope->find (static_cast <const Identifier&> (id))) {
		id.insert (0, "AMI_");
	}
	return id;
}

ScopedNames Compiler::poller_raises (const AST::Location& loc, const AST::Raises& op_raises)
{
	bool wrong_transaction_found = false;
	ScopedName wrong_transaction (loc, true, { "CORBA", "WrongTransaction" });
	ScopedNames exceptions;
	for (auto ex : op_raises) {
		ScopedName sn = ex->scoped_name ();
		if (sn == wrong_transaction)
			wrong_transaction_found = true;
		exceptions.push_front (std::move (sn));
	}
	if (!wrong_transaction_found)
		exceptions.push_front (std::move (wrong_transaction));

	return exceptions;
}
