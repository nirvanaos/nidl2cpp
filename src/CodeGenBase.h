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
#ifndef NIDL2CPP_CODEGENBASE_H_
#define NIDL2CPP_CODEGENBASE_H_

#include "Code.h"

class CodeGenBase : public AST::CodeGen
{
public:
	static bool is_keyword (const AST::Identifier& id);
	inline static const char protected_prefix_ [] = "_cxx_";
	static const char internal_namespace_ [];
	inline static const char typedef_prefix_ [] = "_TD_";

	struct QName
	{
		QName (const AST::NamedItem& ni) :
			item (ni)
		{}

		const AST::NamedItem& item;
	};

	struct ParentName
	{
		ParentName (const AST::NamedItem& ni) :
			item (ni)
		{}

		const AST::NamedItem& item;
	};

	struct TypePrefix
	{
		TypePrefix (const AST::Type& t) :
			type (t)
		{}

		const AST::Type& type;
	};

	struct ABI_ret
	{
		ABI_ret (const AST::Type& t) :
			type (t)
		{}

		const AST::Type& type;
	};

	struct ABI_param
	{
		ABI_param (const AST::Parameter& p) :
			type (p),
			att (p.attribute ())
		{}

		ABI_param (const AST::Attribute& t) :
			type (t),
			att (AST::Parameter::Attribute::IN)
		{}

		const AST::Type& type;
		const AST::Parameter::Attribute att;
	};

	struct Var
	{
		Var (const AST::Type& t) :
			type (t)
		{}

		const AST::Type& type;
	};

	struct TypeCodeName
	{
		TypeCodeName (const AST::NamedItem& ni) :
			item (ni)
		{}

		const AST::NamedItem& item;
	};

	struct ServantParam
	{
		ServantParam (const AST::Parameter& p) :
			type (p),
			att (p.attribute ())
		{}

		ServantParam (const AST::Attribute& t) :
			type (t),
			att (AST::Parameter::Attribute::IN)
		{}

		const AST::Type& type;
		const AST::Parameter::Attribute att;
	};

	struct ServantOp
	{
		ServantOp (const AST::Operation& o) :
			op (o)
		{}

		const AST::Operation& op;
	};

	typedef std::vector <const AST::Member*> Members;

	static Members get_members (const AST::Struct& cont)
	{
		return get_members (cont, AST::Item::Kind::MEMBER);
	}

	static Members get_members (const AST::Exception& cont)
	{
		return get_members (cont, AST::Item::Kind::MEMBER);
	}

	static Members get_members (const AST::Union& cont)
	{
		return get_members (cont, AST::Item::Kind::UNION_ELEMENT);
	}

	static bool is_var_len (const AST::Type& type);
	static bool is_var_len (const Members& members);

	static bool is_pseudo (const AST::NamedItem& item);

	static bool is_ref_type (const AST::Type& type);

protected:
	virtual void leaf (const AST::Include& item) {}
	virtual void leaf (const AST::Native&) {}
	virtual void leaf (const AST::TypeDef& item) {}
	virtual void begin (const AST::ModuleItems&) {}
	virtual void end (const AST::ModuleItems&) {}
	virtual void leaf (const AST::Operation&) {}
	virtual void leaf (const AST::Attribute&) {}
	virtual void leaf (const AST::Member& item) {}

	virtual void leaf (const AST::InterfaceDecl& item) {}
	virtual void end (const AST::Interface& item) {}

	virtual void leaf (const AST::Constant& item) {}
	virtual void begin (const AST::Exception& item) {}
	virtual void end (const AST::Exception& item) {}
	virtual void leaf (const AST::StructDecl& item) {}
	virtual void begin (const AST::Struct& item) {}
	virtual void end (const AST::Struct& item) {}

	virtual void leaf (const AST::Enum& item) {}

	virtual void leaf (const AST::UnionDecl&);
	virtual void begin (const AST::Union&);
	virtual void end (const AST::Union&);
	virtual void leaf (const AST::UnionElement&);

	virtual void leaf (const AST::ValueTypeDecl&);
	virtual void begin (const AST::ValueType&);
	virtual void end (const AST::ValueType&);
	virtual void leaf (const AST::StateMember&);
	virtual void leaf (const AST::ValueFactory&);
	virtual void leaf (const AST::ValueBox&);

private:
	static bool pred (const char* l, const char* r)
	{
		return strcmp (l, r) < 0;
	}

	static Members get_members (const AST::ItemContainer& cont, AST::Item::Kind member_kind);

private:
	static const char* const protected_names_ [];
};

Code& operator << (Code& stm, const CodeGenBase::QName& qn);
Code& operator << (Code& stm, const CodeGenBase::ParentName& qn);
Code& operator << (Code& stm, const CodeGenBase::TypePrefix& t);
Code& operator << (Code& stm, const CodeGenBase::ABI_ret& t);
Code& operator << (Code& stm, const CodeGenBase::ABI_param& t);
Code& operator << (Code& stm, const CodeGenBase::Var& t);
Code& operator << (Code& stm, const CodeGenBase::TypeCodeName& t);
Code& operator << (Code& stm, const CodeGenBase::ServantParam& t);
Code& operator << (Code& stm, const CodeGenBase::ServantOp& t);

#endif
