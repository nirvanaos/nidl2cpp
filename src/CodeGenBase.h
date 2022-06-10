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
		ABI_ret (const AST::Type& t, bool _byref = false) :
			type (t),
			byref (_byref)
		{}

		const AST::Type& type;
		bool byref;
	};

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

	struct Var
	{
		Var (const AST::Type& t) :
			type (t)
		{}

		const AST::Type& type;
	};

	struct VRet
	{
		VRet (const AST::Type& t) :
			type (t)
		{}

		const AST::Type& type;
	};

	struct ConstRef
	{
		ConstRef (const AST::Type& t) :
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

	struct Namespace
	{
		Namespace (const char* ns) :
			prefix (ns)
		{}

		const char* prefix;
	};

	struct ItemNamespace
	{
		ItemNamespace (const AST::NamedItem& ni) :
			item (ni)
		{}

		const AST::NamedItem& item;
	};

	struct MemberType
	{
		MemberType (const AST::Type& t) :
			type (t)
		{}

		const AST::Type& type;
	};

	struct MemberVariable
	{
		MemberVariable (const AST::Member& m) :
			member (m)
		{}

		const AST::Member& member;
	};

	struct Accessors
	{
		Accessors (const AST::Member& m) :
			member (m)
		{}

		const AST::Member& member;
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

	typedef std::vector <const AST::StateMember*> StateMembers;
	static StateMembers get_members (const AST::ValueType& cont);

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

	const Options& options () const
	{
		return options_;
	}

protected:
	CodeGenBase (const Options& options) :
		options_ (options)
	{}

	static Code& indent (Code& stm)
	{
		stm.indent ();
		return stm;
	}

	static Code& unindent (Code& stm)
	{
		stm.unindent ();
		return stm;
	}

	static Code& empty_line (Code& stm)
	{
		stm.empty_line ();
		return stm;
	}

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

	virtual void leaf (const AST::ValueTypeDecl&) {}
	virtual void begin (const AST::ValueType&) {}
	virtual void end (const AST::ValueType&) {}

	virtual void leaf (const AST::StateMember&);
	virtual void leaf (const AST::ValueFactory&);
	virtual void leaf (const AST::ValueBox&);

private:
	static bool pred (const char* l, const char* r)
	{
		return strcmp (l, r) < 0;
	}

	static Members get_members (const AST::ItemContainer& cont, AST::Item::Kind member_kind);
	
	static bool is_native_interface (const AST::NamedItem& type);

	static void get_all_bases (const AST::ValueType& vt,
		std::unordered_set <const AST::ItemContainer*>& bset, Bases& bvec);
	static void get_all_bases (const AST::Interface& ai,
		std::unordered_set <const AST::ItemContainer*>& bset, Bases& bvec);

private:
	static const char* const protected_names_ [];

	const Options& options_;
};

Code& operator << (Code& stm, const CodeGenBase::QName& qn);
Code& operator << (Code& stm, const CodeGenBase::ParentName& qn);
Code& operator << (Code& stm, const CodeGenBase::TypePrefix& t);
Code& operator << (Code& stm, const CodeGenBase::ABI_ret& t);
Code& operator << (Code& stm, const CodeGenBase::ABI_param& t);
Code& operator << (Code& stm, const CodeGenBase::Var& t);
Code& operator << (Code& stm, const CodeGenBase::VRet& t);
Code& operator << (Code& stm, const CodeGenBase::TypeCodeName& t);
Code& operator << (Code& stm, const CodeGenBase::ServantParam& t);
Code& operator << (Code& stm, const CodeGenBase::ServantOp& t);
Code& operator << (Code& stm, const CodeGenBase::Namespace& ns);
Code& operator << (Code& stm, const CodeGenBase::ItemNamespace& ns);
Code& operator << (Code& stm, const CodeGenBase::ConstRef& t);
Code& operator << (Code& stm, const CodeGenBase::MemberType& t);
Code& operator << (Code& stm, const CodeGenBase::MemberVariable& m);
Code& operator << (Code& stm, const CodeGenBase::Accessors& a);
Code& operator << (Code& stm, const CodeGenBase::MemberDefault& val);
Code& operator << (Code& stm, const CodeGenBase::MemberInit& val);

#endif
