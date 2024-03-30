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

using std::filesystem::path;
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

void Code::open (const path& file, const Root& root)
{
	Base::open (file.empty () ? DEVNULL : file);
	cur_namespace_.clear ();
	file_ = file;
	*this << "// This file was generated from " << root.file ().filename () << std::endl;
	*this << "// " << Compiler::name_ << " version ";
	auto it = std::begin (Compiler::version_);
	*this << *it;
	for (++it; it != std::end (Compiler::version_); ++it) {
		*this << '.' << *it;
	}
	*this << std::endl;
}

void Code::close ()
{
	namespace_close ();
	Base::close ();
}

void Code::include_header (const path& file_h)
{
	path cpp_dir = file_.parent_path ();
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
		*this << std::endl;
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
	} else if (cur != cur_namespace_.end () && !ns.empty ())
		*this << ns.back () << "::";
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
	stm << static_cast <const std::string&> (id);
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
			stm.namespace_prefix ("IDL");
			if (t.string_bound ())
				stm << "BoundedString <" << t.string_bound () << '>';
			else
				stm << "String";
			break;
		case Type::Kind::WSTRING:
			stm.namespace_prefix ("IDL");
			if (t.string_bound ())
				stm << "BoundedWString <" << t.string_bound () << '>';
			else
				stm << "WString";
			break;
		case Type::Kind::FIXED:
			stm.namespace_prefix ("IDL");
			stm << "Fixed <" << t.fixed_digits () << ", " << t.fixed_scale () << '>';
			break;
		case Type::Kind::SEQUENCE: {
			const Sequence& seq = t.sequence ();
			stm.namespace_prefix ("IDL");
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
				stm << Namespace ("std") << "array <";
			}
			if (CodeGenBase::is_ref_type (arr))
				stm << TypePrefix (arr) << "Var";
			else
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

		case Variant::VT::FIXED: {
			std::vector <uint8_t> bcd = var.as_Fixed ().to_BCD ();
			stm << "{ ";
			stm << std::hex;
			auto f = stm.fill ('0');
			auto it = bcd.begin ();
			stm << "0x" << (unsigned)*(it++);
			while (it != bcd.end ()) {
				stm << ", 0x" << std::setw (2) << (unsigned)*(it++);
			}
			stm << std::dec;
			stm.fill (f);
			stm << " }";
		} break;

		case Variant::VT::STRING:
			stm << var.to_string ();
			break;

		case Variant::VT::WSTRING:
			stm << "L" << var.to_string ();
			break;

		case Variant::VT::ENUM_ITEM: {
			const EnumItem& item = var.as_enum_item ();
			stm << QName (item.enum_type ()) << "::" << item.name ();
		} break;

		case Variant::VT::CONSTANT:
			stm << QName (var.as_constant ());
			break;

		case Variant::VT::BOOLEAN:
			stm << (var.as_bool () ? "true" : "false");
			break;

		case Variant::VT::USHORT:
			stm << var.as_unsigned_short () << 'U';
			break;

		case Variant::VT::SHORT:
			stm << var.as_short ();
			break;

		case Variant::VT::ULONG:
			stm << var.as_unsigned_long () << "UL";
			break;

		case Variant::VT::LONG:
			stm << var.as_long () << 'L';
			break;

		case Variant::VT::ULONGLONG:
			stm << var.as_unsigned_long_long () << "ULL";
			break;

		case Variant::VT::LONGLONG:
			stm << var.as_long_long () << "LL";
			break;

		case Variant::VT::FLOAT:
			stm << var.as_float () << 'F';
			break;

		case Variant::VT::DOUBLE:
			stm << var.as_double ();
			break;

		case Variant::VT::LONGDOUBLE:
			stm << var.as_long_double () << 'L';
			break;

		default:
			stm << var.to_string ();
	}

	return stm;
}

Code& operator << (Code& stm, const Raises& raises)
{
	if (!raises.empty ()) {
		Raises::const_iterator it = raises.begin ();
		stm << QName (**it);
		for (++it; it != raises.end (); ++it) {
			stm << ", " << QName (**it);
		}
	}

	return stm;
}

Code& operator << (Code& stm, const QName& qn)
{
	return stm << ParentName (qn.item) << qn.item.name ();
}

