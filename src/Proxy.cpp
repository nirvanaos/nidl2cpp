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
	cpp_.empty_line ();
	cpp_ << "IMPLEMENT_PROXY_FACTORY(" << qualified_parent_name (itf) << ", " << itf.name () << ");\n"
		"\ntemplate <>\n"
		"struct ProxyTraits < " << qualified_name (itf) << ">\n"
		"{\n";
	cpp_.indent ();
	cpp_ << "static const Operation operations_ [];\n"
		"static const char* const interfaces_ [];\n\n";
}

void Proxy::leaf (const Operation& op)
{
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
		struct_in = op.name ();
		struct_in += "_in";
		cpp_ << "struct " << struct_in << "\n{\n";
		cpp_.indent ();
		for (auto p : params_in) {
			cpp_ << "";
		}
	}
}

void Proxy::end (const Interface& itf)
{
	cpp_.unindent ();
	cpp_ << "};\n";
}
