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
	h_ << "const ::Nirvana::ImportInterfaceT < ::CORBA::TypeCode> _tc_" << item.name () << ";\n";
}

void Client::type_code_def (const RepositoryId& type)
{
	cpp_.namespace_open (type.item ());
	cpp_.empty_line ();
	cpp_ << "NIRVANA_OLF_SECTION extern const ::Nirvana::ImportInterfaceT < ::CORBA::TypeCode>\n";
	cpp_ << qualified_parent_name (type.item (), false) << "_tc_" << type.item ().name ()
		<< " = { ::Nirvana::OLF_IMPORT_INTERFACE, \"" << type.repository_id ()
		<< "\", ::CORBA::TypeCode::repository_id_ };\n\n";
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
	type_code_decl (item);
	type_code_def (item);
}

void Client::interface_forward (const NamedItem& item, InterfaceKind ik)
{
	h_.namespace_open (item);
	h_.empty_line ();
	h_ << "class " << item.name () << ";\n";
	h_ << "typedef CORBA::Nirvana::I_ptr <" << item.name () << "> " << item.name () << "_ptr;\n";
	h_ << "typedef CORBA::Nirvana::I_var <" << item.name () << "> " << item.name () << "_var;\n";
	h_ << "typedef CORBA::Nirvana::I_out <" << item.name () << "> " << item.name () << "_out;\n";
	if (ik.interface_kind () != InterfaceKind::PSEUDO)
		type_code_decl (item);
}

void Client::leaf (const InterfaceDecl& itf)
{
	interface_forward (itf, itf);
}

