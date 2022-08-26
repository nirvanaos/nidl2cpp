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
#include "Proxy.h"

#define PREFIX_OP_PROC "__rq_"
#define PREFIX_OP_PARAM_IN "__par_in_"
#define PREFIX_OP_PARAM_OUT "__par_out_"
#define PREFIX_OP_IDX "__OPIDX_"

using namespace std;
using namespace AST;

void Proxy::end (const Root&)
{
	if (custom_) {
		cpp_ << empty_line
			<< "#include \"" << cpp_.file ().stem ().generic_string () << "_native.h\"\n";
	}
	cpp_.close ();
}

void Proxy::get_parameters (const Operation& op, Members& params_in, Members& params_out)
{
	for (auto it = op.begin (); it != op.end (); ++it) {
		const Parameter* par = *it;
		switch (par->attribute ()) {
			case Parameter::Attribute::IN:
				params_in.push_back (par);
				break;
			case Parameter::Attribute::OUT:
				params_out.push_back (par);
				break;
			default:
				params_in.push_back (par);
				params_out.push_back (par);
		}
	}
}

void Proxy::implement (const Operation& op)
{
	cpp_.empty_line ();

	const ItemScope& itf = *op.parent ();

	Members params_in, params_out;
	get_parameters (op, params_in, params_out);

	if (!params_in.empty ())
		cpp_ << "static const Parameter " PREFIX_OP_PARAM_IN << op.name () << " [" << params_in.size () << "];\n";

	if (!params_out.empty ())
		cpp_ << "static const Parameter " PREFIX_OP_PARAM_OUT << op.name () << "[" << params_out.size () << "];\n";

	cpp_ << empty_line
		<< "static void " PREFIX_OP_PROC << static_cast <const string&> (op.name ())
		<< " (" << QName (itf) << "::_ptr_type _servant, IORequest::_ptr_type _call)";

	if (is_custom (op)) {
		custom_ = true;
		cpp_ << ";\n\n";
		return;
	}

	cpp_ << "\n{\n" << indent;

	// out and inout params
	for (auto p : params_out) {
		cpp_ << Var (*p) << ' ' << p->name () << ";\n";
	}
	if (op.tkind () != Type::Kind::VOID)
		cpp_ << Var (op) << " _ret;\n";

	cpp_ << "{\n"
		<< indent;

	// in params
	for (auto it = op.begin (); it != op.end (); ++it) {
		const Parameter* p = *it;
		if (p->attribute () == Parameter::Attribute::IN)
			cpp_ << Var (*p) << ' ' << p->name () << ";\n";
	}

	// Unmarshal in and inout
	for (auto p : params_in) {
		cpp_ << TypePrefix (*p) << "unmarshal (_call, " << p->name () << ");\n";
	}
	cpp_ << "_call->unmarshal_end ();\n";

	// Call
	if (op.tkind () != Type::Kind::VOID)
		cpp_ << "_ret = ";
	cpp_ << "_servant->" << op.name () << " (";
	{
		auto it = op.begin ();
		if (it != op.end ()) {
			cpp_ << (*it)->name ();
			for (++it; it != op.end (); ++it) {
				cpp_ << ", " << (*it)->name ();
			}
		}
	}
	cpp_ << ");\n"
		<< unindent
		<< "}\n"; // Input params are out of scope here

	// Marshal output
	for (auto p : params_out) {
		cpp_ << TypePrefix (*p) << "marshal_out (" << p->name () << ", _call);\n";
	}
	if (op.tkind () != Type::Kind::VOID)
		cpp_ << TypePrefix (op) << "marshal_out (_ret, _call);\n";

	cpp_ << unindent
		<< "}\n";
}

void Proxy::implement (const Attribute& att)
{
	const ItemScope& itf = *att.parent ();

	cpp_ << empty_line
		<< "static void " PREFIX_OP_PROC "_get_" << static_cast <const string&> (att.name ())
		<< " (" << QName (itf) << "::_ptr_type _servant, IORequest::_ptr_type _call)";

	if (is_native (att)) {
		custom_ = true;
		cpp_ << ";\n\n";
		return;
	}

	cpp_ << "\n{\n"
		<< indent;

	// ret
	cpp_ << Var (att) << " _ret;\n";

	// Call
	cpp_ << "_ret = _servant->" << att.name () << " ();\n";

	// Marshal
	cpp_ << TypePrefix (att) << "marshal_out (_ret, _call);\n";

	cpp_ << unindent
		<< "}\n";

	if (!att.readonly ()) {
		cpp_ << empty_line
			<< "static const Parameter " PREFIX_OP_PARAM_IN "_set_"
			<< static_cast <const string&> (att.name ()) << " [1];\n\n"
			<< "static void " PREFIX_OP_PROC "_set_" << static_cast <const string&> (att.name ())
			<< " (" << QName (itf) << "::_ptr_type _servant, IORequest::_ptr_type _call)\n"
			"{\n"
			<< indent

			<< Var (att) << " val;\n"
			<< TypePrefix (att) << "unmarshal (_call, val);\n"
			"_call->unmarshal_end ();\n"
			"_servant->" << att.name () << " (val);\n"

			<< unindent
			<< "}\n";
	}
}

