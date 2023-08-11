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

#include "Compiler.h"
#include "Code.h"
#include "Options.h"
#include <BE/MessageOut.h>
#include <unordered_set>

#define FACTORY_SUFFIX "_factory"
#define EXCEPTION_SUFFIX "::_Data"

#define SKELETON_FUNC_PREFIX "_s_"
#define SKELETON_GETTER_PREFIX "_s__get_"
#define SKELETON_SETTER_PREFIX "_s__set_"
#define SKELETON_MOVE_PREFIX "_s__move_"

#define AMI_TIMEOUT "ami_timeout"
#define AMI_RETURN_VAL "ami_return_val"
#define AMI_POLLER "Poller"
#define AMI_HANDLER "Handler"
#define AMI_EXCEP "_excep"

class CodeGenBase :
	public AST::CodeGen,
	public BE::MessageOut
{
public:
	static bool is_keyword (const AST::Identifier& id);
	inline static const char protected_prefix_ [] = "_cxx_";

	const Options& options () const
	{
		return options_;
	}

	typedef AST::ContainerT <AST::Member> Members;
	typedef AST::ContainerT <AST::UnionElement> UnionElements;

	typedef std::vector <const AST::StateMember*> StateMembers;
	static StateMembers get_members (const AST::ValueType& cont);

	// Value and abstract bases of value base
	typedef std::vector <const AST::IV_Base*> Bases;
	static Bases get_all_bases (const AST::ValueType& vt);

	static const AST::Interface* get_concrete_supports (const AST::ValueType& vt);

	typedef std::vector <const AST::ValueFactory*> Factories;
	static Factories get_factories (const AST::ValueType& vt);
	static bool has_factories (const AST::ValueType& vt);

	static bool is_var_len (const AST::Type& type);
	static bool is_var_len (const Members& members);
	static bool is_CDR (const AST::Type& type);
	static bool is_CDR (const Members& members);
	static bool is_pseudo (const AST::NamedItem& item);
	static bool is_ref_type (const AST::Type& type);
	static bool is_complex_type(const AST::Type& type);
	static const AST::Enum* is_enum (const AST::Type& type);
	static bool is_native_interface (const AST::Type& type);
	static bool is_servant (const AST::Type& type);
	static bool is_native (const AST::Type& type);
	static bool is_native (const Members& members);
	static bool is_boolean (const AST::Type& type);

	static bool is_sequence (const AST::Type& type)
	{
		return type.dereference_type ().tkind () == AST::Type::Kind::SEQUENCE;
	}

	static bool is_aligned_struct (const AST::Type& type);

	static bool is_bounded (const AST::Type& type);
	void init_union (Code& stm, const AST::UnionElement& init_el, const char* prefix = "");

	struct ABI2Servant
	{
		ABI2Servant (const AST::Type& t, const AST::Identifier& n,
			AST::Parameter::Attribute a = AST::Parameter::Attribute::IN) :
			type (t),
			name (n),
			att (a)
		{}

		ABI2Servant (const AST::Parameter& p) :
			type (p),
			name (p.name ()),
			att (p.attribute ())
		{}

		const AST::Type& type;
		const AST::Identifier& name;
		const AST::Parameter::Attribute att;
	};

	struct CatchBlock
	{};

	struct AMI_Name;

	typedef std::vector <AMI_Name> AMI_Bases;

	struct AMI_Name
	{
		const AST::Interface& itf;
		std::string name;
		const char* suf;

		AMI_Name (const AST::Interface& i, const char* suffix);

		AMI_Bases all_bases () const;
		AMI_Bases direct_bases () const;
	};

	static void ami_skeleton_begin (Code& stm, const AMI_Name& ami);
	static void ami_skeleton_bases (Code& stm, const AMI_Name& ami);
	static void fill_epv (Code& stm, const std::vector <std::string>& epv, bool val_with_concrete_itf = false);

protected:
	CodeGenBase (const Compiler& compiler) :
		BE::MessageOut (compiler.err_out ()),
		options_ (compiler)
	{}

	virtual void leaf (const AST::Include& item) {}
	virtual void leaf (const AST::Native&) {}
	virtual void leaf (const AST::TypeDef& item) {}
	virtual void begin (const AST::ModuleItems&) {}
	virtual void end (const AST::ModuleItems&) {}
	virtual void leaf (const AST::Operation&) {}
	virtual void leaf (const AST::Attribute&) {}

	virtual void leaf (const AST::InterfaceDecl& item) {}
	virtual void end (const AST::Interface& item) {}

	virtual void leaf (const AST::Constant& item) {}
	virtual void leaf (const AST::Exception& item) {}
	virtual void leaf (const AST::StructDecl& item) {}
	virtual void leaf (const AST::Struct& item) {}

	virtual void leaf (const AST::Enum& item) {}

	virtual void leaf (const AST::UnionDecl&) {}
	virtual void leaf (const AST::Union&) {}

	virtual void leaf (const AST::ValueTypeDecl&) {}
	virtual void begin (const AST::ValueType&) {}
	virtual void end (const AST::ValueType&) {}

	virtual void leaf (const AST::StateMember&) {}
	virtual void leaf (const AST::ValueFactory&) {}
	virtual void leaf (const AST::ValueBox&) {}

	static void marshal_members (Code& stm, const Members& members, const char* func, const char* prefix);
	static void marshal_member (Code& stm, const AST::Member& m, const char* func, const char* prefix);
	static void unmarshal_members (Code& stm, const Members& members, const char* prefix);
	static void unmarshal_member (Code& stm, const AST::Member& m, const char* prefix);

	static bool is_special_base (const AST::Interface& itf) noexcept;
	static bool is_immutable (const AST::Interface& itf) noexcept;
	static bool is_stateless (const AST::Interface& itf) noexcept;

	static bool is_custom (const AST::Operation& op) noexcept;
	static bool is_custom (const AST::Interface& itf) noexcept;

	static bool async_supported (const AST::Interface& itf) noexcept;
	
	std::string make_async_repository_id (const AMI_Name& async_name);

private:
	static bool pred (const char* l, const char* r)
	{
		return strcmp (l, r) < 0;
	}

	static bool is_native_interface (const AST::NamedItem& type);
	static bool is_servant (const AST::NamedItem& type);

	static void get_all_bases (const AST::ValueType& vt,
		std::unordered_set <const AST::IV_Base*>& bset, Bases& bvec);
	static void get_all_bases (const AST::Interface& ai,
		std::unordered_set <const AST::IV_Base*>& bset, Bases& bvec);

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

	ABI_param (const AST::Type& t, AST::Parameter::Attribute pa = AST::Parameter::Attribute::IN) :
		type (t),
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
	ServantParam (const AST::Parameter& p, bool v = false) :
		type (p),
		att (p.attribute ()),
		virt (v)
	{}

	ServantParam (const AST::Attribute& t, bool v = false) :
		type (t),
		att (AST::Parameter::Attribute::IN),
		virt (v)
	{}

	const AST::Type& type;
	const AST::Parameter::Attribute att;
	const bool virt;
};

Code& operator << (Code& stm, const ServantParam& t);

struct ServantOp
{
	ServantOp (const AST::Operation& o, bool v = false) :
		op (o),
		virt (v)
	{}

	const AST::Operation& op;
	const bool virt;
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

Code& operator << (Code& stm, const CodeGenBase::AMI_Name& val);
Code& operator << (Code& stm, const CodeGenBase::ABI2Servant& val);
Code& operator << (Code& stm, const CodeGenBase::CatchBlock&);

#endif
