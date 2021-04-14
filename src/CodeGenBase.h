#ifndef NIDL2CPP_CODEGENBASE_H_
#define NIDL2CPP_CODEGENBASE_H_

#include "Code.h"

class CodeGenBase : public AST::CodeGen
{
public:
	static bool is_keyword (const AST::Identifier& id);
	inline static const char protected_prefix_ [] = "_cxx_";
	static const char internal_namespace_ [];

	struct QName
	{
		QName (const AST::NamedItem& ni) :
			item (ni)
		{}

		const AST::NamedItem& item;
	};

	struct ParentName
	{
		ParentName (const AST::NamedItem& ni) :
			item (ni)
		{}

		const AST::NamedItem& item;
	};

	struct TypePrefix
	{
		TypePrefix (const AST::Type& t) :
			type (t)
		{}

		const AST::Type& type;
	};

	struct TypeABI_ret
	{
		TypeABI_ret (const AST::Type& t) :
			type (t)
		{}

		const AST::Type& type;
	};

	struct TypeABI_param
	{
		TypeABI_param (const AST::Parameter& p) :
			type (p),
			att (p.attribute ())
		{}

		TypeABI_param (const AST::Type& t) :
			type (t),
			att (AST::Parameter::Attribute::IN)
		{}

		const AST::Type& type;
		const AST::Parameter::Attribute att;
	};

	struct TypeC_param
	{
		TypeC_param (const AST::Parameter& p) :
			type (p),
			att (p.attribute ())
		{}

		TypeC_param (const AST::Type& t) :
			type (t),
			att (AST::Parameter::Attribute::IN)
		{}

		const AST::Type& type;
		const AST::Parameter::Attribute att;
	};

protected:
	virtual void leaf (const AST::Include& item) {}
	virtual void leaf (const AST::Native&) {}
	virtual void leaf (const AST::TypeDef& item) {}
	virtual void begin (const AST::ModuleItems&) {}
	virtual void end (const AST::ModuleItems&) {}
	virtual void leaf (const AST::Operation&) {}
	virtual void leaf (const AST::Attribute&) {}
	virtual void leaf (const AST::Member& item) {}

	virtual void leaf (const AST::InterfaceDecl& item) {}
	virtual void end (const AST::Interface& item) {}

	virtual void leaf (const AST::Constant& item) {}
	virtual void begin (const AST::Exception& item) {}
	virtual void end (const AST::Exception& item) {}
	virtual void leaf (const AST::StructDecl& item) {}
	virtual void begin (const AST::Struct& item) {}
	virtual void end (const AST::Struct& item) {}

	virtual void leaf (const AST::Enum& item) {}

	virtual void leaf (const AST::UnionDecl&);
	virtual void begin (const AST::Union&);
	virtual void end (const AST::Union&);
	virtual void leaf (const AST::UnionElement&);

	virtual void leaf (const AST::ValueTypeDecl&);
	virtual void begin (const AST::ValueType&);
	virtual void end (const AST::ValueType&);
	virtual void leaf (const AST::StateMember&);
	virtual void leaf (const AST::ValueFactory&);
	virtual void leaf (const AST::ValueBox&);

	typedef std::vector <const AST::Member*> Members;

	static Members get_members (const AST::Struct& cont)
	{
		return get_members (cont, AST::Item::Kind::MEMBER);
	}

	static Members get_members (const AST::Exception& cont)
	{
		return get_members (cont, AST::Item::Kind::MEMBER);
	}

	static Members get_members (const AST::Union& cont)
	{
		return get_members (cont, AST::Item::Kind::UNION_ELEMENT);
	}

	static bool is_var_len (const AST::Type& type);
	static bool is_var_len (const Members& members);

private:
	static bool pred (const char* l, const char* r)
	{
		return strcmp (l, r) < 0;
	}

	static Members get_members (const AST::ItemContainer& cont, AST::Item::Kind member_kind);

private:
	static const char* const protected_names_ [];
};

Code& operator << (Code& stm, const CodeGenBase::QName& qn);
Code& operator << (Code& stm, const CodeGenBase::ParentName& qn);
Code& operator << (Code& stm, const CodeGenBase::TypePrefix& t);
Code& operator << (Code& stm, const CodeGenBase::TypeABI_ret& t);
Code& operator << (Code& stm, const CodeGenBase::TypeABI_param& t);
Code& operator << (Code& stm, const CodeGenBase::TypeC_param& t);

#endif
