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
#ifndef NIDL2CPP_PROXY_H_
#define NIDL2CPP_PROXY_H_
#pragma once

#include "CodeGenBase.h"
#include "Code.h"

// Proxy code generator
class Proxy : public CodeGenBase
{
public:
	Proxy (const Options& options, const AST::Root& root,
		const std::filesystem::path& file, const std::filesystem::path& servant) :
		CodeGenBase (options),
		cpp_ (file, root),
		custom_ (false)
	{
		cpp_ << "#include <CORBA/Proxy/Proxy.h>\n"
			"#include ";
		cpp_.include_header (servant);
	}

private:
	virtual void end (const AST::Root&);

	virtual void begin (const AST::Interface& itf) {}
	virtual void end (const AST::Interface& itf);

	virtual void leaf (const AST::Exception& item);
	virtual void leaf (const AST::Struct& item);
	virtual void leaf (const AST::Union& item);
	virtual void leaf (const AST::Enum& item);

	virtual void end (const AST::ValueType& item);
	virtual void leaf (const AST::ValueBox& item);

	static bool is_custom (const AST::Operation& op);

	static void get_parameters (const AST::Operation& op, Members& params_in, Members& params_out);

	static std::string export_name (const AST::NamedItem& item);

	void implement (const AST::Operation& op);
	void implement (const AST::Attribute& att);

	void md_members (const Members& members);
	void md_member (const AST::Member& m);
	void md_member (const AST::Type& t, const std::string& name);

	struct OpMetadata
	{
		std::string name;
		const AST::Type* type;
		Members params_in;
		Members params_out;
	};

	typedef std::list <OpMetadata> Metadata;

	void md_operation (const AST::Interface& itf, const OpMetadata& op);

	void type_code_members (const AST::ItemWithId& item, const Members& members);
	void type_code_name (const AST::NamedItem& item);
	void state_member (const AST::StateMember& m);

	Code& exp (const AST::NamedItem& item);

private:
	Code cpp_;
	bool custom_;
};

#endif
