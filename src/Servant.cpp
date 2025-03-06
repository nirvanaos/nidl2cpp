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
#include "Servant.h"

using std::filesystem::path;
using namespace AST;

void Servant::end (const Root&)
{
	h_.close ();
}

void Servant::leaf (const Include& item)
{
	if (!options ().no_servant) {
		h_ << "#include " << (item.system () ? '<' : '"')
			<< path (path (item.file ()).replace_extension ("").string () + options ().servant_suffix).replace_extension ("h").string ()
			<< (item.system () ? '>' : '"')
			<< std::endl;
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

	const Interface* concrete_itf = get_concrete_supports (vt);
	if (concrete_itf && concrete_itf->interface_kind () != InterfaceKind::PSEUDO) {
		h_ << "static Interface* " SKELETON_FUNC_PREFIX "this (Bridge <" << QName (vt) << ">* _b, Interface* _env) noexcept\n"
			"{\n"
			<< indent <<
			"try {\n"
			<< indent <<
			"return " << Namespace ("IDL") << "Type <" << QName (*concrete_itf) << ">::ret (S::_implementation (_b)._this ());\n"
			<< CatchBlock ()
			<< "return nullptr;\n"
			<< unindent
			<< "}\n\n";
	}
}

void Servant::skeleton_begin (const IV_Base& item, const char* suffix)
{
	h_.namespace_open ("CORBA/Internal");
	h_ << empty_line

		<< "template <class S>\n"
		"class Skeleton <S, " << QName (item) << suffix << "> : public Decls <" << QName (item) << ">\n"
		"{\n"
		"public:\n"
		<< indent
		<< "static const typename Bridge <" << QName (item) << suffix << ">::EPV epv_;\n\n"
		<< unindent
		<< "protected:\n"
		<< indent;
}

void Servant::skeleton_end (const IV_Base& item, const char* suffix)
{
	h_ << unindent
		<< "};\n"
		"\ntemplate <class S>\n"
		"const Bridge <" << QName (item) << suffix << ">::EPV Skeleton <S, " << QName (item) << suffix
		<< ">::epv_ = {\n"
		<< indent
		<< "{ // header\n"
		<< indent
		<< "RepIdOf <" << QName (item) << suffix << ">::id,\n"
		"S::template __duplicate <" << QName (item) << suffix << ">,\n"
		"S::template __release <" << QName (item) << suffix << ">\n"
		<< unindent
		<< "}";
}

void Servant::virtual_operations (const IV_Base& itf, const OperationSet* implement_base)
{
	for (auto it = itf.begin (); it != itf.end (); ++it) {
		const Item& item = **it;
		switch (item.kind ()) {
		case Item::Kind::OPERATION: {
			const Operation& op = static_cast <const Operation&> (item);
			h_ << "virtual " << ServantOp (op, true);
			if (implement_base && implement_base->find (&op) != implement_base->end ()) {
				
				// CCM base operation
				h_ << "\n"
					"{\n" << indent;
				if (op.tkind () != Type::Kind::VOID)
					h_ << "return ";
				h_ << BaseImplPOA (itf) << op.name () << " (";
				auto it = op.begin ();
				if (it != op.end ()) {
					h_ << (*it)->name ();
					++it;
					for (; it != op.end (); ++it) {
						h_ << ", " << (*it)->name ();
					}
				}
				h_ << ");\n"
					<< unindent << "}\n";
			} else
				h_ << " = 0;\n";
		} break;
		case Item::Kind::ATTRIBUTE: {
			const Attribute& att = static_cast <const Attribute&> (item);
			h_ << "virtual " << VRet (att) << ' ' << att.name () << " () = 0;\n";
			if (!att.readonly ())
				h_ << "virtual void " << att.name () << " (" << ServantParam (att, true) << ") = 0;\n";
		} break;
		}
	}
}

void Servant::end (const Interface& itf)
{
	skeleton_end (itf);

	// Bases
	const Interfaces all_bases = itf.get_all_bases ();

	if (itf.interface_kind () != InterfaceKind::PSEUDO || !all_bases.empty ()) {
		h_ << ",\n"
			"{ // base\n"
			<< indent;

		if (itf.interface_kind () == InterfaceKind::ABSTRACT)
			h_ << "S::template _wide_abstract <";
		else if (itf.interface_kind () != InterfaceKind::PSEUDO)
			h_ << "S::template _wide_object <";
		h_ << QName (itf) << '>';

		auto b = all_bases.begin ();
		if (itf.interface_kind () == InterfaceKind::PSEUDO) {
			h_ << "S::template _wide <" << QName (**b) << ", " << QName (itf) << '>';
			++b;
		}

		for (; b != all_bases.end (); ++b) {
			h_ << ",\n"
				"S::template _wide <" << QName (**b) << ", " << QName (itf) << '>';
		}

		h_ << std::endl
			<< unindent
			<< "}";
	}

	if (compiler ().ami_interfaces ().find (&itf) != compiler ().ami_interfaces ().end ())
		h_ << ",\n"
		"{ NIRVANA_BRIDGE_AMI_INIT (S, " << QName (itf) << ") }";

	// EPV
	epv ();

	// AMI skeleton
	skeleton_ami (itf);

	// Servant implementations
	if (!options ().no_servant) {

		OperationSet implement_operations;
		ComponentFlags component_type = define_component (itf, implement_operations);

		if (itf.interface_kind () != InterfaceKind::ABSTRACT) {

			// Standard implementation

			h_.empty_line ();
			h_ << "template <class S>\n"
				"class Servant <S, " << QName (itf) << "> :\n"
				<< indent
				<< "public Implementation";
			implementation_suffix (itf);
			implementation_parameters (itf, all_bases);
			h_ << "\n"
				<< unindent;
			if (itf.interface_kind () != InterfaceKind::PSEUDO) {
				h_ << "{\n" << indent;

				if (component_type) {
					h_ << unindent
						<< "public:\n"
						<< indent;

					if (component_type & CCM_FACETS)
						h_ << "using InterfaceImpl <S, " << QName (itf) << ">::provide_facet;\n\n";

					if (component_type & CCM_RECEPTACLES) {
						h_ << "using InterfaceImpl <S, " << QName (itf) << ">::connect;\n"
							"using InterfaceImpl <S, " << QName (itf) << ">::disconnect;\n\n";
					}

					h_ << std::endl;
				}

				h_ << unindent
					<< "protected:\n"
					<< indent
					<< "Servant ()\n"
						"{}\n"
						"Servant (Object::_ptr_type comp) :\n"
					<< indent
					<< "Implementation";
				implementation_suffix (itf);
				implementation_parameters (itf, all_bases);
				h_ << " (comp)\n"
					<< unindent
					<< "{}\n";

				h_ << unindent << "};\n";
			} else
				h_ << "{};\n";

			// Static implementation
			h_.empty_line ();
			h_ << "template <class S>\n"
				"class ServantStatic <S, " << QName (itf) << "> :\n"
				<< indent
				<< "public Implementation";
			implementation_suffix (itf);
			h_ << "Static";
			implementation_parameters (itf, all_bases);
			h_ << unindent << "\n{};\n";
		}

		if (itf.interface_kind () != InterfaceKind::PSEUDO) {

			// POA implementation

			h_ << empty_line
				<< "template <>\n"
				"class ServantPOA <" << QName (itf) << "> :\n"
				<< indent;

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
			h_ << "public InterfaceImpl <ServantPOA <" << QName (itf) << ">, " << QName (itf) << ">\n"
				<< unindent
				<< "{\n"
				<< "public:\n"
				<< indent
				<< "typedef " << QName (itf) << " PrimaryInterface;\n\n";

			if (itf.interface_kind () != InterfaceKind::ABSTRACT) {
				h_ << "virtual I_ptr <Interface> _query_interface (const String& id) override\n"
					"{\n"
					<< indent
					<< "return FindInterface <" << QName (itf);
				for (auto b : all_bases) {
					h_ << ", " << QName (*b);
				}
				h_ << ">::find (*this, id);\n"
					<< unindent
					<< "}\n\n"
					<< Namespace ("IDL") << "Type <" << QName (itf) << ">::VRet _this ()\n"
					"{\n"
					<< indent
					<< "return this->_get_proxy ().template downcast <" << QName (itf)
					<< "> ();\n"
					<< unindent
					<< "}\n";
			}

			h_.empty_line ();

			virtual_operations (itf, &implement_operations);

			if (component_type & CCM_FACETS) {
				h_ << "virtual " << Namespace ("IDL") << "Type <CORBA::Object>::VRet provide_facet "
					"(const Type < ::Components::FeatureName>::Var& name) override\n"
					"{\n" << indent <<
					"return " << BaseImplPOA (itf) << "provide_facet (name);\n"
					<< unindent << "}\n";
			}

			if (component_type & CCM_RECEPTACLES) {
				h_ << "virtual " << Namespace ("IDL") << "Type < ::Components::Cookie>::VRet connect "
					"(const Type < ::Components::FeatureName>::Var& name, Type <CORBA::Object>::ConstRef connection) override\n"
					"{\n" << indent <<
					"return " << BaseImplPOA (itf) << "connect (name, connection);\n"
					<< unindent << "}\n"
					"virtual " << Namespace ("IDL") << "Type <CORBA::Object>::VRet disconnect "
					"(const Type < ::Components::FeatureName>::Var& name, Type < ::Components::Cookie>::ConstRef ck) override\n"
					"{\n" << indent <<
					"return " << BaseImplPOA (itf) << "disconnect (name, ck);\n"
					<< unindent << "}\n";
			}

			if (itf.interface_kind () != InterfaceKind::ABSTRACT) {
				h_
					<< unindent
					<< "\nprotected:\n"
					<< indent
					<< "ServantPOA ()\n"
						"{}\n"
						"ServantPOA (Object::_ptr_type comp)\n"
						"{\n"
					<< indent
					<< "_create_proxy (comp);\n"
					<< unindent
					<< "}\n";
			}
			h_ << unindent << "};\n";
		}

		h_.namespace_close ();

		if (itf.interface_kind () != InterfaceKind::PSEUDO
		&& itf.interface_kind () != InterfaceKind::ABSTRACT) {
			if (options ().legacy) {

				h_ << "\n#ifdef LEGACY_CORBA_CPP\n";
				const NamedItem* ns = itf.parent ();
				if (ns) {
					ScopedName sn = ns->scoped_name ();
					{
						std::string s = "POA_";
						s += sn.front ();
						h_ << "namespace " << s << " {\n";
					}
					for (auto it = sn.cbegin () + 1; it != sn.cend (); ++it) {
						h_ << "namespace " << *it << " {\n";
					}

					h_ << "\n"
						"typedef " << Namespace ("CORBA/Internal")
						<< "ServantPOA <" << QName (itf) << "> " << itf.name () << ";\n";

					if (itf.interface_kind () != InterfaceKind::ABSTRACT)
						h_ << "template <class T> using " << itf.name () << "_tie = "
						<< Namespace ("CORBA/Internal") << "ServantTied <T, " << QName (itf) << ">;\n\n";

					for (size_t cnt = sn.size (); cnt; --cnt) {
						h_ << "}\n";
					}
				} else {

					h_ << "typedef " << Namespace ("CORBA/Internal")
						<< "ServantPOA <" << QName (itf) << "> POA_" << static_cast <const std::string&> (itf.name ()) << ";\n";

					if (itf.interface_kind () != InterfaceKind::ABSTRACT)
						h_ << "template <class T> using POA_" << static_cast <const std::string&> (itf.name ()) << "_tie = "
						<< Namespace ("CORBA/Internal") << "ServantTied <T, " << QName (itf) << ">;\n\n";
				}
				h_ << "#endif\n";
			}

			h_.namespace_open ("CORBA");
			h_ << "template <> struct servant_traits <" << QName (itf)
				<< "> : public Internal::TraitsInterface <I>\n"
				"{\n" << indent <<
				"typedef Internal::ServantPOA <I> base_type;\n"
				"typedef servant_reference <Internal::ServantPOA <I> > ref_type;\n"
				"\n"
				"template <class S>\n"
				"using tie_type = Internal::ServantTied <S, I>;\n"
				"\n"
				"template <class S>\n"
				"using Servant = Internal::Servant <S, I>;\n"
				"template <class S>\n"
				"using ServantStatic = Internal::ServantStatic <S, I>;\n"
				<< unindent << "};\n";
		}
	}
}

void Servant::end (const ValueType& vt)
{
	skeleton_end (vt);

	// Bases

	h_ << ",\n"
		"{ // base\n"
		<< indent
		<< "S::template _wide_val <ValueBase, " << QName (vt) << '>';

	const Bases all_bases = get_all_bases (vt);

	for (auto b : all_bases) {
		h_ << ",\n"
			"S::template _wide_val <" << QName (*b) << ", " << QName (vt) << '>';
	}

	const Interface* concrete_itf = get_concrete_supports (vt);
	if (concrete_itf) {
		h_ << ",\n"
			"S::template _wide_val <" << QName (*concrete_itf) << ", " << QName (vt) << '>';

		Interfaces bases = concrete_itf->get_all_bases ();
		for (auto b : bases) {
			h_ << ",\n"
				"S::template _wide_val <" << QName (*b) << ", " << QName (vt) << '>';
		}
	}

	h_ << std::endl
		<< unindent
		<< "}";

	// EPV
	epv (concrete_itf && concrete_itf->interface_kind () != InterfaceKind::PSEUDO);

	// Servant implementations

	StateMembers members = get_members (vt);
	if (!options ().no_servant) {

		// ValueImpl
		if (vt.modifier () != ValueType::Modifier::ABSTRACT) {
			h_ << "template <class S>\n"
				"class ValueImpl <S, " << QName (vt) << "> :\n"
				<< indent
				<< "public ValueImplBase <S, " << QName (vt) << ">,\n"
				"public ValueData <" << QName (vt) << ">\n"
				<< unindent
				<< "{};";

			// Factory function specialization
			h_ << empty_line
				<< "template <> ValueFactoryBase::_ptr_type get_factory <" << QName (vt) << "> () noexcept;\n";
		}

		// Standard implementation

		h_ << empty_line
			<< "template <class S>\n"
			"class Servant <S, " << QName (vt) << "> :\n"
			<< indent;

		enum class BaseType {
			ABSTRACT,
			CONCRETE,
			SUPPORTS
		} base_type = (concrete_itf && concrete_itf->interface_kind () != InterfaceKind::PSEUDO) ? BaseType::SUPPORTS :
			(vt.modifier () == ValueType::Modifier::ABSTRACT ? BaseType::ABSTRACT : BaseType::CONCRETE);

		h_ << "public ValueBase";
		if (BaseType::ABSTRACT == base_type)
			h_ << "Abstract <S>";
		else {
			if (BaseType::CONCRETE == base_type)
				h_ << "Concrete <S, ";
			else
				h_ << "Supports <S, ";
			h_ << QName (vt);
			if (vt.modifier () == ValueType::Modifier::TRUNCATABLE)
				h_ << ", &" << TC_Name (*vt.bases ().front ());
			h_ << ">";
		}

		if (BaseType::SUPPORTS != base_type)
			h_ << ",\n"
			"public RefCountBase <S>";

		if (concrete_itf)
			h_ << ",\n"
			"public Servant <S, " << QName (*concrete_itf) << ">";

		bool abstract_base = false;
		std::vector <const ValueType*> concrete_bases;
		for (auto it = all_bases.rbegin (); it != all_bases.rend (); ++it) {
			auto b = *it;
			h_ << ",\n"
				"public ";
			if (b->kind () == Item::Kind::VALUE_TYPE) {
				h_ << "Value";

				const ValueType& vt = static_cast <const ValueType&> (*b);
				if (vt.modifier () != ValueType::Modifier::ABSTRACT)
					concrete_bases.push_back (&vt);

			} else {
				assert (static_cast <const Interface&> (*b).interface_kind () == InterfaceKind::ABSTRACT);
				abstract_base = true; // We need abstract base

				// If value supports concrete interface, abstract interface references must be obtained
				// via _this() call. If value supports abstract interfaces only, abstract interface references
				// obtained directly from the servant reference.
				if (concrete_itf)
					h_ << "Interface";
				else
					h_ << "Value";
			}
			h_ << "Impl <S, " << QName (*b) << '>';
		}

		h_ << ",\n"
			"public ValueImpl <S, " << QName (vt) << ">";

		if (abstract_base)
			h_ << ",\n"
				"public InterfaceImpl <S, AbstractBase>";

		h_ << "\n" << unindent <<
			"{\n"
			"public:\n" << indent;

		if (abstract_base)
			h_ << "using InterfaceImpl <S, AbstractBase>::_get_abstract_base;\n";

		h_ << "typedef " << QName (vt) << " PrimaryInterface;\n"
			"\n"
			"I_ptr <Interface> _query_valuetype (const String& id)\n"
			"{\n"
			<< indent
			<< "return FindInterface <" << QName (vt);

		for (auto b : all_bases) {
			h_ << ", " << QName (*b);
		}
		if (concrete_itf && concrete_itf->interface_kind () == InterfaceKind::PSEUDO)
			h_ << ", " << QName (*concrete_itf);

		h_ << ">::find (static_cast <S&> (*this), id);\n"
			<< unindent << "}\n";

		if (vt.modifier () != ValueType::Modifier::ABSTRACT) {
			h_ << "void _marshal (I_ptr <IORequest> rq) const\n"
				"{\n" << indent;
			
			for (auto b : concrete_bases) {
				h_ << "ValueData <" << QName (*b) << ">::_marshal (rq);\n";
			}
			h_ << "ValueData <" << QName (vt) << ">::_marshal (rq);\n"
				<< unindent << "}\n"

				"void _unmarshal (I_ptr <IORequest> rq)\n"
				"{\n" << indent;

			for (auto b : concrete_bases) {
				h_ << "ValueData <" << QName (*b) << ">::_unmarshal (rq);\n";
			}
			h_ << "ValueData <" << QName (vt) << ">::_unmarshal (rq);\n"
				<< unindent << "}\n\n";
		}

		StateMembers all_members;
		for (auto b : concrete_bases) {
			StateMembers members = get_members (*b);
			all_members.insert (all_members.end (), members.begin (), members.end ());
		}
		all_members.insert (all_members.end (), members.begin (), members.end ());

		value_constructors ("Servant", all_members);

		h_ << unindent
			<< "};\n";

		// Static implementation for abstract value types

		if (vt.modifier () == ValueType::Modifier::ABSTRACT) {
			h_ << empty_line
				<< "template <class S>\n"
				"class ServantStatic <S, " << QName (vt) << "> : public ValueStatic";
			
			if (BaseType::CONCRETE == base_type)
				h_ << "Supports";
			else if (abstract_base)
				h_ << "AbstractBase";
			
			h_ << " <S, " << QName (vt);
			if (concrete_itf)
				h_ << ", " << QName (*concrete_itf);

			for (auto b : all_bases) {
				h_ << ", " << QName (*b);
			}

			h_ << ">\n{};";
		}

		// Virtual implementation

		h_ << empty_line
			<< "template <>\n"
			"class ServantPOA <" << QName (vt) << "> :\n"
			<< indent;
		if (vt.bases ().empty ())
			h_ << "public virtual ServantPOA <ValueBase>,\n";
		else
			for (auto b : vt.bases ()) {
				h_ << "public virtual ServantPOA <" << QName (*b) << ">,\n";
			}
		for (auto b : vt.supports ()) {
			h_ << "public virtual ServantPOA <" << QName (*b) << ">,\n";
		}
		h_ << "public ValueImpl <ServantPOA <" << QName (vt) << ">, " << QName (vt) << ">\n"
			<< unindent <<
			"{\n"
			"public:\n" << indent
			<< "typedef " << QName (vt) << " PrimaryInterface;\n\n";

		virtual_operations (vt);

		// Generate virtual accessors for public members only
		for (auto m : members) {
			if (m->is_public ()) {

				// Getter
				h_ << "virtual " << ConstRef (*m) << ' ' << m->name () << " () const\n"
					"{\n" << indent <<
					"return ValueData <" << QName (vt) << ">::" << m->name () << " ();\n"
					<< unindent << "}\n";

				// Setter
				if (is_ref_type (*m)) {
					h_ << "virtual void " << m->name () << " (" << Var (*m) << " val)\n"
						"{\n" << indent <<
						"ValueData <" << QName (vt) << ">::" << m->name () << " (std::move (val)); \n"
						<< unindent << "}\n";
				} else {
					h_ << "virtual void " << m->name () << " (" << ConstRef (*m) << " val)\n"
						"{\n" << indent <<
						"ValueData <" << QName (vt) << ">::" << m->name () << " (val); \n"
						<< unindent << "}\n";

					if (is_var_len (*m)) {
						// The move setter
						h_ << "virtual void " << m->name () << " (" << Var (*m) << "&& val)\n"
							"{\n" << indent <<
							"ValueData <" << QName (vt) << ">::" << m->name () << " (std::move (val)); \n"
							<< unindent << "}\n";
					}
				}
			}
		}

		value_constructors ("ServantPOA", all_members);

		h_ << unindent << 
			"\nprivate:\n"
			<< indent <<
			"virtual I_ptr <Interface> _query_valuetype (const String& id) override\n"
			"{\n" << indent
			<< "return FindInterface <" << QName (vt);
		for (auto b : all_bases) {
			h_ << ", " << QName (*b);
		}
		if (concrete_itf && concrete_itf->interface_kind () == InterfaceKind::PSEUDO)
			h_ << ", " << QName (*concrete_itf);

		h_ << ">::find (*this, id);\n"
			<< unindent << "}\n";

		if (vt.modifier () != ValueType::Modifier::ABSTRACT) {
			h_ << "virtual void _marshal (I_ptr <IORequest> rq) const override\n"
				"{\n" << indent;

			for (auto b : concrete_bases) {
				h_ << "ValueData <" << QName (*b) << ">::_marshal (rq);\n";
			}
			h_ << "ValueData <" << QName (vt) << ">::_marshal (rq);\n"
				<< unindent << "}\n"

				"virtual void _unmarshal (I_ptr <IORequest> rq) override\n"
				"{\n" << indent;

			for (auto b : concrete_bases) {
				h_ << "ValueData <" << QName (*b) << ">::_unmarshal (rq);\n";
			}
			h_ << "ValueData <" << QName (vt) << ">::_unmarshal (rq);\n"
				<< unindent << "}\n\n";
		}

		if (vt.modifier () == ValueType::Modifier::TRUNCATABLE) {
			h_ << "virtual TypeCode::_ptr_type _truncatable_base () const noexcept override\n"
				"{\n" << indent
				<< "return " << TC_Name (*vt.bases ().front ()) << ";\n"
				<< unindent << "}\n";
		}

		if (vt.modifier () != ValueType::Modifier::ABSTRACT) {
			h_ << "virtual ValueFactoryBase::_ptr_type _factory () const override\n"
				"{\n" << indent
				<< "return get_factory <" << QName (vt) << "> ();\n"
				<< unindent << "}\n";
		}

		h_ << unindent << "};\n\n";

		// OBV_ type

		h_.namespace_open (vt);
		h_ << "typedef " << Namespace ("CORBA/Internal") << "ServantPOA <" << vt.name () << "> OBV_"
			<< static_cast <const std::string&> (vt.name ()) << ";\n";

		if (vt.modifier () != ValueType::Modifier::ABSTRACT) {
			
			h_.namespace_open ("CORBA/Internal");

			// TypeCodeValue
			{
				const char* mod = "VM_NONE";
				switch (vt.modifier ()) {
				case ValueType::Modifier::CUSTOM:
					mod = "VM_CUSTOM";
					break;
				case ValueType::Modifier::TRUNCATABLE:
					mod = "TRUNCATABLE";
					break;
				}

				h_ << empty_line <<
					"template <>\n"
					"class TypeCodeValue <" << QName (vt) << "> : public TypeCodeValueConcrete <" << QName (vt) << ", " << mod << ", "
					<< (members.empty () ? "false" : "true") << ", ";

				const ValueType* concrete_base = nullptr;
				if (!vt.bases ().empty ()) {
					concrete_base = vt.bases ().front ();
					if (concrete_base->modifier () == ValueType::Modifier::ABSTRACT)
						concrete_base = nullptr;
				}

				if (concrete_base)
					h_ << Namespace ("IDL") << "Type <" << QName (*concrete_base) << ">::type_code";
				else
					h_ << "nullptr";

				h_ << ">\n"
					"{};\n";
			}

			// Factories

			Factories factories = get_factories (vt);
			if (!factories.empty ()) {
				skeleton_begin (vt, FACTORY_SUFFIX);

				for (auto f : factories) {
					h_ << "static Interface* " SKELETON_FUNC_PREFIX << f->name () << "(Bridge <" << QName (vt)
						<< FACTORY_SUFFIX ">* _b";
					for (auto p : *f) {
						h_ << ", "
							<< ABI_param (*p,
								is_var_len (*p) ? AST::Parameter::Attribute::INOUT : AST::Parameter::Attribute::IN)
							<< ' ' << p->name ();
					}
					h_ << ", Interface* _env) noexcept\n"
						"{\n"
						<< indent
						<< "try {\n"
						<< indent
						<< "return " << Namespace ("IDL") << "Type <" << QName (vt) << ">::ret (S::_implementation (_b)."
						<< f->name () << " (";
					if (!f->empty ()) {
						auto par = f->begin ();
						h_ << ABI2ServantFactory (**par);
						for (++par; par != f->end (); ++par) {
							h_ << ", " << ABI2ServantFactory (**par);
						}
					}
					h_ << "));\n"
						<< CatchBlock ()

						// Return default value on exception
						<< "return " << Namespace ("IDL") << "Type <" << QName (vt) << ">::ret ();\n";

					h_ << unindent << "}\n\n";
				}

				skeleton_end (vt, FACTORY_SUFFIX);
				h_ << ",\n"
					"{ // base\n"
					<< indent
					<< "S::template _wide_val <ValueFactoryBase, " << QName (vt) << FACTORY_SUFFIX ">\n"
					<< unindent << "},\n"
					"{ // EPV\n"
					<< indent;

				{
					auto f = factories.begin ();
					h_ << "S::" SKELETON_FUNC_PREFIX << (*f)->name ();
					for (++f; f != factories.end (); ++f) {
						h_ << ",\nS::" SKELETON_FUNC_PREFIX << (*f)->name ();
					}
					h_ << unindent
						<< "\n}"
						<< unindent
						<< "\n};\n\n";
				}
			}
			h_ << "template <class Impl>\n"
				"class ValueCreator <Impl, " << QName (vt) << "> :\n"
				<< indent;
			if (!factories.empty ()) {
				h_ << "public ValueCreatorImpl <ValueCreator <Impl, " << QName (vt) << ">, " << QName (vt) << FACTORY_SUFFIX ">,\n"
					"public ValueCreatorBase <Impl>\n"
					<< unindent <<
					"{\n"
					"public:\n"
					<< indent
					<< "using ImplType = typename ValueCreatorBase <Impl>::ImplType;\n";

				for (auto f : factories) {
					h_ << "static " << Namespace ("IDL") << "Type <" << QName (vt) << ">::VRet " << f->name () << " (";
					{
						auto it = f->begin ();
						if (it != f->end ()) {
							h_ << ServantParam (**it, false, true) << ' ' << (*it)->name ();
							++it;
							for (; it != f->end (); ++it) {
								h_ << ", " << ServantParam (**it, false, true) << ' ' << (*it)->name ();
							}
						}
					}
					h_ << ")\n"
						"{\n"
						<< indent <<
						"return create_value <ImplType> (";
					{
						auto it = f->begin ();
						if (it != f->end ()) {
							h_ << ConstructorParam (**it);
							for (++it; it != f->end (); ++it) {
								h_ << ", " << ConstructorParam (**it);
							}
						}
					}
					h_ << ");\n"
						<< unindent <<
						"}\n\n";
				}

				h_ << unindent << "};\n";
			} else {
				h_ << "public ValueCreatorNoFactory <Impl>\n"
					<< unindent <<
					"{};\n";
			}
		}
	}
}

void Servant::value_constructors (const char* class_name, const StateMembers& all_members)
{
	h_ << unindent
		<< empty_line
		<< "protected:\n"
		<< indent

		// Default constructor
		<< class_name << " () noexcept\n"
		"{}\n";

	// Explicit constructor
	if (!all_members.empty ()) {
		h_ << "explicit " << class_name << " (";
		auto it = all_members.begin ();
		h_ << Var (**it) << ' ' << (*it)->name ();
		h_.indent ();
		for (++it; it != all_members.end (); ++it) {
			h_ << ",\n" << Var (**it) << ' ' << (*it)->name ();
		}
		h_ << ")\n"
			<< unindent
			<< "{\n"
			<< indent;
		for (auto m : all_members) {
			h_ << "this->" << m->name () << " (std::move (" << m->name () << "));\n";
		}
		h_ << unindent
			<< "}\n";
	}
}

void Servant::epv (bool val_with_concrete_itf)
{
	if (!epv_.empty () || val_with_concrete_itf) {
		h_ << ",\n"
			"{ // EPV\n"
			<< indent;

		if (val_with_concrete_itf)
			h_ << SKELETON_FUNC_PREFIX "this";

		auto n = epv_.begin ();
		if (n != epv_.end ()) {
			if (!val_with_concrete_itf) {
				h_ << "S::" << *(n++);
			}
			for (; n != epv_.end (); ++n) {
				h_ << ",\nS::" << *n;
			}
		}
		h_ << unindent
			<< "\n}";
	}

	h_ << unindent
		<< "\n};\n";
	epv_.clear ();
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
	h_ << " <S, " << QName (primary);
	for (auto b : bases) {
		h_ << ", " << QName (*b);
	}
	h_ << ">";
}

void Servant::leaf (const Operation& op)
{
	if (is_native (op.raises ()))
		return;

	const ItemScope& itf = *op.parent ();

	h_ << "static " << ABI_ret (op);

	{
		std::string name = SKELETON_FUNC_PREFIX;
		name += op.name ();
		h_ << ' ' << name << " (Bridge <" << QName (itf) << ">* _b";
		epv_.push_back (std::move (name));
	}

	for (auto param : op) {
		h_ << ", " << ABI_param (*param) << ' ' << param->name ();
	}

	h_ << ", Interface* _env) noexcept\n"
		"{\n"
		<< indent
		<< "try {\n"
		<< indent;

	// Exception list for ExceptionHolder
	const Raises* holder_raises = nullptr;
	if (op.tkind () == Type::Kind::VOID) {
		// Check for exception callback
		if (itf.kind () == Item::Kind::INTERFACE
			&& op.size () == 1
			&& op.front ()->tkind () == Type::Kind::NAMED_TYPE
			&& op.front ()->named_type ().qualified_name () == "::Messaging::ExceptionHolder"
			&& op.name ().ends_with (AMI_EXCEP)
			) {
			// Probably a handler
			auto f = compiler ().ami_handlers ().find (&static_cast <const Interface&> (itf));
			if (f != compiler ().ami_handlers ().end ()) {
				// Yes it is a handler
				const Interface& main_itf = *f->second;
				Identifier op_name = op.name ();
				// Trim "_excep" suffix
				op_name.resize (op_name.size () - 6);
				const NamedItem* main_op = find_item (main_itf, op_name, Item::Kind::OPERATION);
				if (main_op)
					holder_raises = &static_cast <const Operation&> (*main_op).raises ();
				else {
					// Try attribute
					bool getter = op_name.starts_with ("get_");
					if (getter || op_name.starts_with ("set_")) {
						op_name.erase (0, 4);
						main_op = find_item (main_itf, op_name, Item::Kind::ATTRIBUTE);
						if (main_op) {
							const Attribute& att = static_cast <const Attribute&> (*main_op);
							if (getter || !att.readonly ()) {
								// OK, it is attribute
								holder_raises = getter ? &att.getraises () : &att.setraises ();
							}
						}
					}
				}
			}
		}
	} else
		h_ << "return " << TypePrefix (op) << "ret (";

	if (holder_raises && !holder_raises->empty ()) {
		h_ << "::Messaging::ExceptionHolder::_ptr_type _eh = " << Namespace ("IDL") << "Type < ::Messaging::ExceptionHolder>::in ("
			<< op.front ()->name () << ");\n"
			"set_user_exceptions <" << *holder_raises << "> (_eh);\n"
			"S::_implementation (_b)." << op.name () << " (_eh);\n";
	} else {
		h_ << "S::_implementation (_b)." << op.name () << " (";
		if (!op.empty ()) {
			auto par = op.begin ();
			h_ << ABI2Servant (**par);
			for (++par; par != op.end (); ++par) {
				h_ << ", " << ABI2Servant (**par);
			}
		}
		if (op.tkind () != Type::Kind::VOID)
			h_ << ')';
		h_ << ");\n";
	}
	h_ << CatchBlock ();
	if (op.tkind () != Type::Kind::VOID) {
		// Return default value on exception
		h_ << "return " << TypePrefix (op) << "ret ();\n";
	}

	h_ << unindent
		<< "}\n\n";
}

const NamedItem* Servant::find_item (const Interface& itf, const Identifier& name, Item::Kind kind)
{
	for (auto item : itf) {
		if (item->kind () == kind) {
			const NamedItem& ni = static_cast <const NamedItem&> (*item);
			if (ni.name () == name)
				return &ni;
		}
	}
	return nullptr;
}

void Servant::leaf (const Attribute& att)
{
	attribute (att);

	h_ << std::endl;
}

void Servant::leaf (const StateMember& m)
{
	if (m.is_public ()) {
		attribute (m);
		if (is_var_len (m)) {
			{
				std::string name = SKELETON_MOVE_PREFIX;
				name += m.name ();
				h_ << "static void " << name << " (Bridge <" << QName (*m.parent ()) << ">* _b, "
					<< ABI_param (m, Parameter::Attribute::INOUT) << " val, Interface* _env) noexcept\n"
					"{\n";
				epv_.push_back (std::move (name));
			}
			h_ << indent
				<< "try {\n"
				<< indent
				<< "S::_implementation (_b)." << m.name () << " (std::move ("
				<< ABI2Servant (m, "val", Parameter::Attribute::INOUT) << "));\n"
				<< CatchBlock ()
				<< unindent
				<< "}\n";
		}

		h_ << std::endl;
	}
}

void Servant::attribute (const Member& m)
{
	const Attribute* att = nullptr;
	if (m.kind () == Item::Kind::ATTRIBUTE)
		att = &static_cast <const Attribute&> (m);

	const ItemScope& itf = *m.parent ();

	if (!att || !is_native (att->getraises ())) {
		h_ << "static " << ABI_ret (m, attributes_by_ref_);
		{
			std::string name = SKELETON_GETTER_PREFIX;
			name += m.name ();
			h_ << ' ' << name << " (";
			if (!att)
				h_ << "const ";
			h_ << "Bridge <" << QName (itf) << ">* _b, Interface* _env) noexcept\n"
				"{\n";
			epv_.push_back (std::move (name));
		}
		h_ << indent
			<< "try {\n"
			<< indent
			<< "return " << TypePrefix (m);
		if (attributes_by_ref_)
			h_ << "VT_";
		h_ << "ret (S::_implementation (_b)." << m.name () << " ());\n"
			<< CatchBlock ()
			<< "return " << TypePrefix (m);
		if (attributes_by_ref_)
			h_ << "VT_";
		h_ << "ret ();\n"
			<< unindent
			<< "}\n";
	}

	if (att ? (!(att->readonly () || is_native (att->setraises ()))) : !is_ref_type (m)) {
		{
			std::string name = SKELETON_SETTER_PREFIX;
			name += m.name ();
			h_ << "static void " << name << " (Bridge <" << QName (itf) << ">* _b, "
				<< ABI_param (m) << " val, Interface* _env) noexcept\n"
				"{\n";
			epv_.push_back (std::move (name));
		}
		h_ << indent
			<< "try {\n"
			<< indent
			<< "S::_implementation (_b)." << m.name () << " (" << ABI2Servant (m, "val") << ");\n"
			<< CatchBlock ()
			<< unindent
			<< "}\n";
	}
}

void Servant::skeleton_ami (const AST::Interface& itf)
{
	auto ami_it = compiler ().ami_interfaces ().find (&itf);
	if (ami_it == compiler ().ami_interfaces ().end ())
		return;

	const Type handler_type (ami_it->second.handler);

	h_.namespace_open ("CORBA/Internal");
	h_ << empty_line

		<< "template <class S>\n"
		"class AMI_Skeleton <S, " << QName (itf) << "> : public Decls <" << QName (itf) << ">\n"
		"{\n"
		"public:\n"
		<< indent
		<< "static const AMI_EPV <" << QName (itf) << "> epv_;\n\n"
		<< unindent
		<< "protected:\n"
		<< indent;

	for (auto item : itf) {
		switch (item->kind ()) {

		case Item::Kind::OPERATION: {
			const Operation& op = static_cast <const Operation&> (*item);

			if (is_native (op.raises ()))
				break;

			std::string name = SKELETON_FUNC_PREFIX AMI_SENDC + op.name ();

			h_ << "static void " << name << " (Bridge <"
				<< QName (itf) << ">* _b, Interface* " AMI_HANDLER << AMI_ParametersABI (op) << "noexcept \n"
				"{\n" << indent
				<< "try {\n" << indent
				<< "S::_implementation (_b)." AMI_SENDC << static_cast <const std::string&> (op.name ())
				<< " (" << ABI2Servant (handler_type, AMI_HANDLER);

			for (auto param : op) {
				if (param->attribute () != Parameter::Attribute::OUT)
					h_ << "," << ABI2Servant (*param, param->name ());
			}

			h_ << ");\n"
				<< CatchBlock ()
				<< unindent << "}\n";
			epv_.push_back (std::move (name));

			name = SKELETON_FUNC_PREFIX AMI_SENDP + op.name ();

			h_ << "static Interface* " << name << " (Bridge <"
				<< QName (itf) << ">* _b" << AMI_ParametersABI (op) << "noexcept \n"
				"{\n" << indent
				<< "try {\n" << indent
				<< "return " POLLER_TYPE_PREFIX
				<< "ret (S::_implementation (_b)." AMI_SENDP << static_cast <const std::string&> (op.name ()) << " (";

			auto it = op.begin ();
			while (it != op.end () && (*it)->attribute () == Parameter::Attribute::OUT)
				++it;
			if (it != op.end ()) {
				h_ << ABI2Servant (**it, (*it)->name ());
				for (++it; it != op.end (); ++it) {
					if ((*it)->attribute () != Parameter::Attribute::OUT)
						h_ << ", " << ABI2Servant (**it, (*it)->name ());
				}
			}

			h_ << "));\n"
				<< CatchBlock ()
				<< "return " POLLER_TYPE_PREFIX "ret ();\n"
				<< unindent << "}\n\n";
			epv_.push_back (std::move (name));

		} break;

		case Item::Kind::ATTRIBUTE: {
			const Attribute& att = static_cast <const Attribute&> (*item);

			if (!is_native (att.getraises ())) {

				std::string name = SKELETON_FUNC_PREFIX AMI_SENDC "get_" + att.name ();

				h_ << "static void " << name << " (Bridge <" << QName (itf) << ">* _b, "
					"Interface* " AMI_HANDLER ", Interface* _env) noexcept\n"
					"{\n" << indent
					<< "try {\n" << indent
					<< "S::_implementation (_b)." AMI_SENDC "get_" << static_cast <const std::string&> (att.name ()) << " ("
					<< ABI2Servant (handler_type, AMI_HANDLER) << ");\n"
					<< CatchBlock ()
					<< unindent << "}\n";
				epv_.push_back (std::move (name));

				name = SKELETON_FUNC_PREFIX AMI_SENDP "get_" + att.name ();

				h_ << "static Interface* " << name << " (Bridge <" << QName (itf) << ">* _b, Interface* _env) noexcept\n"
					"{\n" << indent
					<< "try {\n" << indent
					<< "return " POLLER_TYPE_PREFIX
					<< "ret (S::_implementation (_b)." AMI_SENDP "get_" << static_cast <const std::string&> (att.name ()) << " ());\n"
					<< CatchBlock ()
					<< "return " POLLER_TYPE_PREFIX "ret ();\n"
					<< unindent << "}\n";
				epv_.push_back (std::move (name));
			}

			if (!att.readonly () && !is_native (att.setraises ())) {

				std::string name = SKELETON_FUNC_PREFIX AMI_SENDC "set_" + att.name ();

				h_ << "static void " << name << " (Bridge <" << QName (itf) << ">* _b, "
					"Interface* " AMI_HANDLER ", "
					<< ABI_param (att, Parameter::Attribute::IN) << " val, Interface* _env) noexcept\n"
					"{\n" << indent
					<< "try {\n" << indent
					<< "S::_implementation (_b)." AMI_SENDC "set_" << static_cast <const std::string&> (att.name ())
					<< " (" << ABI2Servant (handler_type, AMI_HANDLER) << ", " << ABI2Servant (att, "val") << ");\n"
					<< CatchBlock ()
					<< unindent << "}\n";
				epv_.push_back (std::move (name));

				name = SKELETON_FUNC_PREFIX AMI_SENDP "set_" + att.name ();

				h_ << "static Interface* " << name << " (Bridge <" << QName (itf) << ">* _b, "
					<< ABI_param (att, Parameter::Attribute::IN) << " val, Interface* _env) noexcept\n"
					"{\n" << indent
					<< "try {\n" << indent
					<< "return " POLLER_TYPE_PREFIX
					<< "ret (S::_implementation (_b)." AMI_SENDP "set_" << static_cast <const std::string&> (att.name ()) << " ("
					<< ABI2Servant (att, "val") << "));\n"
					<< CatchBlock ()
					<< "return "  POLLER_TYPE_PREFIX "ret ();\n"
					<< unindent << "}\n";
				epv_.push_back (std::move (name));
			}
			h_ << std::endl;
		} break;
		}
	}

	h_ << unindent
		<< "};\n"
		"\ntemplate <class S>\n"
		"const AMI_EPV <" << QName (itf) << "> AMI_Skeleton <S, " << QName (itf) << ">::epv_ = {\n"
		<< indent;

	auto n = epv_.begin ();
	if (n != epv_.end ()) {
		h_ << "S::" << *(n++);
		for (; n != epv_.end (); ++n) {
			h_ << ",\nS::" << *n;
		}
	}
	epv_.clear ();

	h_ << unindent
		<< "\n};\n";
}

void Servant::leaf (const Constant& c)
{
	if (c.vtype () == Variant::VT::EMPTY) {
		h_.namespace_open (c);
		h_ << "class Static_" << static_cast <const std::string&> (c.name ()) << ";\n";
		h_.namespace_open ("CORBA/Internal");
		h_ << empty_line
			<< "template <>\n"
			"const char StaticId <" << ItemNamespace (c) << "Static_" << static_cast <const std::string&> (c.name ())
			<< ">::id [] = \"" << const_id (c) << "\";\n";
	}
}

const Operation* Servant::find_operation (const Interface& itf, const Identifier& name)
{
	for (const auto& item : itf) {
		if (item->kind () == Item::Kind::OPERATION) {
			const Operation& op = static_cast <const Operation&> (*item);
			if (op.name () == name)
				return &op;
		}
	}
	return nullptr;
}

Code& operator << (Code& stm, const Servant::ABI2Servant& val)
{
	stm << TypePrefix (val.type);
	switch (val.att) {
	case Parameter::Attribute::IN:
		stm << "in";
		break;
	case Parameter::Attribute::OUT:
		stm << "out";
		break;
	case Parameter::Attribute::INOUT:
		stm << "inout";
		break;
	}
	return stm << " (" << val.name << ')';
}

Code& operator << (Code& stm, const Servant::ABI2ServantFactory& val)
{
	stm << TypePrefix (val.param);
	if (CodeGenBase::is_var_len (val.param))
		stm << "inout";
	else
		stm << "in";
	return stm << " (" << val.param.name () << ')';
}

Code& operator << (Code& stm, const Servant::CatchBlock&)
{
	return stm << unindent
		<< "} catch (Exception& e) {\n"
		<< indent
		<< "set_exception (_env, e);\n"
		<< unindent
		<< "} catch (...) {\n"
		<< indent
		<< "set_unknown_exception (_env);\n"
		<< unindent
		<< "}\n";
}

Code& operator << (Code& stm, const Servant::BaseImplPOA& itf)
{
	return stm << "InterfaceImpl <ServantPOA <" << QName (itf.itf) << ">, "
		<< QName (itf.itf) << ">::";
}

Code& operator << (Code& stm, const Servant::ConstructorParam& val)
{
	return stm << (CodeGenBase::is_var_len (val.param) ? "std::move (" : "std::ref (")
		<< val.param.name () << ')';
}

