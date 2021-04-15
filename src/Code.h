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
#ifndef NIDL2CPP_CODE_H_
#define NIDL2CPP_CODE_H_

#include <BE/IndentedOut.h>

// C++ code file output.
class Code : public BE::IndentedOut
{
	typedef BE::IndentedOut Base;
public:
	Code ();
	Code (const std::filesystem::path& file, const AST::Root& root);

	void open (const std::filesystem::path& file, const AST::Root& root);
	void close ();

	void namespace_open (const AST::NamedItem& item);
	void namespace_open (const char* spec_ns);
	void namespace_close ();

	const AST::NamedItem* module_namespace () const
	{
		return module_namespace_;
	}

	const char* spec_namespace () const
	{
		return spec_namespace_;
	}

	bool no_namespace () const
	{
		return !module_namespace_ && !spec_namespace_;
	}

	Code& operator << (short val)
	{
		Base::operator << (val);
		return *this;
	}

	Code& operator << (unsigned short val)
	{
		Base::operator << (val);
		return *this;
	}

	Code& operator << (int val)
	{
		Base::operator << (val);
		return *this;
	}

	Code& operator << (unsigned int val)
	{
		Base::operator << (val);
		return *this;
	}

	Code& operator << (long val)
	{
		Base::operator << (val);
		return *this;
	}

	Code& operator << (unsigned long val)
	{
		Base::operator << (val);
		return *this;
	}

	Code& operator << (unsigned long long val)
	{
		Base::operator << (val);
		return *this;
	}

	Code& operator << (long long val)
	{
		Base::operator << (val);
		return *this;
	}

	Code& operator << (std::ios_base& (*func)(std::ios_base&))
	{
		Base::operator << (func);
		return *this;
	}

	Code& operator << (std::basic_ios <char, std::char_traits <char> >& (*func)(std::basic_ios <char, std::char_traits <char> >&))
	{
		Base::operator << (func);
		return *this;
	}

	Code& operator << (std::ostream& (*func)(std::ostream&))
	{
		Base::operator << (func);
		return *this;
	}

	void check_digraph (char c);

private:
	typedef std::vector <const AST::NamedItem*> Namespaces;
	void get_namespaces (const AST::NamedItem* item, Namespaces& namespaces);

private:
	const AST::NamedItem* module_namespace_;
	const char* spec_namespace_;
};

Code& operator << (Code& stm, char c);
Code& operator << (Code& stm, signed char c);
Code& operator << (Code& stm, unsigned char c);
Code& operator << (Code& stm, const char* s);

inline
Code& operator << (Code& stm, const std::string& s)
{
	static_cast <BE::IndentedOut&> (stm) << s;
	return stm;
}

Code& operator << (Code& stm, const AST::Identifier& id);
Code& operator << (Code& stm, const AST::Type& t);
Code& operator << (Code& stm, const AST::Variant& var);

#endif
