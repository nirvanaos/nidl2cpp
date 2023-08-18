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
#define PREFIX_OP_RAISES "__raises_"
#define PREFIX_OP_CONTEXT "__raises_"
#define PREFIX_OP_IDX "__OPIDX_"

using namespace AST;

void Proxy::end (const Root&)
{
	if (custom_) {
		cpp_ << empty_line
			<< "#include <" << cpp_.file ().stem ().generic_string () << "_native.h>\n";
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

void Proxy::implement (const Operation& op, bool no_rq)
{
	cpp_.empty_line ();

	const ItemScope& itf = *op.parent ();

	Members params_in, params_out;
	get_parameters (op, params_in, params_out);

	if (!params_in.empty ())
		cpp_ << "static const Parameter " PREFIX_OP_PARAM_IN
		<< static_cast <const std::string&> (op.name ()) << " [" << params_in.size () << "];\n";

	if (!params_out.empty ())
		cpp_ << "static const Parameter " PREFIX_OP_PARAM_OUT
		<< static_cast <const std::string&> (op.name()) << "[" << params_out.size () << "];\n";

	if (!op.raises ().empty ())
		cpp_ << "static const TypeCodeImport* const " PREFIX_OP_RAISES
		<< static_cast <const std::string&> (op.name()) << "[" << op.raises().size() << "];\n";

	if (!op.context().empty())
		cpp_ << "static const Char* const " PREFIX_OP_CONTEXT
		<< static_cast <const std::string&> (op.name()) << "[" << op.context().size() << "];\n";

	if (no_rq)
		return;

	// Implement request static function

	cpp_ << empty_line
		<< "static void " PREFIX_OP_PROC << static_cast <const std::string&> (op.name ())
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
	if (op.tkind () != Type::Kind::VOID)
		cpp_ << TypePrefix (op) << "marshal_out (_ret, _call);\n";
	for (auto p : params_out) {
		cpp_ << TypePrefix (*p) << "marshal_out (" << p->name () << ", _call);\n";
	}

	cpp_ << unindent
		<< "}\n\n";
}

void Proxy::implement (const Attribute& att, bool no_rq)
{
	const ItemScope& itf = *att.parent ();

	if (!no_rq) {
		cpp_ << empty_line
			<< "static void " PREFIX_OP_PROC "_get_" << static_cast <const std::string&> (att.name ())
			<< " (" << QName (itf) << "::_ptr_type _servant, IORequest::_ptr_type _call)";

		if (is_native (att)) {
			custom_ = true;
			cpp_ << ";\n\n";
			return;
		}

		cpp_ << "\n{\n"
			<< indent
			<< "_call->unmarshal_end ();\n";

			// ret
		cpp_ << Var (att) << " _ret;\n";

		// Call
		cpp_ << "_ret = _servant->" << att.name () << " ();\n";

		// Marshal
		cpp_ << TypePrefix (att) << "marshal_out (_ret, _call);\n";

		cpp_ << unindent
			<< "}\n\n";
	}

	if (!att.readonly ()) {
		cpp_ << empty_line
			<< "static const Parameter " PREFIX_OP_PARAM_IN "_set_"
			<< static_cast <const std::string&> (att.name ()) << " [1];\n";

		if (!no_rq) {
			cpp_ << "\nstatic void " PREFIX_OP_PROC "_set_" << static_cast <const std::string&> (att.name ())
				<< " (" << QName (itf) << "::_ptr_type _servant, IORequest::_ptr_type _call)\n"
				"{\n"
				<< indent

				<< Var (att) << " val;\n"
				<< TypePrefix (att) << "unmarshal (_call, val);\n"
				"_call->unmarshal_end ();\n"
				"_servant->" << att.name () << " (val);\n"

				<< unindent
				<< "}\n\n";
		}
	}

	if (!att.getraises().empty())
		cpp_ << "static const TypeCodeImport* const " PREFIX_OP_RAISES "_get_"
		<< static_cast <const std::string&> (att.name ()) << "[" << att.getraises().size() << "];\n";

	if (!att.setraises().empty())
		cpp_ << "static const TypeCodeImport* const " PREFIX_OP_RAISES "_set_"
		<< static_cast <const std::string&> (att.name()) << "[" << att.setraises().size() << "];\n";
}

void Proxy::end (const Interface& itf)
{
	if (itf.interface_kind () == InterfaceKind::PSEUDO)
		return;

	const Compiler::AMI_Objects* ami = nullptr;
	{
		auto f = compiler ().ami_interfaces ().find (&itf);
		if (f != compiler ().ami_interfaces ().end ())
			ami = &f->second;
	}
	generate_proxy (itf, ami);
	if (ami)
		generate_poller (itf, *ami->poller);

	cpp_.namespace_close ();
	cpp_ << "NIRVANA_EXPORT (" << export_name (itf) << ", CORBA::Internal::RepIdOf <" << QName (itf)
		<< ">::id, CORBA::Internal::PseudoBase, CORBA::Internal::ProxyFactoryImpl"
		<< " <" << QName (itf);

	if (ami)
		cpp_ << ", " << QName (*ami->poller);

	cpp_ << ">)\n";
}

void Proxy::md_operation (const Interface& itf, const OpMetadata& op, bool no_rq)
{
	cpp_ << "{ \"" << op.name << "\", { ";
	bool in_complex = false, out_complex = false;
	if (!op.params_in.empty ()) {
		for (const Member* par : op.params_in) {
			if (is_complex_type (*par)) {
				in_complex = true;
				break;
			}
		}
		std::string params = PREFIX_OP_PARAM_IN;
		params += op.name;
		cpp_ << params << ", countof (" << params << ')';
	} else
		cpp_ << "0, 0";
	cpp_ << " }, { ";

	if (op.type && is_complex_type (*op.type))
		out_complex = true;

	if (!op.params_out.empty ()) {
		if (!out_complex)
			for (const Member* par : op.params_out) {
				if (is_complex_type (*par)) {
					out_complex = true;
					break;
				}
			}
		std::string params = PREFIX_OP_PARAM_OUT;
		params += op.name;
		cpp_ << params << ", countof (" << params << ')';
	} else
		cpp_ << "0, 0";
	cpp_ << " }, { ";

	if (!op.raises->empty()) {
		std::string raises = PREFIX_OP_RAISES;
		raises += op.name;
		cpp_ << raises << ", countof (" << raises << ')';
	} else
		cpp_ << "0, 0";
	cpp_ << " }, { ";

	if (op.context && !op.context->empty()) {
		std::string context = PREFIX_OP_CONTEXT;
		context += op.name;
		cpp_ << context << ", countof (" << context << ')';
	} else
		cpp_ << "0, 0";
	cpp_ << " }, ";

	if (op.type && op.type->tkind () != Type::Kind::VOID)
		cpp_ << "Type <" << *op.type << ">::type_code";
	else
		cpp_ << "0";

	const char* flags = "0";
	if (in_complex)
		if (out_complex)
			flags = "Operation::FLAG_IN_CPLX | Operation::FLAG_OUT_CPLX";
		else
			flags = "Operation::FLAG_IN_CPLX";
	else if (out_complex)
		flags = "Operation::FLAG_OUT_CPLX";

	if (no_rq)
		cpp_ << ", nullptr, 0 }";
	else
		cpp_ << ", RqProcWrapper <" PREFIX_OP_PROC << op.name << ">, " << flags << " }";
}

std::string Proxy::export_name (const NamedItem& item)
{
	std::string name = "_exp";
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
	cpp_ << unindent << std::endl;
}

void Proxy::md_member (const Type& t, const std::string& name)
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
		"const Char TypeCodeName <" << QName (item) << ">::name_ [] = \"" << static_cast <const std::string&> (item.name ()) << "\";\n";
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
	cpp_ << "{ \"" << static_cast <const std::string&> (m.name ()) << "\", Type <"
		<< static_cast <const Type&> (m) << ">::type_code, "
		<< (m.is_public () ? "PUBLIC_MEMBER" : "PRIVATE_MEMBER")
		<< " }";
}

void Proxy::end (const ValueType& vt)
{
	cpp_.namespace_open ("CORBA/Internal");
	StateMembers members = get_members (vt);
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

void Proxy::generate_proxy (const Interface& itf, const Compiler::AMI_Objects* ami)
{
	cpp_.namespace_open ("CORBA/Internal");
	cpp_.empty_line ();
	type_code_name (itf);

	bool stateless = is_stateless (itf);

	bool local_stateless = stateless && (itf.interface_kind () == InterfaceKind::LOCAL);

	const char* proxy_base = stateless ? "ProxyBaseStateless" : "ProxyBase";

	if (ami)
		cpp_ << "NIRVANA_IMPLEMENT_AMI (Proxy <" << QName (itf) << ">, " << QName (itf) << ");\n\n";

	cpp_ << "template <>\n"
		"class Proxy <" << QName (itf) << "> : public " << proxy_base << " <" << QName (itf)
		<< indent;

	Interfaces bases = itf.get_all_bases ();
	for (auto p : bases) {
		cpp_ << ",\n" << QName (*p);
	}

	cpp_ << '>';

	if (ami)
		cpp_ << ",\n"
		"public AMI_Skeleton <Proxy <" << QName (itf) << ">, " << QName (itf) << '>';

	cpp_ << unindent
		<< "\n{\n"
		<< indent
		<< "typedef " << proxy_base << " <" << QName (itf);
	for (auto p : bases) {
		cpp_ << ", " << QName (*p);
	}
	cpp_ << "> Base;\n\n"
		<< unindent
		<< "public:\n"
		<< indent

		// Constructor
		<< "Proxy (Object::_ptr_type obj, AbstractBase::_ptr_type ab, IOReference::_ptr_type proxy_manager, "
		"uint16_t interface_idx, Interface*& servant) :\n"
		<< indent
		<< "Base (obj, ab, proxy_manager, interface_idx, servant)\n"
		<< unindent
		<< "{}\n";

	// Operations
	Metadata metadata;
	for (auto it = itf.begin (); it != itf.end (); ++it) {
		const Item& item = **it;
		switch (item.kind ()) {

		case Item::Kind::OPERATION: {
			const Operation& op = static_cast <const Operation&> (item);

			implement (op, local_stateless);

			unsigned op_idx = (unsigned)metadata.size ();
			metadata.emplace_back ();
			OpMetadata& op_md = metadata.back ();
			op_md.name = op.name ();
			// Escape fault tolerance FT_HB operation.
			if (op.name () == "FT_HB")
				op_md.name.insert (0, 1, '_');
			op_md.type = &op;
			op_md.raises = &op.raises ();
			op_md.context = &op.context ();
			get_parameters (op, op_md.params_in, op_md.params_out);

			cpp_ << ServantOp (op) << " const";
			if (is_custom (op)) {
				cpp_ << ";\n"
					"static const UShort " PREFIX_OP_IDX << static_cast <const std::string&> (op.name ())
					<< " = " << op_idx << ";\n";
			} else {

				cpp_ << "\n{\n" << indent;

				if (stateless) {
					if (!local_stateless)
						cpp_ << "if (servant ())\n" << indent;

					if (op.tkind () != Type::Kind::VOID)
						cpp_ << "return ";
					cpp_ << "servant ()->" << op.name () << " (";
					{
						auto it = op.begin ();
						if (it != op.end ()) {
							cpp_ << (*it)->name ();
							for (++it; it != op.end (); ++it) {
								cpp_ << ", " << (*it)->name ();
							}
						}
					}
					cpp_ << ");\n";

					if (!local_stateless)
						cpp_ << unindent << "else {\n" << indent;
				}

				if (!local_stateless) {
					// Create request
					cpp_ << "IORequest::_ref_type _call = _target ()->create_request (_make_op_idx (" << op_idx
						<< "), " << (op.oneway () ? "0" : "3") << ", nullptr);\n";

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

						if (op.tkind () != Type::Kind::VOID) {
							cpp_ << Var (op) << " _ret;\n"
								<< TypePrefix (op) << "unmarshal (_call, _ret);\n";
						}
						for (auto p : op_md.params_out) {
							cpp_ << TypePrefix (*p) << "unmarshal (_call, " << p->name () << ");\n";
						}
						cpp_ << "_call->unmarshal_end ();\n";

						if (op.tkind () != Type::Kind::VOID)
							cpp_ << "return _ret;\n";
					}

					if (stateless)
						cpp_ << unindent << "}\n";
				}

				cpp_ << unindent << "}\n\n";

				// Generate AMI calls
				if (ami) {

					// sendc_

					cpp_ << "void " AMI_SENDC << static_cast <const std::string&> (op.name ()) << " ("
						<< ServantParam (Type (ami->handler)) << " " AMI_HANDLER;

					for (auto param : op_md.params_in) {
						cpp_ << ", " << ServantParam (*param) << ' ' << param->name ();
					}

					cpp_ << ") const\n"
						"{\n" << indent
						// Create request
						<< "IORequest::_ref_type _call = _target ()->create_request (_make_op_idx (" << op_idx
						<< "), 3, ::Messaging::ReplyHandler::_ptr_type (" AMI_HANDLER "));\n";

					// Marshal input
					for (auto p : op_md.params_in) {
						cpp_ << TypePrefix (*p) << "marshal_in (" << p->name () << ", _call);\n";
					}

					// Call
					cpp_ << "_call->invoke ();\n"
						<< unindent << "}\n";

					// sendp_

					cpp_ << POLLER_TYPE_PREFIX "VRet " AMI_SENDP << static_cast <const std::string&> (op.name ()) << " (";

					auto param = op_md.params_in.begin ();
					if (param != op_md.params_in.end ()) {
						cpp_ << ServantParam (**param) << ' ' << (*param)->name ();
						for (++param; param != op_md.params_in.end (); ++param) {
							cpp_ << ", " << ServantParam (**param) << ' ' << (*param)->name ();
						}
					}
					cpp_ << ") const\n"
						"{\n" << indent
						// Create poller and request
						<< "OperationIndex _op = _make_op_idx (" << op_idx << ");\n"
							"CORBA::Pollable::_ref_type _poller = _target ()->create_poller (_op);\n"
							"IORequest::_ref_type _call = _target ()->create_request (_op, 3, CORBA::Pollable::_ptr_type (_poller));\n";

					// Marshal input
					for (auto p : op_md.params_in) {
						cpp_ << TypePrefix (*p) << "marshal_in (" << p->name () << ", _call);\n";
					}

					// Call
					cpp_ << "_call->invoke ();\n"
						"return _poller;\n"
						<< unindent << "}\n";
				}
			}
		} break;

		case Item::Kind::ATTRIBUTE: {
			const Attribute& att = static_cast <const Attribute&> (item);

			implement (att, local_stateless);

			unsigned op_idx = (unsigned)metadata.size ();
			metadata.emplace_back ();
			{
				OpMetadata& op_md = metadata.back ();
				op_md.name = "_get_";
				op_md.name += att.name ();
				op_md.type = &att;
				op_md.raises = &att.getraises ();
				op_md.context = nullptr;
			}

			bool custom = is_native (att);

			cpp_ << VRet (att) << ' ' << att.name () << " () const";
			if (custom) {
				cpp_ << ";\n"
					"static const UShort " PREFIX_OP_IDX "_get_" << static_cast <const std::string&> (att.name ())
					<< " = " << op_idx << ";\n";
			} else {

				cpp_ << "\n{\n" << indent;

				if (stateless) {
					if (!local_stateless)
						cpp_ << "if (servant ())\n" << indent;

					cpp_ << "return servant ()->" << att.name () << " ();\n";

					if (!local_stateless)
						cpp_ << unindent << "else {\n" << indent;
				}

				if (!local_stateless) {
					cpp_ << "IORequest::_ref_type _call = _target ()->create_request (_make_op_idx ("
						<< op_idx << "), 3, nullptr);\n"
						"_call->invoke ();\n"
						"check_request (_call);\n"

						<< Var (att) << " _ret;\n"
						<< TypePrefix (att) << "unmarshal (_call, _ret);\n"
						"return _ret;\n";

					if (stateless)
						cpp_ << unindent << "}\n";
				}

				cpp_ << unindent << "}\n\n";

				// Generate AMI calls
				if (ami) {

					// sendc_

					cpp_ << "void " AMI_SENDC "get_" << static_cast <const std::string&> (att.name ()) << " ("
						<< ServantParam (Type (ami->handler)) << " " AMI_HANDLER ") const\n"
						"{\n" << indent
						// Create request and call
						<< "IORequest::_ref_type _call = _target ()->create_request (_make_op_idx (" << op_idx
						<< "), 3, ::Messaging::ReplyHandler::_ptr_type (" AMI_HANDLER "));\n"
						"_call->invoke ();\n"
						<< unindent << "}\n";

					// sendp_

					cpp_ << POLLER_TYPE_PREFIX "VRet " AMI_SENDP "get_" << static_cast <const std::string&> (att.name ())
						<< " () const\n"
						"{\n" << indent
						// Create poller and request
						<< "CORBA::Pollable::_ref_type _poller = _target ()->create_poller (" << op_idx << ");\n"
						"IORequest::_ref_type _call = _target ()->create_request (_make_op_idx (" << op_idx
						<< "), 3, CORBA::Pollable::_ptr_type (_poller));\n"
						"_call->invoke ();\n"
						"return _poller;\n"
						<< unindent << "}\n";
				}
			}

			if (!att.readonly ()) {

				op_idx = (unsigned)metadata.size ();
				metadata.emplace_back ();
				{
					OpMetadata& op_md = metadata.back ();
					op_md.name = "_set_";
					op_md.name += att.name ();
					op_md.type = nullptr;
					op_md.raises = &att.setraises ();
					op_md.context = nullptr;
					op_md.params_in.push_back (&att);
				}

				cpp_ << "void " << att.name () << " (" << ServantParam (att) << " val) const";
				if (custom) {
					cpp_ << ";\n"
						"static const UShort " PREFIX_OP_IDX "_set_" << static_cast <const std::string&> (att.name ())
						<< " = " << op_idx << ";\n";
				} else {

					cpp_ << "\n{\n" << indent;

					if (stateless) {
						if (!local_stateless)
							cpp_ << "if (servant ())\n" << indent;

						cpp_ << "servant ()->" << att.name () << " (val);\n";

						if (!local_stateless)
							cpp_ << unindent << "else {\n" << indent;
					}

					if (!local_stateless) {
						cpp_ << "IORequest::_ref_type _call = _target ()->create_request (_make_op_idx ("
							<< op_idx << "), 3, nullptr);\n"
							<< TypePrefix (att) << "marshal_in (val, _call);\n"
							"_call->invoke ();\n"
							"check_request (_call);\n";

						if (stateless)
							cpp_ << unindent << "}\n";
					}

					cpp_ << unindent << "}\n\n";
				}

				// Generate AMI calls
				if (ami) {

					// sendc_

					cpp_ << "void " AMI_SENDC "set_" << static_cast <const std::string&> (att.name ()) << " ("
						<< ServantParam (Type (ami->handler)) << " " AMI_HANDLER ", "
						<< ServantParam (att) << " val) const\n"
						"{\n" << indent
						// Create request, marshal val and call
						<< "IORequest::_ref_type _call = _target ()->create_request (_make_op_idx (" << op_idx
						<< "), 3, ::Messaging::ReplyHandler::_ptr_type (" AMI_HANDLER "));\n"
						<< TypePrefix (att) << "marshal_in (val, _call);\n"
						"_call->invoke ();\n"
						<< unindent << "}\n";

					// sendp_

					cpp_ << POLLER_TYPE_PREFIX "VRet " AMI_SENDP "set_"
						<< static_cast <const std::string&> (att.name ()) << " ("
						<< ServantParam (att) << " val) const\n"
						"{\n" << indent
						// Create poller and request, marshal val and call
						<< "CORBA::Pollable::_ref_type _poller = _target ()->create_poller (" << op_idx << ");\n"
						"IORequest::_ref_type _call = _target ()->create_request (_make_op_idx (" << op_idx
						<< "), 3, CORBA::Pollable::_ptr_type (_poller));\n"
						<< TypePrefix (att) << "marshal_in (val, _call);\n"
						"_call->invoke ();\n"
						"return _poller;\n"
						<< unindent << "}\n";
				}
			}
		} break;
		}
	}

	cpp_ << "static const char* const __interfaces [];\n";
	if (!metadata.empty ())
		cpp_ << "static const Operation __operations [];\n";

	cpp_ << unindent << "};\n\n";

	if (!metadata.empty ()) {

		for (const auto& op : metadata) {

			if (!op.params_in.empty ()) {
				cpp_ << "const Parameter Proxy <" << QName (itf) << ">::" PREFIX_OP_PARAM_IN << op.name
					<< " [" << op.params_in.size () << "] = {\n";
				if (op.type) {
					md_members (op.params_in);
				} else {
					// _set_ operation
					assert (op.params_in.size () == 1);
					cpp_.indent ();
					md_member (*op.params_in.front (), std::string ());
					cpp_ << std::endl
						<< unindent;
				}
				cpp_ << "};\n";
			}

			if (!op.params_out.empty ()) {
				cpp_ << "const Parameter Proxy <" << QName (itf) << ">::" PREFIX_OP_PARAM_OUT << op.name
					<< " [" << op.params_out.size () << "] = {\n";
				md_members (op.params_out);
				cpp_ << "};\n";
			}

			if (!op.raises->empty ()) {
				cpp_ << "const TypeCodeImport* const Proxy <" << QName (itf) << ">::" PREFIX_OP_RAISES << op.name
					<< " [" << op.raises->size () << "] = {\n";
				auto it = op.raises->begin ();
				cpp_ << '&' << TC_Name (**it);
				for (++it; it != op.raises->end (); ++it) {
					cpp_ << ",\n"
						"&" << TC_Name (**it);
				}
				cpp_ << std::endl;
				cpp_ << "};\n";
			}

			if (op.context && !op.context->empty ()) {
				cpp_ << "const Char* const Proxy <" << QName (itf) << ">::" PREFIX_OP_CONTEXT << op.name
					<< " [" << op.context->size () << "] = {\n";
				auto it = op.context->begin ();
				cpp_ << '"' << *it << '"';
				for (++it; it != op.context->end (); ++it) {
					cpp_ << ",\n"
						"\"" << *it << '"';
				}
				cpp_ << std::endl;
				cpp_ << "};\n";
			}
		}

		cpp_ << "const Operation Proxy <" << QName (itf) << ">::__operations [" << metadata.size () << "] = {\n"
			<< indent;

		auto it = metadata.cbegin ();
		md_operation (itf, *it, local_stateless);
		for (++it; it != metadata.cend (); ++it) {
			cpp_ << ",\n";
			md_operation (itf, *it, local_stateless);
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
	cpp_ << unindent << "\n};\n"
		<< "template <>\n"
		"const InterfaceMetadata MetadataOf <" << QName (itf) << ">::metadata_ = {\n"
		<< indent
		<< "{Proxy <" << QName (itf) << ">::__interfaces, countof (Proxy <" << QName (itf) << ">::__interfaces)},\n";
	if (metadata.empty ())
		cpp_ << "{nullptr, 0},\n";
	else
		cpp_ << "{Proxy <" << QName (itf) << ">::__operations, countof (Proxy <" << QName (itf) << ">::__operations)},\n";
	cpp_ << (local_stateless ? "InterfaceMetadata::FLAG_LOCAL_STATELESS" : "0");
	if (ami) {
		cpp_ << ",\n\"" << ami->poller->repository_id () << '\"'
			<< ",\n\"" << ami->handler->repository_id () << '\"';
	}
	cpp_ << "\n" << unindent << "};\n";
}

void Proxy::generate_poller (const Interface& itf, const ValueType& poller)
{
	cpp_.namespace_open ("CORBA/Internal");

	cpp_ << "\n"
		"template <>\n"
		"class Poller <" << QName (poller) << "> : public PollerBase <" << QName (poller);

	std::vector <const ValueType*> base_pollers;
	{
		Interfaces bases = itf.get_all_bases ();
		for (const auto& b : bases) {
			auto f = compiler ().ami_interfaces ().find (b);
			if (f != compiler ().ami_interfaces ().end ())
				base_pollers.push_back (f->second.poller);
		}
	}
	
	for (const auto& b : base_pollers) {
		cpp_ << ", " << QName (*b);
	}
	
	cpp_ << ">\n"
		"{\n"
		<< indent
		<< "typedef PollerBase <" << QName (poller);
	for (const auto& b : base_pollers) {
		cpp_ << ", " << QName (*b);
	}

	cpp_ << "> Base;\n\n"
		<< unindent
		<< "public:\n"
		<< indent

		// Constructor
		<< "Poller (ValueBase::_ptr_type vb, Messaging::Poller::_ptr_type aggregate, UShort interface_idx) :\n"
		<< indent
		<< "Base (vb, aggregate, interface_idx)\n"
		<< unindent
		<< "{}\n";

	// Operations
	unsigned op_idx = 0;
	for (auto item : itf) {
		switch (item->kind ()) {

		case Item::Kind::OPERATION: {
			const Operation& op = static_cast <const Operation&> (*item);
			cpp_ << "void " << op.name () << " (ULong " AMI_TIMEOUT;
			if (op.tkind () != Type::Kind::VOID)
				cpp_ << ", " << TypePrefix (op) << "Var& " AMI_RETURN_VAL;
			for (auto param : op) {
				if (param->attribute () != Parameter::Attribute::IN)
					cpp_ << ", " << TypePrefix (*param) << "Var& " << param->name ();
			}
			cpp_ << ")\n"
				"{\n" << indent
				<< "IORequest::_ref_type _reply = _get_reply <" << op.raises () << "> (" AMI_TIMEOUT ", " << op_idx << ");\n";

			for (auto param : op) {
				if (param->attribute () != Parameter::Attribute::IN)
					cpp_ << TypePrefix (*param) << "unmarshal (_reply, " << param->name () << ");\n";
			}
			if (op.tkind () != Type::Kind::VOID)
				cpp_ << TypePrefix (op) << "unmarshal (_reply, " AMI_RETURN_VAL ");\n";

			cpp_ << unindent << "}\n";
			++op_idx;
		} break;

		case Item::Kind::ATTRIBUTE: {
			const Attribute& att = static_cast <const Attribute&> (*item);
			cpp_ << "void get_" << att.name () << " (ULong " AMI_TIMEOUT ", "
				<< TypePrefix (att) << "Var& " AMI_RETURN_VAL ")\n"
				"{\n" << indent
				<< "IORequest::_ref_type _reply = _get_reply <" << att.getraises () << "> (" AMI_TIMEOUT ", " << op_idx << ");\n"
				<< TypePrefix (att) << "unmarshal (_reply, " AMI_RETURN_VAL ");\n";

			cpp_ << unindent << "}\n";
			++op_idx;

			if (!att.readonly ()) {
				cpp_ << "void set_" << att.name () << " (ULong " AMI_TIMEOUT ")\n"
					"{\n" << indent
					<< "IORequest::_ref_type _reply = _get_reply <" << att.setraises () << "> (" AMI_TIMEOUT ", " << op_idx << ");\n";
				cpp_ << unindent << "}\n";
				++op_idx;
			}
		} break;

		}
	}

	// End of Poller
	cpp_ << unindent << "};\n";

	cpp_.namespace_close ();
}

