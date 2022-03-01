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

Code& operator << (Code& stm, const Proxy::WithAlias& t)
{
	if (t.type.tkind () == Type::Kind::NAMED_TYPE && t.type.named_type ().kind () == Item::Kind::TYPE_DEF)
		stm << "Alias <&" << CodeGenBase::TypeCodeName (t.type.named_type ()) << ">";
	else
		stm << t.type;
	return stm;
}

void Proxy::end (const Root&)
{
	if (custom_) {
		cpp_.empty_line ();
		cpp_ << "#include \"" << cpp_.file ().stem ().generic_string () << "_native.h\"\n";
	}
	cpp_.close ();
}

void Proxy::leaf (const TypeDef& item)
{
	string name = export_name (item);
	cpp_.empty_line ();
	cpp_ << "extern \"C\" NIRVANA_OLF_SECTION const Nirvana::ExportInterface " << name << ";\n\n";
	cpp_.namespace_open ("CORBA/Internal");
	cpp_ <<
		"template <>\n"
		"class TypeCodeTypeDef <&::" << name << "> :\n";
	cpp_.indent ();
	cpp_ << "public TypeCodeTypeDefImpl <&::" << name << ", " << WithAlias (item) << ">\n";
	cpp_.unindent ();
	cpp_ << "{};\n"
		"\n"
		"template <>\n"
		"const Char TypeCodeName <TypeCodeTypeDef <&::" << name << "> >::name_ [] = \"" << static_cast <const string&> (item.name ()) << "\";\n";
	cpp_.namespace_close ();
	cpp_ << "NIRVANA_EXPORT (" << name << ", \"" << item.repository_id () << "\", CORBA::TypeCode, CORBA::Internal::TypeCodeTypeDef <&" << name << ">)\n";
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

	cpp_.empty_line ();
	cpp_ << "static void " PREFIX_OP_PROC << static_cast <const string&> (op.name ())
		<< " (" << QName (itf) << "::_ptr_type _servant, IORequest::_ptr_type _call)";

	if (is_custom (op)) {
		custom_ = true;
		cpp_ << ";\n\n";
		return;
	}

	cpp_ << "\n{\n";

	cpp_.indent ();

	// out and inout params
	for (auto p : params_out) {
		cpp_ << Var (*p) << ' ' << p->name () << ";\n";
	}
	if (op.tkind () != Type::Kind::VOID)
		cpp_ << Var (op) << " _ret;\n";

	cpp_ << "{\n";
	cpp_.indent ();

	// in params
	for (auto it = op.begin (); it != op.end (); ++it) {
		const Parameter* p = *it;
		if (p->attribute () == Parameter::Attribute::IN)
			cpp_ << Var (*p) << ' ' << p->name () << ";\n";
	}

	// Unmarshal in and inout
	if (!params_in.empty ()) {
		for (auto p : params_in) {
			cpp_ << TypePrefix (*p) << "unmarshal (_call, " << p->name () << ");\n";
		}
		cpp_ << "_call->unmarshal_end ();\n";
	}

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
	cpp_ << ");\n";
	cpp_.unindent ();
	cpp_ << "}\n"; // Input params are out of scope here

	// Marshal output
	for (auto p : params_out) {
		cpp_ << TypePrefix (*p) << "marshal_out (" << p->name () << ", _call);\n";
	}
	if (op.tkind () != Type::Kind::VOID)
		cpp_ << TypePrefix (op) << "marshal_out (_ret, _call);\n";

	cpp_.unindent ();
	cpp_ << "}\n";
}

