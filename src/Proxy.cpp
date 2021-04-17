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

using namespace std;
using namespace AST;

void Proxy::end (const Root&)
{
	cpp_.close ();
}

void Proxy::leaf (const TypeDef& item)
{
	cpp_ << "NIRVANA_EXPORT (" << export_name (item) << "_TC, \"" << item.repository_id ()
		<< "\", CORBA::TypeCode, CORBA::Nirvana::TypeCodeEnum < " << QName (item) << ">)\n";
}

void Proxy::get_parameters (const AST::Operation& op, Members& params_in, Members& params_out)
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
	const ItemScope& itf = *op.parent ();

	Members params_in, params_out;
	get_parameters (op, params_in, params_out);

	string type_in, type_out;
	if (!params_in.empty ()) {
		cpp_.empty_line ();

		type_in = op.name ();
		type_in += "_in";
		cpp_ << "struct " << type_in << "\n{\n";
		cpp_.indent ();
		abi_members (params_in);
		cpp_.unindent ();
		cpp_ << "};\n";
		cpp_ << "static const Parameter " << type_in << "_params_ [" << params_in.size () << "];\n";
	}

	if (!params_out.empty () || op.tkind () != Type::Kind::VOID) {
		cpp_.empty_line ();

		type_out = op.name ();
		type_out += "_out";
		cpp_ << "struct " << type_out << "\n{\n";
		cpp_.indent ();
		abi_members (params_out);
		if (op.tkind () != Type::Kind::VOID)
			abi_member (op) << " _ret;\n";
		cpp_.unindent ();
		cpp_ << "};\n";
		if (!params_out.empty ())
			cpp_ << "static const Parameter " << type_out << "_params_ [" << params_out.size () << "];\n";
	}

	cpp_.empty_line ();
	cpp_ << "static void " << static_cast <const string&> (op.name ()) << "_request (I_ptr < " << QName (itf) << "> _servant, "
		" IORequest_ptr _call, ::Nirvana::ConstPointer _in_ptr, Unmarshal_var& _u, ::Nirvana::Pointer _out_ptr)\n"
		"{\n";
	cpp_.indent ();
	if (!params_in.empty ())
		cpp_ << "const " << type_in << "& _in = *(const " << type_in << "*)_in_ptr;\n\n";

	// out and inout params
	for (auto p : params_out) {
		cpp_ << static_cast <const Type&> (*p) << ' ' << p->name () << ";\n";
	}
	if (op.tkind () != Type::Kind::VOID)
		cpp_ << static_cast <const Type&> (op) << " _ret;\n";

	cpp_ << "{\n";
	cpp_.indent ();

	// in params
	for (auto p : params_in) {
		cpp_ << static_cast <const Type&> (*p) << ' ' << p->name () << ";\n";
	}

	// Unmarshal in and inout
	for (auto p : params_in) {
		cpp_ << "_unmarshal (_in." << p->name () << ", _u, " << p->name () << ");\n";
	}

	// Release resources
	cpp_ << "_u = Unmarshal::_nil ();\n";

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
	// Input params out of scope here
	cpp_ << "}\n";

	if (!type_out.empty ()) {
		// Marshal output
		bool marshal = is_var_len (params_out);
		const char* m_param = marshal ? "_m" : "Marshal::_nil ()";
		if (marshal)
			cpp_ << "Marshal_var _m = _call->marshaler ();\n"
			<< type_out << "& _out = *(" << type_out << "*)_out_ptr;\n";
		for (auto p : params_out) {
			cpp_ << "_marshal_out (" << p->name () << ", " << m_param << " , _out." << p->name () << ");\n";
		}
		if (op.tkind () != Type::Kind::VOID)
			cpp_ << "_marshal_out (_ret, " << m_param << " , _out._ret);\n";
	}

	cpp_.unindent ();
	cpp_ << "}\n";
}

void Proxy::abi_members (const Members& members)
{
	for (auto p : members)
		abi_member (static_cast <const Type&> (*p)) << ' ' << p->name () << ";\n";
}

