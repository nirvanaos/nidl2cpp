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

using std::filesystem::path;
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

Code& operator << (Code& stm, const Client::PollerSignature& op)
{
	stm << op.op.name () << " (ULong " AMI_TIMEOUT;

	if (op.op.tkind () != Type::Kind::VOID)
		stm << ", " << TypePrefix (op.op) << "C_out " AMI_RETURN_VAL;

	// inout/out parameters
	for (auto param : op.op) {
		if (param->attribute () != Parameter::Attribute::IN)
			stm << ", " << TypePrefix (*param) << "C_out " << param->name ();
	}

	return stm << ')';
}

Code& operator << (Code& stm, const Client::SendcSignature& op)
{
	stm << AMI_SENDC << static_cast <const std::string&> (op.op.name ())
		<< " (" << TypePrefix (Type (&op.handler)) << "C_in " AMI_HANDLER;

	// in/inout parameters
	for (auto param : op.op) {
		if (param->attribute () != Parameter::Attribute::OUT)
			stm << ", " << TypePrefix (*param) << "C_in " << param->name ();
	}

	return stm << ')';
}

Code& operator << (Code& stm, const Client::SendpSignature& op)
{
	stm << AMI_SENDP << static_cast <const std::string&> (op.op.name ())
		<< " (";

	// in/inout parameters
	auto it = op.op.begin ();
	while (it != op.op.end () && (*it)->attribute () == Parameter::Attribute::OUT)
		++it;

	if (it != op.op.end ()) {
		stm << TypePrefix (**it) << "C_in " << (*it)->name ();
		for (++it; it != op.op.end (); ++it) {
			if ((*it)->attribute () != Parameter::Attribute::OUT)
				stm << ", " << TypePrefix (**it) << "C_in " << (*it)->name ();
		}
	}

	return stm << ')';
}

void Client::end (const Root&)
{
	h_.close ();
	cpp_.close ();
}

void Client::leaf (const Include& item)
{
	h_.namespace_close ();
	h_ << "#include " << (item.system () ? '<' : '"')
		<< path (path (item.file ()).replace_extension ("").string ()
			+ options ().client_suffix).replace_extension ("h").string ()
		<< (item.system () ? '>' : '"')
		<< std::endl;
}

void Client::type_code_decl (const NamedItem& item)
{
	if (is_pseudo (item))
		return;

	if (!is_nested (item)) {
		h_.namespace_open (item);
		h_ << "extern ";
	} else
		h_ << "static ";
	h_ << "const " << Namespace ("Nirvana") << "ImportInterfaceT <"
		<< Namespace ("CORBA") << "TypeCode> _tc_"
		<< static_cast <const std::string&> (item.name ()) << ";\n";
}

void Client::type_code_def (const ItemWithId& item)
{
	if (is_pseudo (item))
		return;

	cpp_.namespace_close ();
	cpp_ << empty_line
		<< "NIRVANA_OLF_SECTION_OPT";
	if (!is_nested (item))
		 cpp_ << " extern";
	cpp_ << " const " << Namespace ("Nirvana") << "ImportInterfaceT <" << Namespace ("CORBA") << "TypeCode>\n"
		<< TC_Name (item) << " = { Nirvana::OLF_IMPORT_INTERFACE, ";

	switch (item.kind ()) {
		case Item::Kind::TYPE_DEF:
			cpp_ << '"' << item.repository_id () << '"';
			break;
		default:
			cpp_ << "CORBA::Internal::RepIdOf <" << QName (item) << ">::id";
			break;
	}
	cpp_ << ", CORBA::Internal::RepIdOf <CORBA::TypeCode>::id };\n\n";
}

bool Client::is_nested (const NamedItem& item)
{
	return item.parent () && item.parent ()->kind () != Item::Kind::MODULE;
}

void Client::h_namespace_open (const NamedItem& item)
{
	if (!is_nested (item))
		h_.namespace_open (item);
}

void Client::leaf (const TypeDef& item)
{
	h_namespace_open (item);
	h_ << empty_line
		<< "typedef " << static_cast <const Type&> (item) << ' ' << item.name ()
		<< ";\n";
	
	const std::string& name = static_cast <const std::string&> (item.name ());

	if (options ().legacy) {
		// <Type>_var
		switch (item.dereference_type ().tkind ()) {
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
					"typedef " << static_cast <const std::string&> (item.named_type ().name ()) << "_var "
					<< name << "_var;\n";

				if (is_ref_type (item))
					h_ << "typedef " << static_cast <const std::string&> (item.named_type ().name ()) << "_ptr "
					<< name << "_ptr;\n";
				h_ << "#endif\n";
		}
	}

	if (!is_pseudo (item)) {

		// Define type code

		if (!is_nested (item))
			h_ << "extern ";
		else
			h_ << "static ";
		h_ << "const " << Namespace ("CORBA/Internal") << "Alias _tc_" << name << ";\n";

		cpp_.namespace_open ("CORBA/Internal");
		cpp_ << empty_line <<
			"template <>\n"
			"class TypeDef <&" << TC_Name (item) << "> :\n"
			<< indent
			<< "public TypeCodeTypeDef <&" << TC_Name (item) << ", ";
		if (item.tkind () == Type::Kind::NAMED_TYPE && item.named_type ().kind () == Item::Kind::TYPE_DEF)
			cpp_ << "TypeDef <&" << TC_Name (item.named_type ()) << '>';
		else
			cpp_ << static_cast <const Type&> (item);
		cpp_ << ">\n"
			<< unindent
			<< "{};\n";
		if (!is_nested (item)) {
			cpp_.namespace_open (item);
			cpp_ << "extern ";
		}
		cpp_ << "NIRVANA_SELECTANY\n"
			"const " << Namespace ("CORBA/Internal") << "Alias " << TC_Name (item)
			<< " { \"" << item.repository_id () << "\", \"" << name << "\",\n"
			"NIRVANA_STATIC_BRIDGE ("
			<< Namespace ("CORBA") << "TypeCode, "
			<< Namespace ("CORBA/Internal") << "TypeDef <&" << TC_Name (item) << ">) }; \n";
	}
}

void Client::backward_compat_var (const NamedItem& item)
{
	if (options ().legacy) {
		h_namespace_open (item);
		h_ <<
			"#ifdef LEGACY_CORBA_CPP\n"
			"typedef " << Namespace ("CORBA/Internal") << "T_var <" << item.name () << "> " << static_cast <const std::string&> (item.name ()) << "_var;\n"
			"#endif\n";
	}
}

void Client::forward_decl (const NamedItem& item)
{
	h_namespace_open (item);
	h_ << empty_line
		<< "class " << item.name () << ";\n";
	type_code_decl (item);
}