Code& operator << (Code& stm, const ParentName& qn)
{
	const NamedItem* parent = qn.item.parent ();
	if (parent) {
		Item::Kind pk = parent->kind ();
		if (pk == Item::Kind::MODULE) {
			stm.namespace_prefix (static_cast <const Module*> (parent));
		} else {
			if (Item::Kind::INTERFACE == pk || Item::Kind::VALUE_TYPE == pk) {
				stm.namespace_prefix ("CORBA/Internal");
				stm << "Decls <" << QName (*parent) << '>';
			} else {
				stm << QName (*parent);
			}
			stm << "::";
		}
	} else if (!stm.cur_namespace ().empty ())
		stm << "::";
	return stm;
}

Code& operator << (Code& stm, const TypePrefix& t)
{
	// Use namespace IDL to avoid possible collisions with user-defined symbol `Type`.
	return stm << Namespace ("IDL") << "Type" << " <" << t.type << ">::";
}

Code& operator << (Code& stm, const ABI_ret& t)
{
	if (t.type.tkind () == Type::Kind::VOID)
		stm << "void";
	else if (CodeGenBase::is_ref_type (t.type))
		stm << "Interface*";
	else if (CodeGenBase::is_enum (t.type))
		stm << "ABI_enum";
	else
		stm << TypePrefix (t.type) << (t.byref ? "ABI_VT_ret" : "ABI_ret");
	return stm;
}

Code& operator << (Code& stm, const ABI_param& t)
{
	if (CodeGenBase::is_ref_type (t.type)) {
		switch (t.att) {
		case Parameter::Attribute::IN:
			stm << "Interface*";
			break;
		case Parameter::Attribute::OUT:
		case Parameter::Attribute::INOUT:
			stm << "Interface**";
			break;
		}
	} else if (CodeGenBase::is_enum (t.type)) {
		switch (t.att) {
		case Parameter::Attribute::IN:
			stm << "ABI_enum";
			break;
		case Parameter::Attribute::OUT:
		case Parameter::Attribute::INOUT:
			stm << "ABI_enum*";
			break;
		}
	} else {
		stm << TypePrefix (t.type);
		switch (t.att) {
		case Parameter::Attribute::IN:
			stm << "ABI_in";
			break;
		case Parameter::Attribute::OUT:
		case Parameter::Attribute::INOUT:
			stm << "ABI_out";
			break;
		}
	}
	return stm;
}

Code& operator << (Code& stm, const Var& t)
{
	assert (t.type.tkind () != Type::Kind::VOID);
	return stm << TypePrefix (t.type) << "Var";
}

Code& operator << (Code& stm, const VRet& t)
{
	if (t.type.tkind () != Type::Kind::VOID)
		return stm << TypePrefix (t.type) << "VRet";
	else
		return stm << "void";
}

Code& operator << (Code& stm, const ConstRef& t)
{
	assert (t.type.tkind () != Type::Kind::VOID);
	return stm << TypePrefix (t.type) << "ConstRef";
}

Code& operator << (Code& stm, const TC_Name& t)
{
	return stm << ParentName (t.item) << "_tc_" << static_cast <const std::string&> (t.item.name ());
}

Code& operator << (Code& stm, const ServantParam& t)
{
	if (t.att != Parameter::Attribute::IN || (t.factory && CodeGenBase::is_var_len (t.type)))
		stm << TypePrefix (t.type) << "Var&";
	else {
		if (t.virt && !CodeGenBase::is_ref_type (t.type)) {
			// For virtual methods (POA implementation), the type must not depend on
			// machine word size.
			// In parameters for all basic types, except for Any, are passed by value.
			// Other types are passed by reference.
			const Type& dt = t.type.dereference_type ();
			if (dt.tkind () == Type::Kind::BASIC_TYPE && dt.basic_type () != BasicType::ANY)
				stm << TypePrefix (t.type) << "Var";
			else {
				if (!t.factory || !CodeGenBase::is_var_len (t.type))
					stm << "const ";
				stm << TypePrefix (t.type) << "Var&";
			}
		} else
			stm << TypePrefix (t.type) << "ConstRef";
	}

	return stm;
}

