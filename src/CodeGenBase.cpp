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
#include <algorithm>
#include "CodeGenBase.h"

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
	return std::binary_search (protected_names_, std::end (protected_names_), id.c_str (), pred);
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
					return is_var_len (static_cast <const Struct&> (item));

				case Item::Kind::UNION:
					return is_var_len (static_cast <const Union&> (item));

				default:
					return false;
			}
		}
	}
	return false;
}

bool CodeGenBase::is_CDR (const Type& type, SizeAndAlign& sa)
{
	const Type& t = type.dereference_type ();
	switch (t.tkind ()) {
		case Type::Kind::BASIC_TYPE:
			switch (t.basic_type ()) {
				case BasicType::BOOLEAN:
				case BasicType::OCTET:
				case BasicType::CHAR:
					return sa.append (1, 1);

				case BasicType::USHORT:
				case BasicType::SHORT:
					return sa.append (2, 2);

				case BasicType::ULONG:
				case BasicType::LONG:
				case BasicType::FLOAT:
					return sa.append (4, 4);

				case BasicType::ULONGLONG:
				case BasicType::LONGLONG:
				case BasicType::DOUBLE:
					return sa.append (8, 8);

				case BasicType::LONGDOUBLE:
					return sa.append (8, 16);
			}
			break;

		case Type::Kind::FIXED:
			sa.append (1, (t.fixed_digits () + 2) / 2);
			return true;

		case Type::Kind::ARRAY: {
			const Array& ar = t.array ();
			auto cnt = 1;
			for (auto d : ar.dimensions ()) {
				cnt *= d;
			}
			for (; cnt; --cnt) {
				if (!is_CDR (ar, sa))
					break;
			}
			if (!cnt)
				return true;
		} break;

		case Type::Kind::NAMED_TYPE: {
			const NamedItem& item = t.named_type ();
			switch (item.kind ()) {
				case Item::Kind::STRUCT:
					if (is_CDR (static_cast <const Struct&> (item), sa))
						return true;
					break;

				case Item::Kind::ENUM:
					return sa.append (4, 4);
			}
		}
	}
	sa.invalidate ();
	return false;
}

bool CodeGenBase::is_var_len (const Members& members)
{
	for (const auto& member : members) {
		if (is_var_len (*member))
			return true;
	}
	return false;
}

bool CodeGenBase::is_CDR (const Members& members, SizeAndAlign& al)
{
	for (const auto& member : members) {
		if (!is_CDR (*member, al))
			return false;
	}
	return true;
}

bool CodeGenBase::is_bounded (const AST::Type& type)
{
	const Type& t = type.dereference_type ();
	switch (t.tkind ()) {
		case Type::Kind::SEQUENCE:
			return t.sequence ().bound () != 0;
		case Type::Kind::STRING:
		case Type::Kind::WSTRING:
			return t.string_bound () != 0;
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

			case Item::Kind::TYPE_DEF: {
				const Type* t = &static_cast <const TypeDef&> (*p).dereference_type ();
				switch (t->tkind ()) {
					case Type::Kind::SEQUENCE:
						t = &t->sequence ().dereference_type ();
						break;
					case Type::Kind::ARRAY:
						t = &t->array ().dereference_type ();
						break;
				}
				if (t->tkind () == Type::Kind::NAMED_TYPE && is_pseudo (t->named_type ()))
					return true;
			} break;

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
			}
			break;

		case Type::Kind::NAMED_TYPE: {
			const NamedItem& item = t.named_type ();
			switch (item.kind ()) {
				case Item::Kind::INTERFACE_DECL:
				case Item::Kind::INTERFACE:
				case Item::Kind::VALUE_TYPE_DECL:
				case Item::Kind::VALUE_TYPE:
				case Item::Kind::VALUE_BOX:
					return true;

				case Item::Kind::NATIVE:
					return is_native_interface (type)
						|| is_servant (type);
			}
		} break;
	}
	return false;
}

bool CodeGenBase::is_complex_type(const Type& type)
{
	const Type& t = type.dereference_type();
	switch (t.tkind()) {
		case Type::Kind::BASIC_TYPE:
			switch (t.basic_type()) {
				case BasicType::OBJECT:
				case BasicType::VALUE_BASE:
				case BasicType::ANY:
					return true;
			}
			break;

		case Type::Kind::NAMED_TYPE: {
			const NamedItem& item = t.named_type();
			switch (item.kind()) {
				case Item::Kind::INTERFACE:
				case Item::Kind::VALUE_TYPE:
				case Item::Kind::VALUE_BOX:
					return true;

				case Item::Kind::STRUCT:
				case Item::Kind::UNION: {
					const StructBase& sb = static_cast <const StructBase&> (item);
					for (const Member* m : sb) {
						if (is_complex_type (*m))
							return true;
					}
				}
			}
		} break;
	}
	return false;
}

