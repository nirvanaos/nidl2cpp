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
#include "pch.h"
#include "CodeGenBase.h"

using namespace std;
using namespace std::filesystem;
using namespace AST;

const char* const CodeGenBase::protected_names_ [] = {
	"FALSE",
	"TRUE",
	"alignas",
	"alignof",
	"and",
	"and_eq",
	"asm",
	"auto",
	"bitand",
	"bitor",
	"bool",
	"break",
	"case",
	"catch",
	"char",
	"char16_t",
	"char32_t",
	"class",
	"compl",
	"const",
	"const_cast",
	"constexpr",
	"continue",
	"decltype",
	"default",
	"delete",
	"do",
	"double",
	"dynamic_cast",
	"else",
	"enum",
	"explicit",
	"export",
	"extern",
	"float",
	"for",
	"friend",
	"goto",
	"if",
	"inline",
	"int",
	"int16_t",
	"int32_t",
	"int64_t",
	"long",
	"mutable",
	"namespace",
	"new",
	"noexcept",
	"not",
	"not_eq",
	"operator",
	"or",
	"or_eq",
	"private",
	"protected",
	"public",
	"register",
	"reinterpret_cast",
	"return",
	"short",
	"signed",
	"sizeof",
	"static",
	"static_cast",
	"struct",
	"switch",
	"template",
	"this",
	"throw",
	"thread_local",
	"try",
	"typedef",
	"typeid",
	"typename",
	"uint16_t",
	"uint32_t",
	"uint64_t",
	"uint8_t",
	"union",
	"unsigned",
	"what",
	"using",
	"virtual",
	"void",
	"volatile",
	"wchar_t",
	"while",
	"xor",
	"xor_eq"
};

bool CodeGenBase::is_keyword (const Identifier& id)
{
	return binary_search (protected_names_, std::end (protected_names_), id.c_str (), pred);
}

Code& operator << (Code& stm, const CodeGenBase::QName& qn)
{
	stm << CodeGenBase::ParentName (qn.item) << qn.item.name ();
	return stm;
}

Code& operator << (Code& stm, const CodeGenBase::ParentName& qn)
{
	const NamedItem* parent = qn.item.parent ();
	if (parent) {
		Item::Kind pk = parent->kind ();
		if (pk == Item::Kind::MODULE) {
			stm.namespace_prefix (static_cast <const Module*> (parent));
		} else {
			if (Item::Kind::INTERFACE == pk || Item::Kind::VALUE_TYPE == pk) {
				stm.namespace_prefix ("CORBA/Internal");
				stm << "Definitions <" << CodeGenBase::QName (*parent) << '>';
			} else {
				stm << CodeGenBase::QName (*parent);
			}
			stm << "::";
		}
	} else if (stm.cur_namespace ().empty ())
		stm << "::";
	return stm;
}

Code& operator << (Code& stm, const CodeGenBase::TypePrefix& t)
{
	stm.namespace_prefix ("CORBA/Internal");
	stm << "Type";
	return stm << " <" << t.type << ">::";
}

Code& operator << (Code& stm, const CodeGenBase::ABI_ret& t)
{
	if (t.type.tkind () == Type::Kind::VOID)
		stm << "void";
	else if (CodeGenBase::is_ref_type (t.type))
		stm << "Interface*";
	else if (CodeGenBase::is_enum (t.type))
		stm << "ABI_enum";
	else
		stm << CodeGenBase::TypePrefix (t.type) << (t.byref ? "ABI_VT_ret" : "ABI_ret");
	return stm;
}

Code& operator << (Code& stm, const CodeGenBase::ABI_param& t)
{
	if (CodeGenBase::is_ref_type (t.type)) {
		switch (t.att) {
			case Parameter::Attribute::IN:
				stm << "Interface*";
				break;
			case Parameter::Attribute::OUT:
			case Parameter::Attribute::INOUT:
				stm << "Interface**";
				break;
		}
	} else if (CodeGenBase::is_enum (t.type)) {
		switch (t.att) {
			case Parameter::Attribute::IN:
				stm << "ABI_enum";
				break;
			case Parameter::Attribute::OUT:
			case Parameter::Attribute::INOUT:
				stm << "ABI_enum*";
				break;
		}
	} else {
		stm << CodeGenBase::TypePrefix (t.type);
		switch (t.att) {
			case Parameter::Attribute::IN:
				stm << "ABI_in";
				break;
			case Parameter::Attribute::OUT:
			case Parameter::Attribute::INOUT:
				stm << "ABI_out";
				break;
		}
	}
	return stm;
}

