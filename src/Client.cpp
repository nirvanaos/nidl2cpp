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
#include "Client.h"

using namespace std;
using namespace std::filesystem;
using namespace AST;

Code& operator << (Code& stm, const Client::Param& t)
{
	stm << TypePrefix (t.type);
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

Code& operator << (Code& stm, const Client::Signature& op)
{
	stm << op.op.name () << " (";

	auto it = op.op.begin ();
	if (it != op.op.end ()) {
		stm << Client::Param (**it) << ' ' << (*it)->name ();
		++it;
		for (; it != op.op.end (); ++it) {
			stm << ", " << Client::Param (**it) << ' ' << (*it)->name ();
		}
	}

	return stm << ")";
}

void Client::end (const Root&)
{
	h_.close ();
	if (cpp_.size () > initial_cpp_size_)
		cpp_.close ();
	// Otherwise the .cpp file will be deleted.
}

void Client::leaf (const Include& item)
{
	h_ << "#include " << (item.system () ? '<' : '"')
		<< path (path (item.file ()).replace_extension ("").string ()
			+ options ().client_suffix).replace_extension ("h").string ()
		<< (item.system () ? '>' : '"')
		<< endl;
}

void Client::type_code_decl (const NamedItem& item)
{
	if (is_pseudo (item))
		return;

	if (!nested (item))
		h_ << "extern ";
	else
		h_ << "static ";
	h_ << "const " << Namespace ("Nirvana") << "ImportInterfaceT <"
		<< Namespace ("CORBA") << "TypeCode> _tc_"
		<< static_cast <const string&> (item.name ()) << ";\n";
}

void Client::type_code_def (const RepositoryId& rid)
{
	cpp_.namespace_close ();
	const NamedItem& item = rid.item ();
	if (is_pseudo (item))
		return;

	cpp_ << empty_line
		<< "NIRVANA_OLF_SECTION_N (" << (export_count_++) << ')';
	if (!nested (item))
		 cpp_ << " extern";
	cpp_ << " const " << Namespace ("Nirvana") << "ImportInterfaceT <" << Namespace ("CORBA") << "TypeCode>\n"
		<< TC_Name (item) << " = { Nirvana::OLF_IMPORT_INTERFACE, ";

	switch (item.kind ()) {
		case Item::Kind::INTERFACE:
		case Item::Kind::VALUE_TYPE:
			cpp_ << QName (item) << "::repository_id_";
			break;
		case Item::Kind::TYPE_DEF:
			cpp_ << '"' << rid.repository_id () << '"';
			break;
		default:
			cpp_ << "CORBA::Internal::RepIdOf <" << QName (item) << ">::repository_id_";
			break;
	}
	cpp_ << ", CORBA::TypeCode::repository_id_ };\n\n";
}

bool Client::nested (const NamedItem& item)
{
	return item.parent () && item.parent ()->kind () != Item::Kind::MODULE;
}

void Client::h_namespace_open (const NamedItem& item)
{
	if (!nested (item))
		h_.namespace_open (item);
}

void Client::leaf (const TypeDef& item)
{
	h_namespace_open (item);
	h_ << empty_line
		<< "typedef " << static_cast <const Type&> (item) << ' ' << item.name ()
		<< ";\n";
	type_code_decl (item);
	type_code_def (item);

	if (options ().legacy) {
		// <Type>_var
		switch (item.tkind ()) {
			case Type::Kind::STRING:
			case Type::Kind::WSTRING:
			case Type::Kind::FIXED:
			case Type::Kind::SEQUENCE:
			case Type::Kind::ARRAY:
				backward_compat_var (item);
				break;

			case Type::Kind::NAMED_TYPE:
				h_ <<
					"#ifdef LEGACY_CORBA_CPP\n"
					"typedef " << static_cast <const string&> (item.named_type ().name ()) << "_var "
					<< static_cast <const string&> (item.name ()) << "_var;\n"
					"typedef " << static_cast <const string&> (item.named_type ().name ()) << "_out "
					<< static_cast <const string&> (item.name ()) << "_out;\n";

				if (is_ref_type (item))
					h_ << "typedef " << static_cast <const string&> (item.named_type ().name ()) << "_ptr "
					<< static_cast <const string&> (item.name ()) << "_ptr;\n";
				h_ << "#endif\n";
		}
	}
}

void Client::backward_compat_var (const NamedItem& item)
{
	if (options ().legacy) {
		h_.namespace_open (item);
		h_ <<
			"#ifdef LEGACY_CORBA_CPP\n"
			"typedef " << Namespace ("CORBA/Internal") << "Type <" << item.name () << ">::C_var " << static_cast <const string&> (item.name ()) << "_var;\n"
			"typedef " << static_cast <const string&> (item.name ()) << "_var& " << static_cast <const string&> (item.name ()) << "_out;\n"
			"#endif\n";
	}
}

void Client::forward_decl (const NamedItem& item)
{
	h_.namespace_open (item);
	h_ << empty_line
		<< "class " << item.name () << ";\n";
	type_code_decl (item);
}

void Client::forward_guard (const NamedItem& item)
{
	const NamedItem* parent = item.parent ();
	if (parent)
		forward_guard (*parent);
	else {
		h_ << empty_line
			<< "#ifndef IDL_DECLARED";
	}
	h_ << '_' << item.name ();
}

void Client::forward_define (const AST::NamedItem& item)
{
	const NamedItem* parent = item.parent ();
	if (parent)
		forward_define (*parent);
	else
		h_ << "\n#define IDL_DECLARED";
	h_ << '_' << item.name ();
}

void Client::forward_interface (const NamedItem& item)
{
	h_.namespace_close ();
	forward_guard (item);
	forward_define (item);
	forward_decl (item);
	h_ << endl;
	h_.namespace_open ("CORBA/Internal");
	h_ << empty_line
		<< "template <>\n"
		"struct Type <" << QName (item) << "> : ";

	bool has_type_code = true;
	if (item.kind () == Item::Kind::INTERFACE_DECL || item.kind () == Item::Kind::INTERFACE) {
		InterfaceKind::Kind ikind;
		if (item.kind () == Item::Kind::INTERFACE_DECL)
			ikind = static_cast <const InterfaceDecl&> (item).interface_kind ();
		else
			ikind = static_cast <const Interface&> (item).interface_kind ();
		switch (ikind) {
			case InterfaceKind::LOCAL:
				h_ << "TypeLocalObject";
				break;
			case InterfaceKind::ABSTRACT:
				h_ << "TypeAbstractInterface";
				break;
			case InterfaceKind::UNCONSTRAINED:
				h_ << "TypeObject";
				break;
			default:
				h_ << "TypeItf";
				break;
		}

		has_type_code = ikind != InterfaceKind::PSEUDO;
		if (!has_type_code) {
			const NamedItem* parent = item.parent ();
			if (parent && !parent->parent () && parent->name () == "CORBA") {
				if (item.name () == "TypeCode")
					has_type_code = true;
			}
		}

	} else {
		assert (item.kind () == Item::Kind::VALUE_TYPE_DECL || item.kind () == Item::Kind::VALUE_TYPE);
		h_ << "TypeValue";
	}

	h_ << " <" << QName (item) << ">\n"
		"{";

	if (has_type_code) {
		h_ << endl;
		h_.indent ();
		type_code_func (item);
		h_.unindent ();
	}
	h_ << "};\n";

	h_.namespace_close ();
	h_ << empty_line
		<< "#endif\n"; // Close forward guard

	if (options ().legacy) {
		h_.namespace_open (item);
		h_ <<
			"#ifdef LEGACY_CORBA_CPP\n"
			"typedef " << Namespace ("CORBA/Internal") << "Type <" << item.name () << ">::C_ptr "
			<< static_cast <const string&> (item.name ()) << "_ptr;\n"
			"typedef " << Namespace ("CORBA/Internal") << "Type <" << item.name () << ">::C_var "
			<< static_cast <const string&> (item.name ()) << "_var;\n"
			"typedef " << static_cast <const string&> (item.name ()) << "_var& "
			<< static_cast <const string&> (item.name ()) << "_out;\n"
			"#endif\n";
	}
}

void Client::leaf (const InterfaceDecl& itf)
{
	forward_interface (itf);
}

void Client::leaf (const ValueTypeDecl& vt)
{
	forward_interface (vt);
}

void Client::type_code_func (const NamedItem& item)
{
	h_ << empty_line
		<< "static I_ptr <TypeCode> type_code ()\n"
		"{\n"
		<< indent
		<< "return " << TC_Name (item) << ";\n"
		<< unindent
		<< "}\n\n";
}

void Client::begin_interface (const ItemContainer& container)
{
	if (!is_pseudo (container))
		type_code_def (container);

	h_.namespace_open ("CORBA/Internal");
	h_ << empty_line
		<< "template <>\n"
		"struct Definitions <" << QName (container) << ">\n"
		"{\n"
		<< indent;
}

void Client::end_interface (const ItemContainer& container)
{
	// Close struct Definitions
	h_ << unindent
		<< "};\n";

	implement_nested_items (container);

	// Bridge
	h_.namespace_open ("CORBA/Internal");
	h_ << empty_line
		<< "NIRVANA_BRIDGE_BEGIN (" << QName (container) << ", \""
		<< container.repository_id () << "\")\n";

	bool att_byref = false;
	bool pseudo_interface = false;
	Bases bases;
	const Interface* concrete_itf = nullptr;
	Bases supports;
	if (container.kind () == Item::Kind::INTERFACE) {

		const Interface& itf = static_cast <const Interface&> (container);

		switch (itf.interface_kind ()) {
			case InterfaceKind::UNCONSTRAINED:
			case InterfaceKind::LOCAL:
				h_ << "NIRVANA_BASE_ENTRY (Object, CORBA_Object)\n";
				break;
			case InterfaceKind::ABSTRACT:
				h_ << "NIRVANA_BASE_ENTRY (AbstractBase, CORBA_AbstractBase)\n";
				break;

			case InterfaceKind::PSEUDO:
				att_byref = true;
				pseudo_interface = true;
				break;
		}

		Interfaces itf_bases = itf.get_all_bases ();
		bases.assign (itf_bases.begin (), itf_bases.end ());
		bridge_bases (bases);

		if (itf.interface_kind () != InterfaceKind::PSEUDO || !bases.empty ())
			h_ << "NIRVANA_BRIDGE_EPV\n";
	} else {
		const ValueType& vt = static_cast <const ValueType&> (container);
		att_byref = true;
		if (concrete_itf = get_concrete_supports (vt)) {
			supports.push_back (concrete_itf);
			Interfaces itf_bases = concrete_itf->get_all_bases ();
			supports.insert (supports.end (), itf_bases.begin (), itf_bases.end ());
		}

		h_ << "NIRVANA_BASE_ENTRY (ValueBase, CORBA_ValueBase)\n";

		bases = get_all_bases (vt);
		bridge_bases (bases);
		bridge_bases (supports);
		h_ << "NIRVANA_BRIDGE_EPV\n";

		if (concrete_itf)
			h_ << "Interface* (*_this) (Bridge <" << QName (vt) << ">*, Interface*);\n";
	}

	for (auto it = container.begin (); it != container.end (); ++it) {
		const Item& item = **it;
		switch (item.kind ()) {

			case Item::Kind::OPERATION: {
				const Operation& op = static_cast <const Operation&> (item);
				h_ << ABI_ret (op) << " (*" << op.name () << ") (Bridge <"
					<< QName (container) << ">*";

				for (auto it = op.begin (); it != op.end (); ++it) {
					h_ << ", " << ABI_param (**it);
				}

				h_ << ", Interface*);\n";

			} break;

			case Item::Kind::STATE_MEMBER:
				if (!static_cast <const StateMember&> (item).is_public ())
					break;

			case Item::Kind::ATTRIBUTE: {
				const Member& m = static_cast <const Member&> (item);
				h_ << ABI_ret (m, att_byref)
					<< " (*_get_" << m.name () << ") (Bridge <" << QName (container)
					<< ">*, Interface*);\n";

				if (!(m.kind () == Item::Kind::ATTRIBUTE && static_cast <const Attribute&> (m).readonly ())) {
					h_ << "void (*_set_" << m.name () << ") (Bridge <" << QName (container)
						<< ">*, " << ABI_param (m) << ", Interface*);\n";
				}

				if (m.kind () == Item::Kind::STATE_MEMBER && is_var_len (m)) {
					h_ << "void (*_move_" << m.name () << ") (Bridge <" << QName (container)
						<< ">*, " << ABI_param (m, Parameter::Attribute::INOUT) << ", Interface*);\n";
				}
			} break;
		}
	}

	h_ << "NIRVANA_BRIDGE_END ()\n"
		"\n"
		// Client interface
		"template <class T>\n"
		"class Client <T, " << QName (container) << "> :\n"
		<< indent
		<< "public T,\n"
		"public Definitions <" << QName (container) << ">\n"
		<< unindent
		<<
		"{\n"
		"public:\n"
		<< indent;

	if (concrete_itf)
		h_ << "I_ref <" << QName (*concrete_itf) << "> _this ();\n";

	for (auto it = container.begin (); it != container.end (); ++it) {
		const Item& item = **it;
		switch (item.kind ()) {

			case Item::Kind::OPERATION: {
				const Operation& op = static_cast <const Operation&> (item);

				h_ << VRet (op) << ' ' << Signature (op) << ";\n";

				native_itf_template (op);

			} break;

			case Item::Kind::STATE_MEMBER:
				if (!static_cast <const StateMember&> (item).is_public ())
					break;

			case Item::Kind::ATTRIBUTE: {
				const Member& m = static_cast <const Member&> (item);

				if (!att_byref)
					h_ << VRet (m);
				else
					h_ << ConstRef (m);
				h_ << ' ' << m.name () << " ();\n";

				if (!(m.kind () == Item::Kind::ATTRIBUTE && static_cast <const Attribute&> (m).readonly ()))
					h_ << "void " << m.name () << " (" << Param (m) << ");\n";

				if (m.kind () == Item::Kind::STATE_MEMBER && is_var_len (m))
					h_ << "void " << m.name () << " (" << Var (m) << "&&);\n";

			} break;
		}
	}

	h_ << unindent << "};\n";

	if (concrete_itf) {
		h_ << "\ntemplate <class T>\n"
			"I_ref <" << QName (*concrete_itf) << "> Client <T, " << QName (container) << ">::_this ()\n"
			"{\n"
			<< indent;
		environment (Raises ());
		h_ << "Bridge < " << QName (container) << ">& _b (T::_get_bridge (_env));\n"
			"Type <" << QName (*concrete_itf) << ">::C_ret _ret = (_b._epv ().epv._this) (&_b, &_env);\n"
			"_env.check ();\n"
			"return _ret;\n"
			<< unindent <<
			"}\n";
	}

	// Client operations

	for (auto it = container.begin (); it != container.end (); ++it) {
		const Item& item = **it;
		switch (item.kind ()) {

			case Item::Kind::OPERATION: {
				const Operation& op = static_cast <const Operation&> (item);

				h_ << "\ntemplate <class T>\n";

				h_ << VRet (op) << " Client <T, " << QName (container) << ">::" << Signature (op) << "\n"
					"{\n"
					<< indent;

				environment (op.raises ());
				h_ << "Bridge < " << QName (container) << ">& _b (T::_get_bridge (_env));\n";

				if (op.tkind () != Type::Kind::VOID)
					h_ << TypePrefix (op) << "C_ret _ret = ";

				h_ << "(_b._epv ().epv." << op.name () << ") (&_b";
				for (auto it = op.begin (); it != op.end (); ++it) {
					h_ << ", &" << (*it)->name ();
				}
				h_ << ", &_env);\n"
					"_env.check ();\n";

				if (op.tkind () != Type::Kind::VOID)
					h_ << "return _ret;\n";

				h_ << unindent
					<< "}\n";

			} break;

			case Item::Kind::STATE_MEMBER:
				if (!static_cast <const StateMember&> (item).is_public ())
					break;

			case Item::Kind::ATTRIBUTE: {
				const Member& m = static_cast <const Member&> (item);

				h_ << "\ntemplate <class T>\n";
				if (!att_byref)
					h_ << VRet (m);
				else
					h_ << ConstRef (m);

				h_ << " Client <T, " << QName (container) << ">::" << m.name () << " ()\n"
					"{\n";

				h_.indent ();

				const Attribute* att = nullptr;
				if (m.kind () == Item::Kind::ATTRIBUTE)
					att = &static_cast <const Attribute&> (m);

				environment (att ? att->getraises () : Raises ());
				h_ << "Bridge < " << QName (container) << ">& _b (T::_get_bridge (_env));\n"
					<< TypePrefix (m) << 'C';

				if (att_byref)
					h_ << "_VT";

				h_ << "_ret _ret = (_b._epv ().epv._get_" << m.name () << ") (&_b, &_env);\n"
					"_env.check ();\n"
					"return _ret;\n"
					<< unindent
					<< "}\n";

				if (!(att && att->readonly ())) {
					h_ << "\ntemplate <class T>\n";

					h_ << "void Client <T, " << QName (container) << ">::" << m.name ()
						<< " (" << Param (m) << " val)\n"
						"{\n"

					<< indent;
					environment (att ? att->setraises () : Raises ());
					h_ << "Bridge < " << QName (container) << ">& _b (T::_get_bridge (_env));\n"
						"(_b._epv ().epv._set_" << m.name () << ") (&_b, &val, &_env);\n"
						"_env.check ();\n"
					<< unindent
					<< "}\n";
				}

				if (m.kind () == Item::Kind::STATE_MEMBER && is_var_len (m)) {
					h_ << "\ntemplate <class T>\n";

					h_ << "void Client <T, " << QName (container) << ">::" << m.name ()
						<< " (" << Var (m) << "&& val)\n"
						"{\n"

					<< indent;

					environment (Raises ());
					h_ << "Bridge < " << QName (container) << ">& _b (T::_get_bridge (_env));\n"
						"(_b._epv ().epv._move_" << m.name () << ") (&_b, &val, &_env);\n"
						"_env.check ();\n"
						<< unindent
						<< "}\n";
				}

			} break;
		}
	}

	// Interface definition
	h_.namespace_open (container);
	h_ << empty_line
		<< "class " << container.name () << " :\n"
		<< indent <<
		"public " << Namespace ("CORBA/Internal") << "ClientInterface <" << container.name ();
	for (auto b : bases) {
		h_ << ", " << QName (*b);
	}
	if (container.kind () == Item::Kind::INTERFACE) {
		const Interface& itf = static_cast <const Interface&> (container);
		switch (itf.interface_kind ()) {
			case InterfaceKind::UNCONSTRAINED:
			case InterfaceKind::LOCAL:
				h_ << ", " << Namespace ("CORBA") << "Object";
				break;
			case InterfaceKind::ABSTRACT:
				h_ << ", " << Namespace ("CORBA") << "AbstractBase";
				break;
		}
	} else {
		const ValueType& vt = static_cast <const ValueType&> (container);
		h_ << ", " << Namespace ("CORBA") << "ValueBase";
		if (!supports.empty ()) {
			h_ << ">,\n"
				"public " << Namespace ("CORBA/Internal") << "ClientSupports <" << container.name ();

			for (auto itf : supports) {
				h_ << ", " << QName (*itf);
			}
		}
	}
	h_ << ">\n"
		<< unindent <<
		"{\n"
		"public:\n"
		<< indent;
	for (auto it = container.begin (); it != container.end (); ++it) {
		const Item& item = **it;
		switch (item.kind ()) {
			case Item::Kind::OPERATION:
			case Item::Kind::ATTRIBUTE:
			case Item::Kind::STATE_MEMBER:
			case Item::Kind::VALUE_FACTORY:
				break;
			default: {
				const NamedItem& def = static_cast <const NamedItem&> (item);
				h_ << "using " << Namespace ("CORBA/Internal") << "Definitions <" << container.name () << ">::" << def.name () << ";\n";
				if (!pseudo_interface && RepositoryId::cast (&def))
					h_ << "using " << Namespace ("CORBA/Internal") << "Definitions <" << container.name () << ">::_tc_" << def.name () << ";\n";
			}
		}
	}
	h_ << unindent
		<< "};\n";
}

void Client::bridge_bases (const Bases& bases)
{
	for (auto b : bases) {
		string proc_name;
		{
			ScopedName sn = b->scoped_name ();
			for (ScopedName::const_iterator it = sn.begin (); it != sn.end (); ++it) {
				proc_name += '_';
				proc_name += *it;
			}
		}
		h_ << "NIRVANA_BASE_ENTRY (" << QName (*b) << ", " << proc_name << ")\n";
	}
}

void Client::begin (const Interface& itf)
{
	if (!itf.has_forward_dcl ())
		forward_interface (itf);

	begin_interface (itf);
}

void Client::end (const Interface& itf)
{
	end_interface (itf);
}

void Client::begin (const ValueType& itf)
{
	if (!itf.has_forward_dcl ())
		forward_interface (itf);

	begin_interface (itf);
}

inline
size_t Client::version (const string& rep_id)
{

	for (const char* begin = rep_id.data (), *p = begin + rep_id.size () - 1; p > begin; --p) {
		char c = *p;
		if (':' == c)
			return p - begin;
		else if (c != '.' && !('0' <= c && c <= '9'))
			break;
	}
	return rep_id.size ();
}

void Client::end (const ValueType& vt)
{
	end_interface (vt);

	Factories factories = get_factories (vt);

	if (!factories.empty ()) {
		h_.namespace_open (vt);
		h_ << empty_line
			<< "class " << vt.name () << FACTORY_SUFFIX ";\n";

		h_.namespace_open ("CORBA/Internal");
		h_.empty_line ();

		{
			string factory_id = vt.repository_id ();
			factory_id.insert (version (factory_id), FACTORY_SUFFIX);

			h_ << "NIRVANA_BRIDGE_BEGIN (" << QName (vt) << FACTORY_SUFFIX ", \"" << factory_id << "\")\n"
				"NIRVANA_BASE_ENTRY  (ValueFactoryBase, CORBA_ValueFactoryBase)\n"
				"NIRVANA_BRIDGE_EPV\n";
		}

		for (auto f : factories) {
			h_ << "Interface* (*" << f->name () << ") (Bridge <" << QName (vt) << FACTORY_SUFFIX ">*";
			for (auto p : *f) {
				h_ << ", " << ABI_param (*p);
			}
			h_ << ", Interface*);\n";
		}
		h_ << "NIRVANA_BRIDGE_END ()\n"
			"\n"
			// Client interface
			"template <class T>\n"
			"class Client <T, " << QName (vt) << FACTORY_SUFFIX "> :\n"
			<< indent
			<< "public T,\n"
			"public Definitions <" << QName (vt) << ">\n"
			<< unindent
			<<
			"{\n"
			"public:\n"
			<< indent;
		for (auto f : factories) {
			h_ << "Type <" << QName (vt) << ">::VRet " << Signature (*f) << ";\n";
		}
		h_ << unindent
			<< "};\n";

		for (auto f : factories) {
			h_ << "\ntemplate <class T>\n"
				<< "Type <" << QName (vt) << ">::VRet " << " Client <T, " << QName (vt) << FACTORY_SUFFIX ">::" << Signature (*f) << "\n"
				"{\n"
				<< indent;
			environment (f->raises ());
			h_ << "Bridge < " << QName (vt) << FACTORY_SUFFIX ">& _b (T::_get_bridge (_env));\n"
				"Type <" << QName (vt) << ">::C_ret _ret = (_b._epv ().epv." << f->name ()
				<< ") (&_b";
			for (auto it = f->begin (); it != f->end (); ++it) {
				h_ << ", &" << (*it)->name ();
			}
			h_ << ", &_env);\n"
				"_env.check ();\n"
				"return _ret;\n"
				<< unindent
				<< "}\n";
		}

		h_.namespace_open (vt);
		h_ << empty_line
			<< "class " << vt.name () << FACTORY_SUFFIX " : public "
			<< Namespace ("CORBA/Internal") << "ClientInterface <" << vt.name ()
			<< FACTORY_SUFFIX ", CORBA::ValueFactoryBase>\n"
			"{\n"
			"public:\n"
			<< indent <<
			"static const ::Nirvana::ImportInterfaceT <" << vt.name () << FACTORY_SUFFIX "> _factory;\n"
			<< unindent <<
			"};\n";
	
		cpp_ << empty_line
			<< "NIRVANA_OLF_SECTION_N (" << (export_count_++) << ')';
		cpp_ << " const " << Namespace ("Nirvana") << "ImportInterfaceT <" << QName (vt) << FACTORY_SUFFIX ">\n"
			<< QName (vt) << FACTORY_SUFFIX "::_factory = { Nirvana::OLF_IMPORT_INTERFACE, "
			<< QName (vt) << "::repository_id_, " << QName (vt) << FACTORY_SUFFIX "::repository_id_ };\n";
	}
}

inline
void Client::native_itf_template (const Operation& op)
{
	// Generate template method for native Interface returning functions
	if (is_native_interface (static_cast <const Type&> (op))) {
		const Parameter* par_iid = nullptr;
		for (auto it = op.begin (); it != op.end (); ++it) {
			const Parameter& par = **it;
			if (par.dereference_type ().tkind () == Type::Kind::STRING && par.name () == "interface_id") {
				par_iid = &par;
				break;
			}
		}

		if (par_iid) {
			// Generate template
			h_ << "template <class I>\n"
				"I_ref <I> " << op.name () << " (";

			auto it = op.begin ();
			if (par_iid == *it)
				++it;
			if (it != op.end ()) {
				h_ << Client::Param (**it) << ' ' << (*it)->name ();
				++it;
				for (; it != op.end (); ++it) {
					if (par_iid != *it)
						h_ << ", " << Client::Param (**it) << ' ' << (*it)->name ();
				}
			}

			h_ << ")\n"
				"{\n"
				<< indent
				<< "return " << op.name () << " (";
			it = op.begin ();
			if (par_iid == *it)
				h_ << "I::repository_id_";
			else
				h_ << (*it)->name ();
			++it;
			for (; it != op.end (); ++it) {
				h_ << ", ";
				if (par_iid == *it)
					h_ << "I::repository_id_";
				else
					h_ << (*it)->name ();
			}
			h_ << ").template downcast <I> ();\n"
				<< unindent
				<< "}\n";
		}
	}
}

void Client::implement_nested_items (const ItemContainer& parent)
{
	for (auto it = parent.begin (); it != parent.end (); ++it) {
		const Item& item = **it;
		switch (item.kind ()) {
			case Item::Kind::EXCEPTION:
				implement (static_cast <const Exception&> (item));
				break;
			case Item::Kind::STRUCT:
				implement (static_cast <const Struct&> (item));
				break;
			case Item::Kind::UNION:
				implement (static_cast <const Union&> (item));
				break;
			case Item::Kind::ENUM:
				implement (static_cast <const Enum&> (item));
				break;
		}
	}
}

void Client::environment (const Raises& raises)
{
	if (raises.empty ())
		h_ << "Environment _env;\n";
	else {
		h_ << "EnvironmentEx < ";
		auto it = raises.begin ();
		h_ << QName (**it);
		for (++it; it != raises.end (); ++it) {
			h_ << ", " << QName (**it);
		}
		h_ << "> _env;\n";
	}
}

void Client::leaf (const Constant& item)
{
	h_namespace_open (item);

	bool outline = false;
	const char* type = nullptr;
	switch (item.dereference_type ().tkind ()) {
		case Type::Kind::STRING:
		case Type::Kind::FIXED:
			outline = true;
			type = "char* const";
			break;
		case Type::Kind::WSTRING:
			outline = true;
			type = "whar_t* const";
			break;
	}

	if (nested (item))
		h_ << "static ";
	else if (outline)
		h_ << "extern ";

	h_ << "const ";
	
	if (type)
		h_ << type;
	else
		h_ << static_cast <const Type&> (item);

	h_ << ' ' << item.name ();
	if (outline) {
		cpp_.namespace_open (item);

		if (nested (item))
			cpp_ << "static ";
		else
			cpp_ << "extern ";

		cpp_ << "const ";

		if (type)
			cpp_ << type;
		else
			cpp_ << static_cast <const Type&> (item);

		cpp_ << ' ' << QName (item) << " = " << static_cast <const Variant&> (item) << ";\n";
	} else {
		h_ << " = " << static_cast <const Variant&> (item);
	}
	h_ << ";\n";
}

void Client::begin (const Exception& item)
{
	h_namespace_open (item);
	h_ << empty_line
		<< "class " << item.name () << " : public " << Namespace ("CORBA") << "UserException\n"
		"{\n"
		"public:\n"
		<< indent
		<< "NIRVANA_EXCEPTION_DCL (" << item.name () << ");\n\n";
}

void Client::end (const Exception& item)
{
	Members members = get_members (item);
	if (!members.empty ()) {

		// Define struct _Data
		h_ << empty_line
			<< "struct _Data\n"
			"{\n"

			<< indent;
		member_variables (members);
		h_ << unindent

			<< "};\n\n";

		if (options ().legacy) {
			h_ << unindent
				<< "#ifndef LEGACY_CORBA_CPP\n"
				<< indent;
		}

		constructors (item.name (), members, "_");
		accessors (members);
		h_ << empty_line
			<< unindent
			<< "private:\n"
			<< indent;
		member_variables (members);

		if (options ().legacy) {
			h_ << endl
				<< unindent
				<< "#else\n"
				<< indent;
			constructors (item.name (), members, "");
			member_variables_legacy (members);
			h_ << endl
				<< unindent
				<< "#endif\n"
				<< indent;
		}

		h_ << unindent
			<< "private:\n"
			<< indent

			<< "virtual void* __data () NIRVANA_NOEXCEPT\n"
			"{\n"
			<< indent;
		if (options ().legacy)
			h_ << "#ifndef LEGACY_CORBA_CPP\n";
		h_ << "return &_" << static_cast <const string&> (members.front ()->name ()) << ";\n";
		if (options ().legacy) {
			h_ <<
				"#else\n"
				"return &" << members.front ()->name () << ";\n"
				"#endif\n";
		}
		h_ << unindent
			<< "}\n";
	}
	h_ << unindent
		<< "};\n";

	// Type code
	type_code_decl (item);

	if (!nested (item))
		implement (item);
}

void Client::implement (const Exception& item)
{
	Members members = get_members (item);
	if (!members.empty ())
		define_structured_type (item, members, "::_Data");
	else
		rep_id_of (item);
	type_code_def (item);

	// Define exception
	assert (cpp_.cur_namespace ().empty ());
	cpp_ << empty_line
		<< "NIRVANA_EXCEPTION_DEF (" << ParentName (item) << ", " << item.name () << ")\n\n";

	implement_nested_items (item);
}

void Client::rep_id_of (const RepositoryId& rid)
{
	const NamedItem& item = rid.item ();
	if (!is_pseudo (item)) {
		h_.namespace_open ("CORBA/Internal");
		h_ << empty_line
			<< "template <>\n"
			"const Char RepIdOf <" << QName (item) << ">::repository_id_ [] = \""
			<< rid.repository_id () << "\";\n\n";
	}
}

void Client::define_structured_type (const RepositoryId& rid,
	const Members& members, const char* suffix)
{
	rep_id_of (rid);

	const NamedItem& item = rid.item ();

	bool var_len = is_var_len (members);
	if (var_len) {
		// ABI
		h_ << "template <>\n"
			"struct ABI < " << QName (item) << suffix << ">\n"
			"{\n"
			<< indent;
		for (auto member : members) {
			h_ << TypePrefix (*member) << "ABI " << member->name () << ";\n";
		}
		h_ << unindent
			<< "};\n\n";
	}

	// Type
	h_ << "template <>\n"
		"struct Type < " << QName (item) << suffix << "> : ";

	if (var_len) {
		h_ << "TypeVarLen < " << QName (item) << suffix << ", ";
		has_check (members);
		h_ << ">\n"
			"{\n"
			<< indent;

		implement_type (item, members);
		
		if (!*suffix && !is_pseudo (item))
			type_code_func (item);

		h_ << unindent
			<< "};\n";
	} else {
		h_ << "TypeFixLen < " << QName (item) << suffix << ">\n"
			"{\n"
			<< indent

			<< "static const bool has_check = ";
		has_check (members);
		h_ << ";\n";

		implement_type (item, members);
		
		if (!*suffix && !is_pseudo (item))
			type_code_func (item);

		h_ << unindent
			<< "};\n";
	}
}

void Client::implement_type (const AST::NamedItem& cont, const Members& members)
{
	for (auto m : members) {
		if (is_native (*m))
			return;
	}

	// Implement check()

	h_ << "static void check (const ABI& val)\n"
		"{\n"
		<< indent;
	for (auto member : members) {
		if (may_have_check (*member))
			h_ << TypePrefix (*member) << "check (val." << member->name () << ");\n";
	}
	h_ << unindent
		<< "}\n";

	// Implement marshaling
	if (is_var_len (members)) {
		h_ << "\n"
			"static void marshal_in (const Var&, IORequest::_ptr_type);\n"
			"static void marshal_out (Var&, IORequest::_ptr_type);\n"
			"static void unmarshal (IORequest::_ptr_type, Var&);\n";
	} else
		h_ << "static void byteswap (Var&) NIRVANA_NOEXCEPT;\n";

}

void Client::has_check (const Members& members)
{
	auto check_member = members.begin ();
	for (; check_member != members.end (); ++check_member) {
		if (may_have_check (**check_member))
			break;
	}
	if (check_member != members.end ()) {
		h_ << TypePrefix (**(check_member++)) << "has_check";
		for (; check_member != members.end (); ++check_member) {
			if (may_have_check (**check_member))
				break;
		}
		if (check_member != members.end ()) {
			h_ << '\n'
				<< indent;
			do {
				h_ << "| " << TypePrefix (**(check_member++)) << "has_check";
				for (; check_member != members.end (); ++check_member) {
					if (may_have_check (**check_member))
						break;
				}
			} while (check_member != members.end ());
			h_.unindent ();
		}
	} else
		h_ << "false";
}

void Client::leaf (const StructDecl& item)
{
	forward_decl (item);
}

void Client::begin (const Struct& item)
{
	h_namespace_open (item);
	h_ << empty_line
		<< "class " << item.name () << "\n"
		"{\n"
		"public:\n"
		<< indent;
}

void Client::end (const Struct& item)
{
	Members members = get_members (item);

	if (options ().legacy) {
		h_ << unindent
			<< "#ifndef LEGACY_CORBA_CPP\n"
			<< indent;
	}

	constructors (item.name (), members, "_");
	accessors (members);

	// Member variables
	h_ << unindent
		<< empty_line
		<< "private:\n"
		<< indent
		<< "friend struct " << Namespace ("CORBA/Internal") << "Type <" << item.name () << ">;\n";
	member_variables (members);

	if (options ().legacy) {
		h_ << unindent
			<< "#else\n"
			<< indent;
		member_variables_legacy (members);
		h_ << unindent
			<< "#endif\n"
			<< indent;
	}

	h_ << unindent
		<< "};\n";

	// Type code
	type_code_decl (item);

	if (!nested (item))
		implement (item);
}

void Client::implement (const Struct& item)
{
	Members members = get_members (item);
	define_structured_type (item, members);
	backward_compat_var (item);
	type_code_def (item);
	implement_nested_items (item);
}

void Client::constructors (const Identifier& name, const Members& members, const char* prefix)
{
	assert (!members.empty ());
	h_ << empty_line
	// Default constructor
		<< name << " () NIRVANA_NOEXCEPT :\n"
		<< indent;
	auto it = members.begin ();
	h_ << MemberDefault (**it, prefix);
	for (++it; it != members.end (); ++it) {
		h_ << ",\n" << MemberDefault (**it, prefix);
	}
	h_ << unindent
		<< "\n{}\n"

	// Explicit constructor
	"explicit " << name << " (";
	it = members.begin ();
	h_ << Var (**it) << ' ' << (*it)->name ()
		<< indent;
	for (++it; it != members.end (); ++it) {
		h_ << ",\n" << Var (**it) << ' ' << (*it)->name ();
	}
	h_ << ") :\n";

	it = members.begin ();
	h_ << MemberInit (**it, prefix);
	for (++it; it != members.end (); ++it) {
		h_ << ",\n" << MemberInit (**it, prefix);
	}
	h_ << unindent
		<< "\n{}\n";
}

void Client::accessors (const Members& members)
{
	// Accessors
	for (const Member* m : members) {
		h_ << empty_line
			<< Accessors (*m);
	}
}

void Client::member_variables (const Members& members)
{
	for (const Member* m : members) {
		h_ << MemberVariable (*m);
	}
}

void Client::member_variables_legacy (const Members& members)
{
	for (const Member* m : members) {
		if (is_boolean (*m))
			h_ << TypePrefix (*m) << "ABI";
		else {
			switch (m->tkind ()) {
				case Type::Kind::STRING:
					h_ << Namespace ("CORBA") << "String_var";
					break;
				case Type::Kind::WSTRING:
					h_ << Namespace ("CORBA") << "WString_var";
					break;
				default:
					h_ << static_cast <const Type&> (*m);
					if (is_ref_type (*m))
						h_ << "_var";
					else {
						switch (m->dereference_type ().tkind ()) {
							case Type::Kind::STRING:
							case Type::Kind::WSTRING:
								h_ << "_var";
								break;
						}
					}
			}
		}
		h_ << ' ' << m->name () << ";\n";
	}
}

void Client::implement (const Union& item)
{
	// TODO:: Implement
	implement_nested_items (item);
}

void Client::leaf (const Enum& item)
{
	h_namespace_open (item);
	h_ << empty_line
		<< "enum class " << item.name () << " : " << Namespace ("CORBA/Internal") << "ABI_enum\n"
		"{\n"
		<< indent;
	auto it = item.begin ();
	h_ << (*it)->name ();
	for (++it; it != item.end (); ++it) {
		h_ << ",\n" << (*it)->name ();
	}
	h_ << unindent
		<< "\n};\n";
	type_code_decl (item);

	if (!nested (item))
		implement (item);
}

void Client::implement (const Enum& item)
{
	rep_id_of (item);
	h_ << "template <>\n"
		"struct Type <" << QName (item) << "> : public TypeEnum <" << QName (item)
		<< ", " << QName (item) << "::" << item.back ()->name () << ">\n"
		"{\n";
	h_.indent ();
	if (!is_pseudo (item))
		type_code_func (item);
	h_ << unindent
		<< "};\n";

	if (options ().legacy) {
		h_.namespace_open (item);
		h_ <<
			"#ifdef LEGACY_CORBA_CPP\n"
			"typedef " << item.name () << "& " << static_cast <const string&> (item.name ()) << "_out;\n"
			"#endif\n";
	}
	type_code_def (item);
}

void Client::leaf (const AST::ValueBox& item)
{
	throw runtime_error ("Not yet implemented");
}