void Proxy::implement (const Attribute& att)
{
	const ItemScope& itf = *att.parent ();

	cpp_.empty_line ();
	cpp_ << "static void " PREFIX_OP_PROC "_get_" << static_cast <const string&> (att.name ())
		<< " (" << QName (itf) << "::_ptr_type _servant, IORequest::_ptr_type _call)";

	if (is_native (att)) {
		custom_ = true;
		cpp_ << ";\n\n";
		return;
	}

	cpp_ << "\n{\n";
	cpp_.indent ();

	// ret
	cpp_ << Var (att) << " _ret;\n";

	// Call
	cpp_ << "_ret = _servant->" << att.name () << " ();\n";

	// Marshal
	cpp_ << TypePrefix (att) << "marshal_out (_ret, _call);\n";

	cpp_.unindent ();
	cpp_ << "}\n";

	if (!att.readonly ()) {
		cpp_.empty_line ();

		cpp_ << "static const Parameter " PREFIX_OP_PARAM_IN "_set_" << static_cast <const string&> (att.name ()) << " [1];\n\n";

		cpp_ << "static void " PREFIX_OP_PROC "_set_" << static_cast <const string&> (att.name ())
			<< " (" << QName (itf) << "::_ptr_type _servant, IORequest::_ptr_type _call)\n"
			"{\n";
		cpp_.indent ();

		cpp_ << Var (att) << " val;\n";
		cpp_ << TypePrefix (att) << "unmarshal (_call, val);\n"
			"_call->unmarshal_end ();\n"
			"_servant->" << att.name () << " (val);\n";

		cpp_.unindent ();
		cpp_ << "}\n";
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
	cpp_ << "IMPLEMENT_PROXY_FACTORY(" << ParentName (itf) << ", " << itf.name () << ");\n"
		"\n"
		"template <>\n"
		"class Proxy <" << QName (itf) << "> : public ProxyBase <" << QName (itf) << '>';

	Interfaces bases = itf.get_all_bases ();
	cpp_.indent ();
	for (auto p : bases) {
		cpp_ << ",\npublic ProxyBaseInterface <" << QName (*p) << '>';
	}
	cpp_.unindent ();
	cpp_ << "\n{\n";
	cpp_.indent ();
	cpp_ << "typedef ProxyBase <" << QName (itf) << "> Base;\n";
	cpp_.unindent ();
	cpp_ << "public:\n";
	cpp_.indent ();

	// Constructor
	cpp_ << "Proxy (IOReference::_ptr_type proxy_manager, uint16_t interface_idx) :\n";
	cpp_.indent ();
	cpp_ << "Base (proxy_manager, interface_idx)\n";
	cpp_.unindent ();
	cpp_ << '{';
	if (!bases.empty ()) {
		cpp_ << endl;
		cpp_.indent ();
		cpp_ << "AbstractBase::_ptr_type ab = proxy_manager->object ();\n";
		for (auto p : bases) {
			cpp_ << "ProxyBaseInterface <" << QName (*p) << ">::init (ab);\n";
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
					cpp_ << "IORequest::_ref_type _call = _target ()->create_";

					if (op.oneway ())
						cpp_ << "oneway";
					else
						cpp_ << "request";
						
					cpp_ << " (_make_op_idx ("
						<< (metadata.size () - 1) << "));\n";

					// Marshal input
					for (auto p : op_md.params_in) {
						cpp_ << TypePrefix (*p) << "marshal_in (" << p->name () << ", _call);\n";
					}

					// Call
					assert (!op.oneway () || (op_md.params_out.empty () && op.tkind () == Type::Kind::VOID));

					if (op.oneway ())
						cpp_ << "_target ()->send (_call, ::Nirvana::INFINITE_DEADLINE);\n";
					else {

						cpp_ << "_call->invoke ();\n"
							"check_request (_call);\n";

						// Unmarshal output

						for (auto p : op_md.params_out) {
							cpp_ << TypePrefix (*p) << "unmarshal (_call, " << p->name () << ");\n";
						}
						if (op.tkind () != Type::Kind::VOID) {
							cpp_ << Var (op) << " _ret;\n"
								<< TypePrefix (op) << "unmarshal (_call, _ret);\n"
								"return _ret;\n";
						}
					}

					cpp_.unindent ();
					cpp_ << "}\n\n";
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

					cpp_ << "\n{\n";
					cpp_.indent ();

					cpp_ << "IORequest::_ref_type _call = _target ()->create_request (_make_op_idx ("
						<< (metadata.size () - 1) << "));\n"
						"_call->invoke ();\n"
						"check_request (_call);\n";

					cpp_ << Var (att) << " _ret;\n"
						<< TypePrefix (att) << "unmarshal (_call, _ret);\n"
						"return _ret;\n";

					cpp_.unindent ();
					cpp_ << "}\n";
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

						cpp_ << "\n{\n";
						cpp_.indent ();

						cpp_ << "IORequest::_ref_type _call = _target ()->create_request (_make_op_idx ("
							<< (metadata.size () - 1) << "));\n"
							<< TypePrefix (att) << "marshal_in (val, _call);\n"
							"_call->invoke ();\n"
							"check_request (_call);\n";

						cpp_.unindent ();
						cpp_ << "}\n\n";
					}
				}
				cpp_ << endl;
			} break;
		}
	}

	cpp_ << "static const char* const __interfaces [];";
	if (!metadata.empty ())
		cpp_ << "static const Operation __operations [];";

	cpp_.unindent ();
	cpp_ << "};\n\n";

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
					cpp_ << endl;
					cpp_.unindent ();
				}
				cpp_ << "};\n";
			}
			if (!op.params_out.empty ()) {
				cpp_ << "const Parameter Proxy <" << QName (itf) << ">::" PREFIX_OP_PARAM_OUT << op.name << " [" << op.params_out.size () << "] = {\n";
				md_members (op.params_out);
				cpp_ << "};\n";
			}
		}

		cpp_ << "const Operation Proxy <" << QName (itf) << ">::__operations [" << metadata.size () << "] = {\n";
		cpp_.indent ();

		auto it = metadata.cbegin ();
		md_operation (itf, *it);
		for (++it; it != metadata.cend (); ++it) {
			cpp_ << ",\n";
			md_operation (itf, *it);
		}

		cpp_.unindent ();
		cpp_ << "\n};\n";
	}

	cpp_ << "const Char* const Proxy <" << QName (itf) << ">::__interfaces [] = {\n";
	cpp_.indent ();
	cpp_ << QName (itf) << "::repository_id_";
	for (auto p : bases) {
		cpp_ << ",\n" << QName (*p) << "::repository_id_";
	}
	cpp_.unindent ();
	cpp_ << "\n};\n";

	cpp_ <<
		"template <>\n"
		"const InterfaceMetadata ProxyFactoryImpl <" << QName (itf) << ">::metadata_ = {\n";
	cpp_.indent ();
	cpp_ << "{Proxy <" << QName (itf) << ">::__interfaces, countof (Proxy <" << QName (itf) << ">::__interfaces)},\n";
	if (metadata.empty ())
		cpp_ << "{nullptr, 0}";
	else
		cpp_ << "{Proxy <" << QName (itf) << ">::__operations, countof (Proxy <" << QName (itf) << ">::__operations)}\n";
	cpp_.unindent ();
	cpp_ << "};\n";

	cpp_.namespace_close ();
	cpp_ << "NIRVANA_EXPORT (" << export_name (itf) << ", " << QName (itf)
		<< "::repository_id_, CORBA::AbstractBase, CORBA::Internal::ProxyFactoryImpl <" << QName (itf) << ">)\n";
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
	cpp_ << " }, Type <" << WithAlias (op.type ? *op.type : Type ())
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
	cpp_.unindent ();
	cpp_ << endl;
}

