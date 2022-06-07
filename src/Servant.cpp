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
#include "Servant.h"

using namespace std;
using namespace std::filesystem;
using namespace AST;

void Servant::end (const Root&)
{
	h_.close ();
}

void Servant::leaf (const Include& item)
{
	if (!options ().no_POA && !options ().no_servant) {
		h_ << "#include " << (item.system () ? '<' : '"')
			<< path (path (item.file ()).replace_extension ("").string () + options ().servant_suffix).replace_extension ("h").string ()
			<< (item.system () ? '>' : '"')
			<< endl;
	}
}

void Servant::begin (const Interface& itf)
{
	attributes_by_ref_ = itf.interface_kind () == InterfaceKind::PSEUDO;
	skeleton_begin (itf);
}

void Servant::begin (const ValueType& vt)
{
	attributes_by_ref_ = true;
	skeleton_begin (vt);
}

void Servant::skeleton_begin (const ItemContainer& item)
{
	h_.namespace_open ("CORBA/Internal");
	h_.empty_line ();

	h_ << "template <class S>\n"
		"class Skeleton <S, " << QName (item) << "> : public Definitions <" << QName (item) << ">\n"
		"{\n"
		"public:\n";
	h_.indent ();
	h_ << "static const typename Bridge <" << QName (item) << ">::EPV epv_;\n\n";
	h_.unindent ();
	h_ << "protected:\n";
	h_.indent ();
}

void Servant::skeleton_end (const ItemContainer& item)
{
	h_.unindent ();
	h_ << "};\n"
		"\ntemplate <class S>\n"
		"const Bridge <" << QName (item) << ">::EPV Skeleton <S, " << QName (item) << ">::epv_ = {\n";
	h_.indent ();
	h_ << "{ // header\n";
	h_.indent ();
	h_ << "Bridge <" << QName (item) << ">::repository_id_,\n"
		"S::template __duplicate <" << QName (item) << ">,\n"
		"S::template __release <" << QName (item) << ">\n";
	h_.unindent ();
	h_ << "}";

}

void Servant::epv ()
{
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
}

