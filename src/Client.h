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
		cpp_.include_header (file_h);
		initial_cpp_size_ = cpp_.size ();
	}

	struct Param
	{
		Param (const AST::Parameter& p) :
			type (p),
			att (p.attribute ())
		{}

		Param (const AST::Member& m) :
			type (m),
			att (AST::Parameter::Attribute::IN)
		{}

		const AST::Type& type;
		const AST::Parameter::Attribute att;
	};

	struct Signature
	{
		Signature (const AST::OperationBase& o) :
			op (o)
		{}

		const AST::OperationBase& op;
	};

	struct ConstType
	{
		ConstType (const AST::Constant& _c) :
			c (_c)
		{}

		const AST::Constant& c;
	};

private:
	virtual void end (const AST::Root&);

	virtual void leaf (const AST::Include& item);
	virtual void leaf (const AST::TypeDef& item);

	virtual void leaf (const AST::InterfaceDecl& item);
	virtual void begin (const AST::Interface& item);
	virtual void end (const AST::Interface& item);

	virtual void leaf (const AST::Constant& item);

	virtual void leaf (const AST::Exception& item);
	void define (const AST::Exception& item);
	void implement (const AST::Exception& item);

	virtual void leaf (const AST::StructDecl& item);
	virtual void leaf (const AST::Struct& item);
	void define (const AST::Struct& item);
	void implement (const AST::Struct& item);

	virtual void leaf (const AST::UnionDecl& item);
	virtual void leaf (const AST::Union&);
	void define (const AST::Union& item);
	void implement (const AST::Union& item);

	virtual void leaf (const AST::Enum& item);
	void implement (const AST::Enum& item);

	virtual void leaf (const AST::ValueTypeDecl& item);
	virtual void begin (const AST::ValueType& item);
	virtual void end (const AST::ValueType& item);

	virtual void leaf (const AST::ValueBox& item);

	void forward_guard (const AST::NamedItem& item);
	void forward_define (const AST::NamedItem& item);
	void forward_decl (const AST::NamedItem& item);
	void forward_decl (const AST::StructBase& item);
	void forward_decl (const AST::StructDecl& item)
	{
		forward_decl (item.definition ());
	}
	void forward_decl (const AST::UnionDecl& item)
	{
		forward_decl (static_cast <const AST::StructBase&> (item.definition ()));
	}
	void forward_decl (const AST::Union& item)
	{
		forward_decl (static_cast <const AST::StructBase&> (item));
	}

	void forward_interface (const AST::ItemWithId& item);
	void begin_interface (const AST::IV_Base& item);
	void end_interface (const AST::IV_Base& item);
	void backward_compat_var (const AST::NamedItem& item);
	void environment (const AST::Raises& raises);
	void type_code_decl (const AST::NamedItem& item);
	void type_code_def (const AST::ItemWithId& item);
	void rep_id_of (const AST::ItemWithId& item);
	void define_structured_type (const AST::ItemWithId& item);
	void define_ABI (const AST::StructBase& item);
	void define_ABI (const AST::Union& item);
	void type_code_func (const AST::NamedItem& item);
	void constructors (const AST::StructBase& item, const char* prefix);
	void accessors (const AST::StructBase& item);
	void member_variables (const AST::StructBase& item);
	void member_variables_legacy (const AST::StructBase& item);
	static bool has_check (const AST::ItemWithId& item);
	static bool has_check (const AST::Type& type);
	static bool is_nested (const AST::NamedItem& item);
	void h_namespace_open (const AST::NamedItem& item);

	void implement_nested_items (const AST::IV_Base& parent);

	void native_itf_template (const AST::Operation& op);

	void bridge_bases (const Bases& bases);

	static size_t version (const std::string& rep_id);

	void assign_union (const AST::Union& item, bool move);
	void element_case (const AST::UnionElement& el);

	void define_swap (const AST::ItemWithId& item);

	void implement_marshaling (const AST::StructBase& item, const char* prefix);

	void marshal_union (const AST::Union& u, bool out);

private:
	Header h_; // .h file
	Code cpp_; // .cpp file.
	size_t initial_cpp_size_;
	unsigned export_count_;
};

Code& operator << (Code& stm, const Client::Param& t);
Code& operator << (Code& stm, const Client::Signature& op);
Code& operator << (Code& stm, const Client::ConstType& op);

#endif