void Proxy::md_member (const Type& t, const string& name)
{
	cpp_ << "{ \"" << name << "\", Type <" << WithAlias (t) << ">::type_code }";
}

void Proxy::type_code_members (const NamedItem& item, const Members& members)
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
		"CORBA::Internal::RepIdOf <" << QName (item) << ">::repository_id_, CORBA::TypeCode, CORBA::Internal::";
}

void Proxy::end (const Exception& item)
{
	Members members = get_members (item);

	if (!members.empty ()) {
		cpp_.namespace_open ("CORBA/Internal");
		type_code_members (item, members);
	}

	cpp_.namespace_close ();
	exp (item) << "TypeCodeException <" << QName (item) << ", " << (members.empty () ? "false" : "true") << ">)\n";
}

void Proxy::type_code_name (const NamedItem& item)
{
	cpp_ << "template <>\n"
		"const Char TypeCodeName <" << QName (item) << ">::name_ [] = \"" << static_cast <const string&> (item.name ()) << "\";\n";
}

void Proxy::end (const Struct& item)
{
	if (is_pseudo (item))
		return;

	cpp_.namespace_open ("CORBA/Internal");
	type_code_name (item);
	type_code_members (item, get_members (item));
	cpp_.namespace_close ();
	exp (item) << "TypeCodeStruct <" << QName (item) << ">)\n";
}

void Proxy::leaf (const Enum& item)
{
	if (is_pseudo (item))
		return;

	cpp_.namespace_open ("CORBA/Internal");
	cpp_.empty_line ();
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
	cpp_.unindent ();
	cpp_ << "\n};\n";

	cpp_.namespace_close ();
	exp (item) << "TypeCodeEnum <" << QName (item) << ">)\n";
}