void Client::begin (const Interface& itf)
{
	// Forward declarations
	interface_forward (itf, itf);

	if (itf.interface_kind () != InterfaceKind::PSEUDO)
		type_code_def (itf);

	h_.namespace_open (internal_namespace_);
	h_ << "template <>\n"
		"struct Definitions < " << qualified_name (itf) << ">\n"
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
	h_ << "BRIDGE_BEGIN (" << qualified_name (itf) << ", \"" << itf.repository_id () << "\")\n";

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
		h_ << "BASE_STRUCT_ENTRY (" << qualified_name (*b) << ", " << proc_name << ")\n";
	}

	if (itf.interface_kind () != InterfaceKind::PSEUDO || !bases.empty ())
		h_ << "BRIDGE_EPV\n";

	for (auto it = itf.begin (); it != itf.end (); ++it) {
		const Item& item = **it;
		switch (item.kind ()) {

			case Item::Kind::OPERATION: {
				const Operation& op = static_cast <const Operation&> (item);
				bridge_ret (h_, op);

				h_ << " (*" << op.name () << ") (Bridge < " << qualified_name (itf) << ">* _b";

				for (auto it = op.begin (); it != op.end (); ++it) {
					h_ << ", ";
					bridge_param (h_, **it);
				}

				h_ << ", Interface* _env);\n";

			} break;

			case Item::Kind::ATTRIBUTE: {
				const Attribute& att = static_cast <const Attribute&> (item);
				bridge_ret (h_, att);
				h_ << " (*_get_" << att.name () << ") (Bridge < " << qualified_name (itf) << ">*, Interface*);\n";

				if (!att.readonly ()) {
					h_ << "void (*_set_" << att.name () << ") (Bridge < " << qualified_name (itf) << ">* _b, ";
					bridge_param (h_, att);
					h_ << ", Interface* _env);\n";
				}

			} break;
		}
	}

	h_ << "BRIDGE_END ()\n"
		"\n" // Client interface
		"template <class T>\n"
		"class Client <T, " << qualified_name (itf) << "> :\n";
	h_.indent ();
	h_ << "public T,\n"
		"public Definitions <" << qualified_name (itf) << ">\n";
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
					client_param (**it);
					++it;
					for (; it != op.end (); ++it) {
						h_ << ", ";
						client_param (**it);
					}
				}

				h_ << ");\n";

			} break;

			case Item::Kind::ATTRIBUTE: {
				const Attribute& att = static_cast <const Attribute&> (item);

				h_ << static_cast <const Type&> (att) << ' ' << att.name () << " ();\n";

				if (!att.readonly ()) {
					h_ << "void " << att.name () << " (";
					client_param (att);
					h_ << ");\n";
				}

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

				h_ << static_cast <const Type&> (op) << " Client <T, " << qualified_name (itf) << ">::" << op.name () << " (";

				auto it = op.begin ();
				if (it != op.end ()) {
					client_param (**it);
					++it;
					for (; it != op.end (); ++it) {
						h_ << ", ";
						client_param (**it);
					}
				}

				h_ << ")\n"
					"{\n";
				h_.indent ();

				environment (op.raises ());
				h_ << "Bridge < " << qualified_name (itf) << ">& _b (T::_get_bridge (_env));\n";

				if (op.tkind () != Type::Kind::VOID)
					h_ << "Type <" << static_cast <const Type&> (op) << ">::C_ret _ret = ";

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

				h_ << static_cast <const Type&> (att) << " Client <T, " << qualified_name (itf) << ">::" << att.name () << " ()\n"
					"{\n";

				h_.indent ();

				environment (att.getraises ());
				h_ << "Bridge < " << qualified_name (itf) << ">& _b (T::_get_bridge (_env));\n"
					"Type <" << static_cast <const Type&> (att) << ">::C_ret" << " _ret = (_b._epv ().epv._get_" << att.name () << ") (&_b, &_env);\n"
					"_env.check ();\n"
					"return _ret;\n";

				h_.unindent ();
				h_ << "}\n";

				if (!att.readonly ()) {
					h_ << "\ntemplate <class T>\n";

					h_ << "void Client <T, " << qualified_name (itf) << ">::" << att.name () << " (";
					client_param (att);
					h_ << " val)\n"
						"{\n";

					h_.indent ();

					environment (att.setraises ());
					h_ << "Bridge < " << qualified_name (itf) << ">& _b (T::_get_bridge (_env));\n";

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
		h_ << ", " << qualified_name (*b);
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

void Client::client_param (const Parameter& param)
{
	client_param (param, param.attribute ());
	h_ << ' ' << param.name ();
}

void Client::client_param (const Type& t, Parameter::Attribute att)
{
	h_ << "Type < " << t;
	switch (att) {
		case Parameter::Attribute::IN:
			h_ << ">::C_in";
			break;
		case Parameter::Attribute::OUT:
			h_ << ">::C_out";
			break;
		case Parameter::Attribute::INOUT:
			h_ << ">::C_inout";
			break;
	}
}

void Client::environment (const AST::Raises& raises)
{
	if (raises.empty ())
		h_ << "Environment _env;\n";
	else {
		h_ << "EnvironmentEx < ";
		auto it = raises.begin ();
		h_ << qualified_name (**it);
		for (++it; it != raises.end (); ++it) {
			h_ << ", " << qualified_name (**it);
		}
		h_ << "> _env;\n";
	}
}

void Client::leaf (const Constant& item)
{
	h_namespace_open (item);
	bool outline = constant (h_, item);
	h_ << ' ' << item.name ();
	if (outline) {
		cpp_.namespace_open (item);
		constant (cpp_, item);
		cpp_ << ' ' << qualified_name (item, false) << " = ";
		value (cpp_, item);
		cpp_ << ";\n";
	} else {
		h_ << " = ";
		value (h_, item);
	}
	h_ << ";\n";
}

bool Client::constant (Code& stm, const Constant& item)
{
	stm.empty_line ();
	stm << "const ";
	bool outline = false;
	switch (item.dereference_type ().tkind ()) {
		case Type::Kind::STRING:
		case Type::Kind::FIXED:
			outline = true;
			stm << "Char* const";
			break;
		case Type::Kind::WSTRING:
			outline = true;
			stm << "WChar* const";
			break;
		default:
			stm << static_cast <const Type&> (item);
	}
	return outline;
}

void Client::value (ofstream& stm, const Variant& var)
{
	switch (var.vtype ()) {

		case Variant::VT::FIXED:
			stm << '"' << var.to_string () << '"';
			break;

		case Variant::VT::ENUM_ITEM: {
			const EnumItem& item = var.as_enum_item ();
			ScopedName sn = item.scoped_name ();
			sn.insert (sn.begin () + sn.size () - 1, item.enum_type ().name ());
			stm << sn.stringize ();
		} break;

		default:
			stm << var.to_string ();
	}
}

void Client::begin (const Exception& item)
{
	h_namespace_open (item);
	h_.empty_line ();
	h_ << "class " << item.name () << " : public ::CORBA::UserException\n"
		"{\n"
		"public:\n";
	h_.indent ();
	h_ << "DECLARE_EXCEPTION (" << item.name () << ");\n\n";
}

ostream& Client::type_prefix (const Type& t)
{
	h_ << "::CORBA::Nirvana::Type <" << t << ">::";
	return h_;
}

void Client::end (const Exception& item)
{
	Members members = get_members (item);
	if (!members.empty ()) {
		for (const Member* m : members) {
			h_.empty_line ();
			type_prefix (*m) << "Member_ret " << m->name () << " () const\n"
				"{\n";
			h_.indent ();
			h_ << "return _data." << m->name () << " ();\n";
			h_.unindent ();
			h_ << "}\n"
				"void " << m->name () << " (";
			type_prefix (*m) << "C_in val)\n"
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
		struct_end (static_cast <const Identifier&> (string ("_Data")), members);
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
	define_type (qualified_name (item) + "::_Data", members);
	type_code_def (item);

	// Define exception
	cpp_.namespace_open (item);
	cpp_.empty_line ();
	cpp_ << "const char " << qualified_name (item, false) << "::repository_id_ [] = \""
		<< item.repository_id () << "\";\n"
		"const " << qualified_name (item, false) << "* " << qualified_name (item)
		<< "::_downcast (const ::CORBA::Exception* ep) NIRVANA_NOEXCEPT {\n";
	cpp_.indent ();
	cpp_ << "return (ep && ::CORBA::Nirvana::RepositoryId::compatible (ep->_rep_id (), "
		<< qualified_name (item, false) << "::repository_id_)) ? &static_cast <const "
		<< qualified_name (item, false) << "&> (*ep) : nullptr;\n";
	cpp_.unindent ();
	cpp_ << "}\n\n";
}

std::ostream& Client::member_type_prefix (const AST::Type& t)
{
	h_ << "Type <Type < " << t << ">::Member_type>::";
	return h_;
}

void Client::define_type (const std::string& fqname, const Members& members)
{
	h_.namespace_open (internal_namespace_);
	h_.empty_line ();

	// ABI
	h_ << "template <>\n"
		"struct ABI < " << fqname << ">\n"
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
		"struct Type < " << fqname << "> : ";
	if (vl_member != members.end ()) {
		h_ << "TypeVarLen < " << fqname << ", \n";
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
		h_.unindent ();
		h_ << "};\n";
	} else {
		h_ << "TypeFixLen < " << fqname << "> {};\n";
	}
}

void Client::leaf (const StructDecl& item)
{
	h_namespace_open (item);
	h_ << "class " << item.name () << ";\n";
	type_code_decl (item);
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
	define_type (qualified_name (item), members);
	type_code_def (item);
}

void Client::struct_end (const Identifier& name, const Members& members)
{
	// Default constructor
	h_ << name << " ()";
	const char* def_val = nullptr;
	auto it = members.begin ();
	for (; it != members.end (); ++it) {
		if (def_val = default_value (**it))
			break;
	}
	if (def_val) {
		h_ << " :\n";
		h_.indent ();
		h_ << '_' << (*it)->name () << " (" << def_val << ')';
		for (++it; it != members.end (); ++it) {
			if (def_val = default_value (**it)) {
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
		type_prefix (*m) << "Member_ret " << m->name () << " () const\n"
			"{\n";
		h_.indent ();
		h_ << "return _" << m->name () << ";\n";
		h_.unindent ();
		h_ << "}\n"
			"void " << m->name () << " (";
		type_prefix (*m) << "C_in val)\n"
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
	h_ << "friend struct ::CORBA::Nirvana::MarshalTraits <" << name << ">;\n";
	for (const Member* m : members) {
		type_prefix (*m) << "Member_type _" << m->name () << ";\n";
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

	type_code_decl (item);

	if (!nested (item))
		implement (item);
}

void Client::implement (const Enum& item)
{
	h_.namespace_open (internal_namespace_);
	h_.empty_line ();
	h_ << "template <>\n"
		"struct Type < " << qualified_name (item) << "> : public TypeEnum < " << qualified_name (item)
		<< ", " << qualified_name (item) << "::" << item.back ()->name () << ">\n"
		"{};\n";
}
