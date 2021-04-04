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
		"class Skeleton <S, " << qualified_name (itf) << ">\n"
		"{\n"
		"public:\n";
	h_.indent ();
	h_ << "static const typename Bridge < " << qualified_name (itf) << ">::EPV epv_;\n\n";
	h_.unindent ();
	h_ << "protected:\n";
	h_.indent ();
}

void Servant::end (const Interface& itf)
{
	h_.unindent ();
	h_ << "};\n"
		"\ntemplate <class S>\n"
		"const Bridge < " << qualified_name (itf) << ">::EPV Skeleton <S, " << qualified_name (itf) << ">::epv_ = {\n";
	h_.indent ();
	h_ << "{ // header\n";
	h_.indent ();
	h_ << "Bridge < " << qualified_name (itf) << ">::repository_id_,\n"
		"S::template __duplicate < " << qualified_name (itf) << "1>,\n"
		"S::template __release < " << qualified_name (itf) << ">\n";
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
		h_ << qualified_name (itf) << ">\n";

		for (auto b : itf.bases ()) {
			h_ << ",\n"
				"S::template _wide < " << qualified_name (*b) << ", " << qualified_name (itf) << ">\n";
		}

		h_.unindent ();
		h_ << "}";
	}

	// EPV
	if (!epv_.empty ()) {
		h_ << ",\n"
			"{ // EPV\n";
		h_.indent ();
		auto n = epv_.begin ();
		h_ << "S::" << *n;
		for (++n; n != epv_.end (); ++n) {
			h_ << ",\nS::" << *n;
		}
		h_.unindent ();
		h_ << "\n}";
		epv_.clear ();
	}

	h_.unindent ();
	h_ << "\n};\n";

	if (itf.interface_kind () != InterfaceKind::ABSTRACT) {

		Interfaces bases = itf.get_all_bases ();

		// Standard implementation
		h_.empty_line ();
		h_ << "template <class S>\n"
			"class Servant <S, " << qualified_name (itf) << "> : public Implementation";
		implementation_suffix (itf);
		h_ << " <S, ";
		implementation_parameters (itf, bases);

		// POA implementation
		h_.empty_line ();
		h_ << "template <>\n"
			"class ServantPOA < " << qualified_name (itf) << "> : public Implementation";
		implementation_suffix (itf);
		h_ << "POA < ";
		implementation_parameters (itf, bases);

		// Static implementation
		h_.empty_line ();
		h_ << "template <class S>\n"
			"class ServantStatic <S, " << qualified_name (itf) << "> : public Implementation";
		implementation_suffix (itf);
		h_ << "Static <S, ";
		implementation_parameters (itf, bases);

		h_.namespace_close ();

		const NamedItem* ns = itf.parent ();
		if (ns) {
			ScopedName sn = ns->scoped_name ();
			{
				string s = "POA_";
				s += sn.front ();
				h_ << "namespace " << s << " {\n";
			}
			for (auto it = sn.cbegin () + 1; it != sn.cend (); ++it) {
				h_ << "namespace " << *it << " {\n";
			}

			h_ << "\ntypedef ::CORBA::Nirvana::ServantPOA < " << qualified_name (itf) << "> " << itf.name () << ";\n"
				"template <class T> using " << itf.name () << "_tie = ::CORBA::Nirvana::ServantTied <T, " << qualified_name (itf) << ">;\n\n";

			for (size_t cnt = sn.size (); cnt; --cnt) {
				h_ << "}\n";
			}
		} else {
			h_ << "typedef ::CORBA::Nirvana::ServantPOA < " << qualified_name (itf) << "> POA_" << static_cast <const string&> (itf.name ()) << ";\n"
				"template <class T> using POA_" << static_cast <const string&> (itf.name ()) << "_tie = ::CORBA::Nirvana::ServantTied <T, " << qualified_name (itf) << ">;\n\n";
		}
	}
}

void Servant::implementation_suffix (const InterfaceKind ik)
{
	switch (ik.interface_kind ()) {
		case InterfaceKind::LOCAL:
			h_ << "Local";
			break;
		case InterfaceKind::PSEUDO:
			h_ << "Pseudo";
			break;
	}
}

void Servant::implementation_parameters (const Interface& primary, const Interfaces& bases)
{
	h_ << qualified_name (primary);
	for (auto b : bases) {
		h_ << ", " << qualified_name (*b);
	}
	h_ << "> {};\n";
}

void Servant::leaf (const Operation& op)
{
	const ItemScope& itf = *op.parent ();

	h_ << "static ";
	bridge_ret (h_, op);

	{
		string name = "_";
		name += op.name ();
		h_ << ' ' << name << " (Bridge < " << qualified_name (itf) << ">* _b";
		epv_.push_back (move (name));
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
}

void Servant::leaf (const Attribute& att)
{
	const ItemScope& itf = *att.parent ();

	h_ << "static ";
	bridge_ret (h_, att);
	{
		string name = "__get_";
		name += att.name ();
		h_ << ' ' << name << " (Bridge < " << qualified_name (itf) << ">* _b, Interface* _env)\n"
			"{\n";
		epv_.push_back (move (name));
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
			string name = "__set_";
			name += att.name ();
			h_ << "static void " << name << " (Bridge < " << qualified_name (itf) << ">* _b, ";
			epv_.push_back (move (name));
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
