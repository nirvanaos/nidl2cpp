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

void Client::end (const Root&)
{
	h_.close ();
	cpp_.close ();
}

void Client::leaf (const Include& item)
{
	h_ << "#include ";
	h_ << (item.system () ? '<' : '"');
	h_ << filesystem::path (item.file ()).replace_extension (".h").string ();
	h_ << (item.system () ? '>' : '"');
	h_ << endl;
}

void Client::type_code_decl (const NamedItem& item)
{
	if (is_pseudo (item))
		return;

	if (!nested (item))
		h_ << "extern ";
	h_ << "const ::Nirvana::ImportInterfaceT < ::CORBA::TypeCode> _tc_" << item.name () << ";\n";
}

void Client::type_code_def (const RepositoryId& rid)
{
	assert (cpp_.no_namespace ());
	const NamedItem& item = rid.item ();
	if (is_pseudo (item))
		return;

	cpp_.empty_line ();
	cpp_ << "NIRVANA_OLF_SECTION ";
	if (!nested (item))
		cpp_ << "extern ";
	cpp_ << "const Nirvana::ImportInterfaceT <CORBA::TypeCode>\n"
		<< ParentName (item) << "_tc_" << static_cast <const string&> (item.name ())
		<< " = { ::Nirvana::OLF_IMPORT_INTERFACE, ";

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

bool Client::nested (const AST::NamedItem& item)
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
	standard_typedefs (item);
	type_code_decl (item);
	type_code_def (item);
}

void Client::standard_typedefs (const AST::NamedItem& item)
{
	h_.namespace_open (item);
	h_ << "typedef ::CORBA::Nirvana::Type <" << item.name () << ">::C_var " << item.name () << "_var;\n"
		<< "typedef ::CORBA::Nirvana::Type <" << item.name () << ">::C_out " << item.name () << "_out;\n"
		<< "typedef ::CORBA::Nirvana::Type <" << item.name () << ">::C_inout " << item.name () << "_inout;\n";
}

void Client::forward_decl (const NamedItem& item)
{
	h_.namespace_open (item);
	h_.empty_line ();
	h_ << "class " << item.name () << ";\n";
	type_code_decl (item);
}

void Client::leaf (const InterfaceDecl& itf)
{
	forward_decl (itf);
}

void Client::begin (const Interface& itf)
{
	// Forward declarations
	forward_decl (itf);

	h_ << "typedef ::CORBA::Nirvana::I_ptr <" << itf.name () << "> " << itf.name () << "_ptr;\n"
		<< "typedef ::CORBA::Nirvana::I_var <" << itf.name () << "> " << itf.name () << "_var;\n"
		<< "typedef ::CORBA::Nirvana::I_out <" << itf.name () << "> " << itf.name () << "_out;\n"
		<< "typedef ::CORBA::Nirvana::I_inout <" << itf.name () << "> " << itf.name () << "_inout;\n";

	if (itf.interface_kind () != InterfaceKind::PSEUDO) {
		type_code_def (itf);

		h_.namespace_open (internal_namespace_);
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
			default:
				h_ << "TypeObject";
				break;
		}
		h_ << " < " << QName (itf) << ">\n"
			"{\n";
		h_.indent ();
		h_ << "static TypeCode_ptr type_code ()\n"
			"{\n";
		h_.indent ();
		h_ << "return " << ParentName (itf) << "_tc_" << static_cast <const string&> (itf.name ()) << ";\n";
		h_.unindent ();
		h_ << "}\n";
		h_.unindent ();
		h_ << "};\n";
	}
	h_.namespace_open (internal_namespace_);
	h_.empty_line ();
	h_ << "template <>\n"
		"struct Definitions < " << QName (itf) << ">\n"
		"{\n";
	h_.indent ();
}

