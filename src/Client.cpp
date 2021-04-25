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
		<< path (path (item.file ()).replace_extension ("").string () + options ().client_suffix).replace_extension ("h").string ()
		<< (item.system () ? '>' : '"')
		<< endl;
}

void Client::type_code_decl (const NamedItem& item)
{
	if (is_pseudo (item))
		return;

	if (!nested (item))
		h_ << "extern ";
	h_ << "const " << Namespace ("Nirvana") << "ImportInterfaceT <" << Namespace ("CORBA") << "TypeCode> _tc_" << static_cast <const string&> (item.name ()) << ";\n";
}

void Client::type_code_def (const RepositoryId& rid)
{
	assert (cpp_.cur_namespace ().empty ());
	const NamedItem& item = rid.item ();
	if (is_pseudo (item))
		return;

	cpp_.empty_line ();
	cpp_ << "NIRVANA_OLF_SECTION ";
	if (!nested (item))
		cpp_ << "extern ";
	cpp_ << "const " << Namespace ("Nirvana") << "ImportInterfaceT <" << Namespace ("CORBA") << "TypeCode>\n"
		<< TypeCodeName (item) << " = { Nirvana::OLF_IMPORT_INTERFACE, ";

	switch (item.kind ()) {
		case Item::Kind::INTERFACE:
		case Item::Kind::VALUE_TYPE:
			cpp_ << QName (item) << "::repository_id_";
			break;
		case Item::Kind::TYPE_DEF:
			cpp_ << '"' << rid.repository_id () << '"';
			break;
		default:
			cpp_ << "CORBA::Nirvana::RepIdOf <" << QName (item) << ">::repository_id_";
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
	h_.empty_line ();
	h_ << "typedef " << static_cast <const Type&> (item) << ' ' << item.name () << ";\n";
	type_code_decl (item);
	type_code_def (item);

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

void Client::backward_compat_var (const NamedItem& item)
{
	h_.namespace_open (item);
	h_ <<
		"#ifdef LEGACY_CORBA_CPP\n"
		"typedef " << Namespace ("CORBA/Nirvana") << "Type <" << item.name () << ">::C_var " << static_cast <const string&> (item.name ()) << "_var;\n"
		"typedef " << static_cast <const string&> (item.name ()) << "_var& " << static_cast <const string&> (item.name ()) << "_out;\n"
		"#endif\n";
}

void Client::forward_decl (const NamedItem& item)
{
	h_.namespace_open (item);
	h_.empty_line ();
	h_ << "class " << item.name () << ";\n";
	type_code_decl (item);
}

void Client::forward_interface (const NamedItem& item)
{
	forward_decl (item);

	h_ <<
		"#ifdef LEGACY_CORBA_CPP\n"
		"typedef " << Namespace ("CORBA/Nirvana") << "TypeItf <" << item.name () << ">::C_ptr "
		<< static_cast <const string&> (item.name ()) << "_ptr;\n"
		"typedef " << Namespace ("CORBA/Nirvana") << "TypeItf <" << item.name () << ">::C_var "
		<< static_cast <const string&> (item.name ()) << "_var;\n"
		"typedef " << static_cast <const string&> (item.name ()) << "_var& " << static_cast <const string&> (item.name ()) << "_out;\n"
		"#endif\n";
}

void Client::leaf (const InterfaceDecl& itf)
{
	forward_interface (itf);
}

void Client::type_code_func (const NamedItem& item)
{
	h_ << "static I_ptr <TypeCode> type_code ()\n"
		"{\n";
	h_.indent ();
	h_ << "return " << TypeCodeName (item) << ";\n";
	h_.unindent ();
	h_ << "}\n";
}

void Client::begin (const Interface& itf)
{
	// Forward declarations
	forward_interface (itf);

	if (itf.interface_kind () != InterfaceKind::PSEUDO)
		type_code_def (itf);

	h_.namespace_open ("CORBA/Nirvana");
	h_.empty_line ();
	h_ << "template <>\n"
		"struct Type <" << QName (itf) << "> : ";
	switch (itf.interface_kind ()) {
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
	h_ << " <" << QName (itf) << ">\n"
		"{";
	if (itf.interface_kind () != InterfaceKind::PSEUDO) {
		h_ << endl;
		h_.indent ();
		type_code_func (itf);
		h_.unindent ();
	}
	h_ << "};\n";

	h_.empty_line ();
	h_ << "template <>\n"
		"struct Definitions < " << QName (itf) << ">\n"
		"{\n";
	h_.indent ();
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
				"typename TypeItf <I>::Var " << op.name () << " (";

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
				"{\n";
			h_.indent ();
			h_ << "return " << op.name () << " (";
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
			h_ << ").downcast <I> ();\n";
			h_.unindent ();
			h_ << "}\n";
		}
	}
}

void Client::end (const Interface& itf)
{
	// Close struct Definitions
	h_.unindent ();
	h_ << "};\n";

	implement_nested_items (itf);

	// Bridge
	h_.namespace_open ("CORBA/Nirvana");
	h_.empty_line ();
	h_ << "NIRVANA_BRIDGE_BEGIN (" << QName (itf) << ", \"" << itf.repository_id () << "\")\n";

	switch (itf.interface_kind ()) {
		case InterfaceKind::UNCONSTRAINED:
		case InterfaceKind::LOCAL:
			h_ << "NIRVANA_BASE_ENTRY (Object, CORBA_Object)\n";
			break;
		case InterfaceKind::ABSTRACT:
			h_ << "NIRVANA_BASE_ENTRY (AbstractBase, CORBA_AbstractBase)\n";
			break;
	}

	Interfaces bases = itf.get_all_bases ();
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

	if (itf.interface_kind () != InterfaceKind::PSEUDO || !bases.empty ())
		h_ << "NIRVANA_BRIDGE_EPV\n";

	for (auto it = itf.begin (); it != itf.end (); ++it) {
		const Item& item = **it;
		switch (item.kind ()) {

			case Item::Kind::OPERATION: {
				const Operation& op = static_cast <const Operation&> (item);
				h_ << ABI_ret (op) << " (*" << op.name () << ") (Bridge < " << QName (itf) << ">* _b";

				for (auto it = op.begin (); it != op.end (); ++it) {
					h_ << ", " << ABI_param (**it);
				}

				h_ << ", Interface* _env);\n";

			} break;

			case Item::Kind::ATTRIBUTE: {
				const Attribute& att = static_cast <const Attribute&> (item);
				h_ << ABI_ret (att) << " (*_get_" << att.name () << ") (Bridge < " << QName (itf) << ">*, Interface*);\n";

				if (!att.readonly ()) {
					h_ << "void (*_set_" << att.name () << ") (Bridge < " << QName (itf) << ">* _b, " << ABI_param (att) << ", Interface* _env);\n";
				}

			} break;
		}
	}

	h_ << "NIRVANA_BRIDGE_END ()\n"
		"\n"
		// Client interface
		"template <class T>\n"
		"class Client <T, " << QName (itf) << "> :\n"
		<< indent
		<< "public T,\n"
		"public Definitions <" << QName (itf) << ">\n"
		<< unindent
		<<
		"{\n"
		"public:\n";

	h_.indent ();

	for (auto it = itf.begin (); it != itf.end (); ++it) {
		const Item& item = **it;
		switch (item.kind ()) {

			case Item::Kind::OPERATION: {
				const Operation& op = static_cast <const Operation&> (item);

				h_ << Var (op) << ' ' << Signature (op) << ";\n";

				native_itf_template (op);

			} break;

			case Item::Kind::ATTRIBUTE: {
				const Attribute& att = static_cast <const Attribute&> (item);

				h_ << Var (att) << ' ' << att.name () << " ();\n";

				if (!att.readonly ())
					h_ << "void " << att.name () << " (" << Param (att) << ");\n";

			} break;
		}
	}

	h_.unindent ();
	h_ << "};\n";

	// Client operations

	for (auto it = itf.begin (); it != itf.end (); ++it) {
		const Item& item = **it;
		switch (item.kind ()) {

			case Item::Kind::OPERATION: {
				const Operation& op = static_cast <const Operation&> (item);

				h_ << "\ntemplate <class T>\n";

				h_ << Var (op) << " Client <T, " << QName (itf) << ">::" << Signature (op) << "\n"
					"{\n";

				h_.indent ();

				environment (op.raises ());
				h_ << "Bridge < " << QName (itf) << ">& _b (T::_get_bridge (_env));\n";

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

				h_.unindent ();
				h_ << "}\n";

			} break;

			case Item::Kind::ATTRIBUTE: {
				const Attribute& att = static_cast <const Attribute&> (item);

				h_ << "\ntemplate <class T>\n";

				h_ << Var (att) << " Client <T, " << QName (itf) << ">::" << att.name () << " ()\n"
					"{\n";

				h_.indent ();

				environment (att.getraises ());
				h_ << "Bridge < " << QName (itf) << ">& _b (T::_get_bridge (_env));\n"
					<< TypePrefix (att) << "C_ret _ret = (_b._epv ().epv._get_" << att.name () << ") (&_b, &_env);\n"
					"_env.check ();\n"
					"return _ret;\n";

				h_.unindent ();
				h_ << "}\n";

				if (!att.readonly ()) {
					h_ << "\ntemplate <class T>\n";

					h_ << "void Client <T, " << QName (itf) << ">::" << att.name () << " (" << Param (att) << " val)\n"
						"{\n";

					h_.indent ();

					environment (att.setraises ());
					h_ << "Bridge < " << QName (itf) << ">& _b (T::_get_bridge (_env));\n";

					h_ << "(_b._epv ().epv._set_" << att.name () << ") (&_b, &val, &_env);\n"
						"_env.check ();\n";

					h_.unindent ();
					h_ << "}\n";
				}

			} break;
		}
	}

	// Interface definition
	h_.namespace_open (itf);
	h_.empty_line ();
	h_ << "class " << itf.name () << " : public " << Namespace ("CORBA/Nirvana") << "ClientInterface <" << itf.name ();
	for (auto b : bases) {
		h_ << ", " << QName (*b);
	}
	switch (itf.interface_kind ()) {
		case InterfaceKind::UNCONSTRAINED:
		case InterfaceKind::LOCAL:
			h_ << ", " << Namespace ("CORBA") << "Object";
			break;
		case InterfaceKind::ABSTRACT:
			h_ << ", " << Namespace ("CORBA") << "AbstractBase";
			break;
	}
	h_ << ">\n"
		"{\n"
		"public:\n";
	h_.indent ();
	for (auto it = itf.begin (); it != itf.end (); ++it) {
		const Item& item = **it;
		switch (item.kind ()) {
			case Item::Kind::OPERATION:
			case Item::Kind::ATTRIBUTE:
				break;
			default: {
				const NamedItem& def = static_cast <const NamedItem&> (item);
				h_ << "using " << Namespace ("CORBA/Nirvana") << "Definitions <" << itf.name () << ">::" << def.name () << ";\n";
				if (itf.interface_kind () != InterfaceKind::PSEUDO && RepositoryId::cast (&def))
					h_ << "using " << Namespace ("CORBA/Nirvana") << "Definitions <" << itf.name () << ">::_tc_" << def.name () << ";\n";
			}
		}
	}
	h_.unindent ();
	h_ << "};\n";
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

	if (nested (item))
		h_ << "static ";
	else
		h_ << "extern ";
	bool outline = constant (h_, item);
	h_ << ' ' << item.name ();
	if (outline) {
		cpp_.namespace_open (item);
		constant (cpp_, item);
		cpp_ << ' ' << QName (item) << " = " << static_cast <const Variant&> (item) << ";\n";
	} else {
		h_ << " = " << static_cast <const Variant&> (item);
	}
	h_ << ";\n";
}

bool Client::constant (Code& stm, const Constant& item)
{
	stm << "const ";
	bool outline = false;
	switch (item.dereference_type ().tkind ()) {
		case Type::Kind::STRING:
		case Type::Kind::FIXED:
			outline = true;
			stm << "char* const";
			break;
		case Type::Kind::WSTRING:
			outline = true;
			stm << "whar_t* const";
			break;
		default:
			stm << static_cast <const Type&> (item);
	}
	return outline;
}

void Client::begin (const Exception& item)
{
	h_namespace_open (item);
	h_.empty_line ();
	h_ << "class " << item.name () << " : public " << Namespace ("CORBA") << "UserException\n"
		"{\n"
		"public:\n";
	h_.indent ();
	h_ << "NIRVANA_EXCEPTION_DCL (" << item.name () << ");\n\n";
}

void Client::end (const Exception& item)
{
	Members members = get_members (item);
	if (!members.empty ()) {

		// Explicit constructor
		explicit_constructor (item.name (), members);
		h_.indent ();
		auto it = members.begin ();
		h_ << "_data (std::move (" << (*it)->name () << ')';
		for (++it; it != members.end (); ++it) {
			h_ << ", std::move (" << (*it)->name () << ')';
		}
		h_.unindent ();
		h_ << ")\n{}\n";

		// Accessors to members of _data
		accessors (members, "_data._");

		h_ << "\n"
			"struct _Data\n"
			"{\n";

		h_.indent ();
		{
			// Use some trick to prevent removal of the leading underscore in the Identifier constructor
			string name ("_Data");
			constructors_and_assignments (static_cast <const Identifier&> (name), members);
			member_variables (members);
		}
		h_.unindent ();

		h_ << "};\n\n";

		h_.unindent ();
		h_ << "private:\n";
		h_.indent ();

		h_ << "virtual void* __data () NIRVANA_NOEXCEPT\n"
			"{\n";
		h_.indent ();
		h_ << "return &_data;\n";
		h_.unindent ();
		h_ << "}\n\n";

		h_ << "_Data _data;\n";
	}
	h_.unindent ();
	h_ << "};\n";

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
	cpp_.empty_line ();
	cpp_ << "NIRVANA_EXCEPTION_DEF (" << ParentName (item) << ", " << item.name () << ")\n\n";

	implement_nested_items (item);
}

Code& Client::member_type_prefix (const Type& t)
{
	h_ << "Type <" << TypePrefix (t) << "Member>::";
	return h_;
}

void Client::rep_id_of (const RepositoryId& rid)
{
	const NamedItem& item = rid.item ();
	if (!is_pseudo (item)) {
		h_.namespace_open ("CORBA/Nirvana");
		h_.empty_line ();
		h_ << "template <>\n"
			"const Char RepIdOf <" << QName (item) << ">::repository_id_ [] = \"" << rid.repository_id () << "\";\n\n";
	}
}

void Client::define_structured_type (const RepositoryId& rid, const Members& members, const char* suffix)
{
	rep_id_of (rid);

	const NamedItem& item = rid.item ();

	// ABI
	h_ << "template <>\n"
		"struct ABI < " << QName(item) << suffix << ">\n"
		"{\n";
	h_.indent ();
	for (auto member : members) {
		member_type_prefix (*member) << "ABI " << member->name () << ";\n";
	}
	h_.unindent ();
	h_ << "};\n\n";

	auto vl_member = members.begin ();
	for (; vl_member != members.end (); ++vl_member) {
		if (is_var_len (**vl_member))
			break;
	}
	// Type
	h_ << "template <>\n"
		"struct Type < " << QName (item) << suffix << "> : ";
	if (vl_member != members.end ()) {
		h_ << "TypeVarLen < " << QName (item) << suffix << ", \n";
		h_.indent ();

		member_type_prefix (**vl_member) << "has_check";

		while (++vl_member != members.end ()) {
			if (is_var_len (**vl_member)) {
				h_ << "\n| ";
				member_type_prefix (**vl_member) << "has_check";
			}
		}

		h_ << ">\n";
		h_.unindent ();
		h_ << "{\n";
		h_.indent ();
		h_ << "static void check (const ABI& val)\n"
			"{\n";
		h_.indent ();
		for (auto member : members) {
			member_type_prefix (*member) << "check (val." << member->name () << ");\n";
		}
		h_.unindent ();
		h_ << "}\n";

		if (!*suffix) {
			h_ << endl;
			type_code_func (item);
		}

		h_ << "\n"
			"static void marshal_in (const Var& src, Marshal_ptr marshaler, ABI& dst)\n"
			"{\n";
		h_.indent ();
		for (auto m : members) {
			member_type_prefix (*m) << "marshal_in (src._" << m->name () << ", marshaler, dst." << m->name () << ");\n";
		}
		h_.unindent ();
		h_ << "}\n\n"
			"static void marshal_out (Var& src, Marshal_ptr marshaler, ABI& dst)\n"
			"{\n";
		h_.indent ();
		for (auto m : members) {
			member_type_prefix (*m) << "marshal_out (src._" << m->name () << ", marshaler, dst." << m->name () << ");\n";
		}
		h_.unindent ();
		h_ << "}\n\n"
			"static void unmarshal (const ABI& src, Unmarshal_ptr unmarshaler, " << "Var& dst)\n"
			"{\n";
		h_.indent ();
		for (auto m : members) {
			member_type_prefix (*m) << "unmarshal (src." << m->name () << ", unmarshaler, dst._" << m->name () << ");\n";
		}
		h_.unindent ();
		h_ << "}\n";
		h_.unindent ();
		h_ << "};\n";
	} else {
		h_ << "TypeByRef < " << QName (item) << suffix << "> {};\n";
	}
}

void Client::leaf (const StructDecl& item)
{
	forward_decl (item);
}

void Client::begin (const Struct& item)
{
	h_namespace_open (item);
	h_.empty_line ();
	h_ << "class " << item.name () << "\n"
		"{\n"
		"public:\n";
	h_.indent ();
}

void Client::end (const Struct& item)
{
	Members members = get_members (item);

	constructors_and_assignments (item.name (), members);

	accessors (members);

	// Member variables
	h_.unindent ();
	h_.empty_line ();
	h_ << "private:\n";
	h_.indent ();
	h_ << "friend struct " << Namespace ("CORBA/Nirvana") << "Type <" << item.name () << ">;\n";
	member_variables (members);
	h_.unindent ();
	h_ << "};\n";

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

void Client::constructors_and_assignments (const Identifier& name, const Members& members)
{
	assert (!members.empty ());
	// Default constructor
	h_ << name << " () :\n";
	h_.indent ();
	auto it = members.begin ();
	h_ << MemberDefault (**it);
	for (++it; it != members.end (); ++it) {
		h_ << ",\n" << MemberDefault (**it);
	}
	h_.unindent ();
	h_ << "\n{}\n";

	// Constructors and assignments
	h_ << name << " (const " << name << "&) = default;\n"
		<< name << " (" << name << "&&) = default;\n"
		<< name << "& operator = (const " << name << "&) = default;\n"
		<< name << "& operator = (" << name << "&&) = default;\n";

	explicit_constructor (name, members);
	h_.indent ();
	it = members.begin ();
	h_ << MemberInit (**it);
	for (++it; it != members.end (); ++it) {
		h_ << ",\n" << MemberInit (**it);
	}
	h_.unindent ();
	h_ << "\n{}\n";
}

void Client::explicit_constructor (const AST::Identifier& name, const Members& members)
{
	h_ << "explicit " << name << " (";
	auto it = members.begin ();
	h_ << Var (**it) << ' ' << (*it)->name ();
	for (++it; it != members.end (); ++it) {
		h_ << ", " << Var (**it) << ' ' << (*it)->name ();
	}
	h_ << ") :\n";
}

void Client::accessors (const Members& members, const char* prefix)
{
	// Accessors
	for (const Member* m : members) {
		h_.empty_line ();

		h_ << TypePrefix (*m) << "ConstRef " << m->name () << " () const\n"
			"{\n";
		h_.indent ();
		h_ << "return " << prefix << m->name () << ";\n";
		h_.unindent ();
		h_ << "}\n";

		if (is_var_len (*m)) {
			h_ << Var (*m) << "& " << m->name () << " ()\n"
				"{\n";
			h_.indent ();
			h_ << "return " << prefix << m->name () << ";\n";
			h_.unindent ();
			h_ << "}\n";
		}

		h_ << "void " << m->name () << " (" << TypePrefix (*m) << "ConstRef val)\n"
			"{\n";
		h_.indent ();
		h_ << prefix << m->name () << " = val;\n";
		h_.unindent ();
		h_ << "}\n";

		if (is_var_len (*m)) {
			// The move setter
			h_ << "void " << m->name () << " (" << Var (*m) << "&& val)\n"
				"{\n";
			h_.indent ();
			h_ << prefix << m->name () << " = std::move (val);\n";
			h_.unindent ();
			h_ << "}\n";
		}
	}
}

void Client::member_variables (const Members& members)
{
	h_.empty_line ();
	for (const Member* m : members) {
		h_ << TypePrefix (*m) << "Member _" << m->name () << ";\n";
	}
}

Code& operator << (Code& stm, const Client::MemberDefault& m)
{
	stm << '_' << static_cast <const string&> (m.member.name ()) << " (";
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

Code& operator << (Code& stm, const Client::MemberInit& m)
{
	stm << '_' << static_cast <const string&> (m.member.name ()) << " (std::move ("
		<< m.member.name () << "))";
	return stm;
}

void Client::implement (const Union& item)
{
	// TODO:: Implement
	implement_nested_items (item);
}

void Client::leaf (const Enum& item)
{
	h_namespace_open (item);
	h_.empty_line ();
	h_ << "enum class " << item.name () << " : " << Namespace ("CORBA/Nirvana") << "ABI_enum\n"
		"{\n";
	h_.indent ();
	auto it = item.begin ();
	h_ << (*it)->name ();
	for (++it; it != item.end (); ++it) {
		h_ << ",\n" << (*it)->name ();
	}
	h_.unindent ();
	h_ << "\n};\n";
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
		"{};\n";
}
