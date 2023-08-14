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
#ifndef NIDL2CPP_COMPILER_H_
#define NIDL2CPP_COMPILER_H_
#pragma once

#include <IDL_FrontEnd.h>
#include <AST/Interface.h>
#include "Options.h"
#include <unordered_map>

class Compiler :
	public IDL_FrontEnd,
	public Options
{
public:
	std::ostream& err_out () const noexcept
	{
		return IDL_FrontEnd::err_out ();
	}

	int main (int argc, char* argv [])
	{
		char fi [] = "-FI";
		char ami [] = "CORBA/AMI.idl";

		if (argc > 1) {
			const char* arg1 = argv [1];
			size_t len = strlen (arg1);
			if (len >= 8) {
				const char* end = arg1 + len;
				if (std::equal (end - 7, end, "AMI.idl")) {
					const char slash = *(end - 8);
					if ('/' == slash || '\\' == slash)
						return IDL_FrontEnd::main (argc, argv);
				}
			}
		}

		std::vector <char*> args;
		args.resize (argc + 2);
		auto arg = std::copy (argv, argv + argc, args.begin ());
		*(arg++) = fi;
		*(arg++) = ami;

		return IDL_FrontEnd::main ((int)args.size (), args.data ());
	}

	struct AMI_Objects
	{
		const AST::ValueType* poller;
		const AST::Interface* handler;
	};

	typedef std::unordered_map <const AST::Interface*, AMI_Objects> AMI_Map;

	typedef std::vector <const AMI_Map::value_type*> AMI_Bases;

	const AMI_Map& ami_map () const noexcept
	{
		return ami_map_;
	}

private:
	// Override print_usage_info for additional usage information.
	virtual void print_usage_info (const char* exe_name) override;

	// Override parse_command_line to parse own command line switches.
	virtual bool parse_command_line (CmdLine& args) override;

	static const char* option (const char* arg, const char* opt);

	// Override generate_code to build output from the AST.
	virtual void generate_code (const AST::Root& tree) override;

	virtual void file_begin (const std::filesystem::path& file, AST::Builder& builder) override;
	virtual void interface_end (const AST::Interface& itf, AST::Builder& builder) override;

	AST::Identifier make_ami_id (const AST::Interface& itf, const char* suffix);

	static bool async_supported (const AST::Interface& itf);

	static AST::ScopedNames poller_raises (const AST::Location& loc, const AST::Raises& op_raises);

	std::filesystem::path out_file (const AST::Root& tree, const std::filesystem::path& dir,
		const std::string& suffix, const std::filesystem::path& ext) const;

private:
	AMI_Map ami_map_;
};

#endif