void Client::forward_decl (const AST::StructBase& item)
{
	forward_decl (static_cast <const NamedItem&> (item));
	if (item.kind () != Item::Kind::EXCEPTION)
		backward_compat_var (item);
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

void Client::forward_interface (const ItemWithId& item)
{
	h_.namespace_close ();
	forward_guard (item);
	forward_define (item);
	forward_decl (item);

	define_itf_suppl (item.name ());

	// Define type
	h_.namespace_open ("CORBA/Internal");
	h_ << empty_line
		<< "template <>\n"
		"struct Type <" << QName (item) << "> : ";

	bool has_type_code = true;
	InterfaceKind::Kind ikind;
	const ValueType* value_type = nullptr;
	const ValueTypeDecl* value_type_decl = nullptr;
	if (item.kind () == Item::Kind::INTERFACE_DECL || item.kind () == Item::Kind::INTERFACE) {
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
		if (item.kind () == Item::Kind::VALUE_TYPE_DECL)
			value_type_decl = &static_cast <const ValueTypeDecl&> (item);
		else
			value_type = &static_cast <const ValueType&> (item);
		h_ << "TypeValue";
	}

	h_ << " <" << QName (item) << ">\n"
		"{";

	if (has_type_code) {
		h_.indent ();
		type_code_func (item);
		h_.unindent ();
	}
	h_ << "};\n";

	rep_id_of (item);

	h_.namespace_close ();

	// Traits

	iv_traits_begin (item);

	if (value_type || value_type_decl || ikind == InterfaceKind::LOCAL) {
		h_ << "template <class S>\n"
			"using Servant = " << Namespace ("CORBA/Internal") << "Servant <S, " << QName (item) << ">;\n";

		if (value_type || value_type_decl)
			h_ << "using obv_type = " << Namespace ("CORBA/Internal") << "ServantPOA <" << QName (item) << ">;\n";
	}

	bool abstract;
	if (value_type)
		abstract = value_type->modifier () == ValueType::Modifier::ABSTRACT;
	else if (value_type_decl)
		abstract = value_type_decl->is_abstract ();
	else
		abstract = ikind == InterfaceKind::ABSTRACT;

	if (value_type || value_type_decl)
		h_ << "template <class S>\n"
		"using ServantStatic = " << Namespace ("CORBA/Internal") << "ServantStatic <S, " << QName (item) << ">;\n";

	h_ << "using is_abstract = std::" << (abstract ? "true_type" : "false_type") << ";\n";

	if (!(value_type || value_type_decl))
		h_ << "using is_local = std::" << (ikind == InterfaceKind::LOCAL ? "true_type" : "false_type") << ";\n";

	traits_end ();

	h_ << empty_line
		<< "#endif\n"; // Close forward guard
}

void Client::define_itf_suppl (const Identifier& name)
{
	// A namespace level swap must be provided
	h_ << "inline void swap ("
		<< Namespace ("CORBA/Internal") << "I_ref <" << name << ">& x, "
		<< Namespace ("CORBA/Internal") << "I_ref <" << name << ">& y)\n"
		"{\n" << indent <<
		Namespace ("std") << "swap (x, y);\n"
		<< unindent << "}\n";

	// Legacy type definitions
	if (options ().legacy) {
		h_ << "#ifdef LEGACY_CORBA_CPP\n"
			"typedef " << Namespace ("CORBA/Internal") << "I_ptr <" << name << "> "
			<< static_cast <const std::string&> (name) << "_ptr;\n"
			"typedef " << Namespace ("CORBA/Internal") << "I_var <" << name << "> "
			<< static_cast <const std::string&> (name) << "_var;\n"
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

void Client::begin_interface (const IV_Base& container)
{
	type_code_def (container);

	h_.namespace_open ("CORBA/Internal");
	h_ << empty_line
		<< "template <>\n"
		"struct Decls <" << QName (container) << "> : NativeDecls <" << QName (container) << ">\n"
		"{\n"
		<< indent;
}

void Client::end_interface (const IV_Base& container)
{
	// Close struct Decls
	h_ << unindent
		<< "};\n";

	implement_nested_items (container);

	const Compiler::AMI_Objects* ami = nullptr;
	if (container.kind () == Item::Kind::INTERFACE) {
		const Interface& itf = static_cast <const Interface&> (container);
		auto it = compiler ().ami_interfaces ().find (&itf);
		if (it != compiler ().ami_interfaces ().end ()) {
			ami = &it->second;
			generate_ami (itf);
		}
	}

	// Bridge
	h_.namespace_open ("CORBA/Internal");
	h_ << empty_line <<
		"NIRVANA_BRIDGE_BEGIN (" << QName (container) << ")\n";

	bool att_byref = false;
	bool pseudo_interface = false;
	Bases bases; // All bases, direct and indirect
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

		if (itf.interface_kind () != InterfaceKind::PSEUDO || !bases.empty ()) {
			if (ami)
				h_ << "NIRVANA_BRIDGE_AMI_EPV(" << QName (itf) << ")\n";
			else
				h_ << "NIRVANA_BRIDGE_EPV\n";
		}
	} else {
		const ValueType& vt = static_cast <const ValueType&> (container);
		att_byref = true;
		if ((concrete_itf = get_concrete_supports (vt))) {
			supports.push_back (concrete_itf);
			Interfaces itf_bases = concrete_itf->get_all_bases ();
			supports.insert (supports.end (), itf_bases.begin (), itf_bases.end ());
		}

		h_ << "NIRVANA_BASE_ENTRY (ValueBase, CORBA_ValueBase)\n";

		bases = get_all_bases (vt);
		bridge_bases (bases);
		bridge_bases (supports);
		h_ << "NIRVANA_BRIDGE_EPV\n";

		if (concrete_itf && concrete_itf->interface_kind () != InterfaceKind::PSEUDO)
			h_ << "Interface* (*_this) (Bridge <" << QName (vt) << ">*, Interface*);\n";
	}

	for (auto item : container) {
		switch (item->kind ()) {

			case Item::Kind::OPERATION: {
				const Operation& op = static_cast <const Operation&> (*item);

				if (is_native (op.raises ()))
					break;

				h_ << ABI_ret (op) << " (*" << op.name () << ") (Bridge <"
					<< QName (container) << ">*";

				for (auto param : op) {
					h_ << ", " << ABI_param (*param);
				}

				h_ << ", Interface*);\n";

			} break;

			case Item::Kind::STATE_MEMBER:
				if (!static_cast <const StateMember&> (*item).is_public ())
					break;
				[[fallthrough]];
			case Item::Kind::ATTRIBUTE: {
				const Member& m = static_cast <const Member&> (*item);
				const Attribute* att = nullptr;
				if (item->kind () == Item::Kind::ATTRIBUTE)
					att = &static_cast <const Attribute&> (m);

				if (!att || !is_native (att->getraises ())) {
					h_ << ABI_ret (m, att_byref)
						<< " (*_get_" << m.name () << ") (Bridge <" << QName (container)
						<< ">*, Interface*);\n";
				}

				if (!att || !(att->readonly () || is_native (att->setraises ()))) {
					h_ << "void (*_set_" << m.name () << ") (Bridge <" << QName (container)
						<< ">*, " << ABI_param (m) << ", Interface*);\n";
				}

				if (!att && is_var_len (m)) {
					h_ << "void (*_move_" << m.name () << ") (Bridge <" << QName (container)
						<< ">*, " << ABI_param (m, Parameter::Attribute::INOUT) << ", Interface*);\n";
				}
			} break;
		}
	}

	// Client interface
	h_ << "NIRVANA_BRIDGE_END ()\n"
		"\n"
		"template <class T>\n"
		"class Client <T, " << QName (container) << "> :\n"
		<< indent
		<< "public T,\n"
		"public Decls <" << QName (container) << ">\n"
		<< unindent
		<<
		"{\n"
		"public:\n"
		<< indent;

	if (concrete_itf && concrete_itf->interface_kind () != InterfaceKind::PSEUDO)
		h_ << "Type <" << QName (*concrete_itf) << ">::VRet _this ();\n";

	for (auto item : container) {
		switch (item->kind ()) {

			case Item::Kind::OPERATION: {
				const Operation& op = static_cast <const Operation&> (*item);

				h_ << VRet (op) << ' ' << Signature (op) << ";\n";

				native_itf_template (op);

				if (ami) {
					h_ << "void " << SendcSignature (op, *ami->handler) << ";\n"
						<< POLLER_TYPE_PREFIX "VRet " << SendpSignature (op) << ";\n";
				}

			} break;

			case Item::Kind::STATE_MEMBER:
				if (!static_cast <const StateMember&> (*item).is_public ())
					break;
				[[fallthrough]];
			case Item::Kind::ATTRIBUTE: {
				const Member& m = static_cast <const Member&> (*item);

				if (!att_byref)
					h_ << VRet (m);
				else
					h_ << ConstRef (m);
				h_ << ' ' << m.name () << " ();\n";

				if (ami) {
					h_ << "void " AMI_SENDC "get_" << static_cast <const std::string&> (m.name ())
						<< " (" << TypePrefix (Type (ami->handler)) << "C_in " AMI_HANDLER ");\n"
						POLLER_TYPE_PREFIX "VRet " AMI_SENDP "get_" << static_cast <const std::string&> (m.name ())
						<< " ();\n";
				}

				if (!(m.kind () == Item::Kind::ATTRIBUTE && static_cast <const Attribute&> (m).readonly ())) {
					h_ << "void " << m.name () << " (" << Param (m) << ");\n";

					if (ami) {
						h_ << "void " AMI_SENDC "set_" << static_cast <const std::string&> (m.name ())
							<< " (" << TypePrefix (Type (ami->handler)) << "C_in " AMI_HANDLER ", "
							<< Param (m) << ");\n"
							POLLER_TYPE_PREFIX "VRet " AMI_SENDP "set_" << static_cast <const std::string&> (m.name ())
							<< " (" << Param (m) << ");\n";
					}
				}

				if (m.kind () == Item::Kind::STATE_MEMBER && is_var_len (m))
					h_ << "void " << m.name () << " (" << Var (m) << "&&);\n";

			} break;
		}
	}

	h_ << unindent << "};\n";

	if (concrete_itf && concrete_itf->interface_kind () != InterfaceKind::PSEUDO) {
		h_ << "\ntemplate <class T>\n"
			"Type <" << QName (*concrete_itf) << ">::VRet Client <T, " << QName (container) << ">::_this ()\n"
			"{\n"
			<< indent;
		environment (Raises ());
		h_ << "Bridge < " << QName (container) << ">& _b (T::_get_bridge (_env));\n"
			"Type <" << QName (*concrete_itf) << ">::C_ret _ret ((_b._epv ().epv._this) (&_b, &_env));\n"
			"_env.check ();\n"
			"return _ret;\n"
			<< unindent <<
			"}\n";
	}

	// Client operations

	for (auto item : container) {
		switch (item->kind ()) {

			case Item::Kind::OPERATION: {
				const Operation& op = static_cast <const Operation&> (*item);

				if (is_native (op.raises ()))
					break;

				h_ << "\ntemplate <class T>\n"
					<< VRet (op) << " Client <T, " << QName (container) << ">::" << Signature (op) << "\n"
					"{\n" << indent;

				environment (op.raises ());
				h_ << "Bridge <" << QName (container) << ">& _b (T::_get_bridge (_env));\n";

				if (op.tkind () != Type::Kind::VOID)
					h_ << TypePrefix (op) << "C_ret _ret (";

				h_ << "(_b._epv ().epv." << op.name () << ") (&_b";
				for (auto param : op) {
					h_ << ", &" << param->name ();
				}
				h_ << ", &_env)";

				if (op.tkind () != Type::Kind::VOID)
					h_ << ')';

				h_ << ";\n"
					"_env.check ();\n";

				if (op.tkind () != Type::Kind::VOID)
					h_ << "return _ret;\n";

				h_ << unindent << "}\n";

				// AMI
				if (ami) {
					h_ << "\ntemplate <class T>\n"
						"void Client <T, " << QName (container) << ">::" << SendcSignature (op, *ami->handler) << "\n"
						"{\n" << indent;

					environment (Raises ());
					h_ << "Bridge <" << QName (container) << ">& _b (T::_get_bridge (_env));\n"
						"(AMI_epv (_b)." AMI_SENDC << static_cast <const std::string&> (op.name ())
						<< ") (&_b, &" AMI_HANDLER;
					for (auto param : op) {
						if (param->attribute () != Parameter::Attribute::OUT)
							h_ << ", &" << param->name ();
					}
					h_ << ", &_env);\n"
						"_env.check ();\n"
						<< unindent << "}\n"

						"\ntemplate <class T>\n"
						POLLER_TYPE_PREFIX "VRet Client <T, " << QName (container) << ">::" << SendpSignature (op) << "\n"
						"{\n" << indent;

					environment (Raises ());
					h_ << "Bridge <" << QName (container) << ">& _b (T::_get_bridge (_env));\n"
						POLLER_TYPE_PREFIX "C_ret _ret ((AMI_epv (_b)." AMI_SENDP << static_cast <const std::string&> (op.name ())
						<< ") (&_b";

					for (auto param : op) {
						if (param->attribute () != Parameter::Attribute::OUT)
							h_ << ", &" << param->name ();
					}
					h_ << ", &_env));\n"
						"_env.check ();\n"
						"return _ret;\n"
						<< unindent << "}\n";
				}

			} break;

			case Item::Kind::STATE_MEMBER:
				if (!static_cast <const StateMember&> (*item).is_public ())
					break;
				[[fallthrough]];
			case Item::Kind::ATTRIBUTE: {
				const Member& m = static_cast <const Member&> (*item);
				const Attribute* att = nullptr;
				if (m.kind () == Item::Kind::ATTRIBUTE)
					att = &static_cast <const Attribute&> (m);

				// att == nullptr for members

				if (!att || !is_native (att->getraises ())) {
					h_ << "\ntemplate <class T>\n";
					if (!att_byref)
						h_ << VRet (m);
					else
						h_ << ConstRef (m);

					h_ << " Client <T, " << QName (container) << ">::" << m.name () << " ()\n"
						"{\n" << indent;

					environment (att ? att->getraises () : Raises ());
					h_ << "Bridge < " << QName (container) << ">& _b (T::_get_bridge (_env));\n"
						<< TypePrefix (m) << 'C';

					if (att_byref)
						h_ << "_VT";

					h_ << "_ret _ret ((_b._epv ().epv._get_" << m.name () << ") (&_b, &_env));\n"
						"_env.check ();\n"
						"return _ret;\n"
						<< unindent << "}\n";

					// AMI
					if (ami) {
						h_ << "\ntemplate <class T>\n"
							"void Client <T, " << QName (container) << ">::" 
							AMI_SENDC "get_" << static_cast <const std::string&> (m.name ()) << " ("
							<< TypePrefix (ami->handler) << "C_in " AMI_HANDLER ")\n"
							"{\n" << indent;

						environment (Raises ());
						h_ << "Bridge <" << QName (container) << ">& _b (T::_get_bridge (_env));\n"
							"(AMI_epv (_b)."
							AMI_SENDC << "get_" << static_cast <const std::string&> (m.name ())
							<< ") (&_b, &" AMI_HANDLER ", &_env);\n"
							"_env.check ();\n"
							<< unindent << "}\n"

							"\ntemplate <class T>\n"
							POLLER_TYPE_PREFIX "VRet Client <T, " << QName (container) << ">::"
							AMI_SENDP "get_" << static_cast <const std::string&> (m.name ()) << " ()\n"
							"{\n" << indent;

						environment (Raises ());
						h_ << "Bridge <" << QName (container) << ">& _b (T::_get_bridge (_env));\n"
							POLLER_TYPE_PREFIX "C_ret _ret ((AMI_epv (_b)."
							AMI_SENDP "get_" << static_cast <const std::string&> (m.name ())
							<< ") (&_b, &_env));\n"
							"_env.check ();\n"
							"return _ret;\n"
							<< unindent << "}\n";
					}
				}

				if (!(att && (att->readonly () || is_native (att->setraises ())))) {
					h_ << "\ntemplate <class T>\n"
						"void Client <T, " << QName (container) << ">::" << m.name ()
						<< " (" << Param (m) << " val)\n"
						"{\n" << indent;

					environment (att ? att->setraises () : Raises ());
					h_ << "Bridge < " << QName (container) << ">& _b (T::_get_bridge (_env));\n"
						"(_b._epv ().epv._set_" << m.name () << ") (&_b, &val, &_env);\n"
						"_env.check ();\n"
					<< unindent << "}\n";
				}

				if (!att && is_var_len (m)) {
					h_ << "\ntemplate <class T>\n"
						"void Client <T, " << QName (container) << ">::" << m.name ()
						<< " (" << Var (m) << "&& val)\n"
						"{\n" << indent;

					environment (Raises ());
					h_ << "Bridge < " << QName (container) << ">& _b (T::_get_bridge (_env));\n"
						"(_b._epv ().epv._move_" << m.name () << ") (&_b, &" << Param (m, Parameter::Attribute::INOUT) << " (val), & _env); \n"
						"_env.check ();\n"
						<< unindent << "}\n";

					// AMI
					if (ami) {
						h_ << "\ntemplate <class T>\n"
							"void Client <T, " << QName (container) << ">::"
							AMI_SENDC "set_" << static_cast <const std::string&> (m.name ()) << " ("
							<< TypePrefix (ami->handler) << "C_in " AMI_HANDLER ", " << Param (m) << " val)\n"
							"{\n" << indent;

						environment (Raises ());
						h_ << "Bridge <" << QName (container) << ">& _b (T::_get_bridge (_env));\n"
							"(AMI_epv (_b)."
							AMI_SENDC << "set_" << static_cast <const std::string&> (m.name ())
							<< ") (&_b, &" AMI_HANDLER ", &val, &_env);\n"
							"_env.check ();\n"
							<< unindent << "}\n"

							"\ntemplate <class T>\n"
							POLLER_TYPE_PREFIX "VRet Client <T, " << QName (container) << ">::"
							AMI_SENDP "set_" << static_cast <const std::string&> (m.name ()) << " ("
							<< Param (m) << " val)\n"
							"{\n" << indent;

						environment (Raises ());
						h_ << "Bridge <" << QName (container) << ">& _b (T::_get_bridge (_env));\n"
							POLLER_TYPE_PREFIX "C_ret _ret ((AMI_epv (_b)."
							AMI_SENDP "set_" << static_cast <const std::string&> (m.name ())
							<< ") (&_b, &val, &_env));\n"
							"_env.check ();\n"
							"return _ret;\n"
							<< unindent << "}\n";
					}
				}

			} break;
		}
	}

	// Interface definition
	h_.namespace_open (container);

	bool has_factory = false;

	if (container.kind () == Item::Kind::VALUE_TYPE) {
		has_factory = has_factories (static_cast <const ValueType&> (container));
		if (has_factory)
			h_ << empty_line
				<< "class " << container.name () << FACTORY_SUFFIX ";\n";
	}

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
		"{\n";

	if (ami) {
		h_ << indent << "typedef " << Namespace ("CORBA/Internal") << "ClientInterface <" << container.name ();
		for (auto b : bases) {
			h_ << ", " << QName (*b);
		}
		h_ << ", " << Namespace ("CORBA") << "Object> Base;\n"
			<< unindent;
	}

	h_ << "public:\n"
		<< indent;
	for (auto item : container) {
		switch (item->kind ()) {
			case Item::Kind::OPERATION:
			case Item::Kind::ATTRIBUTE:
			case Item::Kind::STATE_MEMBER:
			case Item::Kind::VALUE_FACTORY:
				break;

			default: {
				const NamedItem& def = static_cast <const NamedItem&> (*item);
				h_ << "using " << Namespace ("CORBA/Internal") << "Decls <"
					<< container.name () << ">::" << def.name () << ";\n";
				if (!pseudo_interface && ItemWithId::cast (&def))
					h_ << "using " << Namespace ("CORBA/Internal") << "Decls <"
					<< container.name () << ">::_tc_" << static_cast <const std::string&> (def.name ()) << ";\n";

				if (options ().legacy) {
					switch (item->kind ()) {
						case Item::Kind::TYPE_DEF:
							if (static_cast <const TypeDef&> (*item).dereference_type ().tkind () == Type::Kind::BASIC_TYPE)
								break;
							[[fallthrough]];
						case Item::Kind::STRUCT:
						case Item::Kind::UNION:
							h_ << "#ifdef LEGACY_CORBA_CPP\n";
							h_ << "using " << Namespace ("CORBA/Internal") << "Decls <" << container.name () << ">::" << def.name () << "_var;\n";

							if (item->kind () == Item::Kind::TYPE_DEF) {
								const TypeDef& td = static_cast <const TypeDef&> (*item);
								if (td.tkind () == Type::Kind::NAMED_TYPE && is_ref_type (td))
									h_ << "using " << Namespace ("CORBA/Internal") << "Decls <" << container.name () << ">::" << def.name () << "_ptr;\n";
							}

							h_ << "#endif\n";
							break;
					}
				}
			}
		}
	}

	if (has_factory)
		h_ << "static const ::Nirvana::ImportInterfaceT <" << container.name () << FACTORY_SUFFIX "> _factory;\n";

	if (ami) {
		override_sendp (static_cast <const Interface&> (container), *ami->poller);
		for (auto b : bases)  {
			const Interface& base = static_cast <const Interface&> (*b);
			if (compiler ().ami_interfaces ().find (&base) != compiler ().ami_interfaces ().end ())
				override_sendp (base, *ami->poller);
		}
	}

	h_ << unindent
		<< "};\n";
}

void Client::override_sendp (const Interface& itf, const ValueType& poller)
{
	for (auto item : itf) {
		switch (item->kind ()) {
		case Item::Kind::OPERATION: {
			const Operation& op = static_cast <const Operation&> (*item);
			h_ << TypePrefix (&poller) << "VRet " << SendpSignature (op) << "\n"
				"{\n" << indent
				<< "return Base::" AMI_SENDP
				<< static_cast <const std::string&> (op.name ()) << " (";
			// in/inout parameters
			auto it = op.begin ();
			while (it != op.end () && (*it)->attribute () == Parameter::Attribute::OUT)
				++it;

			if (it != op.end ()) {
				h_ << (*it)->name ();
				for (++it; it != op.end (); ++it) {
					if ((*it)->attribute () != Parameter::Attribute::OUT)
						h_ << ", " << (*it)->name ();
				}
			}

			h_ << ")->_query_valuetype <" << QName (poller) << "> ();\n"
				<< unindent << "}\n";
		} break;

		case Item::Kind::ATTRIBUTE: {
			const Attribute& att = static_cast <const Attribute&> (*item);
			h_ << TypePrefix (&poller) << "VRet " AMI_SENDP "get_"
				<< static_cast <const std::string&> (att.name ()) << " ()\n"
				"{\n" << indent
				<< "return Base::" AMI_SENDP "get_"
				<< static_cast <const std::string&> (att.name ()) << " ()->_query_valuetype <"
				<< QName (poller) << "> ();\n"
				<< unindent << "}\n";
			if (!att.readonly ()) {
				h_ << TypePrefix (&poller) << "VRet " AMI_SENDP "set_"
					<< static_cast <const std::string&> (att.name ()) << " (" << Param (att) << " val)\n"
					"{\n" << indent
					<< "return Base::" AMI_SENDP "set_"
					<< static_cast <const std::string&> (att.name ()) << " (val)->_query_valuetype <"
					<< QName (poller) << "> ();\n"
					<< unindent << "}\n";
			}
		} break;
		}
	}
}

void Client::bridge_bases (const Bases& bases)
{
	for (auto b : bases) {
		std::string proc_name;
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
	if (!itf.has_forward_dcl ()
		&& compiler ().ami_handlers ().find (&itf) == compiler ().ami_handlers ().end ())
			forward_interface (itf);

	auto ami = compiler ().ami_interfaces ().find (&itf);
	if (ami != compiler ().ami_interfaces ().end ()) {
		forward_interface (*ami->second.handler);
		forward_interface (*ami->second.poller);
	}

	begin_interface (itf);
}

void Client::end (const Interface& itf)
{
	end_interface (itf);
}

void Client::begin (const ValueType& itf)
{
	if (!itf.has_forward_dcl ()
		&& compiler ().ami_pollers ().find (&itf) == compiler ().ami_pollers ().end ())
		forward_interface (itf);

	begin_interface (itf);
}

inline
size_t Client::version (const std::string& rep_id)
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

	if (vt.modifier () == ValueType::Modifier::ABSTRACT)
		return;

	StateMembers members = get_members (vt);

	h_.namespace_open ("CORBA/Internal");

	// ValueData
	h_ << empty_line
		<< "template <>\n"
		"class ValueData <" << QName (vt) << ">\n"
		"{\n"
		"public:\n"
		<< indent;

	// Accessors
	{
		bool pub = true;
		for (auto m : members) {

			if (m->is_public () != pub) {
				h_ << unindent;
				if (pub) {
					h_ << "protected:\n";
					pub = false;
				} else {
					h_ << "public:\n";
					pub = false;
				}
				h_ << indent;
			}

			h_ << Accessors (*m)
				<< empty_line;
		}
	}

	// Default constructor
	h_ << unindent
		<< "protected:\n"
		<< indent
		<< "ValueData ()";
	if (!members.empty ()) {
		h_ << " :\n"
			<< indent;
		auto it = members.begin ();
		h_ << MemberDefault (**it, "_");
		for (++it; it != members.end (); ++it) {
			h_ << ",\n" << MemberDefault (**it, "_");
		}
		h_.unindent ();
	}
	h_ << "\n{}\n\n";


	// Marshaling
	h_ << "void _marshal (I_ptr <IORequest>) const";
	if (!members.empty ())
		h_ << ";\n";
	else
		h_ << " {}\n";
	h_ << "void _unmarshal (I_ptr <IORequest>)";
	if (!members.empty ())
		h_ << ";\n";
	else
		h_ << " {}\n";

	// Member variables
	if (!members.empty ()) {
		h_ << std::endl << unindent
			<< "private:\n"
			<< indent;

		for (auto p : members) {
			h_ << MemberVariable (*p);
		}
	}
	h_ << unindent
		<< "};\n"
		"\n";

	// End ValueData

	// Marshaling implementation

	cpp_.namespace_open ("CORBA/Internal");
	if (!members.empty ()) {
		cpp_ << empty_line <<
			"void ValueData <" << QName (vt) << ">::_marshal (I_ptr <IORequest> rq) const\n"
			"{\n";
		marshal_members (cpp_, (const Members&)members, "marshal_in", "_");
		cpp_ << "}\n\n"
			"void ValueData <" << QName (vt) << ">::_unmarshal (I_ptr <IORequest> rq)\n"
			"{\n";
		unmarshal_members (cpp_, (const Members&)members, "_");
		cpp_ << "}\n";
	}

	// Type code

	h_ << TypeCodeName (vt);

	if (!members.empty ()) {
		h_ << empty_line <<
			"template <>\n"
			"const StateMember TypeCodeStateMembers <" << QName (vt) << ">::members_ [" << members.size () << "];\n";

		cpp_ << empty_line <<
			"template <>\n"
			"const StateMember TypeCodeStateMembers <" << QName (vt) << ">::members_ [" << members.size () << "] = {\n"
			<< indent;

		auto it = members.begin ();
		cpp_ << **it;
		for (++it; it != members.end (); ++it) {
			cpp_ << ",\n" << **it;
		}
		cpp_ << unindent
			<< "\n};\n\n";
	}

	// Factory

	Factories factories = get_factories (vt);

	if (!factories.empty ()) {
		h_.namespace_open ("CORBA/Internal");
		h_.empty_line ();

		{
			std::string factory_id = vt.repository_id ();
			factory_id.insert (version (factory_id), FACTORY_SUFFIX);

			h_ <<
				"template <>\n"
				"const Char RepIdOf <" << QName (vt) << FACTORY_SUFFIX << ">::id [] = \"" << factory_id << "\";\n" <<
				"NIRVANA_BRIDGE_BEGIN (" << QName (vt) << FACTORY_SUFFIX << ")\n"
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
			"public Decls <" << QName (vt) << ">\n"
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
				"Type <" << QName (vt) << ">::C_ret _ret ((_b._epv ().epv." << f->name ()
				<< ") (&_b";
			for (auto it = f->begin (); it != f->end (); ++it) {
				h_ << ", &" << (*it)->name ();
			}
			h_ << ", &_env));\n"
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
			"{};\n";
	
		cpp_.namespace_close ();
		cpp_ << empty_line
			<< "NIRVANA_OLF_SECTION_OPT const "
			<< Namespace ("Nirvana") << "ImportInterfaceT <" << QName (vt) << FACTORY_SUFFIX ">\n"
			<< QName (vt) << "::_factory = { Nirvana::OLF_IMPORT_INTERFACE, CORBA::Internal::RepIdOf <"
			<< QName (vt) << ">::id, CORBA::Internal::RepIdOf <" << QName (vt) << FACTORY_SUFFIX ">::id };\n";
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
				"typename Type <I>::VRet " << op.name () << " (";

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
				h_ << "RepIdOf <I>::id";
			else
				h_ << (*it)->name ();
			++it;
			for (; it != op.end (); ++it) {
				h_ << ", ";
				if (par_iid == *it)
					h_ << "RepIdOf <I>::id";
				else
					h_ << (*it)->name ();
			}
			h_ << ").template downcast <I> ();\n"
				<< unindent
				<< "}\n";
		}
	}
}

void Client::implement_nested_items (const IV_Base& parent)
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
	else
		h_ << "EnvironmentEx <" << raises << "> _env;\n";
}

Code& operator << (Code& stm, const Client::ConstType& ct)
{
	const Type& t = ct.c.dereference_type ();
	switch (t.tkind ()) {
		case Type::Kind::STRING:
			stm << Namespace ("CORBA") << "Char*";
			break;
		case Type::Kind::WSTRING:
			stm << Namespace ("CORBA") << "WChar*";
			break;
		case Type::Kind::FIXED:
			stm << Namespace ("IDL") << "FixedBCD <" <<
				ct.c.as_Fixed ().digits () << ", " << ct.c.as_Fixed ().scale () << '>';
			break;
		case Type::Kind::NAMED_TYPE:
			stm << Namespace ("Nirvana") << "ImportInterfaceT <" << QName (ct.c.named_type ()) << '>';
			break;
		default:
			assert (t.tkind () == Type::Kind::BASIC_TYPE);
			// CORBA floating point types may be emulated, use native types.
			switch (t.basic_type ()) {
				case BasicType::FLOAT:
					stm << "float";
					break;
				case BasicType::DOUBLE:
					stm << "double";
					break;
				case BasicType::LONGDOUBLE:
					stm << "long double";
					break;
				default:
					stm << static_cast <const Type&> (ct.c);
			}
	}
	return stm;
}

void Client::leaf (const Constant& item)
{
	bool outline;
	{
		const Type& t = item.dereference_type ();
		outline = t.tkind () != Type::Kind::BASIC_TYPE || !AST::is_integral (t.basic_type ());
	}

	bool nested = is_nested (item);

	if (nested)
		h_ << "static ";
	else {
		h_.namespace_open (item);
		if (outline)
			h_ << "extern ";
	}

	h_ << "const " << ConstType (item) << ' ' << item.name ();

	if (item.vtype () != Variant::VT::EMPTY) {
		if (nested || outline) {
			if (!nested)
				cpp_.namespace_open (item);
			else
				cpp_.namespace_open ("CORBA/Internal");
			cpp_ << "const " << ConstType (item) << ' ' << QName (item);
		}

		(outline ? cpp_ : h_) << " = " << static_cast <const Variant&> (item);
	} else {
		// const object reference
		const NamedItem& type = item.dereference_type ().named_type ();

		const InterfaceKind* ik = nullptr;
		switch (type.kind ()) {
		case Item::Kind::INTERFACE:
			ik = &static_cast <const Interface&> (type);
			break;
		case Item::Kind::INTERFACE_DECL:
			ik = &static_cast <const InterfaceDecl&> (type);
			break;
		}
		bool is_object = ik && (ik->interface_kind () == InterfaceKind::Kind::UNCONSTRAINED || ik->interface_kind () == InterfaceKind::Kind::LOCAL);
		cpp_.namespace_open (item);
		cpp_ << "NIRVANA_OLF_SECTION_OPT extern const "
			<< Namespace ("Nirvana") << "ImportInterfaceT <" << QName (item.named_type ()) << "> " << item.name () << " =\n"
			"{ Nirvana::OLF_IMPORT_" << (is_object ? "OBJECT" : "INTERFACE")
			<< ", \"" << const_id (item) << "\", "
			<< Namespace ("CORBA/Internal") << "RepIdOf <" << QName (item.named_type ()) << ">::id };\n";
	}
	h_ << ";\n";

	if (nested || outline)
		cpp_ << ";\n";
}

void Client::define (const Exception& item)
{
	if (is_nested (item))
		h_.namespace_open ("CORBA/Internal");
	else
		h_.namespace_open (item);

	h_ << empty_line
		<< "class " << QName (item) << " : public " << Namespace ("CORBA") << "UserException\n"
		"{\n"
		"public:\n"
		<< indent
		<< "NIRVANA_EXCEPTION_DCL (" << item.name () << ");\n\n";

	if (!item.empty ()) {

		// Define struct _Data
		h_ << empty_line
			<< "struct _Data\n"
			"{\n"

			<< indent;
		member_variables (item);
		h_ << unindent

			<< "};\n\n";

		if (options ().legacy) {
			h_ << "#ifndef LEGACY_CORBA_CPP\n";
		}

		constructors (item, "_");
		accessors (item);
		h_ << empty_line
			<< unindent
			<< "private:\n"
			<< indent
			<< "alignas (_Data) ";
		member_variables (item);

		if (options ().legacy) {
			h_ << "\n#else\n";
			constructors (item, "");
			h_ << "alignas (_Data) ";
			member_variables_legacy (item);
			h_ << "\n#endif\n";
		}

		h_ << unindent
			<< "private:\n"
			<< indent <<
			"virtual void* __data () noexcept\n"
			"{\n" << indent;
		if (options ().legacy)
			h_ << "#ifndef LEGACY_CORBA_CPP\n";
		h_ << "return &_" << static_cast <const std::string&> (item.front ()->name ()) << ";\n";
		if (options ().legacy) {
			h_ <<
				"#else\n"
				"return &" << item.front ()->name () << ";\n"
				"#endif\n";
		}
		h_ << unindent << "}\n";
	}
	h_ << unindent << "};\n";

	// Define exception
	cpp_.namespace_close ();
	cpp_ << empty_line
		<< "NIRVANA_EXCEPTION_DEF (" << ParentName (item) << ", " << item.name () << ")\n\n";
}

void Client::rep_id_of (const ItemWithId& item)
{
	if (item.kind () == Item::Kind::INTERFACE
		|| item.kind () == Item::Kind::INTERFACE_DECL
		|| !is_pseudo (item)) {
		h_.namespace_open ("CORBA/Internal");
		h_ << empty_line
			<< "template <>\n"
			"const Char RepIdOf <" << QName (item) << ">::id [] = \""
			<< item.repository_id () << "\";\n\n";
	}
}

void Client::define_ABI (const StructBase& item)
{
	if (!is_var_len (item))
		return;

	const char* suffix = "";
	if (item.kind () == Item::Kind::EXCEPTION)
		suffix = EXCEPTION_SUFFIX;

	// ABI
	h_.namespace_open ("CORBA/Internal");
	h_ << "template <>\n"
		"struct ABI <" << QName (item) << suffix << ">\n"
		"{\n" << indent;
		for (const auto& m : item) {
			h_ << TypePrefix (*m) << "ABI _" << static_cast <const std::string&> (m->name ()) << ";\n";
		}
	h_ << unindent << "};\n\n";
}

inline
void Client::define_ABI (const Union& item)
{
	if (!is_var_len (item))
		return;

	// ABI
	h_.namespace_open ("CORBA/Internal");
	h_ << "template <>\n"
		"struct ABI <" << QName (item) << ">\n"
		"{\n" << indent;
	h_ << TypePrefix (item.discriminator_type ()) << "ABI __d;\n"
		"union _U\n"
		"{\n" << indent <<
		"_U () {}\n"
		"~_U () {}\n"
		"\n";
	for (const auto& el : item) {
		h_ << TypePrefix (*el) << "ABI " << el->name () << ";\n";
	}
	h_ << unindent << "} _u;\n";
	h_ << unindent << "};\n\n";

}

void Client::define_structured_type (const ItemWithId& item)
{
	h_.namespace_open ("CORBA/Internal");
	rep_id_of (item);

	const char* suffix = "";
	const Union* u = nullptr;
	if (item.kind () == Item::Kind::EXCEPTION)
		suffix = EXCEPTION_SUFFIX;
	else if (item.kind () == Item::Kind::UNION)
		u = &static_cast <const Union&> (item);

	const Members& members = (u ? static_cast <const Members&> (*u) : static_cast <const Members&> (static_cast <const StructBase&> (item)));
	assert (!members.empty ());

	bool var_len = is_var_len (members);
	bool CDR = !u && is_CDR (members);

	// Type
	h_ << "template <>\n"
		"struct Type <" << QName (item) << suffix << "> :\n" << indent;

	bool check;
	if (var_len) {
		check = true;

		h_ << "TypeVarLen <" << QName (item) << suffix << ">\n"
			<< unindent <<
			"{\n"
			<< indent;

	} else {
		check = has_check (item);

		h_ << "TypeFixLen <" << QName (item) << suffix << ", " << (check ? "true" : "false") << ">";

		if (u)
			h_ << ",\n"
			"MarshalHelper <" << QName (item) << ", " << QName (item) << ">";

		h_ << unindent << "\n{\n" << indent <<

			"static const bool has_check = " << (check ? "true" : "false") << ";\n" <<
			"static const bool is_CDR = " << (CDR ? "true" : "false") << ";\n";

		if (u) {
			h_ << "\n"
				"using MarshalHelper <" << QName (item) << ", "
				<< QName (item) << suffix << ">::marshal_in_a;\n"
				"using MarshalHelper <" << QName (item) << ", "
				<< QName (item) << suffix << ">::marshal_out_a;\n"
				"using MarshalHelper <" << QName (item) << ", "
				<< QName (item) << suffix << ">::unmarshal_a;\n";
		}
	}

	if (u) {
		int default_index;
		if (u->default_element ()) {
			default_index = 0;
			for (auto m : members) {
				if (m == u->default_element ())
					break;
				++default_index;
			}
		} else
			default_index = -1;

		h_ << "typedef " << u->discriminator_type () << " DiscriminatorType;\n"
			"static const Long default_index_ = " << default_index << ";\n\n";
	}

	// Declare marshaling
	if (!(is_pseudo (item) && is_native (members))) {
		if (!CDR) {
			h_ << "\n"
				"static void marshal_in (const Var&, IORequest::_ptr_type);\n";

			if (var_len)
				h_ << "static void marshal_out (Var&, IORequest::_ptr_type);\n";

			h_ << "static void unmarshal (IORequest::_ptr_type, Var&);\n";
		} else {
			std::vector <const Member*> byteswap_members;
			for (const auto& m : members) {
				const Type* t = &m->dereference_type ();
				if (t->tkind () == Type::Kind::ARRAY)
					t = &t->array ();
				if (t->tkind () == Type::Kind::BASIC_TYPE) {
					switch (t->basic_type ()) {
					case BasicType::OCTET:
					case BasicType::CHAR:
					case BasicType::BOOLEAN:
						continue;
					}
				}
				byteswap_members.push_back (m);
			}

			h_ << "\n"
				"static void byteswap (Var& v) noexcept\n"
				"{\n" << indent;
			if (!byteswap_members.empty ()) {
				if (options ().legacy && item.kind () != Item::Kind::EXCEPTION)
					h_ << "#ifndef LEGACY_CORBA_CPP\n";
				for (auto m : byteswap_members) {
					h_ << TypePrefix (*m) << "byteswap (v._" << m->name () << ");\n";
				}
				if (options ().legacy && item.kind () != Item::Kind::EXCEPTION) {
					h_ << "#else\n";
					for (auto m : byteswap_members) {
						h_ << TypePrefix (*m) << "byteswap (v." << m->name () << ");\n";
					}
					h_ << "#endif\n";
				}
			}
			h_ << unindent << "}\n\n"

				"static const size_t CDR_align = " << TypePrefix (*members.front ()) << "CDR_align;\n";

			if (options ().legacy && item.kind () == Item::Kind::STRUCT)
				h_ << "\n#ifndef LEGACY_CORBA_CPP\n";
			CDR_size (static_cast <const StructBase&> (item), "_");

			if (options ().legacy && item.kind () == Item::Kind::STRUCT) {
				h_ << "#else\n";
				CDR_size (static_cast <const StructBase&> (item), "");
				h_ << "#endif\n";
			}
		}
	}

	if (check) {
		// Declare check()
		h_ << "static void check (const ABI& val);\n";

		// Implement check()
		cpp_.namespace_open ("CORBA/Internal");
		cpp_ << "void Type <" << QName (item) << suffix << ">::check (const ABI& val)\n"
			"{\n" << indent;
		if (!u) {
			for (auto m : members) {
				if (has_check (*m))
					cpp_ << TypePrefix (*m) << "check (val._" << static_cast <const std::string&> (m->name ()) << ");\n";
			}
		} else {
			bool member_check = false;
			const Type& d_type = u->discriminator_type ();
			if (has_check (d_type)) {
				cpp_ << TypePrefix (d_type) << "check (val.__d);\n";
				// If discriminator is enum, members may not have checks
				for (auto m : members) {
					if (has_check (*m)) {
						member_check = true;
						break;
					}
				}
			} else
				member_check = true; // Some members definitely have check
			if (member_check) {
				cpp_ << "switch (";
				if (is_enum (d_type)) {
					// Cast enum ABI to enum
					cpp_ << '(' << d_type << ')';
				}
				cpp_ << "val.__d) {\n";
				for (auto m : members) {
					element_case (static_cast <const UnionElement&> (*m));
					cpp_.indent ();
					if (has_check (*m))
						cpp_ << TypePrefix (*m) << "check (val._u." << m->name () << ");\n";
					cpp_ << "break;\n"
						<< unindent;
				}
				cpp_ << "}\n";
			}
		}
		cpp_ << unindent << "}\n";
	}

	if (item.kind () != Item::Kind::EXCEPTION && !is_pseudo (item))
		type_code_func (item);

	h_ << unindent << "};\n";
}

void Client::CDR_size (const StructBase& item, const char* prefix)
{
	assert (!item.empty ());

	const char* suffix = "";
	if (item.kind () == Item::Kind::EXCEPTION)
		suffix = EXCEPTION_SUFFIX;

	h_ << "static const size_t CDR_size = offsetof ("
		<< QName (item) << suffix << ", " << prefix << item.back ()->name () << ") + "
		<< TypePrefix (*item.back ()) << "CDR_size;\n";
}

bool Client::has_check (const Type& type)
{
	if (is_var_len (type) || is_enum (type))
		return true;
	const Type& t = type.dereference_type ();
	switch (t.tkind ()) {
		case Type::Kind::FIXED:
			return true;
		case Type::Kind::BASIC_TYPE:
			return t.basic_type () == BasicType::CHAR;
	}
	return false;
}

bool Client::has_check (const ItemWithId& item)
{
	const Members* members;
	if (item.kind () == Item::Kind::UNION) {
		const Union& u = static_cast <const Union&> (item);
		if (has_check (u.discriminator_type ()))
			return true;
		members = &static_cast <const StructBase&> (u);
	} else
		members = &static_cast <const StructBase&> (item);

	for (auto m : *members) {
		if (has_check (*m))
			return true;
	}

	return false;
}

void Client::leaf (const Exception& item)
{
	if (is_nested (item))
		forward_decl (item);
	else {
		type_code_decl (item);
		implement (item);
	}
}

void Client::implement (const Exception& item)
{
	define (item);
	if (!item.empty ()) {
		define_swap (item);
		define_ABI (item);
		define_structured_type (item);
		implement_marshaling (item);
	} else
		rep_id_of (item);

	type_code_def (item);
}

void Client::leaf (const StructDecl& item)
{
	forward_decl (item);
}

void Client::leaf (const UnionDecl& item)
{
	forward_decl (item);
}

void Client::leaf (const Struct& item)
{
	if (is_nested (item)) {
		if (!item.has_forward_dcl ())
			forward_decl (item);
	} else {
		h_.empty_line ();
		if (!item.has_forward_dcl ())
			type_code_decl (item);
		implement (item);
		if (!item.has_forward_dcl ())
			backward_compat_var (item);
	}
}

void Client::leaf (const Union& item)
{
	if (is_nested (item)) {
		if (!item.has_forward_dcl ())
			forward_decl (item);
	} else {
		h_.empty_line ();
		if (!item.has_forward_dcl ())
			type_code_decl (item);
		implement (item);
		if (!item.has_forward_dcl ())
			backward_compat_var (item);
	}
}

void Client::implement (const Struct& item)
{
	type_code_def (item);
	define (item);
	define_swap (item);
	define_ABI (item);
	define_structured_type (item);

	if (!(is_pseudo (item) && is_native (item))) {
		// Marshaling
		cpp_.namespace_open ("CORBA/Internal");
		implement_marshaling (item);
	}
}

void Client::implement (const Union& item)
{
	type_code_def (item);
	define (item);
	define_swap (item);
	define_ABI (item);
	define_structured_type (item);

	if (!(is_pseudo (item) && is_native (item))) {
		// Marshaling
		marshal_union (item, false);
		if (is_var_len (item))
			marshal_union (item, true);
		cpp_ << empty_line <<
			"void Type <" << QName (item)
			<< ">::unmarshal (IORequest::_ptr_type rq, Var& v)\n"
			"{\n" << indent <<
			"v._destruct ();\n" <<
			TypePrefix (item.discriminator_type ()) << "unmarshal (rq, v.__d);\n"
			"switch (v.__d) {\n";
		for (const auto& el : item) {
			if (el->is_default ())
				cpp_ << "default:\n";
			else
				for (const auto& l : el->labels ()) {
					cpp_ << "case " << l << ":\n";
				}
			cpp_.indent ();
			init_union (cpp_, *el, "v.");
			cpp_ << TypePrefix (*el) << "unmarshal (rq, v._u." << el->name () << ");\n"
				"break;\n" << unindent;
		}
		cpp_ << "}\n"
			<< unindent << "}\n";
	}
}

void Client::define (const Struct& item)
{
	if (is_nested (item))
		h_.namespace_open ("CORBA/Internal");
	else
		h_.namespace_open (item);

	h_ << empty_line
		<< "class " << QName (item) << "\n"
		"{\n"
		"public:\n"
		<< indent;

	if (options ().legacy)
		h_ << "#ifndef LEGACY_CORBA_CPP\n";

	constructors (item, "_");
	accessors (item);

	// Member variables
	h_ << unindent
		<< empty_line
		<< "private:\n"
		<< indent
		<< "friend struct " << Namespace ("CORBA/Internal") << "Type <" << item.name () << ">;\n";
	member_variables (item);

	if (options ().legacy) {
		h_ << "#else\n";
		member_variables_legacy (item);
		h_ << "#endif\n";
	}

	h_ << unindent
		<< "};\n";
}

inline
void Client::define (const Union& item)
{
	if (is_nested (item)) {
		h_.namespace_open ("CORBA/Internal");
		cpp_.namespace_open ("CORBA/Internal");
	} else {
		h_.namespace_open (item);
		cpp_.namespace_open (item);
	}

	h_ << empty_line
		<< "class " << QName (item) << "\n"
		"{\n"
		"public:\n"
		<< indent;

	const Variant* init_d;
	const UnionElement* init_el;
	if ((init_el = item.default_element ())) {
		init_d = &item.default_label ();
	} else if (item.default_label ().empty ()) {
		init_el = item.front ();
		init_d = &init_el->labels ().front ();
	} else {
		// A union has an implicit default member if it does not have a default case
		// and not all permissible values of the union discriminant are listed.
		init_el = nullptr;
		init_d = &item.default_label ();
	}

	assert (init_d);

	// Constructors

	h_ << item.name () << " () noexcept";
	if (init_el) {
		h_ << ";\n";
		cpp_ << QName (item) << "::" << item.name () << " () noexcept :\n"
			<< indent <<
			"__d (" << *init_d << ")\n"
			<< unindent <<
			"{\n"
			<< indent;

		init_union (cpp_, *init_el);

		cpp_ << unindent <<
			"}\n";
	} else {
		h_ << " :\n"
			<< indent <<
			"__d (" << *init_d << ")\n"
			<< unindent <<
			"{}\n";
	}

	h_
		<< item.name () << " (const " << item.name () << "& src)\n"
		"{\n" << indent <<
		"_assign (src);\n"
		<< unindent << "}\n"

		<< item.name () << " (" << item.name () << "&& src) noexcept\n"
		"{\n" << indent <<
		"_assign (std::move (src));\n"
		<< unindent << "}\n"

		// Destructor
		"~" << item.name () << " ()\n"
		"{\n"
		<< indent << "_destruct ();\n" << unindent <<
		"}\n\n"

		// Assignments

		<< item.name () << "& operator = (const " << item.name () << "& src)\n"
		"{\n" << indent <<
		"_destruct ();\n"
		"_assign (src);\n"
		"return *this;\n"
		<< unindent << "}\n"

		<< item.name () << "& operator = (" << item.name () << "&& src) noexcept\n"
		"{\n" << indent <<
		"_destruct ();\n"
		"_assign (std::move (src));\n"
		"return *this;\n"
		<< unindent << "}\n"

		// _d ();

		"void _d (" << item.discriminator_type () << " d);\n" <<
		item.discriminator_type () << " _d () const noexcept\n"
		"{\n" << indent <<
		"return __d;\n"
		<< unindent << "}\n\n";

	cpp_ << empty_line <<
		"void " << QName (item) << "::_d (" << item.discriminator_type () << " d)\n"
		"{\n" << indent <<
		item.discriminator_type () << " nd = _switch (d);\n";
		if (!init_el) {
			cpp_ << "if (nd == " << *init_d << ")\n"
				<< indent << "_destruct ();\n" << unindent
				<< "else ";
		}
		cpp_ << "if (nd != _switch (__d))\n"
			<< indent << "throw " << Namespace ("CORBA") << "BAD_PARAM ();\n"
			<< unindent << "__d = d;\n"
		<< unindent << "}\n\n";

	// If union has implicit default, create _default () method;
	if (!init_el) {
		h_ << "void _default () noexcept\n"
			"{\n"
			<< indent <<
			"_destruct ();\n"
			"__d = " << *init_d << ";\n"
			<< unindent <<
			"}\n\n";
	}

	// Accessors

	for (const auto& el : item) {

		h_.empty_line ();
		cpp_.empty_line ();

		const Variant& label = el->is_default () ? *init_d : el->labels ().front ();
		bool multi = el->is_default () || el->labels ().size () > 1;

		// const getter
		h_ << ConstRef (*el) << ' ' << el->name () << " () const\n"
			"{\n" << indent <<
			"return const_cast <" << item.name () << "&> (*this)." << el->name () << " ();\n"
			<< unindent << "}\n";

		// ref getter
		h_ << MemberType (*el) << "& " << el->name () << " ();\n"
			// setter
			"void " << el->name () << " (" << ConstRef (*el) << " val";
		if (multi)
			h_ << ", " << item.discriminator_type () << " = " << label;
		h_ << ");\n";

		// ref getter
		cpp_ << MemberType (*el) << "& " << QName (item) << "::" << el->name () << " ()\n"
			"{\n" << indent <<
			"if (_switch (__d) != " << label << ")\n" << indent <<
			"throw " << Namespace ("CORBA") << "BAD_PARAM ();\n"
			<< unindent <<
			"return _u." << el->name () << ";\n"
			<< unindent << "}\n"

			// setter
			<< "void " << QName (item) << "::" << el->name () << " (" << ConstRef (*el) << " val";
		if (multi)
			cpp_ << ", " << item.discriminator_type () << " label";
		cpp_ << ")\n"
			"{\n" << indent;

		if (multi)
			cpp_ << "if (_switch (label) != " << label << ")\n" << indent <<
			"throw " << Namespace ("CORBA") << "BAD_PARAM ();\n" << unindent;

		cpp_ << "if (_switch (__d) != " << label << ") {\n" << indent <<
			"_destruct ();\n" <<
			Namespace ("CORBA/Internal") << "construct (_u." << el->name () << ", val);\n"
			"__d = ";
		if (multi)
			cpp_ << "label";
		else
			cpp_ << label;
		cpp_ << ";\n"
			<< unindent << "} else\n" << indent <<
			"_u." << el->name () << " = val;\n"
			<< unindent
			<< unindent << "}\n";

		if (is_var_len (*el)) {
			// The move setter
			h_ << "void " << el->name () << " (" << Var (*el) << "&& val";
			if (multi)
				h_ << ", " << item.discriminator_type () << " = " << label;
			h_ << ");\n";

			cpp_ << "void " << QName (item) << "::" << el->name () << " (" << Var (*el) << "&& val";
			if (multi)
				cpp_ << ", " << item.discriminator_type () << " label";
			cpp_ << ")\n"
				"{\n" << indent;

			if (multi)
				cpp_ << "if (_switch (label) != " << label << ")\n" << indent <<
				"throw " << Namespace ("CORBA") << "BAD_PARAM ();\n" << unindent;

			cpp_ << "if (_switch (__d) != " << label << ") {\n" << indent <<
				"_destruct ();\n" <<
				Namespace ("CORBA/Internal") << "construct (_u." << el->name () << ", std::move (val));"
				"__d = ";
			if (multi)
				cpp_ << "label";
			else
				cpp_ << label;
			cpp_ << ";\n"
				<< unindent << "} else\n" << indent <<
				"_u." << el->name () << " = std::move (val);\n"
				<< unindent
				<< unindent << "}\n";
		}
	}

	// _destruct ()

	h_ << unindent << "\nprivate:\n" << indent <<
		"friend struct " << Namespace ("CORBA/Internal") << "Type <" << item.name () << ">;\n"
		"\n"
		"void _destruct () noexcept";

	if (is_var_len (item)) {
		h_ << ";\n";
		cpp_ << empty_line <<
			"void " << QName (item) << "::_destruct () noexcept\n"
			"{\n"
			<< indent <<
			"switch (__d) {\n";
		for (const auto& el : item) {
			element_case (*el);
			cpp_ << indent
				<< Namespace ("CORBA/Internal") << "destruct (_u." << el->name () << ");\n"
				"break;\n"
				<< unindent;
		}
		cpp_ << "}\n" // switch

			"__d = " << *init_d << ";\n";
		// _destruct() must leave union in a consistent state
		if (init_el)
			init_union (cpp_, *init_el);
		cpp_ << unindent << "}\n\n";

	} else {
		h_ << "\n"
			"{}\n\n";
	}

	// _assign ()
	assign_union (item, false);
	assign_union (item, true);

	// _switch ()

	bool switch_is_trivial = false;
	if (is_boolean (item.discriminator_type ()))
		switch_is_trivial = true;
	else {
		const Enum* en = is_enum (item.discriminator_type ());
		switch_is_trivial = en && item.size () >= en->size () - 1;
	}

	h_ << "\n"
		"static " << item.discriminator_type () << " _switch (" <<
		item.discriminator_type () << " d) noexcept";
	if (switch_is_trivial) {
		h_ << "\n"
			"{\n"
			<< indent <<
			"return d;\n"
			<< unindent <<
			"}\n";
	} else {
		h_ << ";\n";
		cpp_ << empty_line << item.discriminator_type () << ' ' << QName (item)
			<< "::_switch (" << item.discriminator_type () << " d) noexcept\n"
			"{\n"
			<< indent <<
			"switch (d) {\n";

		for (const auto& el : item) {
			if (!el->is_default ()) {
				element_case (*el);
				cpp_ << indent << "return " <<
					(el->is_default () ? item.default_label () : el->labels ().front ()) << ";\n"
					<< unindent;
			}
		}

		if (!item.default_label ().empty ())
			cpp_ << "default:\n"
			<< indent <<
			"return " << item.default_label () << ";\n"
			<< unindent;

		cpp_ << "}\n"
			<< unindent <<
			"}\n";
	}

	// Data
	h_ << std::endl <<
		item.discriminator_type () << " __d;\n"
		"union _U\n"
		"{\n" << indent <<
		"_U () {}\n"
		"~_U () {}\n"
		"\n";

	for (const auto& el : item) {
		h_ << MemberType (*el) << ' ' << el->name () << ";\n";
	}

	h_ << unindent <<
		"} _u;\n"
		<< unindent << "};\n";
}

void Client::assign_union (const AST::Union& item, bool move)
{
	h_ << "void _assign (";
	if (!move)
		h_ << "const ";
	h_ << item.name ();
	if (move)
		h_ << "&& src) noexcept;\n";
	else
		h_ << "& src);\n";

	cpp_ << "void " << QName (item) << "::_assign (";
	if (!move)
		cpp_ << "const ";
	cpp_ << item.name ();
	if (move)
		cpp_ << "&& src) noexcept\n";
	else
		cpp_ << "& src)\n";
	cpp_ <<
		"{\n" << indent <<
		"switch (src.__d) {\n";
	for (const auto& el : item) {
		element_case (*el);
		cpp_ << indent
			<< Namespace ("CORBA/Internal") << "construct (_u." << el->name ()
			<< ", ";
		if (move)
			cpp_ << "std::move (";
		cpp_ << "src._u." << el->name ();
		if (move)
			cpp_ << ')';
		cpp_ << "); \n"
			"break;\n" << unindent;
	}
	cpp_ << "}\n" // switch
		"__d = src.__d;\n"
		<< unindent << "}\n";
}

void Client::element_case (const UnionElement& el)
{
	if (el.is_default ())
		cpp_ << "default:\n";
	else
		for (const auto& l : el.labels ()) {
			cpp_ << "case " << l << ":\n";
		}
}

void Client::constructors (const StructBase& item, const char* prefix)
{
	assert (!item.empty ());
	h_ << empty_line
	// Default constructor
		<< item.name () << " () noexcept :\n"
		<< indent;
	auto it = item.begin ();
	h_ << MemberDefault (**it, prefix);
	for (++it; it != item.end (); ++it) {
		h_ << ",\n" << MemberDefault (**it, prefix);
	}
	h_ << unindent
		<< "\n{}\n"

	// Explicit constructor
	"explicit " << item.name () << " (";
	it = item.begin ();
	h_ << Var (**it) << ' ' << (*it)->name ()
		<< indent;
	for (++it; it != item.end (); ++it) {
		h_ << ",\n" << Var (**it) << ' ' << (*it)->name ();
	}
	h_ << ") :\n";

	it = item.begin ();
	h_ << MemberInit (**it, prefix);
	for (++it; it != item.end (); ++it) {
		h_ << ",\n" << MemberInit (**it, prefix);
	}
	h_ << unindent
		<< "\n{}\n";
}

void Client::accessors (const StructBase& item)
{
	// Accessors
	for (const auto& m : item) {
		h_ << empty_line
			<< Accessors (*m);
	}
}

void Client::member_variables (const StructBase& item)
{
	for (const auto& m : item) {
		h_ << MemberVariable (*m);
	}
}

void Client::member_variables_legacy (const StructBase& item)
{
	for (const auto& m : item) {
		h_ << MemberType (*m) << ' '
			<< static_cast <const std::string&> (m->name ()) << ";\n";
	}
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

	if (!is_nested (item))
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

	type_code_def (item);
}

void Client::leaf (const ValueBox& vb)
{
	h_.namespace_open (vb);
	h_ << empty_line
		<< "class " << vb.name () << " : public CORBA::Internal::ValueBox <"
		<< vb.name () << ", " << static_cast <const Type&> (vb) << ">\n"
		"{\n"
		"public:\n" << indent <<
		"~" << vb.name () << " ()\n"
		"{\n" << indent <<
		"_value ().~BoxedType ();\n"
		<< unindent << "}\n";

	if (options ().legacy)
		h_ << "#ifndef LEGACY_CORBA_CPP\n";

	h_ << unindent <<
		"private:\n"
		<< indent <<
		"template <class T1, class ... Args>\n"
		"friend CORBA::servant_reference <T1> CORBA::make_reference (Args ...);\n";

	if (options ().legacy)
		h_ << "#endif\n";

	h_ <<
		vb.name () << " ()\n"
		"{\n" << indent <<
		"::new (&value_) BoxedType ();\n"
		<< unindent << "}\n" <<
		vb.name () << " (const BoxedType& v)\n"
		"{\n" << indent <<
		"::new (&value_) BoxedType (v);\n"
		<< unindent << "}\n" <<
		vb.name () << " (BoxedType&& v)\n"
		"{\n" << indent <<
		"::new (&value_) BoxedType (std::move (v));\n"
		<< unindent << "}\n"
		<< unindent << "};\n\n";

	type_code_decl (vb);
	type_code_def (vb);

	if (options ().legacy) {
		h_ << "\n#ifdef LEGACY_CORBA_CPP\n"
			"typedef " << vb.name () << "::_ptr_type " << vb.name () << "_ptr;\n"
			"typedef " << vb.name () << "::_var_type " << vb.name () << "_var;\n"
			"#endif\n";
	}

	rep_id_of (vb);

	h_ << empty_line
		<< "template <>\n"
		"struct Type <" << QName (vb) << "> : TypeValueBox <" << QName (vb) << ">\n"
		"{\n" << indent;
	type_code_func (vb);
	h_ << unindent << "};\n";
}

void Client::define_swap (const ItemWithId& item)
{
	// A namespace level swap() must be provided to exchange the values of two structs in an efficient manner.
	h_.namespace_open (item);
	
	h_ << "inline void swap (" << QName (item) << "& x, " << QName (item) << "& y)\n"
		"{\n" << indent <<
		Namespace ("std") << "swap (x, y);\n"
		<< unindent << "}\n";
}

void Client::implement_marshaling (const StructBase& item)
{
	const char* suffix = "";
	if (item.kind () == Item::Kind::EXCEPTION)
		suffix = EXCEPTION_SUFFIX;

	if (!is_CDR (item)) {

		std::string my_prefix = "v._";

		cpp_.namespace_open ("CORBA/Internal");
		cpp_ << "\n"
			"void Type <" << QName (item) << suffix
			<< ">::marshal_in (const Var& v, IORequest::_ptr_type rq)\n"
			"{\n";
		marshal_members (cpp_, item, "marshal_in", my_prefix.c_str ());
		cpp_ << "}\n";
		if (is_var_len (item)) {
			cpp_ << "\n"
				"void Type <" << QName (item) << suffix
				<< ">::marshal_out (Var& v, IORequest::_ptr_type rq)\n"
				"{\n";
			marshal_members (cpp_, item, "marshal_out", my_prefix.c_str ());
			cpp_ << "}\n";
		}
		cpp_ << "\n"
			"void Type <" << QName (item) << suffix
			<< ">::unmarshal (IORequest::_ptr_type rq, Var& v)\n"
			"{\n";
		unmarshal_members (cpp_, item, my_prefix.c_str ());
		cpp_ << "}\n";
	}
}

void Client::marshal_union (const Union& u, bool out)
{
	cpp_.namespace_open ("CORBA/Internal");

	const char* func = out ? "marshal_out" : "marshal_in";

	cpp_ << empty_line <<
		"void Type <" << QName (u) << ">::" << func << " (";
	if (!out)
		cpp_ << "const ";
	cpp_ << "Var& v, IORequest::_ptr_type rq)\n"
		"{\n" << indent <<
		TypePrefix (u.discriminator_type ()) << func << " (v.__d, rq);\n"
		"switch (v.__d) {\n";
	for (const auto& el : u) {
		if (el->is_default ())
			cpp_ << "default:\n";
		else
			for (const auto& l : el->labels ()) {
				cpp_ << "case " << l << ":\n";
			}
		cpp_.indent ();
		cpp_ << TypePrefix (*el) << func << " (v._u." << el->name () << ", rq);\n"
			"break;\n" << unindent;
	}
	cpp_ << "}\n"
		<< unindent << "}\n";
}

inline
void Client::generate_ami (const Interface& itf)
{
	assert (compiler ().ami_interfaces ().find (&itf) != compiler ().ami_interfaces ().end ());

	h_.namespace_open ("CORBA/Internal");

	h_ << empty_line <<
		"NIRVANA_AMI_BEGIN (" << QName (itf) << ")\n";

	for (auto item : itf) {
		switch (item->kind ()) {

		case Item::Kind::OPERATION: {
			const Operation& op = static_cast <const Operation&> (*item);

			if (is_native (op.raises ()))
				break;

			h_ << "void (*" AMI_SENDC << op.name () << ") (Bridge <"
				<< QName (itf) << ">*, Interface*" << AMI_ParametersABI (op) << ";\n"
				"Interface* (*" AMI_SENDP << op.name () << ") (Bridge <"
				<< QName (itf) << ">*" << AMI_ParametersABI (op) << ";\n";

		} break;

		case Item::Kind::ATTRIBUTE: {
			const Attribute& att = static_cast <const Attribute&> (*item);

			if (!is_native (att.getraises ())) {
				h_ << "void (*" AMI_SENDC "get_" << att.name () << ") (Bridge <" << QName (itf)
					<< ">*, Interface*, Interface*);\n"
					"Interface* (*" AMI_SENDP "get_" << att.name () << ") (Bridge <" << QName (itf)
					<< ">*, Interface*);\n";
			}

			if (!att.readonly () && !is_native (att.setraises ())) {
				h_ << "void (*" AMI_SENDC "set_" << att.name () << ") (Bridge <" << QName (itf)
					<< ">*, Interface*, " << ABI_param (att, Parameter::Attribute::IN) << ", Interface*);\n"
					"Interface* (*" AMI_SENDP "set_" << att.name () << ") (Bridge <" << QName (itf)
					<< ">*, " << ABI_param (att, Parameter::Attribute::IN) << ", Interface*);\n";
			}

		} break;
		}
	}

	h_ << "NIRVANA_AMI_END ()\n";
}

void Client::iv_traits_begin (const ItemWithId& item)
{
	h_.namespace_open ("IDL");
	h_ << "template <> struct traits <" << QName (item) << ">\n"
		"{\n" << indent <<
		"using ref_type = " << Namespace ("CORBA/Internal") << "I_ref <" << QName (item) << ">;\n"
		"using ptr_type = " << Namespace ("CORBA/Internal") << "I_ptr <" << QName (item) << ">;\n";
}

void Client::traits_end ()
{
	h_ << unindent << "};\n";
	h_.namespace_close ();
}
