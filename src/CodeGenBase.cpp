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

bool CodeGenBase::is_CDR (const Type& type)
{
	const Type& t = type.dereference_type ();
	switch (t.tkind ()) {
		case Type::Kind::BASIC_TYPE:
			switch (t.basic_type ()) {
				case BasicType::ANY:
				case BasicType::OBJECT:
				case BasicType::VALUE_BASE:
				case BasicType::WCHAR:
					return false;
				default:
					return true;
			}

		case Type::Kind::STRING:
		case Type::Kind::WSTRING:
		case Type::Kind::SEQUENCE:
			return false;

		case Type::Kind::ARRAY:
			return is_CDR (t.array ());

		case Type::Kind::NAMED_TYPE: {
			const NamedItem& item = t.named_type ();
			switch (item.kind ()) {
				case Item::Kind::INTERFACE:
				case Item::Kind::INTERFACE_DECL:
				case Item::Kind::VALUE_TYPE:
				case Item::Kind::VALUE_TYPE_DECL:
				case Item::Kind::VALUE_BOX:
				case Item::Kind::NATIVE:
				case Item::Kind::UNION:
					return false;

				case Item::Kind::STRUCT:
					return is_CDR (static_cast <const Struct&> (item));

				default:
					return true;
			}
		}
	}
	return true;
}

bool CodeGenBase::is_var_len (const Members& members)
{
	for (const auto& member : members) {
		if (is_var_len (*member))
			return true;
	}
	return false;
}

