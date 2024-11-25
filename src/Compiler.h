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

#include <idlfe/IDL_FrontEnd.h>
#include <idlfe/AST/Interface.h>
#include <idlfe/AST/Exception.h>
#include "Options.h"
#include <unordered_map>

class Compiler :
	public IDL_FrontEnd,
	public Options
{
public:
	static const char name_ [];
	static const unsigned short version_ [3];

	Compiler () :
		IDL_FrontEnd (IDL_FrontEnd::FLAG_ENABLE_CONST_OBJREF)
	{}

	std::ostream& err_out () const noexcept
	{
		return IDL_FrontEnd::err_out ();
	}

	struct AMI_Objects
	{
		const AST::ValueType* poller;
		const AST::Interface* handler;
	};

	typedef std::unordered_map <const AST::Interface*, AMI_Objects> AMI_Interfaces;

	typedef std::vector <const AMI_Interfaces::value_type*> AMI_Bases;

	const AMI_Interfaces& ami_interfaces () const noexcept
	{
		return ami_interfaces_;
	}

	// Handler interface -> interface.
	typedef std::unordered_map <const AST::Interface*, const AST::Interface*> AMI_Handlers;

	const AMI_Handlers& ami_handlers () const noexcept
	{
		return ami_handlers_;
	}

	// Pollers
	typedef std::unordered_set <const AST::ValueType*> AMI_Pollers;

	const AMI_Pollers& ami_pollers () const noexcept
	{
		return ami_pollers_;
	}

private:
	// Override print_usage_info for additional usage information.
	virtual void print_usage_info (const char* exe_name) override;

	virtual void parse_arguments (CmdLine& args) override;

	// Override parse_command_line to parse own command line switches.
	virtual bool parse_command_line (CmdLine& args) override;

	static const char* option (const char* arg, const char* opt);

	// Override generate_code to build output from the AST.
	virtual void generate_code (const AST::Root& tree) override;

	virtual void file_begin (const std::filesystem::path& file, AST::Builder& builder) override;
	virtual void interface_end (const AST::Interface& itf, AST::Builder& builder) override;

	AST::Identifier make_ami_id (const AST::Interface& itf, const char* suffix);

	bool async_supported (const AST::Interface& itf) const;

	static AST::ScopedNames poller_raises (const AST::Location& loc, const AST::Raises& op_raises);

	std::filesystem::path out_file (const AST::Root& tree, const std::filesystem::path& dir,
		const std::string& suffix, const char* ext) const;

private:
	AMI_Interfaces ami_interfaces_;
	AMI_Handlers ami_handlers_;
	AMI_Pollers ami_pollers_;
};

#endif
