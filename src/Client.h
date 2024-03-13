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
	Client (const Compiler& compiler, const AST::Root& root,
		const std::filesystem::path& file_h, const std::filesystem::path& file_cpp) :
		CodeGenBase (compiler),
		h_ (file_h, root),
		cpp_ (file_cpp, root)
	{
		if (!compiler.inc_cpp.empty ())
			cpp_ << "#include \"" << compiler.inc_cpp << "\"\n";

		cpp_ << "#include <CORBA/CORBA.h>\n"
			"#include <Nirvana/OLF.h>\n"
			"#include ";
		cpp_.include_header (file_h);
	}

	struct Param
	{
		Param (const AST::Parameter& p) :
			type (p),
			att (p.attribute ())
		{}

		Param (const AST::Type& t, AST::Parameter::Attribute pa = AST::Parameter::Attribute::IN) :
			type (t),
			att (pa)
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

	struct FactoryParam
	{
		FactoryParam (const AST::Type& t) :
			type (t)
		{}

		const AST::Type& type;
	};

	struct FactorySignature
	{
		FactorySignature (const AST::ValueFactory& o) :
			op (o)
		{}

		const AST::ValueFactory& op;
	};

	struct PollerSignature
	{
		PollerSignature (const AST::Operation& o) :
			op (o)
		{}

		const AST::Operation& op;
	};

	struct SendcSignature
	{
		SendcSignature (const AST::Operation& o, const AST::Interface& h) :
			op (o),
			handler (h)
		{}

		const AST::Operation& op;
		const AST::Interface& handler;
	};

	struct SendpSignature
	{
		SendpSignature (const AST::Operation& o) :
			op (o)
		{}

		const AST::Operation& op;
	};

	struct ConstType
	{
		ConstType (const AST::Constant& _c) :
			c (_c)
		{}

		const AST::Constant& c;
	};

private:
	virtual void end (const AST::Root&) override;

	virtual void leaf (const AST::Include& item) override;
	virtual void leaf (const AST::TypeDef& item) override;

	virtual void leaf (const AST::InterfaceDecl& item) override;
	virtual void begin (const AST::Interface& item) override;
	virtual void end (const AST::Interface& item) override;

	virtual void leaf (const AST::Constant& item) override;

	virtual void leaf (const AST::Exception& item) override;
	void define (const AST::Exception& item);
	void implement (const AST::Exception& item);

	virtual void leaf (const AST::StructDecl& item) override;
	virtual void leaf (const AST::Struct& item) override;
	void define (const AST::Struct& item);
	void implement (const AST::Struct& item);

	virtual void leaf (const AST::UnionDecl& item) override;
	virtual void leaf (const AST::Union&) override;
	void define (const AST::Union& item);
	void implement (const AST::Union& item);

	virtual void leaf (const AST::Enum& item) override;
	void implement (const AST::Enum& item);

	virtual void leaf (const AST::ValueTypeDecl& item) override;
	virtual void begin (const AST::ValueType& item) override;
	virtual void end (const AST::ValueType& item) override;

	virtual void leaf (const AST::ValueBox& item) override;

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
	void define_itf_suppl (const AST::Identifier& name);
	void implement_marshaling (const AST::StructBase& item);
	void marshal_union (const AST::Union& u, bool out);

	void generate_ami (const AST::Interface& itf);

	void override_sendp (const AST::Interface& itf, const AST::ValueType& poller);

	void traits_begin (const AST::ItemWithId& item);
	void iv_traits_begin (const AST::ItemWithId& item);
	void iv_traits_end ();
	void structured_type_traits (const AST::ItemWithId& item);

private:
	Header h_; // .h file
	Code cpp_; // .cpp file.
};

Code& operator << (Code& stm, const Client::Param& t);
Code& operator << (Code& stm, const Client::Signature& op);
Code& operator << (Code& stm, const Client::FactoryParam& t);
Code& operator << (Code& stm, const Client::FactorySignature& op);
Code& operator << (Code& stm, const Client::ConstType& op);
Code& operator << (Code& stm, const Client::PollerSignature& op);
Code& operator << (Code& stm, const Client::SendcSignature& op);
Code& operator << (Code& stm, const Client::SendpSignature& op);

#endif