void Client::end (const Interface& itf)
{
	// Close struct Definitions
	h_.unindent ();
	h_ << "};\n";

	// Implement nested items
	for (auto it = itf.begin (); it != itf.end (); ++it) {
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

	// Bridge
	h_.namespace_open (internal_namespace_);
	h_.empty_line ();
	h_ << "BRIDGE_BEGIN (" << QName (itf) << ", \"" << itf.repository_id () << "\")\n";

	switch (itf.interface_kind ()) {
		case InterfaceKind::UNCONSTRAINED:
		case InterfaceKind::LOCAL:
			h_ << "BASE_STRUCT_ENTRY (Object, CORBA_Object)\n";
			break;
		case InterfaceKind::ABSTRACT:
			h_ << "BASE_STRUCT_ENTRY (AbstractBase, CORBA_AbstractBase)\n";
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
		h_ << "BASE_STRUCT_ENTRY (" << QName (*b) << ", " << proc_name << ")\n";
	}

	if (itf.interface_kind () != InterfaceKind::PSEUDO || !bases.empty ())
		h_ << "BRIDGE_EPV\n";

	for (auto it = itf.begin (); it != itf.end (); ++it) {
		const Item& item = **it;
		switch (item.kind ()) {

			case Item::Kind::OPERATION: {
				const Operation& op = static_cast <const Operation&> (item);
				h_ << TypeABI_ret (op) << " (*" << op.name () << ") (Bridge < " << QName (itf) << ">* _b";

				for (auto it = op.begin (); it != op.end (); ++it) {
					h_ << ", " << TypeABI_param (**it);
				}

				h_ << ", Interface* _env);\n";

			} break;

			case Item::Kind::ATTRIBUTE: {
				const Attribute& att = static_cast <const Attribute&> (item);
				h_ << TypeABI_ret (att) << " (*_get_" << att.name () << ") (Bridge < " << QName (itf) << ">*, Interface*);\n";

				if (!att.readonly ()) {
					h_ << "void (*_set_" << att.name () << ") (Bridge < " << QName (itf) << ">* _b, " << TypeABI_param (att) << ", Interface* _env);\n";
				}

			} break;
		}
	}

	h_ << "BRIDGE_END ()\n"
		"\n" // Client interface
		"template <class T>\n"
		"class Client <T, " << QName (itf) << "> :\n";
	h_.indent ();
	h_ << "public T,\n"
		"public Definitions <" << QName (itf) << ">\n";
	h_.unindent ();
	h_ << "{\n"
		"public:\n";
	h_.indent ();

	for (auto it = itf.begin (); it != itf.end (); ++it) {
		const Item& item = **it;
		switch (item.kind ()) {

			case Item::Kind::OPERATION: {
				const Operation& op = static_cast <const Operation&> (item);

				h_ << static_cast <const Type&> (op) << ' ' << op.name () << " (";

				auto it = op.begin ();
				if (it != op.end ()) {
					h_ << TypeC_param (**it);
					++it;
					for (; it != op.end (); ++it) {
						h_ << ", " << TypeC_param (**it);
					}
				}

				h_ << ");\n";

			} break;

			case Item::Kind::ATTRIBUTE: {
				const Attribute& att = static_cast <const Attribute&> (item);

				h_ << static_cast <const Type&> (att) << ' ' << att.name () << " ();\n";

				if (!att.readonly ())
					h_ << "void " << att.name () << " (" << TypeC_param (att) << ");\n";

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

				h_ << static_cast <const Type&> (op) << " Client <T, " << QName (itf) << ">::" << op.name () << " (";

				auto it = op.begin ();
				if (it != op.end ()) {
					h_ << TypeC_param (**it) << ' ' << (*it)->name ();
					++it;
					for (; it != op.end (); ++it) {
						h_ << ", " << TypeC_param (**it) << ' ' << (*it)->name ();
					}
				}

				h_ << ")\n"
					"{\n";
				h_.indent ();

				environment (op.raises ());
				h_ << "Bridge < " << QName (itf) << ">& _b (T::_get_bridge (_env));\n";

				if (op.tkind () != Type::Kind::VOID)
					h_ << TypePrefix (op) << "C_ret _ret = ";

				h_ << "(_b._epv ().epv." << op.name () << ") (&_b";
				for (it = op.begin (); it != op.end (); ++it) {
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

				h_ << static_cast <const Type&> (att) << " Client <T, " << QName (itf) << ">::" << att.name () << " ()\n"
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

					h_ << "void Client <T, " << QName (itf) << ">::" << att.name () << " (" << TypeC_param (att) << " val)\n"
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
	h_ << "class " << itf.name () << " : public CORBA::Nirvana::ClientInterface <" << itf.name ();
	for (auto b : bases) {
		h_ << ", " << QName (*b);
	}
	switch (itf.interface_kind ()) {
		case InterfaceKind::UNCONSTRAINED:
		case InterfaceKind::LOCAL:
			h_ << ", ::CORBA::Object";
			break;
		case InterfaceKind::ABSTRACT:
			h_ << ", ::CORBA::AbstractBase";
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
				h_ << "using ::CORBA::Nirvana::Definitions <" << itf.name () << ">::" << def.name () << ";\n";
				if (RepositoryId::cast (&def))
					h_ << "using ::CORBA::Nirvana::Definitions <" << itf.name () << ">::_tc_" << def.name () << ";\n";
			}
		}
	}
	h_.unindent ();
	h_ << "};\n";
}

void Client::environment (const AST::Raises& raises)
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

	if (!nested (item))
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
	h_ << "class " << item.name () << " : public ::CORBA::UserException\n"
		"{\n"
		"public:\n";
	h_.indent ();
	h_ << "NIRVANA_EXCEPTION_DCL (" << item.name () << ");\n\n";
}

void Client::end (const Exception& item)
{
	Members members = get_members (item);
	if (!members.empty ()) {
		for (const Member* m : members) {
			h_.empty_line ();
			h_ << TypePrefix (*m) << "Member_ret " << m->name () << " () const\n"
				"{\n";
			h_.indent ();
			h_ << "return _data." << m->name () << " ();\n";
			h_.unindent ();
			h_ << "}\n"
				"void " << m->name () << " (" << TypeC_param (*m) << " val)\n"
				"{\n";
			h_.indent ();
			h_ << "_data." << m->name () << "(val);\n";
			h_.unindent ();
			h_ << "}\n";
		}

		h_ << "\nclass _Data\n"
			"{\n"
			"public:\n";

		h_.indent ();
		{
			string name ("_Data");
			struct_end (static_cast <const Identifier&> (name), members);
		}
		h_.unindent ();

		h_ << "\nprivate:\n";
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

void Client::implement (const AST::Exception& item)
{
	Members members = get_members (item);
	define_type (item, members, "::_Data");
	type_code_def (item);

	// Define exception
	assert (cpp_.no_namespace ());
	cpp_.empty_line ();
	cpp_ << "NIRVANA_EXCEPTION_DEF (" << ParentName (item) << ", " << item.name () << ")\n\n";
}

std::ostream& Client::member_type_prefix (const AST::Type& t)
{
	h_ << "Type <Type < " << t << ">::Member_type>::";
	return h_;
}

void Client::define_type (const AST::NamedItem& item, const Members& members, const char* suffix)
{
	h_.namespace_open (internal_namespace_);
	h_.empty_line ();

	// ABI
	h_ << "template <>\n"
		"struct ABI < " << QName(item) << suffix << ">\n"
		"{\n";
	h_.indent ();
	for (auto member : members) {
		member_type_prefix (*member) << "ABI_type " << member->name () << ";\n";
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
		h_ << "static void check (const ABI_type& val)\n"
			"{\n";
		h_.indent ();
		for (auto member : members) {
			member_type_prefix (*member) << "check (val." << member->name () << ");\n";
		}
		h_.unindent ();
		h_ << "}\n";

		h_ << "static const bool has_marshal = " << (is_var_len (members) ? "true" : "false") << ";\n\n"
			"static void marshal_in (const " << QName (item) << suffix << "& src, Marshal_ptr marshaler, Type < " << QName (item) << suffix << ">::ABI_type& dst)\n"
			"{\n";
		h_.indent ();
		for (auto m : members) {
			h_ << "_marshal_in (src._" << m->name () << ", marshaler, dst." << m->name () << ");\n";
		}
		h_.unindent ();
		h_ << "}\n\n"
			"static void marshal_out (" << QName (item) << suffix << "& src, Marshal_ptr marshaler, Type < " << QName (item) << suffix << ">::ABI_type& dst)\n"
			"{\n";
		h_.indent ();
		for (auto m : members) {
			h_ << "_marshal_out (src._" << m->name () << ", marshaler, dst." << m->name () << ");\n";
		}
		h_.unindent ();
		h_ << "}\n\n"
			"static void unmarshal (const Type < " << QName (item) << suffix << ">::ABI_type& src, Unmarshal_ptr unmarshaler, " << QName (item) << suffix << "& dst)\n"
			"{\n";
		h_.indent ();
		for (auto m : members) {
			h_ << "_unmarshal (src." << m->name () << ", unmarshaler, dst._" << m->name () << ");\n";
		}
		h_.unindent ();
		h_ << "}\n";
		h_.unindent ();
		h_ << "};\n";
	} else {
		h_ << "TypeFixLen < " << QName (item) << suffix << "> {};\n";
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

	struct_end (item.name (), members);

	// Type code
	type_code_decl (item);

	if (!nested (item))
		implement (item);
}

void Client::implement (const Struct& item)
{
	Members members = get_members (item);
	define_type (item, members);
	standard_typedefs (item);
	type_code_def (item);
}

void Client::struct_end (const Identifier& name, const Members& members)
{
	// Default constructor
	h_ << name << " ()";
	const char* def_val = nullptr;
	auto it = members.begin ();
	for (; it != members.end (); ++it) {
		if ((def_val = default_value (**it)))
			break;
	}
	if (def_val) {
		h_ << " :\n";
		h_.indent ();
		h_ << '_' << (*it)->name () << " (" << def_val << ')';
		for (++it; it != members.end (); ++it) {
			if ((def_val = default_value (**it))) {
				h_ << ",\n"
					<< '_' << (*it)->name () << " (" << def_val << ')';
			}
		}
		h_.unindent ();
	}
	h_ << "\n {}\n";

	// Constructors and assignments
	h_ << name << "(const " << name << "&) = default;\n"
		<< name << "(" << name << "&&) = default;\n"
		<< name << "& operator = (const " << name << "&) = default;\n"
		<< name << "& operator = (" << name << "&&) = default;\n";

	// Accessors
	for (const Member* m : members) {
		h_.empty_line ();
		h_ << TypePrefix (*m) << "Member_ret " << m->name () << " () const\n"
			"{\n";
		h_.indent ();
		h_ << "return _" << m->name () << ";\n";
		h_.unindent ();
		h_ << "}\n"
			"void " << m->name () << " (" << TypeC_param (*m) << " val)\n"
			"{\n";
		h_.indent ();
		h_ << '_' << m->name () << " = val;\n";
		h_.unindent ();
		h_ << "}\n\n";
	}

	// Member variables
	h_.unindent ();
	h_ << "private:\n";
	h_.indent ();
	h_ << "friend struct ::CORBA::Nirvana::Type <" << name << ">;\n";
	for (const Member* m : members) {
		h_ << TypePrefix (*m) << "Member_type _" << m->name () << ";\n";
	}
	h_.unindent ();
	h_ << "};\n";
}

const char* Client::default_value (const Type& t)
{
	const Type& td = t.dereference_type ();
	if (td.tkind () == Type::Kind::BASIC_TYPE) {
		if (td.basic_type () == BasicType::BOOLEAN)
			return "false";
		else if (td.basic_type () < BasicType::OBJECT)
			return "0";
	}
	return nullptr;
}

void Client::implement (const Union& item)
{
	// TODO:: Implement
}

void Client::leaf (const Enum& item)
{
	h_namespace_open (item);
	h_.empty_line ();
	h_ << "enum class " << item.name () << " : ::CORBA::Nirvana::ABI_enum\n"
		"{\n";
	h_.indent ();
	auto it = item.begin ();
	h_ << (*it)->name ();
	for (++it; it != item.end (); ++it) {
		h_ << ",\n" << (*it)->name ();
	}
	h_.unindent ();
	h_ << "\n};\n";
	standard_typedefs (item);
	type_code_decl (item);

	if (!nested (item))
		implement (item);
}

void Client::implement (const Enum& item)
{
	h_.namespace_open (internal_namespace_);
	h_.empty_line ();
	h_ << "template <>\n"
		"struct Type < " << QName (item) << "> : public TypeEnum < " << QName (item)
		<< ", " << QName (item) << "::" << item.back ()->name () << ">\n"
		"{};\n";
}
