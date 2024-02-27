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
#ifndef NIDL2CPP_SERVANT_H_
#define NIDL2CPP_SERVANT_H_
#pragma once

#include "CodeGenBase.h"
#include "Header.h"
#include <unordered_set>

// Servant code generator
class Servant : public CodeGenBase
{
public:
	Servant (const Compiler& compiler, const AST::Root& root,
		const std::filesystem::path& file, const std::filesystem::path& client) :
		CodeGenBase (compiler),
		h_ (file, root),
		attributes_by_ref_ (false)
	{
		h_ << "#include " << client.filename () << std::endl << std::endl;
	}

	struct ABI2Servant
	{
		ABI2Servant (const AST::Type& t, const AST::Identifier& n,
			AST::Parameter::Attribute a = AST::Parameter::Attribute::IN) :
			type (t),
			name (n),
			att (a)
		{}

		ABI2Servant (const AST::Parameter& p) :
			type (p),
			name (p.name ()),
			att (p.attribute ())
		{}

		const AST::Type& type;
		const AST::Identifier& name;
		const AST::Parameter::Attribute att;
	};

	struct ABI2ServantFactory
	{
		ABI2ServantFactory (const AST::Parameter& p) :
			param (p)
		{}

		const AST::Parameter& param;
	};

	struct ConstructorParam
	{
		ConstructorParam (const AST::Parameter& p) :
			param (p)
		{}

		const AST::Parameter& param;
	};

	struct CatchBlock
	{};

	struct BaseImplPOA
	{
		BaseImplPOA (const AST::NamedItem& i) :
			itf (i)
		{}

		const AST::NamedItem& itf;
	};

protected:
	virtual void end (const AST::Root&) override;
	virtual void leaf (const AST::Include& item) override;
	virtual void begin (const AST::Interface& itf) override;
	virtual void end (const AST::Interface& itf) override;
	virtual void begin (const AST::ValueType& vt) override;
	virtual void end (const AST::ValueType& vt) override;
	virtual void leaf (const AST::Operation& op) override;
	virtual void leaf (const AST::Attribute& att) override;
	virtual void leaf (const AST::StateMember& sm) override;
	virtual void leaf (const AST::Constant& c) override;

private:
	void skeleton_begin (const AST::IV_Base& item, const char* suffix = "");
	void skeleton_end (const AST::IV_Base& item, const char* suffix = "");
	
	void epv (bool val_with_concrete_itf = false);
	
	void implementation_suffix (const AST::InterfaceKind ik);
	void implementation_parameters (const AST::Interface& primary, const AST::Interfaces& bases);
	void attribute (const AST::Member& m);

	static const AST::NamedItem* find_item (const AST::Interface& itf, const AST::Identifier& name, AST::Item::Kind kind);

	void skeleton_ami (const AST::Interface& itf);

	typedef std::unordered_set <const AST::Operation*> OperationSet;

	void virtual_operations (const AST::IV_Base& item, const OperationSet* implement_base = nullptr);

	void value_constructors (const char* class_name, const StateMembers& all_members);

	// Component flags
	typedef unsigned ComponentFlags;
	static const ComponentFlags CCM_FACETS = 0x01;
	static const ComponentFlags CCM_RECEPTACLES = 0x02;

	struct Receptacle {
		std::string name;
		const AST::NamedItem* conn_type;
		const AST::Type* multi_connections; // nullptr for single receptacle
	};

	struct Ports
	{
		std::vector <std::string> facets;
		std::vector <Receptacle> receptacles;
	};

	static bool collect_ports (const AST::Interface& itf, Ports& ports, OperationSet* implement_operations);

	static const AST::Operation* find_operation (const AST::Interface& itf, const AST::Identifier& name);

	ComponentFlags define_component (const AST::Interface& itf, OperationSet& implement_operations);

	static bool component_base (const AST::Interface& itf, OperationSet& implement_operations);

private:
	Header h_;
	std::vector <std::string> epv_;
	bool attributes_by_ref_;
};

Code& operator << (Code& stm, const Servant::ABI2Servant& val);
Code& operator << (Code& stm, const Servant::ABI2ServantFactory& val);
Code& operator << (Code& stm, const Servant::CatchBlock&);
Code& operator << (Code& stm, const Servant::BaseImplPOA& itf);
Code& operator << (Code& stm, const Servant::ConstructorParam& val);

#endif
