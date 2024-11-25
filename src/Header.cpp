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
#include "Header.h"
#include <sstream>

using std::filesystem::path;

inline
std::string Header::get_guard_macro (const path& file)
{
	std::string name = file.filename ().replace_extension ("").string ();
	std::string ext = file.extension ().string ().substr (1);
	to_upper (name);
	to_upper (ext);
	size_t dir_hash = std::hash <std::string> {} (
		std::filesystem::canonical (file).parent_path ().string ());
	std::ostringstream ss;
	ss <<  "IDL_"
		<< std::setfill ('0') << std::setw (sizeof (size_t) * 2) << std::hex << dir_hash
		<< '_' << name << '_' << ext << '_';
	return ss.str ();
}

void Header::to_upper (std::string& s)
{
	for (auto& c : s)
		c = toupper (c);
}

Header::Header (const path& file, const AST::Root& root) :
	Code (file, root)
{
	std::string guard = get_guard_macro (file);
	*this << "#ifndef " << guard << std::endl;
	*this << "#define " << guard << std::endl;
}

void Header::close ()
{
	namespace_close ();
	empty_line ();
	*this << "#endif\n";
	Code::close ();
}
