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
#pragma once

#include "Code.h"
#include "Options.h"
#include <unordered_set>

#define FACTORY_SUFFIX "_factory"

class CodeGenBase : public AST::CodeGen
{
public:
	static bool is_keyword (const AST::Identifier& id);
	inline static const char protected_prefix_ [] = "_cxx_";

	typedef std::vector <const AST::Member*> Members;

	static Members get_members (const AST::ItemContainer& cont);

	typedef std::vector <const AST::StateMember*> StateMembers;
	static StateMembers get_members (const AST::ValueType& cont);

	typedef std::vector <const AST::UnionElement*> UnionElements;
	static UnionElements get_elements (const AST::Union& cont);

	// Value and abstract bases of value base
	typedef std::vector <const AST::ItemContainer*> Bases;
	static Bases get_all_bases (const AST::ValueType& vt);

	static const AST::Interface* get_concrete_supports (const AST::ValueType& vt);

	typedef std::vector <const AST::ValueFactory*> Factories;
	static Factories get_factories (const AST::ValueType& vt);

	static bool is_var_len (const AST::Type& type);
	static bool is_var_len (const Members& members);
	static bool is_pseudo (const AST::NamedItem& item);
	static bool is_ref_type (const AST::Type& type);
	static bool is_enum (const AST::Type& type);
	static bool is_native_interface (const AST::Type& type);
	static bool is_native (const AST::Type& type);
	static bool is_boolean (const AST::Type& t);
	static bool may_have_check (const AST::Type& type);
	static bool may_have_check (const AST::NamedItem& cont, const AST::Type& type, bool& recursive_seq);
	static bool is_recursive_seq (const AST::NamedItem& cont, const AST::Type& type);

	const Options& options () const
	{
		return options_;
	}

protected:
	CodeGenBase (const Options& options) :
		options_ (options)
	{}

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

	virtual void leaf (const AST::UnionDecl&) {}
	virtual void begin (const AST::Union&) {}
	virtual void end (const AST::Union&) {}
	virtual void leaf (const AST::UnionElement&) {}

	virtual void leaf (const AST::ValueTypeDecl&) {}
	virtual void begin (const AST::ValueType&) {}
	virtual void end (const AST::ValueType&) {}

	virtual void leaf (const AST::StateMember&) {}
	virtual void leaf (const AST::ValueFactory&) {}
	virtual void leaf (const AST::ValueBox&) {}

private:
	static bool pred (const char* l, const char* r)
	{
		return strcmp (l, r) < 0;
	}

	static bool is_native_interface (const AST::NamedItem& type);

	static void get_all_bases (const AST::ValueType& vt,
		std::unordered_set <const AST::ItemContainer*>& bset, Bases& bvec);
	static void get_all_bases (const AST::Interface& ai,
		std::unordered_set <const AST::ItemContainer*>& bset, Bases& bvec);

private:
	static const char* const protected_names_ [];

	const Options& options_;
};

struct QName
{
	QName (const AST::NamedItem& ni) :
		item (ni)
	{}

	const AST::NamedItem& item;
};

Code& operator << (Code& stm, const QName& qn);

struct ParentName
{
	ParentName (const AST::NamedItem& ni) :
		item (ni)
	{}

	const AST::NamedItem& item;
};

Code& operator << (Code& stm, const ParentName& qn);

struct TypePrefix
{
	TypePrefix (const AST::Type& t) :
		type (t)
	{}

	const AST::Type& type;
};

Code& operator << (Code& stm, const TypePrefix& t);

struct ABI_ret
{
	ABI_ret (const AST::Type& t, bool _byref = false) :
		type (t),
		byref (_byref)
	{}

	const AST::Type& type;
	bool byref;
};

Code& operator << (Code& stm, const ABI_ret& t);

struct ABI_param
{
	ABI_param (const AST::Parameter& p) :
		type (p),
		att (p.attribute ())
	{}

	ABI_param (const AST::Member& m, AST::Parameter::Attribute pa = AST::Parameter::Attribute::IN) :
		type (m),
		att (pa)
	{}

	const AST::Type& type;
	const AST::Parameter::Attribute att;
};

Code& operator << (Code& stm, const ABI_param& t);

struct Var
{
	Var (const AST::Type& t) :
		type (t)
	{}

	const AST::Type& type;
};

Code& operator << (Code& stm, const Var& t);

struct VRet
{
	VRet (const AST::Type& t) :
		type (t)
	{}

	const AST::Type& type;
};

Code& operator << (Code& stm, const VRet& t);

struct ConstRef
{
	ConstRef (const AST::Type& t) :
		type (t)
	{}

	const AST::Type& type;
};

Code& operator << (Code& stm, const ConstRef& t);

struct TC_Name
{
	TC_Name (const AST::NamedItem& ni) :
		item (ni)
	{}

	const AST::NamedItem& item;
};

Code& operator << (Code& stm, const TC_Name& t);

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

Code& operator << (Code& stm, const ServantParam& t);

struct ServantOp
{
	ServantOp (const AST::Operation& o) :
		op (o)
	{}

	const AST::Operation& op;
};

Code& operator << (Code& stm, const ServantOp& t);

struct Namespace
{
	Namespace (const char* ns) :
		prefix (ns)
	{}

	const char* prefix;
};

inline
Code& operator << (Code& stm, const Namespace& ns)
{
	stm.namespace_prefix (ns.prefix);
	return stm;
}

struct ItemNamespace
{
	ItemNamespace (const AST::NamedItem& ni) :
		item (ni)
	{}

	const AST::NamedItem& item;
};

inline
Code& operator << (Code& stm, const ItemNamespace& ns)
{
	stm.namespace_prefix (ns.item);
	return stm;
}

struct MemberType
{
	MemberType (const AST::Type& t) :
		type (t)
	{}

	const AST::Type& type;
};

Code& operator << (Code& stm, const MemberType& t);

struct MemberVariable
{
	MemberVariable (const AST::Member& m) :
		member (m)
	{}

	const AST::Member& member;
};

Code& operator << (Code& stm, const MemberVariable& m);

struct Accessors
{
	Accessors (const AST::Member& m) :
		member (m)
	{}

	const AST::Member& member;
};

Code& operator << (Code& stm, const Accessors& a);

struct MemberDefault
{
	MemberDefault (const AST::Member& m, const char* pref) :
		member (m),
		prefix (pref)
	{}

	const AST::Member& member;
	const char* prefix;
};

Code& operator << (Code& stm, const MemberDefault& val);

struct MemberInit
{
	MemberInit (const AST::Member& m, const char* pref) :
		member (m),
		prefix (pref)
	{}

	const AST::Member& member;
	const char* prefix;
};

Code& operator << (Code& stm, const MemberInit& val);

#endif
