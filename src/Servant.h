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

#include "CodeGenBase.h"
#include "Header.h"

// Servant code generator
class Servant : public CodeGenBase
{
public:
	Servant (const std::filesystem::path& file, const std::filesystem::path& client, const AST::Root& root) :
		h_ (file, root)
	{
		h_ << "#include " << client.filename () << std::endl << std::endl;
	}

	struct S_param
	{
		S_param (const AST::Parameter& p) :
			type (p),
			att (p.attribute ())
		{}

		S_param (const AST::Attribute& t) :
			type (t),
			att (AST::Parameter::Attribute::IN)
		{}

		const AST::Type& type;
		const AST::Parameter::Attribute att;
	};

protected:
	virtual void end (const AST::Root&);
	virtual void begin (const AST::Interface& itf);
	virtual void leaf (const AST::Operation& op);
	virtual void leaf (const AST::Attribute& att);
	virtual void end (const AST::Interface& itf);

private:
	void servant_param (const AST::Parameter& param)
	{
		servant_param (param, param.name (), param.attribute ());
	}

	void servant_param (const AST::Type& t, const std::string& name, AST::Parameter::Attribute att = AST::Parameter::Attribute::IN);

	void catch_block ();

	void implementation_suffix (const AST::InterfaceKind ik);
	void implementation_parameters (const AST::Interface& primary, const AST::Interfaces& bases);

private:
	Header h_;
	std::vector<std::string> epv_;
};

Code& operator << (Code& stm, const Servant::S_param& t);

#endif