ostream& Proxy::abi_member (const Type& t)
{
	cpp_ << "Type < " << t << ">::ABI ";
	return cpp_;
}

void Proxy::implement (const Attribute& att)
{
	const ItemScope& itf = *att.parent ();

	cpp_.empty_line ();
	string att_abi = att.name ();
	att_abi += "_att";
	cpp_ << "typedef ";
	abi_member (att) << att_abi << ";\n";

	cpp_.empty_line ();
	cpp_ << "static void _get_" << static_cast <const string&> (att.name ()) << "_request (I_ptr < " << QName (itf) << "> _servant, "
		" IORequest_ptr _call, ::Nirvana::ConstPointer _in_ptr, Unmarshal_var& _u, ::Nirvana::Pointer _out_ptr)\n"
		"{\n";
	cpp_.indent ();

	// ret
	cpp_ << static_cast <const Type&> (att) << " _ret;\n";

	// Call
	cpp_ << "_ret = _servant->" << att.name () << " ();\n";

	bool marshal = is_var_len (att);
	if (marshal)
		cpp_ << "Marshal_var _m = _call->marshaler ()";
	cpp_ << ";\n";
	cpp_ << "_marshal_out (_ret, " << (marshal ? "_m" : "Marshal::_nil ()") << " , *(" << att_abi << "*)_out_ptr);\n";

	cpp_.unindent ();
	cpp_ << "}\n";

	if (!att.readonly ()) {
		cpp_.empty_line ();
		cpp_ << "static const Parameter _set_" << static_cast <const string&> (att.name ()) << "_in_params_ [1];\n\n";

		cpp_ << "static void _set_" << static_cast <const string&> (att.name ()) << "_request (I_ptr < " << QName (itf) << "> _servant, "
			" IORequest_ptr _call, ::Nirvana::ConstPointer _in_ptr, Unmarshal_var& _u, ::Nirvana::Pointer _out_ptr)\n"
			"{\n";
		cpp_.indent ();

		cpp_ << static_cast <const Type&> (att) << " val;\n";
		cpp_ << "_unmarshal (*(const " << att_abi << "*)_in_ptr, _u, val);\n";

		// Release resources
		cpp_ << "_u = Unmarshal::_nil ();\n";

		// Call
		cpp_ << "_servant->" << att.name () << " (val);\n";

		cpp_.unindent ();
		cpp_ << "}\n";
	}
}