Code& operator << (Code& stm, const CodeGenBase::Var& t)
{
	assert (t.type.tkind () != Type::Kind::VOID);
	return stm << CodeGenBase::TypePrefix (t.type) << "Var";
}

Code& operator << (Code& stm, const CodeGenBase::VRet& t)
{
	if (t.type.tkind () != Type::Kind::VOID)
		return stm << CodeGenBase::TypePrefix (t.type) << "VRet";
	else
		return stm << "void";
}

Code& operator << (Code& stm, const CodeGenBase::ConstRef& t)
{
	assert (t.type.tkind () != Type::Kind::VOID);
	return stm << CodeGenBase::TypePrefix (t.type) << "ConstRef";
}

Code& operator << (Code& stm, const CodeGenBase::TypeCodeName& t)
{
	return stm << CodeGenBase::ParentName (t.item) << "_tc_" << static_cast <const string&> (t.item.name ());
}

Code& operator << (Code& stm, const CodeGenBase::ServantParam& t)
{
	stm << CodeGenBase::TypePrefix (t.type);
	if (t.att == Parameter::Attribute::IN)
		stm << "ConstRef";
	else
		stm << "Var&";
	return stm;
}

Code& operator << (Code& stm, const CodeGenBase::ServantOp& op)
{
	stm << CodeGenBase::VRet (op.op) << ' ' << op.op.name () << " (";
	auto it = op.op.begin ();
	if (it != op.op.end ()) {
		stm << CodeGenBase::ServantParam (**it) << ' ' << (*it)->name ();
		++it;
		for (; it != op.op.end (); ++it) {
			stm << ", " << CodeGenBase::ServantParam (**it) << ' ' << (*it)->name ();
		}
	}
	return stm << ')';
}

Code& operator << (Code& stm, const CodeGenBase::Namespace& ns)
{
	stm.namespace_prefix (ns.prefix);
	return stm;
}

Code& operator << (Code& stm, const CodeGenBase::ItemNamespace& ns)
{
	stm.namespace_prefix (ns.item);
	return stm;
}

Code& operator << (Code& stm, const CodeGenBase::MemberType& t)
{
	if (CodeGenBase::is_boolean (t.type))
		stm << CodeGenBase::TypePrefix (t.type) << "ABI";
	else if (CodeGenBase::is_ref_type (t.type))
		stm << CodeGenBase::Var (t.type);
	else
		stm << t.type;
	return stm;
}

Code& operator << (Code& stm, const CodeGenBase::MemberVariable& m)
{
	return stm << CodeGenBase::MemberType (m.member)
		<< " _" << static_cast <const string&> (m.member.name ()) << ";\n";
}

Code& operator << (Code& stm, const CodeGenBase::Accessors& a)
{
	// const getter
	stm << CodeGenBase::ConstRef (a.member) << ' ' << a.member.name () << " () const\n"
		"{\n";
	stm.indent ();
	stm << "return _" << a.member.name () << ";\n";
	stm.unindent ();
	stm << "}\n";

	// reference getter
	stm << CodeGenBase::MemberType (a.member)
		<< "& " << a.member.name () << " ()\n"
		"{\n";
	stm.indent ();
	stm << "return _" << a.member.name () << ";\n";
	stm.unindent ();
	stm << "}\n";

	// setter
	stm << "void " << a.member.name () << " (" << CodeGenBase::TypePrefix (a.member) << "ConstRef val)\n"
		"{\n";
	stm.indent ();
	stm << '_' << a.member.name () << " = val;\n";
	stm.unindent ();
	stm << "}\n";

	if (CodeGenBase::is_var_len (a.member)) {
		// The move setter
		stm << "void " << a.member.name () << " (" << CodeGenBase::Var (a.member) << "&& val)\n"
			"{\n";
		stm.indent ();
		stm << '_' << a.member.name () << " = std::move (val);\n";
		stm.unindent ();
		stm << "}\n";
	}

	return stm;
}

Code& operator << (Code& stm, const CodeGenBase::MemberDefault& m)
{
	stm << m.prefix << m.member.name () << " (";
	const Type& td = m.member.dereference_type ();
	switch (td.tkind ()) {
		case Type::Kind::BASIC_TYPE:
			if (td.basic_type () == BasicType::BOOLEAN)
				stm << CodeGenBase::Namespace ("CORBA") << "FALSE";
			else if (td.basic_type () < BasicType::OBJECT)
				stm << "0";
			break;

		case Type::Kind::NAMED_TYPE: {
			const NamedItem& nt = td.named_type ();
			if (nt.kind () == Item::Kind::ENUM) {
				const Enum& en = static_cast <const Enum&> (nt);
				stm << CodeGenBase::QName (*en.front ());
			}
		} break;
	}
	return stm << ')';
}

