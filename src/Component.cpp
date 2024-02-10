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

bool Servant::collect_ports (const AST::Interface& itf, Ports& ports)
{
	if (!is_component (itf))
		return false;

	for (const auto item : itf) {
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

Servant::ComponentType Servant::define_component (const AST::Interface& itf)
{
	Ports ports;
	if (collect_ports (itf, ports)) {

		// Generate CCMObjectImpl

		h_ << empty_line
			<< "template <class S>\n"
			"class CCMObjectImpl <S, " << QName (itf) << "> : public CCMObjectImplBase\n"
			"{\n"
			"public:\n"
			<< indent;

		if (!ports.facets.empty ()) {
			h_ << "Type <Object>::VRet provide_facet (const ::Components::FeatureName & name)\n"
				"{\n"
				<< indent;

			for (const auto& name : ports.facets) {
				h_ << "if (name == \"" << name << "\")\n"
					<< indent
					<< "return static_cast <S&> (*this).provide_" << name << " ();\n"
					<< unindent
					<< "else ";
			}

			h_ << indent
				<< "\n"
				"return CCMObjectImplBase::provide_facet (name);\n"
				<< unindent
				<< unindent << "}\n\n";
		}

		if (!ports.receptacles.empty ()) {
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
				"ret = CCMObjectImplBase::connect (name, connection);\n"
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

				h_ << "return static_cast <S&> (*this).disconnect_" << receptacle.name;

				if (receptacle.multi_connections)
					h_ << " (ck);\n";
				else
					h_ << " ();\n";

				h_ << unindent
					<< "} else ";
			}

			h_ << indent
				<< "\n"
				"return CCMObjectImplBase::disconnect (name, ck);\n"
				<< unindent
				<< unindent << "}\n\n";

			/* Not used in CCM_LW
			h_ <<	"::Components::ConnectionDescriptions get_connections (const ::Components::FeatureName& name)\n"
				"{\n"
				<< indent
				<< "::Components::ConnectionDescriptions ret;\n";
			for (const auto& receptacle : receptacles) {
				h_ << "if (name == \"" << receptacle.name << "\")\n"
					<< indent
					<< receptacle.name << "_.get_connections (ret);\n";
				h_ << unindent
					<< "else ";
			}

			h_ << indent
				<< "\n"
				"return CCMObjectImplBase::get_connections (name);\n"
				<< unindent
				<< "return ret;\n"
				<< unindent << "}\n";
				*/

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
						<< receptacle.name << "_.get_connections (reinterpret_cast <ConnectionsBase&> (ret));\n"
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
					h_ << "Receptacles <";
				else
					h_ << "Receptacle <";
				h_ << QName (*receptacle.conn_type) << "> receptacle_"
					<< receptacle.name << "_;\n";
			}
		}

		h_ << unindent << "};\n";

		return ports.receptacles.empty () ? ComponentType::COMPONENT : ComponentType::COMPONENT_WITH_CONNECTIONS;
	} else
		return ComponentType::NOT_COMPONENT;
}