void Proxy::end (const Interface& itf)
{
	if (itf.interface_kind () == InterfaceKind::PSEUDO)
		return;

	cpp_.namespace_open (internal_namespace_);
	cpp_.empty_line ();
	cpp_ << "IMPLEMENT_PROXY_FACTORY(";
	{
		auto parent = itf.parent ();
		if (parent)
			cpp_ << QName (*parent);
	}
	cpp_ << ", " << itf.name () << ");\n"
		"\n"
		"template <>\n"
		"struct ProxyTraits < " << QName (itf) << ">\n"
		"{\n";
	cpp_.indent ();
	cpp_ << "static const Operation operations_ [];\n"
		"static const char* const interfaces_ [];\n\n";

	for (auto it = itf.begin (); it != itf.end (); ++it) {
		const Item& item = **it;
		switch (item.kind ()) {
			case Item::Kind::OPERATION:
				implement (static_cast <const Operation&> (item));
				break;
			case Item::Kind::ATTRIBUTE:
				implement (static_cast <const Attribute&> (item));
				break;
		}
	}

	cpp_.unindent ();
	cpp_ << "};\n\n"
		"template <>\n"
		"class Proxy < " << QName (itf) << "> : public ProxyBase < " << QName (itf) << ">\n"
		"{\n";
	cpp_.indent ();
	cpp_ << "typedef ProxyBase < " << QName (itf) << "> Base;\n"
		"typedef ProxyTraits < " << QName (itf) << "> Traits;\n";
	cpp_.unindent ();
	cpp_ << "public:\n";
	cpp_.indent ();
	cpp_ << "Proxy (IOReference_ptr proxy_manager, uint16_t interface_idx) :\n"
		"Base (proxy_manager, interface_idx) {}\n\n";

	Metadata metadata;
	for (auto it = itf.begin (); it != itf.end (); ++it) {
		const Item& item = **it;
		switch (item.kind ()) {

			case Item::Kind::OPERATION: {
				const Operation& op = static_cast <const Operation&> (item);

				metadata.emplace_back ();
				OpMetadata& op_md = metadata.back ();
				op_md.name = op.name ();
				op_md.type = &op;

				cpp_ << static_cast <const Type&> (op) << ' ' << op.name () << " (";
				auto it = op.begin ();
				if (it != op.end ()) {
					proxy_param (**it);
					++it;
					for (; it != op.end (); ++it) {
						cpp_ << ", ";
						proxy_param (**it);
					}
				}
				cpp_ << ") const\n"
					"{\n";
				cpp_.indent ();

				get_parameters (op, op_md.params_in, op_md.params_out);

				if (!op_md.params_in.empty ()) {
					cpp_ << "Traits::" << static_cast <const string&> (op.name ()) << "_in _in;\n"
						"Marshal_var _m";

					if (is_var_len (op_md.params_in))
						cpp_ << " = _target ()->create_marshaler ()";
					cpp_ << ";\n";

					for (auto p : op_md.params_in) {
						cpp_ << "_marshal_in (" << p->name () << ", _m, _in." << p->name () << ");\n";
					}
				} else
					cpp_ << "Marshal_var _m;\n";

				bool has_out = !op_md.params_out.empty () || op.tkind () != Type::Kind::VOID;

				if (has_out)
					cpp_ << "Traits::" << static_cast <const string&> (op.name ()) << "_out _out;\n";
				cpp_ << "Unmarshal_var _u = _target ()->call (::CORBA::Nirvana::OperationIndex { _interface_idx (), " << (metadata.size () - 1) << " }, ";
				if (!op_md.params_in.empty ())
					cpp_ << "&_in, sizeof (_in)";
				else
					cpp_ << "0, 0";
				cpp_ << ", _m, ";
				if (has_out)
					cpp_ << "&_out, sizeof (_out)";
				else
					cpp_ << "0, 0";
				cpp_ << ");\n";

				for (auto p : op_md.params_out) {
					cpp_ << "_unmarshal (_out." << p->name () << ", _u, " << p->name () << ");\n";
				}

				if (op.tkind () != Type::Kind::VOID) {
					cpp_ << static_cast <const Type&> (op) << " _ret;\n"
						"_unmarshal (_out._ret, _u, _ret);\n"
						"return _ret;\n";
				}

				cpp_.unindent ();
				cpp_ << "}\n\n";

			} break;

			case Item::Kind::ATTRIBUTE: {
				const Attribute& att = static_cast <const Attribute&> (item);

				metadata.emplace_back ();
				{
					OpMetadata& op_md = metadata.back ();
					op_md.name = "_get_";
					op_md.name += att.name ();
					op_md.type = &att;
				}

				cpp_ << static_cast <const Type&> (att) << ' ' << att.name () << " () const\n"
					"{\n";
				cpp_.indent ();

				cpp_ << "Traits::" << static_cast <const string&> (att.name ()) << "_att _out;\n"
					"Marshal_var _m;\n";
				cpp_ << "Unmarshal_var _u = _target ()->call (::CORBA::Nirvana::OperationIndex { _interface_idx (), " << (metadata.size () - 1) << " }, 0, 0, _m, ";
				cpp_ << "&_out, sizeof (_out)";
				cpp_ << ");\n";

				cpp_ << static_cast <const Type&> (att) << " _ret;\n"
					"_unmarshal (_out, _u, _ret);\n"
					"return _ret;\n";

				cpp_.unindent ();
				cpp_ << "}\n";

				if (!att.readonly ()) {

					metadata.emplace_back ();
					{
						OpMetadata& op_md = metadata.back ();
						op_md.name = "_set_";
						op_md.name += att.name ();
						op_md.type = nullptr;
						op_md.params_in.push_back (&att);
					}

					cpp_ << "void " << att.name () << " (const " << static_cast <const Type&> (att) << "& val) const\n"
						"{\n";
					cpp_.indent ();

					cpp_ << "Traits::" << static_cast <const string&> (att.name ()) << "_att _in;\n"
						"Marshal_var _m";

					if (is_var_len (att))
						cpp_ << " = _target ()->create_marshaler ()";
					cpp_ << ";\n";

					cpp_ << "_marshal_in (val, _m, _in);\n";

					cpp_ << "Unmarshal_var _u = _target ()->call (::CORBA::Nirvana::OperationIndex { _interface_idx (), " << (metadata.size () - 1)
						<< " }, &_in, sizeof (_in), _m, 0, 0);\n";

					cpp_.unindent ();
					cpp_ << "}\n\n";
				}

			} break;
		}
	}

	cpp_.unindent ();
	cpp_ << "};\n\n";

	if (!metadata.empty ()) {

		for (const auto& op : metadata) {
			if (!op.params_in.empty ()) {
				cpp_ << "const Parameter ProxyTraits < " << QName (itf) << ">::" << op.name << "_in_params_ [" << op.params_in.size () << "] = {\n";
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
				cpp_ << "const Parameter ProxyTraits < " << QName (itf) << ">::" << op.name << "_out_params_ [" << op.params_out.size () << "] = {\n";
				md_members (op.params_out);
				cpp_ << "};\n";
			}
		}

		cpp_ << "const Operation ProxyTraits < " << QName (itf) << ">::operations_ [" << metadata.size () << "] = {\n";
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

	cpp_.namespace_close ();
	cpp_ << "NIRVANA_EXPORT (" << export_name (itf) << "_ProxyFactory, " << QName (itf)
		<< "::repository_id_, CORBA::AbstractBase, CORBA::Nirvana::ProxyFactoryImpl <" << QName (itf) << ">)\n";
}

void Proxy::md_operation (const Interface& itf, const OpMetadata& op)
{
	cpp_ << "{ \"" << op.name << "\", { ";
	if (!op.params_in.empty ()) {
		string params = op.name + "_in_params_";
		cpp_ << params << ", std::size (" << params << ')';
	} else
		cpp_ << "0, 0";
	cpp_ << " }, { ";
	if (!op.params_out.empty ()) {
		string params = op.name + "_out_params_";
		cpp_ << params << ", std::size (" << params << ')';
	} else
		cpp_ << "0, 0";
	cpp_ << " }, ";
	tc_name (op.type ? *op.type : Type ());
	cpp_ << ", RqProcWrapper < " << QName (itf) << ", " << op.name << "_request> }";
}

void Proxy::proxy_param (const Parameter& param)
{
	if (param.attribute () == Parameter::Attribute::IN)
		cpp_ << "const ";
	cpp_ << static_cast <const Type&> (param) << "& " << param.name ();
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

void Proxy::md_member (const AST::Type& t, const std::string& name)
{
	cpp_ << "{ \"" << name << "\", ";
	tc_name (t);
	cpp_ << " }";
}

void Proxy::tc_name (const Type& t)
{
	static const char* const basic_types [(size_t)BasicType::ANY + 1] = {
		"_tc_boolean",
		"_tc_octet",
		"_tc_char",
		"_tc_wchar",
		"_tc_ushort",
		"_tc_ulong",
		"_tc_ulonglong",
		"_tc_short",
		"_tc_long",
		"_tc_longlong",
		"_tc_float",
		"_tc_double",
		"_tc_longdouble",
		"_tc_Object",
		"_tc_ValueBase",
		"_tc_any"
	};
	
	switch (t.tkind ()) {
		case Type::Kind::VOID:
			cpp_ << "::CORBA::_tc_void";
			break;
		case Type::Kind::BASIC_TYPE:
			cpp_ << "::CORBA::" << basic_types [(size_t)t.basic_type ()];
			break;
		case Type::Kind::NAMED_TYPE: {
			const NamedItem& item = t.named_type ();
			cpp_ << ParentName (item) << "_tc_" << item.name ();
		} break;
		case Type::Kind::STRING:
			assert (!t.string_bound ()); // Anonymous types are not allowed.
			cpp_ << "::CORBA::" << "_tc_string";
			break;
		case Type::Kind::WSTRING:
			assert (!t.string_bound ()); // Anonymous types are not allowed.
			cpp_ << "::CORBA::" << "_tc_wstring";
			break;
		default:
			assert (false); // Anonymous types are not allowed.
	}
}

void Proxy::type_code_members (const NamedItem& item, const Members& members)
{
	assert (!members.empty ());
	cpp_ << "template <>\n"
		"const Parameter TypeCodeMembers < " << QName (item) << ">::members_ [] = {\n";

	md_members (members);

	cpp_ << "};\n\n";
}

void Proxy::end (const Exception& item)
{
	cpp_.namespace_open (internal_namespace_);
	Members members = get_members (item);

	if (!members.empty ())
		type_code_members (item, members);

	cpp_.namespace_close ();
	cpp_ << "NIRVANA_EXPORT (" << export_name (item) << "_TC, "
		<< QName (item) << "::repository_id_, CORBA::TypeCode, CORBA::Nirvana::TypeCodeException < " << QName (item)
		<< ", " << (members.empty () ? "false" : "true") << ">)\n";
}

void Proxy::type_code_name (const NamedItem& item)
{
	cpp_ << "template <>\n"
		"const char TypeCodeName < " << QName (item) << ">::name_ [] = \"" << static_cast <const string&> (item.name ()) << "\";\n";
}

void Proxy::rep_id_of (const RepositoryId& rid)
{
	cpp_ << "template <>\n"
		"const char RepIdOf < " << QName (rid.item ()) << ">::repository_id_ [] = \"" << rid.repository_id () << "\";\n";
}

void Proxy::end (const Struct& item)
{
	if (is_pseudo (item))
		return;

	cpp_.namespace_open (internal_namespace_);
	rep_id_of (item);
	type_code_name (item);
	type_code_members (item, get_members (item));
	cpp_.namespace_close ();
	cpp_ << "NIRVANA_EXPORT (" << export_name (item) << "_TC, "
		"CORBA::Nirvana::RepIdOf < " << QName (item) << "::repository_id_, CORBA::TypeCode, CORBA::Nirvana::TypeCodeStruct < " << QName (item) << ">)\n";
}

void Proxy::leaf (const Enum& item)
{
	if (is_pseudo (item))
		return;

	cpp_.namespace_open (internal_namespace_);
	cpp_.empty_line ();
	rep_id_of (item);
	cpp_ << "\n"
		"template <>\n"
		"const char TypeCodeEnum < " << QName (item) << ">::name_ [] = \"" << static_cast <const string&> (item.name ()) << "\";\n"
		"\n"
		"template <>\n"
		"const char* const TypeCodeEnum < " << QName (item) << ">::members_ [] = {\n";

	cpp_.indent ();
	auto it = item.begin ();
	cpp_ << '\"' << (*it)->name () << '\"';
	for (++it; it != item.end (); ++it) {
		cpp_ << ",\n\"" << (*it)->name () << '\"';
	};
	cpp_.unindent ();
	cpp_ << "\n};\n";

	cpp_.namespace_close ();
	cpp_ << "NIRVANA_EXPORT (" << export_name (item) << "_TC, \"" << item.repository_id ()
		<< "\", CORBA::TypeCode, CORBA::Nirvana::TypeCodeEnum < " << QName (item) << ">)\n";
}
