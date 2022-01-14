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
#ifndef NIDL2CPP_CLIENT_H_
#define NIDL2CPP_CLIENT_H_
#pragma once

#include "CodeGenBase.h"
#include "Header.h"

// Client code generator
class Client : public CodeGenBase
{
public:
	Client (const Options& options, const AST::Root& root,
		const std::filesystem::path& file_h, const std::filesystem::path& file_cpp) :
		CodeGenBase (options),
		h_ (file_h, root),
		cpp_ (file_cpp, root),
		export_count_ (0)
	{
		cpp_ << "#include <CORBA/CORBA.h>\n"
			"#include <Nirvana/OLF.h>\n"
			"#include ";
		std::filesystem::path cpp_dir = file_cpp.parent_path ();
		if (file_h.parent_path () == cpp_dir)
			cpp_ << file_h.filename ();
		else
			cpp_ << '"' << std::filesystem::relative (file_h, cpp_dir).generic_string () << '"';
		cpp_  << std::endl;
		initial_cpp_size_ = cpp_.size ();
	}

	struct Param
	{
		Param (const AST::Parameter& p) :
			type (p),
			att (p.attribute ())
		{}

		Param (const AST::Attribute& t) :
			type (t),
			att (AST::Parameter::Attribute::IN)
		{}

		const AST::Type& type;
		const AST::Parameter::Attribute att;
	};

	struct Signature
	{
		Signature (const AST::Operation& o) :
			op (o)
		{}

		const AST::Operation& op;
	};

	struct MemberDefault
	{
		MemberDefault (const AST::Member& m, const char* pref) :
			member (m),
			prefix (pref)
		{}

		const AST::Member& member;
		const char* prefix;
	};

	struct MemberInit
	{
		MemberInit (const AST::Member& m, const char* pref) :
			member (m),
			prefix (pref)
		{}

		const AST::Member& member;
		const char* prefix;
	};

protected:
	virtual void end (const AST::Root&);

	virtual void leaf (const AST::Include& item);
	virtual void leaf (const AST::TypeDef& item);

	virtual void leaf (const AST::InterfaceDecl& item);
	virtual void begin (const AST::Interface& item);
	virtual void end (const AST::Interface& item);

	virtual void leaf (const AST::Constant& item);

	virtual void begin (const AST::Exception& item);
	virtual void end (const AST::Exception& item);

	virtual void leaf (const AST::StructDecl& item);
	virtual void begin (const AST::Struct& item);
	virtual void end (const AST::Struct& item);

	virtual void leaf (const AST::Enum& item);

private:
	void forward_decl (const AST::NamedItem& item);
	void forward_interface (const AST::NamedItem& item, AST::InterfaceKind kind);
	void backward_compat_var (const AST::NamedItem& item);
	void environment (const AST::Raises& raises);
	void type_code_decl (const AST::NamedItem& item);
	void type_code_def (const AST::RepositoryId& rid);
	static bool constant (Code& stm, const AST::Constant& item);
	void rep_id_of (const AST::RepositoryId& rid);
	void define_structured_type (const AST::RepositoryId& rid, const Members& members, const char* suffix = "");
	void type_code_func (const AST::NamedItem& item);
	void constructors (const AST::Identifier& name, const Members& members, const char* prefix);
	void accessors (const Members& members);
	void member_variables (const Members& members);
	void member_variables_legacy (const Members& members);
	void marshal (const Members& members, const char* prefix);
	static bool nested (const AST::NamedItem& item);
	void h_namespace_open (const AST::NamedItem& item);

	void implement_nested_items (const AST::ItemContainer& parent);
	void implement (const AST::Exception& item);
	void implement (const AST::Struct& item);
	void implement (const AST::Union& item);
	void implement (const AST::Enum& item);

	void native_itf_template (const AST::Operation& op);

	static bool is_boolean (const AST::Type& t)
	{
		const AST::Type& dt = t.dereference_type ();
		return dt.tkind () == AST::Type::Kind::BASIC_TYPE && dt.basic_type () == AST::BasicType::BOOLEAN;
	}

private:
	Header h_; // .h file
	Code cpp_; // .cpp file.
	size_t initial_cpp_size_;
	unsigned export_count_;
};

Code& operator << (Code& stm, const Client::Param& t);
Code& operator << (Code& stm, const Client::Signature& op);
Code& operator << (Code& stm, const Client::MemberDefault& val);
Code& operator << (Code& stm, const Client::MemberInit& val);

#endif