bool Proxy::is_custom (const AST::Operation& op)
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

void Proxy::end (const Interface& itf)
{
	if (itf.interface_kind () == InterfaceKind::PSEUDO)
		return;

	cpp_.namespace_open ("CORBA/Internal");
	cpp_.empty_line ();
	type_code_name (itf);
	cpp_ << "\n"
		"template <>\n"
		"class Proxy <" << QName (itf) << "> : public ProxyBase <" << QName (itf) << '>';

	Interfaces bases = itf.get_all_bases ();
	cpp_.indent ();
	for (auto p : bases) {
		cpp_ << ",\npublic ProxyBaseInterface <" << QName (*p) << '>';
	}
	cpp_ << unindent
		<< "\n{\n"
		<< indent
		<< "typedef ProxyBase <" << QName (itf) << "> Base;\n"
		<< unindent
		<< "public:\n"
		<< indent

	// Constructor
		<< "Proxy (IOReference::_ptr_type proxy_manager, uint16_t interface_idx) :\n"
		<< indent
		<< "Base (proxy_manager, interface_idx)\n"
		<< unindent
		<< '{';
	if (!bases.empty ()) {
		cpp_ << endl;
		cpp_.indent ();
		cpp_ << "Object::_ptr_type obj = static_cast <Object*> (proxy_manager->get_object (RepIdOf <Object>::id));\n";
		for (auto p : bases) {
			cpp_ << "ProxyBaseInterface <" << QName (*p) << ">::init (obj);\n";
		}
		cpp_.unindent ();
	}
	cpp_ << "}\n";

	// Operations
	Metadata metadata;
	for (auto it = itf.begin (); it != itf.end (); ++it) {
		const Item& item = **it;
		switch (item.kind ()) {

			case Item::Kind::OPERATION: {
				const Operation& op = static_cast <const Operation&> (item);
				
				implement (op);

				metadata.emplace_back ();
				OpMetadata& op_md = metadata.back ();
				op_md.name = op.name ();
				op_md.type = &op;
				get_parameters (op, op_md.params_in, op_md.params_out);

				cpp_ << ServantOp (op) << " const";
				if (is_custom (op)) {
					cpp_ << ";\n"
						"static const UShort " PREFIX_OP_IDX << op.name () << " = " << (metadata.size () - 1) << ";\n";
				} else {
					
					cpp_ << "\n{\n";
					cpp_.indent ();

					// Create request
					cpp_ << "IORequest::_ref_type _call = _target ()->create_request (_make_op_idx ("
						<< (metadata.size () - 1) << "), " << (op.oneway () ? "4" : "3") << ");\n";

					// Marshal input
					for (auto p : op_md.params_in) {
						cpp_ << TypePrefix (*p) << "marshal_in (" << p->name () << ", _call);\n";
					}

					// Call
					assert (!op.oneway () || (op_md.params_out.empty () && op.tkind () == Type::Kind::VOID));

					cpp_ << "_call->invoke ();\n";

					if (!op.oneway ()) {

						cpp_ << "check_request (_call);\n";

						// Unmarshal output

						for (auto p : op_md.params_out) {
							cpp_ << TypePrefix (*p) << "unmarshal (_call, " << p->name () << ");\n";
						}
						if (op.tkind () != Type::Kind::VOID) {
							cpp_ << Var (op) << " _ret;\n"
								<< TypePrefix (op) << "unmarshal (_call, _ret);\n";
						}
						cpp_ << "_call->unmarshal_end ();\n";

						if (op.tkind () != Type::Kind::VOID)
							cpp_ << "return _ret;\n";
					}

					cpp_ << unindent
						<< "}\n\n";
				}
			} break;

			case Item::Kind::ATTRIBUTE: {
				const Attribute& att = static_cast <const Attribute&> (item);

				implement (att);

				metadata.emplace_back ();
				{
					OpMetadata& op_md = metadata.back ();
					op_md.name = "_get_";
					op_md.name += att.name ();
					op_md.type = &att;
				}

				bool custom = is_native (att);

				cpp_ << VRet (att) << ' ' << att.name () << " () const";
				if (custom) {
					cpp_ << ";\n"
						"static const UShort " PREFIX_OP_IDX "_get_" << att.name () << " = " << (metadata.size () - 1) << ";\n";
				} else {

					cpp_ << "\n{\n"
						<< indent

						<< "IORequest::_ref_type _call = _target ()->create_request (_make_op_idx ("
						<< (metadata.size () - 1) << "), 3);\n"
						"_call->invoke ();\n"
						"check_request (_call);\n"

						<< Var (att) << " _ret;\n"
						<< TypePrefix (att) << "unmarshal (_call, _ret);\n"
						"return _ret;\n"

						<< unindent
						<< "}\n";
				}

				if (!att.readonly ()) {

					metadata.emplace_back ();
					{
						OpMetadata& op_md = metadata.back ();
						op_md.name = "_set_";
						op_md.name += att.name ();
						op_md.type = nullptr;
						op_md.params_in.push_back (&att);
					}

					cpp_ << "void " << att.name () << " (" << ServantParam (att) << " val) const";
					if (custom) {
						cpp_ << ";\n"
							"static const UShort " PREFIX_OP_IDX "_set_" << att.name () << " = " << (metadata.size () - 1) << ";\n";
					} else {

						cpp_ << "\n{\n"
							<< indent

							<< "IORequest::_ref_type _call = _target ()->create_request (_make_op_idx ("
							<< (metadata.size () - 1) << "), 3);\n"
							<< TypePrefix (att) << "marshal_in (val, _call);\n"
							"_call->invoke ();\n"
							"check_request (_call);\n"

							<< unindent
							<< "}\n\n";
					}
				}
				cpp_ << endl;
			} break;
		}
	}

	cpp_ << "static const char* const __interfaces [];\n";
	if (!metadata.empty ())
		cpp_ << "static const Operation __operations [];\n";

	cpp_ << unindent
		<< "};\n\n";

	if (!metadata.empty ()) {

		for (const auto& op : metadata) {
			if (!op.params_in.empty ()) {
				cpp_ << "const Parameter Proxy <" << QName (itf) << ">::" PREFIX_OP_PARAM_IN << op.name << " [" << op.params_in.size () << "] = {\n";
				if (op.type) {
					md_members (op.params_in);
				} else {
					// _set_ operation
					assert (op.params_in.size () == 1);
					cpp_.indent ();
					md_member (*op.params_in.front (), string ());
					cpp_ << endl
						<< unindent;
				}
				cpp_ << "};\n";
			}
			if (!op.params_out.empty ()) {
				cpp_ << "const Parameter Proxy <" << QName (itf) << ">::" PREFIX_OP_PARAM_OUT << op.name << " [" << op.params_out.size () << "] = {\n";
				md_members (op.params_out);
				cpp_ << "};\n";
			}
		}

		cpp_ << "const Operation Proxy <" << QName (itf) << ">::__operations [" << metadata.size () << "] = {\n"
			<< indent;

		auto it = metadata.cbegin ();
		md_operation (itf, *it);
		for (++it; it != metadata.cend (); ++it) {
			cpp_ << ",\n";
			md_operation (itf, *it);
		}

		cpp_ << unindent
			<< "\n};\n";
	}

	cpp_ << "const Char* const Proxy <" << QName (itf) << ">::__interfaces [] = {\n"
		<< indent
		<< "RepIdOf <" << QName (itf) << ">::id";
	for (auto p : bases) {
		cpp_ << ",\n"
			<< "RepIdOf <" << QName (*p) << ">::id";
	}
	cpp_ << unindent
		<< "\n};\n"

		<< "template <>\n"
		"const InterfaceMetadata ProxyFactoryImpl <" << QName (itf) << ">::metadata_ = {\n"
		<< indent
		<< "{Proxy <" << QName (itf) << ">::__interfaces, countof (Proxy <" << QName (itf) << ">::__interfaces)},\n";
	if (metadata.empty ())
		cpp_ << "{nullptr, 0}";
	else
		cpp_ << "{Proxy <" << QName (itf) << ">::__operations, countof (Proxy <" << QName (itf) << ">::__operations)}\n";
	cpp_ << unindent
		<< "};\n";

	cpp_.namespace_close ();
	cpp_ << "NIRVANA_EXPORT (" << export_name (itf) << ", CORBA::Internal::RepIdOf <" << QName (itf)
		<< ">::id, CORBA::Internal::PseudoBase, CORBA::Internal::ProxyFactoryImpl <" << QName (itf) << ">)\n";
}

