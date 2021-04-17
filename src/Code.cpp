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

Code::Code () :
	module_namespace_ (nullptr),
	spec_namespace_ (nullptr)
{}

Code::Code (const path& file, const Root& root) :
	module_namespace_ (nullptr),
	spec_namespace_ (nullptr)
{
	open (file, root);
}

void Code::open (const std::filesystem::path& file, const Root& root)
{
	Base::open (file);
	*this << "// This file was generated from " << root.file ().filename () << endl;
	*this << "// Nirvana IDL compiler version 1.0\n";
}

void Code::close ()
{
	namespace_close ();
	Base::close ();
}

void Code::namespace_open (const NamedItem& item)
{
	if (spec_namespace_)
		namespace_close ();
	auto p = item.parent ();
	while (p && p->kind () != Item::Kind::MODULE)
		p = p->parent ();

	if (p != module_namespace_) {
		Namespaces cur_namespaces, req_namespaces;
		get_namespaces (module_namespace_, cur_namespaces);
		get_namespaces (p, req_namespaces);
		Namespaces::const_iterator cur = cur_namespaces.begin (), req = req_namespaces.begin ();
		for (; cur != cur_namespaces.end () && req != req_namespaces.end (); ++cur, ++req) {
			if (*cur != *req)
				break;
		}
		empty_line ();
		for (; cur != cur_namespaces.end (); ++cur) {
			*this << "}\n";
		}
		empty_line ();
		for (; req != req_namespaces.end (); ++req) {
			*this << "namespace " << (*req)->name () << " {\n";
		}
		module_namespace_ = p;
		*this << endl;
	}
}

void Code::get_namespaces (const NamedItem* item, Namespaces& namespaces)
{
	if (item) {
		get_namespaces (item->parent (), namespaces);
		namespaces.push_back (item);
	}
}

void Code::namespace_open (const char* spec_ns)
{
	if (module_namespace_)
		namespace_close ();
	if (spec_ns != spec_namespace_) {
		empty_line ();
		*this << spec_ns << endl;
		spec_namespace_ = spec_ns;
	}
}

void Code::namespace_close ()
{
	if (module_namespace_) {
		do {
			empty_line ();
			*this << "}\n";
			module_namespace_ = static_cast <const Module*> (module_namespace_->parent ());
		} while (module_namespace_);
		*this << endl;
	} else if (spec_namespace_) {
		empty_line ();
		for (const char* s = spec_namespace_, *open; (open = strchr (s, '{')); s = open + 1) {
			*this << "}\n";
		}
		spec_namespace_ = nullptr;
		*this << endl;
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
		"Wchar",
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
			if (stm.spec_namespace () != CodeGenBase::internal_namespace_)
				stm << "::CORBA::";
			stm << basic_types [(size_t)t.basic_type ()];
			break;
		case Type::Kind::NAMED_TYPE:
			stm << CodeGenBase::QName (t.named_type ());
			break;
		case Type::Kind::STRING:
			if (stm.spec_namespace () != CodeGenBase::internal_namespace_)
				stm << "::CORBA::Nirvana::";
			if (t.string_bound ())
				stm << "BoundedString <" << t.string_bound () << '>';
			else
				stm << "String";
			break;
		case Type::Kind::WSTRING:
			if (stm.spec_namespace () != CodeGenBase::internal_namespace_)
				stm << "::CORBA::Nirvana::";
			if (t.string_bound ())
				stm << "BoundedWString <" << t.string_bound () << '>';
			else
				stm << "WString";
			break;
		case Type::Kind::FIXED:
			if (stm.spec_namespace () != CodeGenBase::internal_namespace_)
				stm << "::CORBA::Nirvana::";
			stm << "Fixed <" << t.fixed_digits () << ", " << t.fixed_scale () << '>';
			break;
		case Type::Kind::SEQUENCE: {
			const Sequence& seq = t.sequence ();
			if (stm.spec_namespace () != CodeGenBase::internal_namespace_)
				stm << "::CORBA::Nirvana::";
			if (seq.bound ())
				stm << "BoundedSequence <" << static_cast <const Type&> (t.sequence ()) << ", " << seq.bound ();
			else
				stm << "Sequence <" << static_cast <const Type&> (t.sequence ());
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
			stm << CodeGenBase::QName (var.as_constant ());
			break;

		default:
			stm << var.to_string ();
	}

	return stm;
}