Code& operator << (Code& stm, const CodeGenBase::MemberInit& m)
{
	stm << m.prefix << m.member.name () << " (std::move ("
		<< m.member.name () << "))";
	return stm;
}

CodeGenBase::Members CodeGenBase::get_members (const ItemContainer& cont, Item::Kind member_kind)
{
	Members ret;
	for (auto it = cont.begin (); it != cont.end (); ++it) {
		const Item& item = **it;
		if (item.kind () == member_kind)
			ret.push_back (&static_cast <const Member&> (item));
	}
	return ret;
}

bool CodeGenBase::is_var_len (const Type& type)
{
	const Type& t = type.dereference_type ();
	switch (t.tkind ()) {
		case Type::Kind::BASIC_TYPE:
			switch (t.basic_type ()) {
				case BasicType::ANY:
				case BasicType::OBJECT:
				case BasicType::VALUE_BASE:
					return true;
				default:
					return false;
			}

		case Type::Kind::STRING:
		case Type::Kind::WSTRING:
		case Type::Kind::SEQUENCE:
			return true;

		case Type::Kind::ARRAY:
			return is_var_len (t.array ());

		case Type::Kind::NAMED_TYPE: {
			const NamedItem& item = t.named_type ();
			switch (item.kind ()) {
				case Item::Kind::INTERFACE:
				case Item::Kind::INTERFACE_DECL:
				case Item::Kind::VALUE_TYPE:
				case Item::Kind::VALUE_TYPE_DECL:
				case Item::Kind::VALUE_BOX:
					return true;

				case Item::Kind::NATIVE:
					return is_native_interface (item);

				case Item::Kind::STRUCT:
					return is_var_len (get_members (static_cast <const Struct&> (item)));
					break;
				case Item::Kind::UNION:
					return is_var_len (get_members (static_cast <const Union&> (item)));
					break;
				default:
					return false;
			}
		}
	}
	return false;
}

bool CodeGenBase::is_var_len (const Members& members)
{
	for (auto member : members) {
		if (is_var_len (*member))
			return true;
	}
	return false;
}

bool CodeGenBase::may_have_check (const AST::Type& type)
{
	const Type& t = type.dereference_type ();
	return
		((Type::Kind::NAMED_TYPE == t.tkind () && t.named_type ().kind () == Item::Kind::UNION))
		|| is_var_len (t) || is_enum (t);
}

bool CodeGenBase::is_pseudo (const NamedItem& item)
{
	const NamedItem* p = &item;
	do {
		switch (p->kind ()) {
			case Item::Kind::INTERFACE: {
				const Interface& itf = static_cast <const Interface&> (*p);
				if (itf.interface_kind () == InterfaceKind::PSEUDO)
					return true;
			} break;

			case Item::Kind::INTERFACE_DECL: {
				const InterfaceDecl& itf = static_cast <const InterfaceDecl&> (*p);
				if (itf.interface_kind () == InterfaceKind::PSEUDO)
					return true;
			} break;

			case Item::Kind::EXCEPTION:
			case Item::Kind::MODULE:
				return false;
		}
		p = p->parent ();
	} while (p);
	return false;
}

bool CodeGenBase::is_ref_type (const Type& type)
{
	const Type& t = type.dereference_type ();
	switch (t.tkind ()) {
		case Type::Kind::BASIC_TYPE:
			switch (t.basic_type ()) {
				case BasicType::OBJECT:
				case BasicType::VALUE_BASE:
					return true;
			} break;
		case Type::Kind::NAMED_TYPE:
			switch (t.named_type ().kind ()) {
				case Item::Kind::INTERFACE_DECL:
				case Item::Kind::INTERFACE:
				case Item::Kind::VALUE_TYPE_DECL:
				case Item::Kind::VALUE_TYPE:
					return true;

				case Item::Kind::NATIVE:
					return is_native_interface (t.named_type ());
			} break;
	}
	return false;
}