bool CodeGenBase::is_CDR (const Members& members)
{
	for (const auto& member : members) {
		if (!is_CDR (*member))
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

bool CodeGenBase::is_aligned_struct (const Type& type)
{
	// TODO: Check for CDR size == native size
	const Type& t = type.dereference_type ();
	return t.tkind () == Type::Kind::NAMED_TYPE && t.named_type ().kind () == Item::Kind::STRUCT;
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
		// Marshal variable-length members
		while (!is_CDR (**m) && !is_aligned_struct (**m)) {
			marshal_member (stm, **m, func, prefix);
			if (members.end () == ++m)
				break;
		}
		if (m != members.end ()) {
			// Marshal fixed-length members
			auto begin = m;
			const Member* last;
			do {
				last = *m;
				++m;
			} while (m != members.end () && is_CDR (**m) && !is_aligned_struct (*last));

			if (m > begin + 1) {
				stm << "marshal_members (&" << prefix << (*begin)->name () << ", ";
				stm << "&" << prefix << (*(m - 1))->name ();
				stm << ", rq);\n";
			} else
				marshal_member (stm, **begin, func, prefix);
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
		// Unmarshal variable-length members
		while (!is_CDR (**m) && !is_aligned_struct (**m)) {
			unmarshal_member (stm , **m, prefix);
			if (members.end () == ++m)
				break;
		}
		if (m != members.end ()) {
			// Unmarshal fixed-length members
			auto begin = m;
			const Member* last;
			do {
				last = *m;
				++m;
			} while (m != members.end () && is_CDR (**m) && !is_aligned_struct (*last));
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

bool CodeGenBase::async_supported (const Interface& itf) noexcept
{
	if (itf.interface_kind () == InterfaceKind::UNCONSTRAINED) {
		//if (is_stateless (itf) || is_custom (itf))
		//	return false;
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

Code& operator << (Code& stm, const QName& qn)
{
	return stm << ParentName (qn.item) << qn.item.name ();
}

Code& operator << (Code& stm, const ParentName& qn)
{
	const NamedItem* parent = qn.item.parent ();
	if (parent) {
		Item::Kind pk = parent->kind ();
		if (pk == Item::Kind::MODULE) {
			stm.namespace_prefix (static_cast <const Module*> (parent));
		} else {
			if (Item::Kind::INTERFACE == pk || Item::Kind::VALUE_TYPE == pk) {
				stm.namespace_prefix ("CORBA/Internal");
				stm << "Decls <" << QName (*parent) << '>';
			} else {
				stm << QName (*parent);
			}
			stm << "::";
		}
	} else if (!stm.cur_namespace ().empty ())
		stm << "::";
	return stm;
}

Code& operator << (Code& stm, const TypePrefix& t)
{
	return stm << Namespace ("CORBA/Internal") << "Type" << " <" << t.type << ">::";
}

Code& operator << (Code& stm, const ABI_ret& t)
{
	if (t.type.tkind () == Type::Kind::VOID)
		stm << "void";
	else if (CodeGenBase::is_ref_type (t.type))
		stm << "Interface*";
	else if (CodeGenBase::is_enum (t.type))
		stm << "ABI_enum";
	else
		stm << TypePrefix (t.type) << (t.byref ? "ABI_VT_ret" : "ABI_ret");
	return stm;
}

Code& operator << (Code& stm, const ABI_param& t)
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
		stm << TypePrefix (t.type);
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

Code& operator << (Code& stm, const Var& t)
{
	assert (t.type.tkind () != Type::Kind::VOID);
	return stm << TypePrefix (t.type) << "Var";
}

Code& operator << (Code& stm, const VRet& t)
{
	if (t.type.tkind () != Type::Kind::VOID)
		return stm << TypePrefix (t.type) << "VRet";
	else
		return stm << "void";
}

Code& operator << (Code& stm, const ConstRef& t)
{
	assert (t.type.tkind () != Type::Kind::VOID);
	return stm << TypePrefix (t.type) << "ConstRef";
}

Code& operator << (Code& stm, const TC_Name& t)
{
	return stm << ParentName (t.item) << "_tc_" << static_cast <const std::string&> (t.item.name ());
}

Code& operator << (Code& stm, const ServantParam& t)
{
	if (t.att == Parameter::Attribute::IN) {
		if (t.virt && !CodeGenBase::is_ref_type (t.type)) {
			// For virtual methods (POA implementation), the type must not depend on
			// machine word size.
			// In parameters for all basic types, except for Any, are passed by value.
			// Other types are passed by reference.
			const Type& dt = t.type.dereference_type ();
			if (dt.tkind () == Type::Kind::BASIC_TYPE && dt.basic_type () != BasicType::ANY)
				stm << TypePrefix (t.type) << "Var";
			else
				stm << "const " << TypePrefix (t.type) << "Var&";
		} else
			stm << TypePrefix (t.type) << "ConstRef";
	} else
		stm << TypePrefix (t.type) << "Var&";
	return stm;
}

Code& operator << (Code& stm, const ServantOp& op)
{
	stm << VRet (op.op) << ' ' << op.op.name () << " (";
	auto it = op.op.begin ();
	if (it != op.op.end ()) {
		stm << ServantParam (**it, op.virt) << ' ' << (*it)->name ();
		++it;
		for (; it != op.op.end (); ++it) {
			stm << ", " << ServantParam (**it, op.virt) << ' ' << (*it)->name ();
		}
	}
	return stm << ')';
}

Code& operator << (Code& stm, const MemberType& t)
{
	if (CodeGenBase::is_boolean (t.type))
		stm << TypePrefix (t.type) << "ABI";
	else if (CodeGenBase::is_ref_type (t.type))
		stm << Var (t.type);
	else
		stm << t.type;
	return stm;
}

Code& operator << (Code& stm, const MemberVariable& m)
{
	return stm << MemberType (m.member)
		<< " _" << static_cast <const std::string&> (m.member.name ()) << ";\n";
}

Code& operator << (Code& stm, const Accessors& a)
{
	// const getter
	stm << ConstRef (a.member) << ' ' << a.member.name () << " () const\n"
		"{\n" << indent <<
		"return _" << static_cast <const std::string&> (a.member.name ()) << ";\n"
		<< unindent << "}\n";

	if (a.member.kind () != Item::Kind::STATE_MEMBER
		|| !static_cast <const StateMember&> (a.member).is_public ()) {
		// reference getter
		stm << MemberType (a.member)
			<< "& " << a.member.name () << " ()\n"
			"{\n" << indent <<
			"return _" << static_cast <const std::string&> (a.member.name ()) << ";\n"
			<< unindent << "}\n";
	}

	// setter
	if (CodeGenBase::is_ref_type (a.member)) {
		stm << "void " << a.member.name () << " (" << Var (a.member) << " val)\n"
			"{\n" << indent <<
			'_' << static_cast <const std::string&> (a.member.name ()) << " = std::move (val);\n"
			<< unindent << "}\n";
	} else {
		stm << "void " << a.member.name () << " (" << ConstRef (a.member) << " val)\n"
			"{\n" << indent <<
			'_' << static_cast <const std::string&> (a.member.name ()) << " = val;\n"
			<< unindent << "}\n";

		if (CodeGenBase::is_var_len (a.member)) {
			// The move setter
			stm << "void " << a.member.name () << " (" << Var (a.member) << "&& val)\n"
				"{\n" << indent <<
				'_' << static_cast <const std::string&> (a.member.name ()) << " = std::move (val);\n"
				<< unindent << "}\n";
		}
	}

	return stm;
}

Code& operator << (Code& stm, const MemberDefault& m)
{
	stm << m.prefix << m.member.name () << " (";
	const Type& td = m.member.dereference_type ();
	switch (td.tkind ()) {
		
		case Type::Kind::BASIC_TYPE:
			if (td.basic_type () == BasicType::BOOLEAN)
				stm << "false";
			else if (td.basic_type () < BasicType::OBJECT)
				stm << "0";
			break;

		case Type::Kind::NAMED_TYPE: {
			const NamedItem& nt = td.named_type ();
			if (nt.kind () == Item::Kind::ENUM) {
				const Enum& en = static_cast <const Enum&> (nt);
				stm << QName (en) << "::" << en.front ()->name ();
			}
		} break;
	}
	return stm << ')';
}

Code& operator << (Code& stm, const MemberInit& m)
{
	return stm << m.prefix << m.member.name () << " (std::move ("
		<< m.member.name () << "))";
}

Code& operator << (Code& stm, const AMI_ParametersABI& op)
{
	for (auto param : op.op) {
		if (param->attribute () != Parameter::Attribute::OUT)
			stm << ", " << ABI_param (*param, Parameter::Attribute::IN) << ' ' << param->name ();
	}

	return stm << ", Interface* _env)";
}

Code& operator << (Code& stm, const TypeCodeName& it)
{
	stm.namespace_open ("CORBA/Internal");
	return stm << empty_line
		<< "template <>\n"
		"const Char TypeCodeName <" << QName (it.item) << ">::name_ [] = \"" << static_cast <const std::string&> (it.item.name ()) << "\";\n";
}
