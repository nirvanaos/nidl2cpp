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
	auto parent = item.parent ();
	if (!parent || parent->kind () == Item::Kind::MODULE)
		implement (item);
}

void Proxy::end (const Exception& item)
{
	auto parent = item.parent ();
	if (!parent || parent->kind () == Item::Kind::MODULE)
		implement (item);
}

void Proxy::end (const Struct& item)
{
	auto parent = item.parent ();
	if (!parent || parent->kind () == Item::Kind::MODULE)
		implement (item);
}

void Proxy::leaf (const Enum& item)
{
	auto parent = item.parent ();
	if (!parent || parent->kind () == Item::Kind::MODULE)
		implement (item);
}

void Proxy::begin (const Interface& itf)
{
	// Implement nested types
	for (auto it = itf.begin (); it != itf.end (); ++it) {
		const Item& item = **it;
		switch (item.kind ()) {
			case Item::Kind::TYPE_DEF:
				implement (static_cast <const TypeDef&> (item));
				break;
			case Item::Kind::EXCEPTION:
				implement (static_cast <const Exception&> (item));
				break;
			case Item::Kind::STRUCT:
				implement (static_cast <const Struct&> (item));
				break;
			case Item::Kind::ENUM:
				implement (static_cast <const Enum&> (item));
				break;
		}
	}

	cpp_.namespace_open (internal_namespace_);
	cpp_.empty_line ();
	cpp_ << "template <> const char ProxyFactoryImpl < " << qualified_name (itf) << ">::name_ [] = \"" << static_cast <const string&> (itf.name ()) << "\";\n"
		"\ntemplate <>\n"
		"struct ProxyTraits < " << qualified_name (itf) << ">\n"
		"{\n";
	cpp_.indent ();
	cpp_ << "static const Operation operations_ [];\n"
		"static const char* const interfaces_ [];\n\n";
}

void Proxy::get_parameters (const AST::Operation& op, Parameters& params_in, Parameters& params_out)
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

bool Proxy::need_marshal (const Parameters& params)
{
	for (auto p : params)
		if (is_var_len (*p))
			return true;
	return false;
}

void Proxy::leaf (const Operation& op)
{
	const ItemScope& itf = *op.parent ();

	Parameters params_in, params_out;
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
	cpp_ << "static void " << static_cast <const string&> (op.name ()) << "_request (I_ptr < " << qualified_name (itf) << "> _servant, "
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
		cpp_ << "Marshal_ptr _m";
		if (need_marshal (params_out))
			cpp_ << " = _call->marshaler ()";
		cpp_ << ";\n"
			<< type_out << "& _out = *(" << type_out << "*)_out_ptr;\n";
		for (auto p : params_out) {
			cpp_ << "_marshal_out (" << p->name () << ", _m, _out." << p->name () << ");\n";
		}
		if (op.tkind () != Type::Kind::VOID)
			cpp_ << "_marshal_out (_ret, _m, _out._ret);\n";
	}

	cpp_.unindent ();
	cpp_ << "}\n";
}

void Proxy::abi_members (const vector <const Parameter*>& params)
{
	for (auto p : params)
		abi_member (static_cast <const Type&> (*p)) << ' ' << p->name () << ";\n";
}

ostream& Proxy::abi_member (const Type& t)
{
	cpp_ << "Type < " << t << ">::ABI_type ";
	return cpp_;
}

