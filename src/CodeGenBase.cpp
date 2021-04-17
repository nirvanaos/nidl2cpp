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

const char CodeGenBase::internal_namespace_ [] =
"namespace CORBA {\n"
"namespace Nirvana {\n";

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
			if (stm.module_namespace () != parent) {
				stm << CodeGenBase::QName (*parent);
				stm << "::";
			}
		} else {
			if (Item::Kind::INTERFACE == pk || Item::Kind::VALUE_TYPE == pk) {
				if (stm.spec_namespace () != CodeGenBase::internal_namespace_) {
					if (!stm.no_namespace ())
						stm << "::";
					stm << "CORBA::Nirvana::";
				}
				stm << "Definitions <" << CodeGenBase::QName (*parent) << '>';
			} else {
				stm << CodeGenBase::QName (*parent);
			}
			stm << "::";
		}
	} else if (!stm.no_namespace ())
		stm << "::";
	return stm;
}

Code& operator << (Code& stm, const CodeGenBase::TypePrefix& t)
{
	if (stm.spec_namespace () != CodeGenBase::internal_namespace_) {
		if (stm.module_namespace ())
			stm << "::";
		stm << "CORBA::Nirvana::";
	}
	stm << "Type";
	if (CodeGenBase::is_ref_type (t.type))
		stm << "Itf";
	return stm << " <" << t.type << ">::";
}

Code& operator << (Code& stm, const CodeGenBase::ABI_ret& t)
{
	if (t.type.tkind () != Type::Kind::VOID)
		return stm << CodeGenBase::TypePrefix (t.type) << "ABI_ret";
	else
		return stm << "void";
}

Code& operator << (Code& stm, const CodeGenBase::ABI_param& t)
{
	stm << CodeGenBase::TypePrefix (t.type);
	switch (t.att) {
		case Parameter::Attribute::IN:
			stm << "ABI_in";
			break;
		case Parameter::Attribute::OUT:
			stm << "ABI_out";
			break;
		case Parameter::Attribute::INOUT:
			stm << "ABI_inout";
			break;
	}
	return stm;
}

Code& operator << (Code& stm, const CodeGenBase::C_param& t)
{
	stm << CodeGenBase::TypePrefix (t.type);
	switch (t.att) {
		case Parameter::Attribute::IN:
			stm << "C_in";
			break;
		case Parameter::Attribute::OUT:
			stm << "C_out";
			break;
		case Parameter::Attribute::INOUT:
			stm << "C_inout";
			break;
	}
	return stm;
}

Code& operator << (Code& stm, const CodeGenBase::Var_type& t)
{
	if (t.type.tkind () != Type::Kind::VOID)
		return stm << CodeGenBase::TypePrefix (t.type) << "Var_type";
	else
		return stm << "void";
}

Code& operator << (Code& stm, const CodeGenBase::ClientOp& op)
{
	stm << op.op.name () << " (";

	auto it = op.op.begin ();
	if (it != op.op.end ()) {
		stm << CodeGenBase::C_param (**it);
		if (op.names)
			stm << ' ' << (*it)->name ();
		++it;
		for (; it != op.op.end (); ++it) {
			stm << ", " << CodeGenBase::C_param (**it);
			if (op.names)
				stm << ' ' << (*it)->name ();
		}
	}

	return stm << ")";
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
	if (t.tkind () == Type::Kind::NAMED_TYPE) {
		switch (t.named_type ().kind ()) {
			case Item::Kind::INTERFACE_DECL:
			case Item::Kind::INTERFACE:
			case Item::Kind::VALUE_TYPE_DECL:
			case Item::Kind::VALUE_TYPE:
				return true;
		}
	}
	return false;
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

void CodeGenBase::leaf (const ValueTypeDecl&)
{
	throw runtime_error ("Not yet implemented");
}

void CodeGenBase::begin (const ValueType&)
{
	throw runtime_error ("Not yet implemented");
}

void CodeGenBase::end (const ValueType&)
{
	throw runtime_error ("Not yet implemented");
}

void CodeGenBase::leaf (const StateMember&)
{
	throw runtime_error ("Not yet implemented");
}

void CodeGenBase::leaf (const ValueFactory&)
{
	throw runtime_error ("Not yet implemented");
}

void CodeGenBase::leaf (const ValueBox&)
{
	throw runtime_error ("Not yet implemented");
}