void Proxy::md_operation (const Interface& itf, const OpMetadata& op)
{
	cpp_ << "{ \"" << op.name << "\", { ";
	if (!op.params_in.empty ()) {
		string params = PREFIX_OP_PARAM_IN;
		params += op.name;
		cpp_ << params << ", countof (" << params << ')';
	} else
		cpp_ << "0, 0";
	cpp_ << " }, { ";
	if (!op.params_out.empty ()) {
		string params = PREFIX_OP_PARAM_OUT;
		params += op.name;
		cpp_ << params << ", countof (" << params << ')';
	} else
		cpp_ << "0, 0";
	cpp_ << " }, Type <" << (op.type ? *op.type : Type ())
		<< ">::type_code, RqProcWrapper <" PREFIX_OP_PROC << op.name << "> }";
}

string Proxy::export_name (const NamedItem& item)
{
	string name = "_exp";
	ScopedName sn = item.scoped_name ();
	for (const auto& n : sn) {
		name += '_';
		name += n;
	}
	return name;
}

inline
void Proxy::md_member (const Member& m)
{
	md_member (m, m.name ());
}

void Proxy::md_members (const Members& members)
{
	assert (!members.empty ());
	cpp_.indent ();
	auto it = members.begin ();
	md_member (**it);
	for (++it; it != members.end (); ++it) {
		cpp_ << ",\n";
		md_member (**it);
	}
	cpp_ << unindent << endl;
}