void Proxy::leaf (const Attribute& att)
{
	const ItemScope& itf = *att.parent ();

	cpp_.empty_line ();
	string att_abi = att.name ();
	att_abi += "_att";
	cpp_ << "typedef ";
	abi_member (att) << att_abi << ";\n";
	cpp_ << "static const Parameter " << att_abi << "_param_;\n";

	cpp_.empty_line ();
	cpp_ << "static void _get_" << static_cast <const string&> (att.name ()) << "_request (I_ptr < " << qualified_name (itf) << "> _servant, "
		" IORequest_ptr _call, ::Nirvana::ConstPointer _in_ptr, Unmarshal_var& _u, ::Nirvana::Pointer _out_ptr)\n"
		"{\n";
	cpp_.indent ();

	// ret
	cpp_ << static_cast <const Type&> (att) << " _ret;\n";

	// Call
	cpp_ << "_ret = _servant->" << att.name () << " ();\n"
		"Marshal_ptr _m";
	if (is_var_len (att))
		cpp_ << " = _call->marshaler ()";
	cpp_ << ";\n";
	cpp_ << "_marshal_out (_ret, _m, *(" << att_abi << "*)_out_ptr);\n";

	cpp_.unindent ();
	cpp_ << "}\n";

	if (!att.readonly ()) {
		cpp_.empty_line ();
		cpp_ << "static void _set_" << static_cast <const string&> (att.name ()) << "_request (I_ptr < " << qualified_name (itf) << "> _servant, "
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
	cpp_.unindent ();
	cpp_ << "};\n\n"
		"template <>\n"
		"class Proxy < " << qualified_name (itf) << "> : public ProxyBase < " << qualified_name (itf) << ">\n"
		"{\n";
	cpp_.indent ();
	cpp_ << "typedef ProxyBase < " << qualified_name (itf) << "> Base;\n"
		"typedef ProxyTraits < " << qualified_name (itf) << "> Traits;\n";
	cpp_.unindent ();
	cpp_ << "public:\n";
	cpp_.indent ();
	cpp_ << "Proxy (IOReference_ptr proxy_manager, uint16_t interface_idx) :\n"
		"Base (proxy_manager, interface_idx) {}\n\n";

	unsigned op_idx = 0;
	for (auto it = itf.begin (); it != itf.end (); ++it) {
		const Item& item = **it;
		switch (item.kind ()) {

			case Item::Kind::OPERATION: {
				const Operation& op = static_cast <const Operation&> (item);

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

				Parameters params_in, params_out;
				get_parameters (op, params_in, params_out);

				if (!params_in.empty ()) {
					cpp_ << "Traits::" << static_cast <const string&> (op.name ()) << "_in _in;\n"
						"Marshal_var _m";

					if (need_marshal (params_in))
						cpp_ << " = _target ()->create_marshaler ()";
					cpp_ << ";\n";

					for (auto p : params_in) {
						cpp_ << "_marshal_in (" << p->name () << ", _m, _in." << p->name () << ");\n";
					}
				} else
					cpp_ << "Marshal_var _m;\n";

				bool has_out = !params_out.empty () || op.tkind () != Type::Kind::VOID;

				if (has_out)
					cpp_ << "Traits::" << static_cast <const string&> (op.name ()) << "_out _out;\n";
				cpp_ << "Unmarshal_var _u = _target ()->call (::CORBA::Nirvana::OperationIndex { _interface_idx (), " << op_idx << " }, ";
				if (!params_in.empty ())
					cpp_ << "&_in, sizeof (_in)";
				else
					cpp_ << "0, 0";
				cpp_ << ", _m, ";
				if (has_out)
					cpp_ << "&_out, sizeof (_out)";
				else
					cpp_ << "0, 0";
				cpp_ << ");\n";

				for (auto p : params_out) {
					cpp_ << "_unmarshal (_out." << p->name () << ", _u, " << p->name () << ");\n";
				}

				if (op.tkind () != Type::Kind::VOID) {
					cpp_ << static_cast <const Type&> (op) << " _ret;\n"
						"_unmarshal (_out._ret, _u, _ret);\n"
						"return _ret;\n";
				}

				cpp_.unindent ();
				cpp_ << "}\n\n";

				++op_idx;

			} break;

			case Item::Kind::ATTRIBUTE: {
				const Attribute& att = static_cast <const Attribute&> (item);

				cpp_ << static_cast <const Type&> (att) << ' ' << att.name () << " () const\n"
					"{\n";
				cpp_.indent ();

				cpp_ << "Traits::" << static_cast <const string&> (att.name ()) << "_att _out;\n"
					"Marshal_var _m;\n";
				cpp_ << "Unmarshal_var _u = _target ()->call (::CORBA::Nirvana::OperationIndex { _interface_idx (), " << op_idx << " }, 0, 0, _m, ";
				cpp_ << "&_out, sizeof (_out)";
				cpp_ << ");\n";

				cpp_ << static_cast <const Type&> (att) << " _ret;\n"
					"_unmarshal (_out, _u, _ret);\n"
					"return _ret;\n";

				cpp_.unindent ();
				cpp_ << "}\n";

				++op_idx;

				if (!att.readonly ()) {
					cpp_ << "void " << att.name () << " (const " << static_cast <const Type&> (att) << "& val) const\n"
						"{\n";
					cpp_.indent ();

					cpp_ << "Traits::" << static_cast <const string&> (att.name ()) << "_att _in;\n"
						"Marshal_var _m";

					if (is_var_len (att))
						cpp_ << " = _target ()->create_marshaler ()";
					cpp_ << ";\n";

					cpp_ << "_marshal_in (val, _m, _in);\n";

					cpp_ << "Unmarshal_var _u = _target ()->call (::CORBA::Nirvana::OperationIndex { _interface_idx (), " << op_idx << " }, &_in, sizeof (_in), _m, 0, 0);\n";

					cpp_.unindent ();
					cpp_ << "}\n\n";

					++op_idx;

				}

			} break;
		}
	}

	cpp_.unindent ();
	cpp_ << "};\n\n";

	cpp_.namespace_close ();
	cpp_ << "NIRVANA_EXPORT (" << export_name (itf) << "_ProxyFactory, " << qualified_name (itf)
		<< "::repository_id_, CORBA::AbstractBase, CORBA::Nirvana::ProxyFactoryImpl <" << qualified_name (itf) << ">)\n";
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

void Proxy::implement (const TypeDef& item)
{
}

void Proxy::implement (const Exception& item)
{
	cpp_.namespace_open (internal_namespace_);
	Members members = get_members (item);
	marshal_traits (qualified_name (item) + "::_Data", members);

	cpp_ << "template <>\n"
		"class TypeCodeException < " << qualified_name (item) << "> : public TypeCodeExceptionImpl < " << qualified_name (item)
		<< ", " << members.size () << ">\n"
		"{};\n\n";

	if (!members.empty ()) {
		cpp_ << "template <>\n"
			"const Parameter TypeCodeMembers <TypeCodeException < " << qualified_name (item) << "> >::members_ [] = {\n";
		cpp_.indent ();

		auto it = members.begin ();
		parameter (**it);
		for (++it; it != members.end (); ++it) {
			cpp_ << ",\n";
			parameter (**it);
		}

		cpp_.unindent ();
		cpp_ << "\n};\n\n";
	}
}

void Proxy::parameter (const Member& m)
{
	cpp_ << "{ \"" << static_cast <const string&> (m.name ()) << "\", ";
	tc_name (m);
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
			const NamedItem& item = *t.named_type ();
			cpp_ << qualified_parent_name (item) << "_tc_" << item.name ();
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

void Proxy::implement (const Struct& item)
{
}

void Proxy::implement (const Enum& item)
{
}

void Proxy::marshal_traits (const std::string& name, const Members& members)
{
	if (!members.empty ()) {
		cpp_.empty_line ();
		cpp_ << "template <> struct MarshalTraits < " << name << ">\n"
			"{\n";
		cpp_.indent ();

		cpp_ << "static const bool has_marshal = " << (is_var_len (members) ? "true" : "false") << ";\n\n"
			"static void marshal_in (const " << name << "& src, Marshal_ptr marshaler, Type < " << name << ">::ABI_type& dst)\n"
			"{\n";
		cpp_.indent ();
		for (auto m : members) {
			cpp_ << "_marshal_in (src._" << m->name () << ", marshaler, dst." << m->name () << ");\n";
		}
		cpp_.unindent ();
		cpp_ << "}\n\n"
			"static void marshal_out (" << name << "& src, Marshal_ptr marshaler, Type < " << name << ">::ABI_type& dst)\n"
			"{\n";
		cpp_.indent ();
		for (auto m : members) {
			cpp_ << "_marshal_out (src._" << m->name () << ", marshaler, dst." << m->name () << ");\n";
		}
		cpp_.unindent ();
		cpp_ << "}\n\n"
			"static void unmarshal (const Type < " << name << ">::ABI_type& src, Unmarshal_ptr unmarshaler, " << name << "& dst)\n"
			"{\n";
		cpp_.indent ();
		for (auto m : members) {
			cpp_ << "_unmarshal (src." << m->name () << ", unmarshaler, dst._" << m->name () << ");\n";
		}
		cpp_.unindent ();
		cpp_ << "}\n";
		cpp_.unindent ();
		cpp_ << "};\n";
	}
}
