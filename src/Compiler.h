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
#include "Options.h"

class Compiler :
	public IDL_FrontEnd,
	public Options
{
private:
	// Override print_usage_info for additional usage information.
	virtual void print_usage_info (const char* exe_name);

	// Override parse_command_line to parse own command line switches.
	virtual bool parse_command_line (CmdLine& args);

	static const char* option (const char* arg, const char* opt);

	// Override generate_code to build output from the AST.
	virtual void generate_code (const AST::Root& tree);

	std::filesystem::path out_file (const AST::Root& tree, const std::filesystem::path& dir, const std::string& suffix, const std::filesystem::path& ext) const;
};

#endif