void Proxy::md_member (const Type& t, const string& name)
{
	cpp_ << "{ \"" << name << "\", Type <" << t << ">::type_code }";
}

void Proxy::type_code_members (const ItemWithId& item, const Members& members)
{
	assert (!members.empty ());
	cpp_ << "template <>\n"
		"const Parameter TypeCodeMembers <" << QName (item) << ">::members_ [] = {\n";

	md_members (members);

	cpp_ << "};\n\n";
}

Code& Proxy::exp (const NamedItem& item)
{
	return cpp_ <<
		"NIRVANA_EXPORT (" << export_name (item) << ", "
		"CORBA::Internal::RepIdOf <" << QName (item) << ">::id, CORBA::TypeCode, CORBA::Internal::";
}

void Proxy::type_code_name (const NamedItem& item)
{
	cpp_ << empty_line
		<< "template <>\n"
		"const Char TypeCodeName <" << QName (item) << ">::name_ [] = \"" << static_cast <const string&> (item.name ()) << "\";\n";
}

void Proxy::leaf (const Enum& item)
{
	if (is_pseudo (item))
		return;

	cpp_.namespace_open ("CORBA/Internal");
	type_code_name (item);
	cpp_ << "\n"
		"template <>\n"
		"const char* const TypeCodeEnum <" << QName (item) << ">::members_ [] = {\n";

	cpp_.indent ();
	auto it = item.begin ();
	cpp_ << '\"' << (*it)->name () << '\"';
	for (++it; it != item.end (); ++it) {
		cpp_ << ",\n\"" << (*it)->name () << '\"';
	};
	cpp_ << unindent
		<< "\n};\n";

	cpp_.namespace_close ();
	exp (item) << "TypeCodeEnum <" << QName (item) << ">)\n";
}

void Proxy::leaf (const Exception& item)
{
	if (!item.empty ()) {
		cpp_.namespace_open ("CORBA/Internal");
		type_code_members (item, item);
	}

	cpp_.namespace_close ();
	exp (item) << "TypeCodeException <" << QName (item) << ", " << (item.empty () ? "false" : "true") << ">)\n";
}

void Proxy::leaf (const Struct& item)
{
	if (is_pseudo (item) || is_native (item))
		return;

	cpp_.namespace_open ("CORBA/Internal");
	type_code_name (item);
	type_code_members (item, item);

	// Export TypeCode
	cpp_.namespace_close ();
	exp (item) << "TypeCodeStruct <" << QName (item) << ">)\n";
}

void Proxy::state_member (const AST::StateMember& m)
{
	cpp_ << "{ \"" << static_cast <const string&> (m.name ()) << "\", Type <"
		<< static_cast <const Type&> (m) << ">::type_code, "
		<< (m.is_public () ? "PUBLIC_MEMBER" : "PRIVATE_MEMBER")
		<< " }";
}

