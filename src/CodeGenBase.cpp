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
			stm.namespace_prefix (parent);
		} else {
			if (Item::Kind::INTERFACE == pk || Item::Kind::VALUE_TYPE == pk) {
				stm.namespace_prefix ("CORBA/Nirvana");
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
	stm.namespace_prefix ("CORBA/Nirvana");
	stm << "Type";
	if (CodeGenBase::is_ref_type (t.type))
		stm << "Itf";
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
		stm << CodeGenBase::TypePrefix (t.type) << "ABI_ret";
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
	if (t.type.tkind () != Type::Kind::VOID)
		return stm << CodeGenBase::TypePrefix (t.type) << "Var";
	else
		return stm << "void";
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
	stm << CodeGenBase::Var (op.op) << ' ' << op.op.name () << " (";
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
		return parent && parent->kind () == Item::Kind::MODULE && parent->name () == "Nirvana";
	}
	return false;
}

bool CodeGenBase::is_enum (const Type& type)
{
	const Type& t = type.dereference_type ();
	return t.tkind () == Type::Kind::NAMED_TYPE && t.named_type ().kind () == Item::Kind::ENUM;
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

