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
	h_ << "extern const ::Nirvana::ImportInterfaceT <::CORBA::TypeCode> _tc_" << item.name () << ";\n";
}

void Client::type_code_def (const RepositoryId& type)
{
	cpp_ << "NIRVANA_OLF_SECTION extern const ::Nirvana::ImportInterfaceT <::CORBA::TypeCode>\n";
	cpp_ << qualified_parent_name (type.item ()) << "_tc_" << type.item ().name () << " = { ::Nirvana::OLF_IMPORT_INTERFACE, \"" << type.repository_id () << "\", ::CORBA::TypeCode::repository_id_ };\n";
}

void Client::leaf (const TypeDef& item)
{
	h_.namespace_open (item);
	h_ << "typedef ";
	type (h_, item);
	h_ << ' ' << item.name () << ";\n";
	type_code_decl (item);
	type_code_def (item);
}

void Client::interface_forward (const NamedItem& item, InterfaceKind ik)
{
	h_.namespace_open (item);
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
		"struct Definitions < " << itf.qualified_name () << ">\n"
		"{\n";
	h_.indent ();
}

void Client::end (const Interface& itf)
{
	// Close struct Definitions
	h_.unindent ();
	h_ << "};\n";

	// Bridge
	h_.namespace_open (internal_namespace_);
	h_ << "BRIDGE_BEGIN (" << itf.qualified_name () << ", \"" << itf.repository_id () << "\")\n";

	switch (itf.interface_kind ()) {
		case InterfaceKind::UNCONSTRAINED:
		case InterfaceKind::LOCAL:
			h_ << "BASE_STRUCT_ENTRY (Object, CORBA_Object)\n";
			break;
		case InterfaceKind::ABSTRACT:
			h_ << "BASE_STRUCT_ENTRY (AbstractBase, CORBA_AbstractBase)\n";
			break;
	}

	for (auto b = itf.bases ().begin (); b != itf.bases ().end (); ++b) {
		ScopedName sn = (*b)->scoped_name ();
		ScopedName::const_iterator it = sn.begin ();
		string proc_name = *(it++);
		for (; it != sn.end (); ++it) {
			proc_name += '_';
			proc_name += *it;
		}
		h_ << "BASE_STRUCT_ENTRY (" << sn.stringize () << ", " << proc_name << ")\n";
	}

	if (itf.interface_kind () != InterfaceKind::PSEUDO)
		h_ << "BRIDGE_EPV\n";

	for (auto it = itf.begin (); it != itf.end (); ++it) {
		const Item& item = **it;
		switch (item.kind ()) {

			case Item::Kind::OPERATION: {
				const Operation& op = static_cast <const Operation&> (item);
				bridge_ret (h_, op);

				h_ << " (*" << op.name () << ") (Bridge < " << itf.qualified_name () << ">*";

				for (auto it = op.begin (); it != op.end (); ++it) {
					h_ << ", ";
					bridge_param (h_, **it);
				}

				h_ << ", Interface*);\n";

			} break;

			case Item::Kind::ATTRIBUTE: {
				const Attribute& att = static_cast <const Attribute&> (item);
				bridge_ret (h_, att);
				h_ << " (*_get_" << att.name () << ") (Bridge < " << itf.qualified_name () << ">*, Interface*);\n";

				if (!att.readonly ()) {
					h_ << "void (*_set_" << att.name () << ") (Bridge < " << itf.qualified_name () << ">*, ";
					bridge_param (h_, att);
					h_ << ", Interface*);\n";
				}

			} break;
		}
	}

	h_ << "BRIDGE_END ()\n";

	// Client interface

	h_ <<
		"template <class T>\n"
		"class Client <T, " << itf.qualified_name () << "> :\n";
	h_.indent ();
	h_ << "public T,\n"
		"public Definitions <" << itf.qualified_name () << ">\n";
	h_.unindent ();
	h_ << "{\n"
		"public:\n";
	h_.indent ();

	for (auto it = itf.begin (); it != itf.end (); ++it) {
		const Item& item = **it;
		switch (item.kind ()) {

			case Item::Kind::OPERATION: {
				const Operation& op = static_cast <const Operation&> (item);

				type (h_, op);
				h_ << ' ' << op.name () << " (";

				auto it = op.begin ();
				if (it != op.end ()) {
					client_param (h_, **it);
					++it;
					for (; it != op.end (); ++it) {
						h_ << ", ";
						client_param (h_, **it);
					}
				}

				h_ << ");\n";

			} break;

			case Item::Kind::ATTRIBUTE: {
				const Attribute& att = static_cast <const Attribute&> (item);

				type (h_, att);
				h_ << ' ' << att.name () << " () const;\n";

				if (!att.readonly ()) {
					h_ << "void " << att.name () << '(';
					client_param (h_, att);
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

				type (h_, op);
				h_ << " Client <T, " << itf.qualified_name () << ">::" << op.name () << " (";

				auto it = op.begin ();
				if (it != op.end ()) {
					client_param (h_, **it);
					++it;
					for (; it != op.end (); ++it) {
						h_ << ", ";
						client_param (h_, **it);
					}
				}

				h_ << ")\n"
					"{\n";
				h_.indent ();

				environment (op.raises ());
				h_ << "Bridge < " << itf.scoped_name () << ">& _b (T::_get_bridge (_env));\n";

				client_ret (h_, op);
				h_ << " _ret = (_b._epv ().epv." << op.name () << ") (&_b";
				for (it = op.begin (); it != op.end (); ++it) {
					h_ << ", &" << (*it)->name ();
				}
				h_ << ", &_env);\n"
					"_env.check ();\n"
					"return _ret;\n";

				h_.unindent ();
				h_ << "}\n";

			} break;

			case Item::Kind::ATTRIBUTE: {
				const Attribute& att = static_cast <const Attribute&> (item);

				h_ << "\ntemplate <class T>\n";

				type (h_, att);
				h_ << " Client <T, " << itf.qualified_name () << ">::" << att.name () << " () const\n"
					"{\n";

				h_.indent ();

				environment (att.getraises ());
				h_ << "Bridge < " << itf.scoped_name () << ">& _b (T::_get_bridge (_env));\n";

				client_ret (h_, att);
				h_ << " _ret = (_b._epv ().epv._get_" << att.name () << ") (&_b, &_env);\n"
					"_env.check ();\n"
					"return _ret;\n";

				h_.unindent ();
				h_ << "}\n";

				if (!att.readonly ()) {
					h_ << "\ntemplate <class T>\n";

					h_ << "void Client <T, " << itf.qualified_name () << ">::" << att.name () << '(';
					client_param (h_, att);
					h_ << " val)\n"
						"{\n";

					h_.indent ();

					environment (att.setraises ());
					h_ << "Bridge < " << itf.scoped_name () << ">& _b (T::_get_bridge (_env));\n";

					h_ << "(_b._epv ().epv._set_" << att.name () << ") (&_b, &val, &_env);\n"
						"_env.check ();\n"
						"return _ret;\n";

					h_.unindent ();
					h_ << "}\n";
				}

			} break;
		}
	}

	// Interface definition
	h_.namespace_open (itf);
	h_ << "class " << itf.name () << " : public CORBA::Nirvana::ClientInterface <" << itf.name ();
	for (auto b = itf.bases ().begin (); b != itf.bases ().end (); ++b) {
		h_ << ", " << (*b)->qualified_name ();
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
		h_ << (*it)->qualified_name ();
		for (++it; it != raises.end (); ++it) {
			h_ << ", " << (*it)->qualified_name ();
		}
		h_ << "> _env;\n";
	}
}

void Client::leaf (const Operation&)
{
	// Processed in begin (const Interface&);
}

void Client::leaf (const Attribute&)
{
	// Processed in begin (const Interface&);
}

void Client::leaf (const Constant& item)
{
	h_.namespace_open (item);
	bool outline = constant (h_, item);
	h_ << ' ' << item.name ();
	if (outline) {
		constant (cpp_, item);
		cpp_ << ' ' << qualified_name (item) << " = ";
		value (cpp_, item);
		cpp_ << ";\n";
	} else {
		h_ << " = ";
		value (h_, item);
	}
	h_ << ";\n";
}

bool Client::constant (ofstream& stm, const Constant& item)
{
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
			type (stm, item);
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
	h_ << "class " << item.name () << " : public ::CORBA::UserException\n"
		"{\n"
		"public:\n";
	h_.indent ();
	h_ << "DECLARE_EXCEPTION (" << item.name () << ");\n";
}

void Client::end (const Exception& item)
{
	Members members = get_members (item);
	if (!members.empty ()) {
		for (const Member* m : members) {
			h_ << "\n::CORBA::Nirvana::Type <";
			type (h_, *m);
			h_ << ">::Member_ret " << m->name () << " () const\n"
				"{\n";
			h_.indent ();
			h_ << "return _data." << m->name () << ";\n";
			h_.unindent ();
			h_ << "}\n";
			h_ << "void " << m->name () << " (::CORBA::Nirvana::Type <";
			type (h_, *m);
			h_ << ">::C_in val)\n"
				"{\n";
			h_.indent ();
			h_ << "_data." << m->name () << " = val;\n";
			h_.unindent ();
			h_ << "}\n";
		}

		h_ << "struct _Data\n"
			"{\n";
		h_.indent ();

		for (const Member* m : members) {
			h_ << "::CORBA::Nirvana::Type <";
			type (h_, *m);
			h_ << ">::Member_type " << m->name () << ";\n";
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
	h_ << "}\n";
	type_code_decl (item);
	type_code_def (item);
	cpp_ << "const char " << qualified_name (item) << "::repository_id_ [] = \"" << item.repository_id () << "\";\n"
		"const " << qualified_name (item) << "* " << qualified_name (item) << "::_downcast (const ::CORBA::Exception* ep) NIRVANA_NOEXCEPT {\n";
	cpp_.indent ();
	cpp_ << "return (ep && ::CORBA::Nirvana::RepositoryId::compatible (ep->_rep_id (), " << qualified_name (item) << "::repository_id_)) ? &static_cast <const "
		<< qualified_name (item) << "&> (*ep) : nullptr;\n";
	cpp_.unindent ();
	cpp_ << "}\n";
}

void Client::leaf (const StructDecl& item)
{
	h_.namespace_open (item);
	h_ << "class " << item.name () << ";\n";
	type_code_decl (item);
}

void Client::begin (const Struct& item)
{}
void Client::end (const Struct& item)
{}

void Client::leaf (const Member& item)
{}

void Client::leaf (const Enum& item)
{
	h_.namespace_open (item);
	h_ << "enum class " << item.name () << " : uint32_t\n"
		"{\n";
	h_.indent ();
	auto it = item.begin ();
	h_ << (*it)->name ();
	for (++it; it != item.end (); ++it) {
		h_ << ",\n" << (*it)->name ();
	}
	h_.unindent ();
	h_ << "};\n";
}