bool CodeGenBase::is_object (const AST::Type& type)
{
	const Type& t = type.dereference_type ();
	if (t.tkind () == Type::Kind::NAMED_TYPE) {
		const NamedItem& item = t.named_type ();
		InterfaceKind ik (InterfaceKind::ABSTRACT);
		switch (item.kind ()) {
		case Item::Kind::INTERFACE:
			ik = static_cast <const Interface&> (item).interface_kind ();
			break;
		case Item::Kind::INTERFACE_DECL:
			ik = static_cast <const InterfaceDecl&> (item).interface_kind ();
			break;
		}
		return ik.interface_kind () == InterfaceKind::UNCONSTRAINED
			|| ik.interface_kind () == InterfaceKind::LOCAL;
	}
	return false;
}

bool CodeGenBase::is_native_interface (const Type& type)
{
	const Type& t = type.dereference_type ();
	if (t.tkind () == Type::Kind::NAMED_TYPE) {
		const NamedItem& item = t.named_type ();
		return item.kind () == Item::Kind::NATIVE && is_native_interface (item);
	}
	return false;
}

bool CodeGenBase::is_servant (const Type& type)
{
	const Type& t = type.dereference_type ();
	if (t.tkind () == Type::Kind::NAMED_TYPE) {
		const NamedItem& item = t.named_type ();
		return item.kind () == Item::Kind::NATIVE && is_servant (item);
	}
	return false;
}

bool CodeGenBase::is_native_interface (const NamedItem& type)
{
	assert (type.kind () == Item::Kind::NATIVE);
	if (type.name () == "Interface") {
		const NamedItem* parent = type.parent ();
		if (parent && parent->kind () == Item::Kind::MODULE
			&& parent->name () == "Internal") {
			parent = parent->parent ();
			return parent && parent->name () == "CORBA" && !parent->parent ();
		}
	}
	return false;
}

bool CodeGenBase::is_servant (const NamedItem& type)
{
	assert (type.kind () == Item::Kind::NATIVE);
	if (type.name () == "Servant") {
		const NamedItem* parent = type.parent ();
		return parent && parent->kind () == Item::Kind::MODULE
			&& parent->name () == "PortableServer" && !parent->parent ();
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

bool CodeGenBase::is_native (const Members& members)
{
	for (const auto& member : members) {
		if (is_native (*member))
			return true;
	}
	return false;
}

bool CodeGenBase::is_native (const Raises& raises)
{
	for (const auto& ex : raises) {
		if (ex->kind () == Item::Kind::NATIVE)
			return true;
	}
	return false;
}

const AST::Enum* CodeGenBase::is_enum (const Type& type)
{
	const Type& t = type.dereference_type ();
	if (t.tkind () == Type::Kind::NAMED_TYPE) {
		const auto& nt = t.named_type ();
		if (nt.kind () == Item::Kind::ENUM)
			return &static_cast <const Enum&> (nt);
	}
	return nullptr;
}

bool CodeGenBase::is_boolean (const Type& type)
{
	const Type& t = type.dereference_type ();
	return t.tkind () == Type::Kind::BASIC_TYPE && t.basic_type () == BasicType::BOOLEAN;
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
		std::unordered_set <const IV_Base*> bset;
		get_all_bases (vt, bset, bvec);
	}
	const Interface* itf = get_concrete_supports (vt);
	if (itf) {
		Interfaces exclude = itf->get_all_bases ();
		for (auto it = bvec.begin (); it != bvec.end ();) {
			if (find (exclude.begin (), exclude.end (), *it) != exclude.end ())
				it = bvec.erase (it);
			else
				++it;
		}
	}
	return bvec;
}

void CodeGenBase::get_all_bases (const ValueType& vt,
	std::unordered_set <const IV_Base*>& bset, Bases& bvec)
{
	for (auto pb : vt.bases ()) {
		if (bset.insert (pb).second) {
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
				get_all_bases (*pai, bset, bvec);
				bvec.push_back (pai);
			}
		}
	}
}

