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
#ifndef NIDL2CPP_CODE_H_
#define NIDL2CPP_CODE_H_
#pragma once

#include <string_view>

#include <idlfe/AST/Root.h>
#include <idlfe/AST/Module.h>
#include <idlfe/AST/Variant.h>
#include <idlfe/AST/Parameter.h>
#include <idlfe/AST/Attribute.h>
#include <idlfe/AST/Operation.h>
#include <idlfe/AST/StateMember.h>

#include <idlfe/BE/IndentedOut.h>

// C++ code file output.
class Code : public BE::IndentedOut
{
	typedef BE::IndentedOut Base;
public:
	Code ();
	Code (const std::filesystem::path& file, const AST::Root& root);
	~Code ();

	void open (const std::filesystem::path& file, const AST::Root& root);
	void close ();

	const std::filesystem::path& file () const
	{
		return file_;
	}

	void include_header (const std::filesystem::path& file_h);

	void namespace_open (const AST::NamedItem& item);
	void namespace_open (const char* ns);
	void namespace_close ();

	typedef std::vector <std::string_view> Namespaces;

	const Namespaces& cur_namespace () const
	{
		return cur_namespace_;
	}

	void namespace_prefix (const char* pref);
	void namespace_prefix (const AST::NamedItem& item);
	void namespace_prefix (const AST::Module* mod);

	Code& operator << (short val)
	{
		Base::operator << (val);
		return *this;
	}

	Code& operator << (unsigned short val)
	{
		Base::operator << (val);
		return *this;
	}

	Code& operator << (int val)
	{
		Base::operator << (val);
		return *this;
	}

	Code& operator << (unsigned int val)
	{
		Base::operator << (val);
		return *this;
	}

	Code& operator << (long val)
	{
		Base::operator << (val);
		return *this;
	}

	Code& operator << (unsigned long val)
	{
		Base::operator << (val);
		return *this;
	}

	Code& operator << (unsigned long long val)
	{
		Base::operator << (val);
		return *this;
	}

	Code& operator << (long long val)
	{
		Base::operator << (val);
		return *this;
	}

	Code& operator << (float val)
	{
		Base::operator << (val);
		return *this;
	}

	Code& operator << (double val)
	{
		Base::operator << (val);
		return *this;
	}

	Code& operator << (long double val)
	{
		Base::operator << (val);
		return *this;
	}

	Code& operator << (std::ios_base& (*func)(std::ios_base&))
	{
		Base::operator << (func);
		return *this;
	}

	Code& operator << (std::basic_ios <char, std::char_traits <char> >& (*func)(std::basic_ios <char, std::char_traits <char> >&))
	{
		Base::operator << (func);
		return *this;
	}

	Code& operator << (std::ostream& (*func)(std::ostream&))
	{
		Base::operator << (func);
		return *this;
	}

	Code& operator << (Code& (*func)(Code&))
	{
		return (func) (*this);
	}

	void check_digraph (char c);

private:
	void namespace_open (const Namespaces& ns);
	void namespace_prefix (const Namespaces& ns);
	static void get_namespace (const AST::NamedItem& item, Namespaces& ns);
	static void get_namespace (const AST::Module* item, Namespaces& ns);
	static void get_namespace (const char* s, Namespaces& ns);

private:
	Namespaces cur_namespace_;
	std::filesystem::path file_;
};

Code& operator << (Code& stm, char c);
Code& operator << (Code& stm, signed char c);
Code& operator << (Code& stm, unsigned char c);
Code& operator << (Code& stm, const char* s);

inline
Code& operator << (Code& stm, const std::string& s)
{
	static_cast <BE::IndentedOut&> (stm) << s;
	return stm;
}

Code& operator << (Code& stm, const AST::Identifier& id);
Code& operator << (Code& stm, const AST::Type& t);
Code& operator << (Code& stm, const AST::Variant& var);
Code& operator << (Code& stm, const AST::Raises& raises);

inline Code& indent (Code& stm)
{
	stm.indent ();
	return stm;
}

inline Code& unindent (Code& stm)
{
	stm.unindent ();
	return stm;
}

inline Code& empty_line (Code& stm)
{
	stm.empty_line ();
	return stm;
}

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
	ServantParam (const AST::Parameter& p, bool v = false, bool f = false) :
		type (p),
		att (p.attribute ()),
		virt (v),
		factory (f)
	{}

	ServantParam (const AST::Attribute& t, bool v = false) :
		type (t),
		att (AST::Parameter::Attribute::IN),
		virt (v),
		factory (false)
	{}

	ServantParam (const AST::Type& t) :
		type (t),
		att (AST::Parameter::Attribute::IN),
		virt (false),
		factory (false)
	{}

	const AST::Type& type;
	const AST::Parameter::Attribute att;
	const bool virt;
	const bool factory;
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

struct AMI_ParametersABI
{
	AMI_ParametersABI (const AST::Operation& operation) :
		op (operation)
	{}

	const AST::Operation& op;
};

Code& operator << (Code& stm, const AMI_ParametersABI& op);

struct AMI_Parameters
{
	AMI_Parameters (const AST::Operation& operation) :
		op (operation)
	{}

	const AST::Operation& op;
};

Code& operator << (Code& stm, const AMI_ParametersABI& op);

struct TypeCodeName
{
	TypeCodeName (const AST::NamedItem& ni) :
		item (ni)
	{}

	const AST::NamedItem& item;
};

Code& operator << (Code& stm, const TypeCodeName& it);

Code& operator << (Code& stm, const AST::StateMember& m);

#endif
