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
#include "Code.h"
#include "CodeGenBase.h"

using namespace std;
using namespace std::filesystem;
using namespace AST;

#ifdef _WIN32
#define DEVNULL ".\\nul"
#else
#define DEVNULL "/dev/null"
#endif

Code::Code ()
{}

Code::Code (const path& file, const Root& root)
{
	open (file, root);
}

Code::~Code ()
{
	if (is_open ()) {
		Base::close ();
		if (!file_.empty ())
			remove (file_);
	}
}

void Code::open (const std::filesystem::path& file, const Root& root)
{
	Base::open (file.empty () ? DEVNULL : file);
	cur_namespace_.clear ();
	file_ = file;
	*this << "// This file was generated from " << root.file ().filename () << endl;
	*this << "// Nirvana IDL compiler version 1.0\n";
}

void Code::close ()
{
	namespace_close ();
	Base::close ();
}

void Code::include_header (const std::filesystem::path& file_h)
{
	filesystem::path cpp_dir = file_.parent_path ();
	if (file_h.parent_path () == cpp_dir)
		*this << file_h.filename ();
	else
		*this << '"' << std::filesystem::relative (file_h, cpp_dir).generic_string () << '"';
	*this << std::endl;
}

void Code::namespace_open (const char* ns)
{
	Namespaces nsv;
	get_namespace (ns, nsv);
	namespace_open (nsv);
}

void Code::namespace_open (const NamedItem& item)
{
	Namespaces ns;
	get_namespace (item, ns);
	namespace_open (ns);
}

void Code::get_namespace (const NamedItem& item, Namespaces& ns)
{
	auto p = item.parent ();
	while (p && p->kind () != Item::Kind::MODULE)
		p = p->parent ();

	get_namespace (static_cast <const Module*> (p), ns);
}

void Code::get_namespace (const Module* mod, Namespaces& ns)
{
	if (mod) {
		assert (mod->kind () == Item::Kind::MODULE);
		get_namespace (static_cast <const Module*> (mod->parent ()), ns);
		ns.emplace_back (mod->name ());
	}
}

void Code::get_namespace (const char* s, Namespaces& ns)
{
	if (s && *s) {
		for (;;) {
			const char* end = strchr (s, '/');
			if (end) {
				ns.emplace_back (s, end - s);
				s = end + 1;
			} else {
				ns.emplace_back (s);
				break;
			}
		}
	}
}

void Code::namespace_close ()
{
	if (!cur_namespace_.empty ()) {
		empty_line ();
		for (size_t cnt = cur_namespace_.size (); cnt; --cnt) {
			*this << "}\n";
		}
		cur_namespace_.clear ();
	}
}

void Code::namespace_prefix (const AST::NamedItem& item)
{
	Namespaces ns;
	get_namespace (item, ns);
	namespace_prefix (ns);
}

void Code::namespace_prefix (const Module* mod)
{
	Namespaces ns;
	get_namespace (mod, ns);
	namespace_prefix (ns);
}

void Code::namespace_prefix (const char* ns)
{
	Namespaces nsv;
	get_namespace (ns, nsv);
	namespace_prefix (nsv);
}

void Code::namespace_open (const Namespaces& ns)
{
	Namespaces::const_iterator cur = cur_namespace_.begin (), req = ns.begin ();
	for (; cur != cur_namespace_.end () && req != ns.end (); ++cur, ++req) {
		if (*cur != *req)
			break;
	}
	if (cur != cur_namespace_.end () || req != ns.end ()) {
		empty_line ();
		for (size_t cnt = cur_namespace_.end () - cur; cnt; --cnt) {
			*this << "}\n";
		}
		cur_namespace_.erase (cur, cur_namespace_.end ());
		empty_line ();
		for (; req != ns.end (); ++req) {
			*this << "namespace " << *req << " {\n";
			cur_namespace_.push_back (*req);
		}
		*this << endl;
	}
}

void Code::namespace_prefix (const Namespaces& ns)
{
	Namespaces::const_iterator cur = cur_namespace_.begin (), req = ns.begin ();
	for (; cur != cur_namespace_.end () && req != ns.end (); ++cur, ++req) {
		if (*cur != *req)
			break;
	}
	if (req != ns.end ()) {
		if (!cur_namespace_.empty () && req == ns.begin ())
			*this << "::";
		for (; req != ns.end (); ++req) {
			*this << *req << "::";
		}
	}
}

