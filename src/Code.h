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
#include <string_view>

// C++ code file output.
class Code : public BE::IndentedOut
{
	typedef BE::IndentedOut Base;
public:
	Code ();
	Code (const std::filesystem::path& file, const AST::Root& root);
	~Code ();

	void open (const std::filesystem::path& file, const AST::Root& root);
	void close ();

	void namespace_open (const AST::NamedItem& item);
	void namespace_open (const char* ns);
	void namespace_close ();

	typedef std::vector <std::string_view> Namespaces;

	const Namespaces& cur_namespace () const
	{
		return cur_namespace_;
	}

	void namespace_prefix (const char* pref);
	void namespace_prefix (const AST::NamedItem* mod);

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
	void namespace_open (const Namespaces& ns);
	void namespace_prefix (const Namespaces& ns);
	static void get_namespace (const AST::NamedItem* item, Namespaces& ns);
	static void get_namespace (const char* s, Namespaces& ns);

private:
	Namespaces cur_namespace_;
	std::filesystem::path file_;
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