void Servant::end (const Interface& itf)
{
	skeleton_end (itf);

	// Bases
	const Interfaces all_bases = itf.get_all_bases ();

	if (itf.interface_kind () != InterfaceKind::PSEUDO || !all_bases.empty ()) {
		h_ << ",\n"
			"{ // base\n";
		h_.indent ();

		if (itf.interface_kind () == InterfaceKind::ABSTRACT)
			h_ << "S::template _wide_abstract <";
		else if (itf.interface_kind () != InterfaceKind::PSEUDO)
			h_ << "S::template _wide_object <";
		h_ << QName (itf) << ">";

		auto b = all_bases.begin ();
		if (itf.interface_kind () == InterfaceKind::PSEUDO) {
			h_ << "S::template _wide <" << QName (**b) << ", " << QName (itf) << ">";
			++b;
		}

		for (; b != all_bases.end (); ++b) {
			h_ << ",\n"
				"S::template _wide <" << QName (**b) << ", " << QName (itf) << ">";
		}

		h_ << endl;
		h_.unindent ();
		h_ << "}";
	}

	// EPV
	epv ();

	// Servant implementations
	if (!options ().no_servant) {

		if (itf.interface_kind () != InterfaceKind::ABSTRACT) {
			
			// Standard implementation
			h_.empty_line ();
			h_ << "template <class S>\n"
				"class Servant <S, " << QName (itf) << "> : public Implementation";
			implementation_suffix (itf);
			h_ << " <S, ";
			implementation_parameters (itf, all_bases);
			h_ << "{};\n";

			// Static implementation
			h_.empty_line ();
			h_ << "template <class S>\n"
				"class ServantStatic <S, " << QName (itf) << "> : public Implementation";
			implementation_suffix (itf);
			h_ << "Static <S, ";
			implementation_parameters (itf, all_bases);
			h_ << "{};\n";
		}

		if (!options ().no_POA && itf.interface_kind () != InterfaceKind::PSEUDO) {
			// POA implementation
			h_.empty_line ();
			h_ << "template <>\n"
				"class NIRVANA_NOVTABLE ServantPOA <" << QName (itf) << "> :\n";
			h_.indent ();

			bool has_base = false;
			for (auto b : all_bases) {
				if (b->interface_kind () == itf.interface_kind ()) {
					has_base = true;
					break;
				}
			}

			if (!has_base) {
				const char* base;
				switch (itf.interface_kind ()) {
					case InterfaceKind::LOCAL:
						base = "LocalObject";
						break;
					case InterfaceKind::ABSTRACT:
						base = "AbstractBase";
						break;
					default:
						base = "PortableServer::ServantBase";
				}
				h_ << "public virtual ServantPOA <" << base << ">,\n";
			}

			for (auto b : itf.bases ()) { // Direct bases only
				h_ << "public virtual ServantPOA <" << QName (*b) << ">,\n";
			}
			h_ << "public InterfaceImpl <ServantPOA <" << QName (itf) << ">, "
				<< QName (itf) << ">\n";
			h_.unindent ();
			h_ << "{\n"
				"public:\n";
			h_.indent ();
			h_ << "typedef " << QName (itf) << " PrimaryInterface;\n\n";

			if (itf.interface_kind () != InterfaceKind::ABSTRACT) {
				h_ << "virtual Interface* _query_interface (const String& id) override\n"
					"{\n";
				h_.indent ();
				h_ << "return FindInterface <" << QName (itf);
				for (auto b : all_bases) {
					h_ << ", " << QName (*b);
				}
				h_ << ">::find (*this, id);\n";
				h_.unindent ();
				h_ << "}\n\n";
				h_ << "I_ref <" << QName (itf) << "> _this ()\n"
					"{\n";
				h_.indent ();
				h_ << "return this->_get_proxy ().template downcast <" << QName (itf)
					<< "> ();\n";
				h_.unindent ();
				h_ << "}\n";
			}

			h_.empty_line ();

			for (auto it = itf.begin (); it != itf.end (); ++it) {
				const Item& item = **it;
				switch (item.kind ()) {
					case Item::Kind::OPERATION: {
						const Operation& op = static_cast <const Operation&> (item);
						h_ << "virtual " << ServantOp (op) << " = 0;\n";
					} break;
					case Item::Kind::ATTRIBUTE: {
						const Attribute& att = static_cast <const Attribute&> (item);
						h_ << "virtual " << VRet (att) << ' ' << att.name () << " () = 0;\n";
						if (!att.readonly ())
							h_ << "virtual void " << att.name () << " (" << ServantParam (att) << ") = 0;\n";
					} break;
				}
			}
			
			h_.unindent ();
			
			h_ << "\nprotected:\n";
			h_.indent ();
			h_ << "ServantPOA ()\n"
				"{}\n";

			h_.unindent ();
			h_ << "};\n";
		}

		h_.namespace_close ();

		if (options ().legacy && itf.interface_kind () != InterfaceKind::PSEUDO
			&& (!options ().no_POA || itf.interface_kind () != InterfaceKind::ABSTRACT)) {

			h_ << endl << "#ifdef LEGACY_CORBA_CPP\n";
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

				if (!options ().no_POA)
					h_ << "\ntypedef " << Namespace ("CORBA/Internal")
						<< "ServantPOA <" << QName (itf) << "> " << itf.name () << ";\n";

				if (itf.interface_kind () != InterfaceKind::ABSTRACT)
					h_ << "template <class T> using " << itf.name () << "_tie = "
						<< Namespace ("CORBA/Internal") << "ServantTied <T, " << QName (itf) << ">;\n\n";

				for (size_t cnt = sn.size (); cnt; --cnt) {
					h_ << "}\n";
				}
			} else {

				if (!options ().no_POA)
					h_ << "typedef " << Namespace ("CORBA/Internal")
					<< "ServantPOA <" << QName (itf) << "> POA_" << static_cast <const string&> (itf.name ()) << ";\n";

				if (itf.interface_kind () != InterfaceKind::ABSTRACT)
					h_ << "template <class T> using POA_" << static_cast <const string&> (itf.name ()) << "_tie = "
						<< Namespace ("CORBA/Internal") << "ServantTied <T, " << QName (itf) << ">;\n\n";
			}
			h_ << "#endif\n";
		}
	}
}