void Proxy::end (const ValueType& vt)
{
	cpp_.namespace_open ("CORBA/Internal");
	StateMembers members = get_members (vt);
	if (!members.empty ()) {
		cpp_ << empty_line
			<< "void ValueData <" << QName (vt) << ">::_marshal (I_ptr <IORequest> rq)\n"
			"{\n";
		marshal_members (cpp_, (const Members&)members, "marshal_in", "_", "this + 1");
		cpp_ << "}\n\n"
			"void ValueData <" << QName (vt) << ">::_unmarshal (I_ptr <IORequest> rq)\n"
			"{\n";
		unmarshal_members (cpp_, (const Members&)members, "_", "this + 1");
		cpp_ << "}\n";
	}

	type_code_name (vt);

	if (!members.empty ()) {
		cpp_ << empty_line
			<< "template <>\n"
			"const StateMember TypeCodeStateMembers <" << QName (vt) << ">::members_ [] = {\n";

		cpp_.indent ();
		auto it = members.begin ();
		state_member (**it);
		for (++it; it != members.end (); ++it) {
			cpp_ << ",\n";
			state_member (**it);
		}
		cpp_ << unindent
			<< "\n};\n\n";
	}

	cpp_ << empty_line <<
		"template <>\n"
		"class TypeCodeValue <" << QName (vt) << "> : public TypeCodeValue";
	if (vt.modifier () == ValueType::Modifier::ABSTRACT) {
		cpp_ << "Abstract <" << QName (vt);
	} else {
		const char* mod = "VM_NONE";
		switch (vt.modifier ()) {
			case ValueType::Modifier::CUSTOM:
				mod = "VM_CUSTOM";
				break;
			case ValueType::Modifier::TRUNCATABLE:
				mod = "TRUNCATABLE";
				break;
		}
		cpp_ << "Concrete <" << QName (vt) << ", " << mod << ", "
			<< (members.empty () ? "false" : "true") << ", ";

		const ValueType* concrete_base = nullptr;
		if (!vt.bases ().empty ()) {
			concrete_base = vt.bases ().front ();
			if (concrete_base->modifier () == ValueType::Modifier::ABSTRACT)
				concrete_base = nullptr;
		}

		if (concrete_base) {
			cpp_ << "Type <" << QName (*concrete_base) << ">::type_code";
		} else {
			cpp_ << "nullptr";
		}
	}
	cpp_ << ">\n"
		"{};\n";

	cpp_.namespace_close ();
	cpp_ << "NIRVANA_EXPORT (" << export_name (vt) << ", CORBA::Internal::RepIdOf <" << QName (vt) << ">::id, CORBA"
	<< (vt.modifier () != ValueType::Modifier::ABSTRACT ?
		"::Internal::PseudoBase, CORBA::Internal::ValueFactoryImpl <"
		:
		"::TypeCode, CORBA::Internal::TypeCodeValue <")
	<< QName (vt) << ">)\n";
}

void Proxy::leaf (const ValueBox& vb)
{
	cpp_.namespace_open ("CORBA/Internal");

	type_code_name (vb);

	cpp_.namespace_close ();
	cpp_ << "NIRVANA_EXPORT (" << export_name (vb) << ", CORBA::Internal::RepIdOf <" << QName (vb) << ">::id, CORBA"
		<< "::Internal::PseudoBase, CORBA::Internal::ValueBoxFactory <"
		<< QName (vb) << ">)\n";
}

void Proxy::leaf (const Union& item)
{
	if (is_pseudo (item) || is_native (item))
		return;

	cpp_.namespace_open ("CORBA/Internal");

	type_code_name (item);
	type_code_members (item, item);

	cpp_ << "template <>\n"
		"const TypeCodeUnion <" << QName (item) << ">::DiscriminatorType TypeCodeUnion <" << QName (item) << ">::labels_ [] = {\n"
		<< indent;

	auto it = item.begin ();
	assert (it != item.end ());
	cpp_ << ((*it)->is_default () ? item.default_label () : (*it)->labels ().front ());
	for (++it; it != item.end (); ++it) {
		cpp_ << ",\n" << ((*it)->is_default () ? item.default_label () : (*it)->labels ().front ());
	}
	cpp_ << "\n"
		<< unindent << "};\n";

	// Export TypeCode
	cpp_.namespace_close ();
	exp (item) << "TypeCodeUnion <" << QName (item) << ">)\n";
}