void CodeGenBase::get_all_bases (const Interface& ai,
	std::unordered_set <const IV_Base*>& bset, Bases& bvec)
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
	if (!vt.supports ().empty ()) {
		const Interface* itf = vt.supports ().front ();
		if (itf->interface_kind () != InterfaceKind::ABSTRACT)
			return itf;
	}
	for (auto pb : vt.bases ()) {
		const Interface* itf = get_concrete_supports (*pb);
		if (itf)
			return itf;
	}
	return nullptr;
}

CodeGenBase::Factories CodeGenBase::get_factories (const ValueType& vt)
{
	Factories factories;
	for (auto it = vt.begin (); it != vt.end (); ++it) {
		const Item& item = **it;
		if (item.kind () == Item::Kind::VALUE_FACTORY)
			factories.push_back (&static_cast <const ValueFactory&> (item));
	}
	return factories;
}

bool CodeGenBase::has_factories (const AST::ValueType& vt)
{
	for (auto it = vt.begin (); it != vt.end (); ++it) {
		const Item& item = **it;
		if (item.kind () == Item::Kind::VALUE_FACTORY)
			return true;
	}
	return false;
}

void CodeGenBase::init_union (Code& stm, const UnionElement& init_el,
	const char* prefix)
{
	const Enum* en = is_enum (init_el);
	if (en)
		stm << prefix << "_u." << init_el.name () << " = " << QName (*en) << "::"
		<< QName (*(*en).front ()) << ";\n";
	else
		stm << Namespace ("CORBA/Internal") << "construct (" << prefix << "_u."
		<< init_el.name () << ");\n";
}

void CodeGenBase::marshal_member (Code& stm, const Member& m, const char* func, const char* prefix)
{
	stm << TypePrefix (m) << func << " (" << prefix;
	if (stm.last_char () == '_')
		stm << static_cast <const std::string&> (m.name ()); // No check for C++ keywords
	else
		stm << m.name ();
	stm << ", rq);\n";
}

void CodeGenBase::marshal_members (Code& stm, const Members& members, const char* func, const char* prefix)
{
	stm.indent ();

	for (Members::const_iterator m = members.begin (); m != members.end ();) {

		// Marshal CDR members
		SizeAndAlign al;
		if (is_CDR (**m, al)) {
			do {
				auto begin = m;
				while (members.end () != ++m) {
					if (!is_CDR (**m, al))
						break;
				}
				if (m > begin + 1) {
					stm << "marshal_members (&" << prefix << (*begin)->name () << ", ";
					stm << "&" << prefix << (*(m - 1))->name ();
					stm << ", rq);\n";
				} else
					marshal_member (stm, **begin, func, prefix);
			} while (m != members.end () && al.is_valid ());
		}

		// Marshal non-CDR member
		if (m != members.end ()) {
			marshal_member (stm, **m, func, prefix);
			++m;
		}
	}

	stm.unindent ();
}

void CodeGenBase::unmarshal_member (Code& stm, const Member& m, const char* prefix)
{
	stm << TypePrefix (m) << "unmarshal (rq, " << prefix;
	if (stm.last_char () == '_')
		stm << static_cast <const std::string&> (m.name ()); // No check for C++ keywords
	else
		stm << m.name ();
	stm << ");\n";
}

void CodeGenBase::unmarshal_members (Code& stm, const Members& members, const char* prefix)
{
	stm.indent ();
	for (Members::const_iterator m = members.begin (); m != members.end ();) {

		// Marshal CDR members
		SizeAndAlign al;
		if (is_CDR (**m, al)) {
			do {
				auto begin = m;
				while (members.end () != ++m) {
					if (!is_CDR (**m, al))
						break;
				}
				auto end = m;

				if (end > begin + 1) {
					stm << "if (unmarshal_members (rq, &" << prefix << (*begin)->name ()
						<< ", &" << prefix << (*(end - 1))->name ()
						<< ")) {\n"
						<< indent;

					// Swap bytes
					m = begin;
					do {
						stm << TypePrefix (**m) << "byteswap (" << prefix << (*m)->name () << ");\n";
					} while (end != ++m);
					stm << unindent
						<< "}\n";

					// If some members have check, check them
					m = begin;
					do {
						stm << TypePrefix (**m) << "check ((const " << TypePrefix (**m) << "ABI&)" << prefix << (*m)->name () << ");\n";
					} while (end != ++m);
				} else
					unmarshal_member (stm, **begin, prefix);
			} while (m != members.end () && al.is_valid ());
		}

		// Unmarshal non-CDR member
		if (m != members.end ()) {
			unmarshal_member (stm, **m, prefix);
			++m;
		}
	}

	stm.unindent ();
}