void Servant::end (const ValueType& vt)
{
	skeleton_end (vt);

	// Bases

	h_ << ",\n"
		"{ // base\n";
	h_.indent ();

	h_ << "S::template _wide <ValueBase, " << QName (vt) << ">";

	const Bases all_bases = get_all_bases (vt);

	for (auto b : all_bases) {
		h_ << ",\n"
			"S::template _wide <" << QName (*b) << ", " << QName (vt) << ">";
	}

	h_ << endl;
	h_.unindent ();
	h_ << "}";

	// EPV
	epv ();

	// Servant implementations
	if (!options ().no_servant) {

		if (vt.modifier () != ValueType::Modifier::ABSTRACT) {

			// Standard implementation
			h_.empty_line ();
			h_ << "template <class S>\n"
				"class Servant <S, " << QName (vt) << "> :\n";

			h_.indent ();

			const Interface* concrete_itf = get_concrete_supports (vt);
			if (concrete_itf)
				h_ << "public ValueImplBase <S, ValueBase>,\n"
				"public Servant <S, " << QName (*concrete_itf) << ">";
			else
				h_ << "public ValueImpl <S, ValueBase>";

			bool abstract_base = false;
			for (auto b : all_bases) {
				h_ << ",\n"
					"public ";
				if (b->kind () == Item::Kind::VALUE_TYPE)
					h_ << "Value";
				else {
					assert (static_cast <const Interface&> (*b).interface_kind () == InterfaceKind::ABSTRACT);
					abstract_base = true;
					h_ << "Interface";
				}
				h_ << "Impl <S, " << QName (*b) << ">";
			}

			if (abstract_base)
				h_ << ",\n"
					"public InterfaceImpl <S, AbstractBase>";

				h_.unindent ();

			h_ << "\n"
				"{\n"
				"public:\n";

			h_.indent ();
			h_ << "typedef " << QName (vt) << " PrimaryInterface;\n"
				"\nInterface* _query_valuetype (String_in id) NIRVANA_NOEXCEPT\n"
				"{\n";
			h_.indent ();
			h_ << "return FindInterface <" << QName (vt);
			for (auto b : all_bases) {
				h_ << ", " << QName (*b);
			}
			h_ << ">::find (static_cast <S&> (*this), id);\n";
			h_.unindent ();
			h_ << "}\n";
			h_.unindent ();
			h_ << "};\n";
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
	h_ << QName (primary);
	for (auto b : bases) {
		h_ << ", " << QName (*b);
	}
	h_ << ">\n";
}

void Servant::leaf (const Operation& op)
{
	const ItemScope& itf = *op.parent ();

	h_ << "static " << ABI_ret (op);

	{
		string name = "_";
		name += op.name ();
		h_ << ' ' << name << " (Bridge <" << QName (itf) << ">* _b";
		epv_.push_back (move (name));
	}

	for (auto it = op.begin (); it != op.end (); ++it) {
		h_ << ", " << ABI_param (**it) << ' ' << (*it)->name ();
	}

	h_ << ", Interface* _env)\n"
		"{\n";
	h_.indent ();
	h_ << "try {\n";
	h_.indent ();
	if (op.tkind () != Type::Kind::VOID)
		h_ << "return " << TypePrefix (op) << "ret (";
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
		h_ << "return " << TypePrefix (op) << "ret ();\n";
	}

	h_.unindent ();
	h_ << "}\n\n";
}

void Servant::leaf (const Attribute& att)
{
	attribute (att);
}

void Servant::leaf (const StateMember& m)
{
	if (m.is_public ())
		attribute (m);
}

void Servant::attribute (const Member& m)
{
	const ItemScope& itf = *m.parent ();

	h_ << "static " << ABI_ret (m, attributes_by_ref_);
	{
		string name = "__get_";
		name += m.name ();
		h_ << ' ' << name << " (Bridge <" << QName (itf) << ">* _b, Interface* _env)\n"
			"{\n";
		epv_.push_back (move (name));
	}
	h_.indent ();
	h_ << "try {\n";
	h_.indent ();
	h_ << "return " << TypePrefix (m);
	if (attributes_by_ref_)
		h_ << "VT_";
	h_ << "ret (S::_implementation (_b)." << m.name () << " ());\n";
	catch_block ();
	h_ << "return " << TypePrefix (m);
	if (attributes_by_ref_)
		h_ << "VT_";
	h_ << "ret ();\n";
	h_.unindent ();
	h_ << "}\n";

	if (!(m.kind () == Item::Kind::ATTRIBUTE && static_cast <const Attribute&> (m).readonly ())) {
		{
			string name = "__set_";
			name += m.name ();
			h_ << "static void " << name << " (Bridge <" << QName (itf) << ">* _b, "
				<< ABI_param (m) << " val, Interface* _env)\n"
				"{\n";
			epv_.push_back (move (name));
		}
		h_.indent ();
		h_ << "try {\n";
		h_.indent ();
		h_ << "S::_implementation (_b)." << m.name () << " (";
		servant_param (m, "val");
		h_ << ");\n";
		catch_block ();
		h_.unindent ();
		h_ << "}\n";
	}

	if (m.kind () == Item::Kind::STATE_MEMBER && is_var_len (m)) {
		{
			string name = "__move_";
			name += m.name ();
			h_ << "static void " << name << " (Bridge <" << QName (itf) << ">* _b, "
				<< ABI_param (m, Parameter::Attribute::INOUT) << " val, Interface* _env)\n"
				"{\n";
			epv_.push_back (move (name));
		}
		h_.indent ();
		h_ << "try {\n";
		h_.indent ();
		h_ << "S::_implementation (_b)." << m.name () << " (std::move (";
		servant_param (m, "val", Parameter::Attribute::INOUT);
		h_ << "));\n";
		catch_block ();
		h_.unindent ();
		h_ << "}\n";
	}

	h_ << endl;
}

void Servant::servant_param (const Type& t, const string& name, Parameter::Attribute att)
{
	h_ << TypePrefix (t);
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
	h_ << "} catch (Exception& e) {\n";
	h_.indent ();
	h_ << "set_exception (_env, e);\n";
	h_.unindent ();
	h_ << "} catch (...) {\n";
	h_.indent ();
	h_ << "set_unknown_exception (_env);\n";
	h_.unindent ();
	h_ << "}\n";
}
