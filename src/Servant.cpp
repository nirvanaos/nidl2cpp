#include "pch.h"
#include "Servant.h"

using namespace std;
using namespace AST;

void Servant::end (const Root&)
{
	h_.close ();
}

void Servant::begin (const Interface& itf)
{
	h_.namespace_open (internal_namespace_);
	h_.empty_line ();

	h_ << "template <class S>\n"
		"class Skeleton <S, " << itf.qualified_name () << ">\n"
		"{\n"
		"public:\n";
	h_.indent ();
	h_ << "static const typename Bridge < " << itf.qualified_name () << ">::EPV epv_;\n\n";
	h_.unindent ();
	h_ << "protected:\n";
	h_.indent ();

	vector <string> ops;
	for (auto it = itf.begin (); it != itf.end (); ++it) {
		const Item& item = **it;
		switch (item.kind ()) {

			case Item::Kind::OPERATION: {
				const Operation& op = static_cast <const Operation&> (item);
				h_ << "static ";
				bridge_ret (h_, op);

				{
					string name = "_";
					name += op.name ();
					h_ << ' ' << name << " (Bridge < " << itf.qualified_name () << ">* _b";
					ops.push_back (move (name));
				}

				for (auto it = op.begin (); it != op.end (); ++it) {
					h_ << ", ";
					bridge_param (h_, **it);
				}

				h_ << ", Interface* _env)\n"
					"{\n";
				h_.indent ();
				h_ << "try {\n";
				h_.indent ();
				if (op.tkind () != Type::Kind::VOID)
					h_ << "return Type < " << static_cast <const Type&> (op) << "::ret (";
				h_ << "S::_implementation (_b)." << op.name () << " (";
				if (!op.empty ()) {
					auto par = op.begin ();
					servant_param (**par);
					for (++par; par != op.end (); ++par) {
						h_ << ", ";
						servant_param (**par);
					}
				}
				if (op.tkind () != Type::Kind::VOID)
					h_ << ')';
				h_ << ");\n";
				catch_block ();
				if (op.tkind () != Type::Kind::VOID) {
					// Return default value on exception
					h_ << "return ";
					bridge_ret (h_, op);
					h_ << " ();\n";
				}

				h_.unindent ();
				h_ << "}\n\n";
			} break;

			case Item::Kind::ATTRIBUTE: {
				const Attribute& att = static_cast <const Attribute&> (item);
				h_ << "static ";
				bridge_ret (h_, att);
				{
					string name = "__get_";
					name += att.name ();
					h_ << ' ' << name << " (Bridge < " << itf.qualified_name () << ">* _b, Interface* _env)\n"
						"{\n";
					ops.push_back (move (name));
				}
				h_.indent ();
				h_ << "try {\n";
				h_.indent ();
				h_ << "return Type < " << static_cast <const Type&> (att) << "::ret (S::_implementation (_b)." << att.name () << " ();\n";
				catch_block ();
				h_ << "return ";
				bridge_ret (h_, att);
				h_ << " ();\n";
				h_.unindent ();
				h_ << "}\n";

				if (!att.readonly ()) {
					{
						string name = "__get_";
						name += att.name ();
						h_ << "static void " << name << " (Bridge < " << itf.qualified_name () << ">* _b, ";
						ops.push_back (move (name));
					}
					bridge_param (h_, att);
					h_ << " val, Interface* _env)\n"
						"{\n";
					h_.indent ();
					h_ << "try {\n";
					h_.indent ();
					h_ << "S::_implementation (_b)." << att.name () << " (";
					servant_param (att, "val");
					h_ << ");\n";
					catch_block ();
					h_.unindent ();
					h_ << "}\n\n";
				}

			} break;
		}
	}

	h_.unindent ();
	h_ << "};\n"
		"\ntemplate <class S>\n"
		"const Bridge < " << itf.qualified_name () << ">::EPV Skeleton <S, " << itf.qualified_name () << ">::epv_ = {\n";
	h_.indent ();
	h_ << "{ // header\n";
	h_.indent ();
	h_ << "Bridge < " << itf.qualified_name () << ">::repository_id_,\n"
		"S::template __duplicate < " << itf.qualified_name () << "1>,\n"
		"S::template __release < " << itf.qualified_name () << ">\n";
	h_.unindent ();
	h_ << "}";
	
	// Bases
	if (itf.interface_kind () != InterfaceKind::PSEUDO) {
		h_ << ",\n"
			"{ // base\n";
		h_.indent ();

		if (itf.interface_kind () == InterfaceKind::ABSTRACT)
			h_ << "S::template _wide <AbstractBase, ";
		else
			h_ << "S::template _wide_object < ";
		h_ << itf.qualified_name () << ">\n";

		for (auto b : itf.bases ()) {
			h_ << ",\n"
				"S::template _wide < " << b->qualified_name () << ", " << itf.qualified_name () << ">\n";
		}

		h_.unindent ();
		h_ << "}";
	}

	// EPV
	if (!ops.empty ()) {
		h_ << ",\n"
			"{ // EPV\n";
		h_.indent ();
		auto n = ops.begin ();
		h_ << "S::" << *n;
		for (++n; n != ops.end (); ++n) {
			h_ << ",\nS::" << *n;
		}
		h_.unindent ();
		h_ << "\n}";
	}

	h_.unindent ();
	h_ << "\n};\n";
}

void Servant::servant_param (const Type& t, const string& name, Parameter::Attribute att)
{
	h_ << "Type <" << t << ">::";
	switch (att) {
		case Parameter::Attribute::IN:
			h_ << "in";
			break;
		case Parameter::Attribute::OUT:
			h_ << "out";
			break;
		case Parameter::Attribute::INOUT:
			h_ << "inout";
			break;
	}
	h_ << " (" << name << ')';
}

void Servant::catch_block ()
{
	h_.unindent ();
	h_ << "} catch (const Exception& e) {\n";
	h_.indent ();
	h_ << "set_exception (_env, e);\n";
	h_.unindent ();
	h_ << "} catch (...) {\n";
	h_.indent ();
	h_ << "set_unknown_exception (_env);\n";
	h_.unindent ();
	h_ << "}\n";
}
