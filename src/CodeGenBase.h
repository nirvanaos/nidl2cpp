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
#ifndef NIDL2CPP_CODEGENBASE_H_
#define NIDL2CPP_CODEGENBASE_H_
#pragma once

#include "Compiler.h"
#include "Code.h"
#include "Compiler.h"
#include <BE/MessageOut.h>
#include <unordered_set>

#define FACTORY_SUFFIX "_factory"
#define EXCEPTION_SUFFIX "::_Data"

#define SKELETON_FUNC_PREFIX "_s_"
#define SKELETON_GETTER_PREFIX "_s__get_"
#define SKELETON_SETTER_PREFIX "_s__set_"
#define SKELETON_MOVE_PREFIX "_s__move_"

#define AMI_TIMEOUT "ami_timeout"
#define AMI_RETURN_VAL "ami_return_val"
#define AMI_HANDLER "ami_handler"
#define AMI_POLLER_SUFFIX "Poller"
#define AMI_HANDLER_SUFFIX "Handler"
#define AMI_EXCEP "_excep"
#define AMI_SENDC "sendc_"
#define AMI_SENDP "sendp_"
#define POLLER_TYPE_PREFIX "Type <CORBA::ValueBase>::"

class CodeGenBase :
	public AST::CodeGen,
	public BE::MessageOut
{
public:
	static bool is_keyword (const AST::Identifier& id);
	inline static const char protected_prefix_ [] = "_cxx_";

	const Options& options () const noexcept
	{
		return compiler_;
	}

	const Compiler& compiler () const noexcept
	{
		return compiler_;
	}

	typedef AST::ContainerT <AST::Member> Members;
	typedef AST::ContainerT <AST::UnionElement> UnionElements;

	typedef std::vector <const AST::StateMember*> StateMembers;
	static StateMembers get_members (const AST::ValueType& cont);

	// Value and abstract bases of value base
	typedef std::vector <const AST::IV_Base*> Bases;
	static Bases get_all_bases (const AST::ValueType& vt);

	static const AST::Interface* get_concrete_supports (const AST::ValueType& vt);

	typedef std::vector <const AST::ValueFactory*> Factories;
	static Factories get_factories (const AST::ValueType& vt);
	static bool has_factories (const AST::ValueType& vt);

	static bool is_var_len (const AST::Type& type);
	static bool is_var_len (const Members& members);

	struct SizeAndAlignment
	{
		unsigned alignment; // Alignment of first element after a gap
		unsigned size;      // Size of data after the gap.

		SizeAndAlignment () :
			alignment (0),
			size (0)
		{}

		bool append (unsigned member_size) noexcept
		{
			return append (std::max (member_size, 8U), member_size);
		}

		bool append (unsigned member_align, unsigned member_size) noexcept;

		void invalidate () noexcept
		{
			size = 0;
		}

		bool is_valid () const noexcept
		{
			return size != 0;
		}
	};


	static bool is_CDR (const AST::Type& type, SizeAndAlignment& sa);
	static bool is_CDR (const Members& members, SizeAndAlignment& sa);

	static bool is_pseudo (const AST::NamedItem& item);
	static bool is_ref_type (const AST::Type& type);
	static bool is_complex_type(const AST::Type& type);
	static const AST::Enum* is_enum (const AST::Type& type);
	static bool is_native_interface (const AST::Type& type);
	static bool is_servant (const AST::Type& type);
	static bool is_native (const AST::Type& type);
	static bool is_native (const Members& members);
	static bool is_native (const AST::Raises& raises);
	static bool is_boolean (const AST::Type& type);
	static bool is_object (const AST::Type& type);
	static bool is_native_interface (const AST::NamedItem& type);
	static bool is_servant (const AST::NamedItem& type);

	static bool is_sequence (const AST::Type& type)
	{
		return type.dereference_type ().tkind () == AST::Type::Kind::SEQUENCE;
	}

	static bool is_bounded (const AST::Type& type);

	static bool is_named_type (const AST::Type& type, const char* qualified_name);

	void init_union (Code& stm, const AST::UnionElement& init_el, const char* prefix = "");

	static bool async_supported (const AST::Interface& itf) noexcept;

protected:
	CodeGenBase (const Compiler& compiler) :
		BE::MessageOut (compiler.err_out ()),
		compiler_ (compiler)
	{}

	virtual void leaf (const AST::Include& item) {}
	virtual void leaf (const AST::Native&) {}
	virtual void leaf (const AST::TypeDef& item) {}
	virtual void begin (const AST::ModuleItems&) {}
	virtual void end (const AST::ModuleItems&) {}
	virtual void leaf (const AST::Operation&) {}
	virtual void leaf (const AST::Attribute&) {}

	virtual void leaf (const AST::InterfaceDecl& item) {}
	virtual void end (const AST::Interface& item) {}

	virtual void leaf (const AST::Constant& item) {}
	virtual void leaf (const AST::Exception& item) {}
	virtual void leaf (const AST::StructDecl& item) {}
	virtual void leaf (const AST::Struct& item) {}

	virtual void leaf (const AST::Enum& item) {}

	virtual void leaf (const AST::UnionDecl&) {}
	virtual void leaf (const AST::Union&) {}

	virtual void leaf (const AST::ValueTypeDecl&) {}
	virtual void begin (const AST::ValueType&) {}
	virtual void end (const AST::ValueType&) {}

	virtual void leaf (const AST::StateMember&) {}
	virtual void leaf (const AST::ValueFactory&) {}
	virtual void leaf (const AST::ValueBox&) {}

	static void marshal_members (Code& stm, const Members& members, const char* func, const char* prefix);
	static void marshal_member (Code& stm, const AST::Member& m, const char* func, const char* prefix);
	static void unmarshal_members (Code& stm, const Members& members, const char* prefix);
	static void unmarshal_member (Code& stm, const AST::Member& m, const char* prefix);

	static bool is_special_base (const AST::Interface& itf) noexcept;
	static bool is_immutable (const AST::Interface& itf) noexcept;
	static bool is_stateless (const AST::Interface& itf) noexcept;

	static bool is_custom (const AST::Operation& op) noexcept;
	static bool is_custom (const AST::Interface& itf) noexcept;
	static bool is_component (const AST::Interface& itf) noexcept;

	static std::string const_id (const AST::Constant& c);

	static std::string skip_prefix (const AST::Identifier& id, const char* prefix);

private:
	static bool pred (const char* l, const char* r)
	{
		return strcmp (l, r) < 0;
	}

	static void get_all_bases (const AST::ValueType& vt,
		std::unordered_set <const AST::IV_Base*>& bset, Bases& bvec);
	static void get_all_bases (const AST::Interface& ai,
		std::unordered_set <const AST::IV_Base*>& bset, Bases& bvec);

private:
	static const char* const protected_names_ [];

	const Compiler& compiler_;
};

#endif