bool CodeGenBase::is_special_base (const Interface& itf) noexcept
{
	const ItemScope* parent = itf.parent ();
	if (parent && !parent->parent ()) {
		if (parent->name () == "CORBA")
			return itf.name () == "Policy" || itf.name () == "Current";
	}
	return false;
}

bool CodeGenBase::is_immutable (const Interface& itf) noexcept
{
	const ItemScope* parent = itf.parent ();
	if (parent && !parent->parent ()) {
		if (parent->name () == "CosTime")
			return itf.name () == "TIO" || itf.name () == "UTO";
		else if (parent->name () == "CORBA")
			return itf.name () == "DomainManager";
	}
	return false;
}

bool CodeGenBase::is_stateless (const Interface& itf) noexcept
{
	bool stateless = is_special_base (itf) || is_immutable (itf);
	if (!stateless) {
		Interfaces bases = itf.get_all_bases ();
		for (auto base : bases) {
			if (is_special_base (*base)) {
				stateless = true;
				break;
			}
		}
	}
	return stateless;
}

bool CodeGenBase::is_custom (const Operation& op) noexcept
{
	bool custom = is_native (op);
	if (!custom) {
		for (auto par : op) {
			if (is_native (*par)) {
				custom = true;
				break;
			}
		}
	}
	return custom;
}

bool CodeGenBase::is_custom (const Interface& itf) noexcept
{
	for (const Item* item : itf) {
		switch (item->kind ()) {
			case Item::Kind::OPERATION:
				if (is_custom (static_cast <const Operation&> (*item)))
					return true;
				break;
			case Item::Kind::ATTRIBUTE:
				if (is_native (static_cast <const Attribute&> (*item)))
					return true;
				break;
		}
	}
	return false;
}

bool CodeGenBase::is_named_type (const AST::Type& type, const char* qualified_name)
{
	const Type& t = type.dereference_type ();
	return t.tkind () == Type::Kind::NAMED_TYPE
		&& t.named_type ().qualified_name () == qualified_name;
}

bool CodeGenBase::async_supported (const Interface& itf) noexcept
{
	if (itf.interface_kind () == InterfaceKind::UNCONSTRAINED) {

		// Do not generate AMI for Policies and other immutable, and also for custom.
		if (is_stateless (itf) || is_custom (itf))
			return false;

		// Do not generate AMI for Messaging::ReplyHandler
		if (itf.qualified_name () == "::Messaging::ReplyHandler")
			return false;

		// Do not generate AMI for empty interfaces
		for (auto item : itf) {
			switch (item->kind ()) {
			case Item::Kind::OPERATION:
			case Item::Kind::ATTRIBUTE:
				return true;
			}
		}
	}
	return false;
}

std::string CodeGenBase::const_id (const Constant& c)
{
	ScopedName sn = c.scoped_name ();
	ScopedName::const_iterator n = sn.begin ();
	std::string name = *n;
	for (++n; n != sn.end (); ++n) {
		name += '/';
		name += *n;
	}
	return name;
}

std::string CodeGenBase::skip_prefix (const AST::Identifier& id, const char* prefix)
{
	size_t cc = strlen (prefix);
	if (id.length () > cc && id.starts_with (prefix))
		return id.substr (cc);
	else
		return std::string ();
}

bool CodeGenBase::is_component (const AST::Interface& itf) noexcept
{
	Interfaces bases = itf.get_all_bases ();
	for (const auto b : bases) {
		if (b->qualified_name () == "::Components::CCMObject") {
			return true;
			break;
		}
	}
	return false;
}

bool CodeGenBase::SizeAndAlign::append (unsigned member_align, unsigned member_size) noexcept
{
	assert (1 <= member_align && member_align <= 8);
	assert (member_size);
	assert (is_valid ());

	if (!size) {
		if (alignment < member_align)
			alignment = member_align;
		size = member_size;
		return true;
	}

	if (alignment < member_align) {
		// Gap may be occur here ocassionally, depending on the real alignment.
		// We must break here.
		alignment = member_align;
		size = member_size;
		return false;
	}

	size = (size + member_align - 1) / member_align * member_align + member_size;
	return true;
}

