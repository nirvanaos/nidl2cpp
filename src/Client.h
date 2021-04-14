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
#ifndef NIDL2CPP_CLIENT_H_
#define NIDL2CPP_CLIENT_H_

#include "CodeGenBase.h"
#include "Header.h"

// Client code generator
class Client : public CodeGenBase
{
public:
	Client (const std::filesystem::path& file_h, const std::filesystem::path& file_cpp, const AST::Root& root) :
		h_ (file_h, root),
		cpp_ (file_cpp, root)
	{
		cpp_ << "#include <CORBA/CORBA.h>\n"
			"#include <Nirvana/OLF.h>\n"
			"#include " << file_h.filename () << std::endl;
	}

protected:
	virtual void end (const AST::Root&);

	virtual void leaf (const AST::Include& item);
	virtual void leaf (const AST::TypeDef& item);

	virtual void leaf (const AST::InterfaceDecl& item);
	virtual void begin (const AST::Interface& item);
	virtual void end (const AST::Interface& item);

	virtual void leaf (const AST::Constant& item);

	virtual void begin (const AST::Exception& item);
	virtual void end (const AST::Exception& item);

	virtual void leaf (const AST::StructDecl& item);
	virtual void begin (const AST::Struct& item);
	virtual void end (const AST::Struct& item);

	virtual void leaf (const AST::Enum& item);

private:
	void interface_forward (const AST::NamedItem& item, AST::InterfaceKind ik);
	void environment (const AST::Raises& raises);
	void type_code_decl (const AST::NamedItem& item);
	void type_code_def (const AST::NamedItem& item);
	static bool constant (Code& stm, const AST::Constant& item);
	void define_type (const AST::NamedItem& item, const Members& members, const char* suffix = "");
	std::ostream& member_type_prefix (const AST::Type& t);
	void struct_end (const AST::Identifier& name, const Members& members);
	static const char* default_value (const AST::Type& t);
	static bool nested (const AST::NamedItem& item);
	void h_namespace_open (const AST::NamedItem& item);

	void implement (const AST::Exception& item);
	void implement (const AST::Struct& item);
	void implement (const AST::Union& item);
	void implement (const AST::Enum& item);

private:
	Header h_; // .h file
	Code cpp_; // .cpp file.
};

#endif