bool CodeGenBase::is_native_interface (const Type& type)
{
	const Type& t = type.dereference_type ();
	return t.tkind () == Type::Kind::NAMED_TYPE
		&& t.named_type ().kind () == Item::Kind::NATIVE
		&& is_native_interface (t.named_type ());
}

bool CodeGenBase::is_native_interface (const NamedItem& type)
{
	assert (type.kind () == Item::Kind::NATIVE);
	if (type.name () == "Interface") {
		const NamedItem* parent = type.parent ();
		return parent && parent->kind () == Item::Kind::MODULE && parent->name () == "Internal";
	}
	return false;
}

bool CodeGenBase::is_native (const Type& type)
{
	const Type& t = type.dereference_type ();
	if (t.tkind () == Type::Kind::NAMED_TYPE)
		return t.named_type ().kind () == Item::Kind::NATIVE;
	else
		return false;
}

bool CodeGenBase::is_enum (const Type& type)
{
	const Type& t = type.dereference_type ();
	return t.tkind () == Type::Kind::NAMED_TYPE && t.named_type ().kind () == Item::Kind::ENUM;
}

bool CodeGenBase::is_boolean (const AST::Type& t)
{
	const Type& dt = t.dereference_type ();
	return dt.tkind () == Type::Kind::BASIC_TYPE && dt.basic_type () == BasicType::BOOLEAN;
}

void CodeGenBase::leaf (const UnionDecl&)
{
	throw runtime_error ("Not yet implemented");
}

void CodeGenBase::begin (const Union&)
{
	throw runtime_error ("Not yet implemented");
}

void CodeGenBase::end (const Union&)
{
	throw runtime_error ("Not yet implemented");
}

void CodeGenBase::leaf (const UnionElement&)
{
	throw runtime_error ("Not yet implemented");
}

void CodeGenBase::leaf (const StateMember&)
{
}

void CodeGenBase::leaf (const ValueFactory&)
{
}

void CodeGenBase::leaf (const ValueBox&)
{
	throw runtime_error ("Not yet implemented");
}

CodeGenBase::StateMembers CodeGenBase::get_members (const ValueType& cont)
{
	StateMembers ret;
	for (auto it = cont.begin (); it != cont.end (); ++it) {
		const Item& item = **it;
		if (item.kind () == Item::Kind::STATE_MEMBER)
			ret.push_back (&static_cast <const StateMember&> (item));
	}
	return ret;
}

CodeGenBase::Bases CodeGenBase::get_all_bases (const ValueType& vt)
{
	Bases bvec;
	{
		unordered_set <const ItemContainer*> bset;
		get_all_bases (vt, bset, bvec);
	}
	const Interface* itf = get_concrete_supports (vt);
	if (itf) {
		Interfaces exclude = itf->get_all_bases ();
		for (auto it = bvec.begin (); it != bvec.end ();) {
			if (find (exclude.begin (), exclude.end (), *it) != exclude.end ())
				bvec.erase (it);
			else
				++it;
		}
	}
	return bvec;
}

void CodeGenBase::get_all_bases (const ValueType& vt,
	unordered_set <const ItemContainer*>& bset, Bases& bvec)
{
	for (auto pb : vt.bases ()) {
		if (bset.insert (pb).second) {
			// Derived must be first
			get_all_bases (*pb, bset, bvec);
			bvec.push_back (pb);
		}
	}
	if (!vt.supports ().empty ()) {
		auto it = vt.supports ().begin ();
		if ((*it)->interface_kind () != InterfaceKind::ABSTRACT)
			++it;
		for (; it != vt.supports ().end (); ++it) {
			const Interface* pai = *it;
			assert (pai->interface_kind () == InterfaceKind::ABSTRACT);
			if (bset.insert (pai).second) {
				bvec.push_back (pai);
				get_all_bases (*pai, bset, bvec);
			}
		}
	}
}

void CodeGenBase::get_all_bases (const Interface& ai,
	unordered_set <const ItemContainer*>& bset, Bases& bvec)
{
	for (auto pb : ai.bases ()) {
		if (bset.insert (pb).second) {
			bvec.push_back (pb);
			get_all_bases (*pb, bset, bvec);
		}
	}
}

const Interface* CodeGenBase::get_concrete_supports (const ValueType& vt)
{
	if (!vt.supports ().empty () && vt.supports ().front ()->interface_kind () != InterfaceKind::ABSTRACT)
		return vt.supports ().front ();
	for (auto pb : vt.bases ()) {
		const Interface* itf = get_concrete_supports (*pb);
		if (itf)
			return itf;
	}
	return nullptr;
}