void Code::check_digraph (char c)
{
	switch (c) {
		case '>':
			if (last_char () == '>')
				static_cast <Base&> (*this) << ' ';
			break;

		case ':':
			if (last_char () == '<')
				static_cast <Base&> (*this) << ' ';
			break;
	}
}

Code& operator << (Code& stm, char c)
{
	stm.check_digraph (c);
	static_cast <BE::IndentedOut&> (stm) << c;
	return stm;
}

Code& operator << (Code& stm, signed char c)
{
	stm.check_digraph (c);
	static_cast <BE::IndentedOut&> (stm) << c;
	return stm;
}

Code& operator << (Code& stm, unsigned char c)
{
	stm.check_digraph (c);
	static_cast <BE::IndentedOut&> (stm) << c;
	return stm;
}

Code& operator << (Code& stm, const char* s)
{
	stm.check_digraph (*s);
	static_cast <BE::IndentedOut&> (stm) << s;
	return stm;
}

Code& operator << (Code& stm, const Identifier& id)
{
	if (CodeGenBase::is_keyword (id))
		stm << CodeGenBase::protected_prefix_;
	stm << static_cast <const string&> (id);
	return stm;
}

Code& operator << (Code& stm, const Type& t)
{
	static const char* const basic_types [(size_t)BasicType::ANY + 1] = {
		"Boolean",
		"Octet",
		"Char",
		"WChar",
		"UShort",
		"ULong",
		"ULongLong",
		"Short",
		"Long",
		"LongLong",
		"Float",
		"Double",
		"LongDouble",
		"Object",
		"ValueBase",
		"Any"
	};

	switch (t.tkind ()) {
		case Type::Kind::VOID:
			stm << "void";
			break;
		case Type::Kind::BASIC_TYPE:
			stm.namespace_prefix ("CORBA");
			stm << basic_types [(size_t)t.basic_type ()];
			break;
		case Type::Kind::NAMED_TYPE:
			stm << QName (t.named_type ());
			break;
		case Type::Kind::STRING:
			stm.namespace_prefix ("CORBA/Internal");
			if (t.string_bound ())
				stm << "BoundedString <" << t.string_bound () << '>';
			else
				stm << "String";
			break;
		case Type::Kind::WSTRING:
			stm.namespace_prefix ("CORBA/Internal");
			if (t.string_bound ())
				stm << "BoundedWString <" << t.string_bound () << '>';
			else
				stm << "WString";
			break;
		case Type::Kind::FIXED:
			stm.namespace_prefix ("CORBA/Internal");
			stm << "Fixed <" << t.fixed_digits () << ", " << t.fixed_scale () << '>';
			break;
		case Type::Kind::SEQUENCE: {
			const Sequence& seq = t.sequence ();
			stm.namespace_prefix ("CORBA/Internal");
			if (seq.bound ())
				stm << "Bounded";
			stm << "Sequence <";
			if (CodeGenBase::is_ref_type (seq))
				stm << TypePrefix (seq) << "Var";
			else
				stm << static_cast <const Type&> (seq);
			if (seq.bound ())
				stm << ", " << seq.bound ();
			stm << '>';
		} break;
		case Type::Kind::ARRAY: {
			const Array& arr = t.array ();
			for (size_t cnt = arr.dimensions ().size (); cnt; --cnt) {
				stm << "std::array <";
			}
			stm << static_cast <const Type&> (arr);
			for (auto dim = arr.dimensions ().rbegin (); dim != arr.dimensions ().rend (); ++dim) {
				stm << ", " << *dim << '>';
			}
		} break;
		default:
			assert (false);
	}

	return stm;
}

Code& operator << (Code& stm, const Variant& var)
{
	switch (var.vtype ()) {

		case Variant::VT::FIXED:
			stm << '"' << var.to_string () << '"';
			break;

		case Variant::VT::ENUM_ITEM: {
			const EnumItem& item = var.as_enum_item ();
			ScopedName sn = item.scoped_name ();
			sn.insert (sn.begin () + sn.size () - 1, item.enum_type ().name ());
			stm << sn.stringize ();
		} break;

		case Variant::VT::CONSTANT:
			stm << QName (var.as_constant ());
			break;

		default:
			stm << var.to_string ();
	}

	return stm;
}
