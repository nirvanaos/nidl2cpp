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

using namespace AST;

bool Servant::collect_ports (const AST::Interface& itf, Ports& ports, OperationSet* implement_operations)
{
	if (!is_component (itf))
		return false;

	for (const auto& item : itf) {
		if (item->kind () == Item::Kind::OPERATION) {
			const Operation& op = static_cast <const Operation&> (*item);
			if (op.empty ()) {
				if (is_object (op)) {
					std::string facet = skip_prefix (op.name (), "provide_");
					if (!facet.empty ())
						ports.facets.push_back (std::move (facet));
				}
			} else if (op.size () == 1) {
				const Type& conn_itf_type = op.front ()->dereference_type ();
				if (is_object (conn_itf_type)) {
					std::string receptacle = skip_prefix (op.name (), "connect_");
					if (!receptacle.empty ()) {
						const Operation* disconnect = find_operation (itf, "disconnect_" + receptacle);
						if (disconnect && static_cast <const Type&> (conn_itf_type) == *disconnect) {
							if (op.tkind () == Type::Kind::VOID) {
								// Simple receptacle
								// disconnect_ must not have params
								if (disconnect->empty ()) {
									const Operation* get_connection = find_operation (itf, "get_connection_" + receptacle);
									if (get_connection && static_cast <const Type&> (conn_itf_type) == *get_connection
										&& get_connection->empty ()) {
										ports.receptacles.push_back ({ std::move (receptacle), &conn_itf_type.named_type (), nullptr });

										if (implement_operations) {
											implement_operations->insert (&op);
											implement_operations->insert (disconnect);
											implement_operations->insert (get_connection);
										}
									}
								}
							} else if (is_named_type (op, "::Components::Cookie")) {
								// Multi receptacle
								// disconnect_ must have 1 param of "::Components::Cookie" type
								if (disconnect->size () == 1 && is_named_type (*disconnect->front (), "::Components::Cookie")) {
									const Operation* get_connections = find_operation (itf, "get_connections_" + receptacle);
									if (get_connections && get_connections->empty ()) {
										const Type& get_connections_ret = get_connections->dereference_type ();
										if (get_connections_ret.tkind () == Type::Kind::SEQUENCE) {
											const Type& connection_type = get_connections_ret.sequence ().dereference_type ();
											if (connection_type.tkind () == Type::Kind::NAMED_TYPE) {
												const NamedItem& cs = connection_type.named_type ();
												if (cs.kind () == Item::Kind::STRUCT) {
													const Struct& connection_struct = static_cast <const Struct&> (cs);
													if (connection_struct.size () == 2
														&& static_cast <const Type&> (conn_itf_type) == *connection_struct [0]
														&& is_named_type (*connection_struct [1], "::Components::Cookie")
														) {
														ports.receptacles.push_back ({ std::move (receptacle), &conn_itf_type.named_type (), &get_connections_ret });

														if (implement_operations) {
															implement_operations->insert (&op);
															implement_operations->insert (disconnect);
															implement_operations->insert (get_connections);
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	return true;
}

bool Servant::component_base (const AST::Interface& itf, OperationSet& implement_operations)
{
	// Operations implemented by the CCM_ObjectBase
	static const char* const object_base_ops [] = {
		"get_ccm_home",
		"configuration_complete",
		"remove",
		nullptr
	};

	// Operations implemented by the CCM_NavigationBase
	static const char* const navigation_base_ops [] = {
		"provide_facet",
		nullptr
	};

	// Operations implemented by the CCM_ReceptaclesBase
	static const char* const receptacles_base_ops [] = {
		"connect",
		"disconnect",
		nullptr
	};

	static const struct BaseOp {
		const char* qname;
		const char* const* ops;
	} base_ops [] = {
		{ "::Components::CCMObject", object_base_ops },
		{ "::Components::Receptacles", receptacles_base_ops },
		{ "::Components::Navigation", navigation_base_ops }
	};

	const BaseOp* p = nullptr;
	{
		auto qn = itf.qualified_name ();
		for (const auto& b : base_ops) {
			if (qn == b.qname) {
				p = &b;
				break;
			}
		}
	}

	if (p) {
		for (const char* const* it = p->ops;; ++it ) {
			const char* name = *it;
			if (!name)
				break;
			const Operation* op = find_operation (itf, name);
			assert (op);
			implement_operations.insert (op);
		}

		return true;
	}

	return false;
}

Servant::ComponentFlags Servant::define_component (const AST::Interface& itf, OperationSet& implement_operations)
{
	if (component_base (itf, implement_operations))
		return 0;

	Ports ports;
	if (collect_ports (itf, ports, &implement_operations)) {

		ComponentFlags flags = 0;

		// CCM_ObjectConnections
		bool connections = false;
		if (!ports.receptacles.empty ()) {

			connections = true;
			flags |= CCM_RECEPTACLES;

			h_ << empty_line
				<< "template <>\n"
				"class CCM_ObjectConnections <" << QName (itf) << ">\n"
				"{\n"
				"public:\n"
				<< indent;

			for (const auto& receptacle : ports.receptacles) {
				if (receptacle.multi_connections)
					h_ << "Type < ::Components::Cookie>::VRet";
				else
					h_ << "void ";
				h_ << " connect_" << receptacle.name << " (I_ptr <" << QName (*receptacle.conn_type)
					<< "> connection)\n"
					"{\n" << indent;
				if (receptacle.multi_connections)
					h_ << "return ";
				h_ << "receptacle_" << receptacle.name << "_.connect (connection);\n"
					<< unindent << "}\n"

					<< "Type <" << QName (*receptacle.conn_type) << ">::VRet disconnect_" << receptacle.name << " (";
				if (receptacle.multi_connections)
					h_ << "::Components::Cookie::_ptr_type ck";
				h_ << ")\n"
					"{\n" << indent
					<< "return receptacle_" << receptacle.name << "_.disconnect (";
				if (receptacle.multi_connections)
					h_ << "ck";
				h_ << ");\n"
					<< unindent << "}\n\n";

				if (receptacle.multi_connections) {
					h_ << *receptacle.multi_connections << " get_connections_" << receptacle.name << " ()\n"
						"{\n" << indent
						<< *receptacle.multi_connections << " ret;\n"
						"receptacle_"
						<< receptacle.name << "_.get_connections (reinterpret_cast <Connections&> (ret));\n"
						"return ret;\n"
						<< unindent << "}\n";
				} else {
					h_ << "Type <" << QName (*receptacle.conn_type) << ">::VRet get_connection_" << receptacle.name
						<< " ()\n"
						"{\n" << indent
						<< "return receptacle_" << receptacle.name << "_.get_connection ();\n"
						<< unindent << "}\n";
				}
				h_ << std::endl;
			}

			for (const auto& receptacle : ports.receptacles) {
				if (receptacle.multi_connections)
					h_ << "const std::vector <I_ref <" << QName (*receptacle.conn_type) << "> >& receptacle_";
				else
					h_ << "I_ptr <" << QName (*receptacle.conn_type) << "> receptacle_";
				h_ << receptacle.name << " () const noexcept\n"
					"{\n" << indent
					<< "return receptacle_" << receptacle.name << "_;\n"
					<< unindent << "}\n";
			}

			h_ << unindent << "\nprivate:\n" << indent;

			for (const auto& receptacle : ports.receptacles) {
				if (receptacle.multi_connections)
					h_ << "ReceptacleMultiplex <";
				else
					h_ << "ReceptacleSimplex <";
				h_ << QName (*receptacle.conn_type) << "> receptacle_"
					<< receptacle.name << "_;\n";
			}

			h_ << unindent << "};\n\n";
		}

		// CCM_FACETS flag is set when:
		// 1. Interface has facets.
		// or
		// 2. More than one of bases have facet.
		if (!ports.facets.empty ())
			flags |= CCM_FACETS;

		// Collect ports from all bases
		{
			Interfaces bases = itf.get_all_bases ();
			for (const auto b : bases) {
				size_t sz_fac = ports.facets.size ();
				size_t sz_rec = ports.receptacles.size ();
				collect_ports (*b, ports, nullptr);
				if (sz_fac && ports.facets.size () > sz_fac)
					flags |= CCM_FACETS;
				if (sz_rec && ports.receptacles.size () > sz_rec)
					flags |= CCM_RECEPTACLES;
			}
		}

		// Generate CCM_ObjectFeatures
		if (flags) {
			h_ << empty_line
				<< "template <class S>\n"
				"class CCM_ObjectFeatures <S, " << QName (itf) << ">\n"
				"{\n"
				"public:\n"
				<< indent;

			if (flags & CCM_FACETS) {
				h_ << "Type <Object>::VRet provide_facet (const ::Components::FeatureName & name)\n"
					"{\n"
					<< indent
					<< "Type <Object>::Var ret;\n";

				for (const auto& name : ports.facets) {
					h_ << "if (name == \"" << name << "\")\n"
						<< indent
						<< "ret = interface2object (static_cast <S&> (*this).provide_" << name << " ());\n"
						<< unindent
						<< "else ";
				}

				h_ << indent
					<< "\n"
					"ret = CCM_NavigationBase::provide_facet (name);\n"
					<< unindent;
				if (options ().legacy) {
					h_ << "#ifdef LEGACY_CORBA_CPP\n"
						"return ret._retn ();\n"
						"#else\n";
				}

				h_ << "return ret;\n";

				if (options ().legacy)
					h_ << "#endif\n";

				h_ << unindent << "}\n\n";
			}

			if (flags & CCM_RECEPTACLES) {
				h_ << "Type < ::Components::Cookie>::VRet connect (const ::Components::FeatureName& name, "
					"Object::_ptr_type connection)\n"
					"{\n"
					<< indent
					<< "Type < ::Components::Cookie>::Var ret;\n";

				for (const auto& receptacle : ports.receptacles) {
					h_ << "if (name == \"" << receptacle.name << "\")\n"
						<< indent;

					if (receptacle.multi_connections)
						h_ << "ret = ";

					h_ << "static_cast <S&> (*this).connect_" << receptacle.name << " ("
						<< QName (*receptacle.conn_type) << "::_narrow (connection));\n"
						<< unindent
						<< "else ";
				}

				h_ << indent
					<< "\n"
					"ret = CCM_ReceptaclesBase::connect (name, connection);\n"
					<< unindent;

				if (options ().legacy) {
					h_ << "#ifdef LEGACY_CORBA_CPP\n"
						"return ret._retn ();\n"
						"#else\n";
				}

				h_ << "return ret;\n";

				if (options ().legacy)
					h_ << "#endif\n";

				h_ << unindent << "}\n"

					"Type <Object>::VRet disconnect (const ::Components::FeatureName& name, ::Components::Cookie::_ptr_type ck)\n"
					"{\n"
					<< indent;

				for (const auto& receptacle : ports.receptacles) {
					h_ << "if (name == \"" << receptacle.name << "\") {\n"
						<< indent;

					if (receptacle.multi_connections)
						h_ << "if (!ck)\n"
						<< indent
						<< "throw Components::CookieRequired ();\n"
						<< unindent;

					h_ << "return interface2object (static_cast <S&> (*this).disconnect_" << receptacle.name;

					if (receptacle.multi_connections)
						h_ << " (ck));\n";
					else
						h_ << " ());\n";

					h_ << unindent
						<< "} else ";
				}

				h_ << indent
					<< "\n"
					"return CCM_ReceptaclesBase::disconnect (name, ck);\n"
					<< unindent
					<< unindent << "}\n\n";
			}

			h_ << unindent << "};\n\n"

				// InterfaceImpl

				"template <class S>\n"
				"class InterfaceImpl <S, " << QName (itf) << "> :\n"
				<< indent
				<< "public InterfaceImplBase <S, " << QName (itf) << '>';
			if (connections)
				h_ << ",\n"
				"public CCM_ObjectConnections <" << QName (itf) << '>';
			h_ << ",\n"
				"public CCM_ObjectFeatures <S, " << QName (itf) << ">\n"
				<< unindent
				<< "{};\n";
		}

		return flags;
	} else
		return 0;
}
