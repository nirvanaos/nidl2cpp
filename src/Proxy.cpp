#include "pch.h"
#include "Proxy.h"

using namespace std;
using namespace AST;

void Proxy::end (const Root&)
{
	cpp_.close ();
}

void Proxy::begin (const Interface& itf)
{
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

void Proxy::leaf (const Operation& op)
{
	const ItemScope& itf = *op.parent ();

	vector <const Parameter*> params_in, params_out;
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

	string struct_in, struct_out;
	if (!params_in.empty ()) {
		cpp_.empty_line ();

		struct_in = op.name ();
		struct_in += "_in";
		cpp_ << "struct " << struct_in << "\n{\n";
		cpp_.indent ();
		abi_members (params_in);
		cpp_.unindent ();
		cpp_ << "};\n";
		cpp_ << "static const Parameter " << struct_in << "_params_ [" << params_in.size () << "];\n";
	}

	if (!params_out.empty () || op.tkind () != Type::Kind::VOID) {
		cpp_.empty_line ();

		struct_out = op.name ();
		struct_out += "_out";
		cpp_ << "struct " << struct_out << "\n{\n";
		cpp_.indent ();
		abi_members (params_out);
		if (op.tkind () != Type::Kind::VOID)
			abi_member (op) << " _ret;\n";
		cpp_.unindent ();
		cpp_ << "};\n";
		if (!params_out.empty ())
			cpp_ << "static const Parameter " << struct_out << "_params_ [" << params_out.size () << "];\n";
	}

	cpp_.empty_line ();
	cpp_ << "static void " << static_cast <const string&> (op.name ()) << "_request (I_ptr < " << qualified_name (itf) << "> _servant, "
		" IORequest_ptr _call, ::Nirvana::ConstPointer _in_ptr, Unmarshal_var& _u, ::Nirvana::Pointer _out_ptr)\n"
		"{\n";
	cpp_.indent ();
	if (!params_in.empty ())
		cpp_ << "const " << struct_in << "& _in = *(const " << struct_in << "*)_in_ptr;\n\n";

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

	if (!struct_out.empty ()) {
		// Marshal output
		cpp_ << "Marshal_ptr _m = _call->marshaler ();\n"
			<< struct_out << "& _out = *(" << struct_out << "*)_out_ptr;\n";
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
{}

void Proxy::end (const Interface& itf)
{
	cpp_.unindent ();
	cpp_ << "};\n";
}