Code& operator << (Code& stm, const ServantOp& op)
{
	stm << VRet (op.op) << ' ' << op.op.name () << " (";
	auto it = op.op.begin ();
	if (it != op.op.end ()) {
		stm << ServantParam (**it, op.virt) << ' ' << (*it)->name ();
		++it;
		for (; it != op.op.end (); ++it) {
			stm << ", " << ServantParam (**it, op.virt) << ' ' << (*it)->name ();
		}
	}
	return stm << ')';
}

Code& operator << (Code& stm, const MemberType& t)
{
	if (CodeGenBase::is_boolean (t.type))
		stm << TypePrefix (t.type) << "ABI";
	else if (CodeGenBase::is_ref_type (t.type))
		stm << Var (t.type);
	else
		stm << t.type;
	return stm;
}

Code& operator << (Code& stm, const MemberVariable& m)
{
	return stm << MemberType (m.member)
		<< " _" << static_cast <const std::string&> (m.member.name ()) << ";\n";
}

Code& operator << (Code& stm, const Accessors& a)
{
	// const getter
	stm << ConstRef (a.member) << ' ' << a.member.name () << " () const\n"
		"{\n" << indent <<
		"return _" << static_cast <const std::string&> (a.member.name ()) << ";\n"
		<< unindent << "}\n";

	// reference getter
	stm << MemberType (a.member)
		<< "& " << a.member.name () << " ()\n"
		"{\n" << indent <<
		"return _" << static_cast <const std::string&> (a.member.name ()) << ";\n"
		<< unindent << "}\n";

	// setter
	if (CodeGenBase::is_ref_type (a.member)) {
		stm << "void " << a.member.name () << " (" << Var (a.member) << " val)\n"
			"{\n" << indent <<
			'_' << static_cast <const std::string&> (a.member.name ()) << " = std::move (val);\n"
			<< unindent << "}\n";
	} else {
		stm << "void " << a.member.name () << " (" << ConstRef (a.member) << " val)\n"
			"{\n" << indent <<
			'_' << static_cast <const std::string&> (a.member.name ()) << " = val;\n"
			<< unindent << "}\n";

		if (CodeGenBase::is_var_len (a.member)) {
			// The move setter
			stm << "void " << a.member.name () << " (" << Var (a.member) << "&& val)\n"
				"{\n" << indent <<
				'_' << static_cast <const std::string&> (a.member.name ()) << " = std::move (val);\n"
				<< unindent << "}\n";
		}
	}

	return stm;
}

Code& operator << (Code& stm, const MemberDefault& m)
{
	stm << m.prefix << m.member.name () << " (";
	const Type& td = m.member.dereference_type ();
	switch (td.tkind ()) {

	case Type::Kind::BASIC_TYPE:
		if (td.basic_type () == BasicType::BOOLEAN)
			stm << "false";
		else if (td.basic_type () < BasicType::OBJECT)
			stm << "0";
		break;

	case Type::Kind::NAMED_TYPE: {
		const NamedItem& nt = td.named_type ();
		if (nt.kind () == Item::Kind::ENUM) {
			const Enum& en = static_cast <const Enum&> (nt);
			stm << QName (en) << "::" << en.front ()->name ();
		}
	} break;
	}
	return stm << ')';
}

Code& operator << (Code& stm, const MemberInit& m)
{
	return stm << m.prefix << m.member.name () << " (std::move ("
		<< m.member.name () << "))";
}

Code& operator << (Code& stm, const AMI_ParametersABI& op)
{
	for (auto param : op.op) {
		if (param->attribute () != Parameter::Attribute::OUT)
			stm << ", " << ABI_param (*param, Parameter::Attribute::IN) << ' ' << param->name ();
	}

	return stm << ", Interface* _env)";
}

Code& operator << (Code& stm, const TypeCodeName& it)
{
	stm.namespace_open ("CORBA/Internal");
	return stm << empty_line
		<< "template <>\n"
		"const Char TypeCodeName <" << QName (it.item) << ">::name_ [] = \"" << static_cast <const std::string&> (it.item.name ()) << "\";\n";
}

Code& operator << (Code& stm, const AST::StateMember& m)
{
	return
		stm << "{ \"" << static_cast <const std::string&> (m.name ()) << "\", Type <"
		<< static_cast <const Type&> (m) << ">::type_code, "
		<< (m.is_public () ? "PUBLIC_MEMBER" : "PRIVATE_MEMBER")
		<< " }";
}
