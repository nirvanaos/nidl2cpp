#ifndef NIDL2CPP_CODEGENBASE_H_
#define NIDL2CPP_CODEGENBASE_H_

class CodeGenBase : public AST::CodeGen
{
public:
	static bool is_keyword (const AST::Identifier& id);

protected:
	virtual void leaf (const AST::Native&) {}
	virtual void begin (const AST::ModuleItems&) {}
	virtual void end (const AST::ModuleItems&) {}

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

	static void type (std::ofstream& stm, const AST::Type& t);

	static void bridge_ret (std::ofstream& stm, const AST::Type& t);
	static void bridge_param (std::ofstream& stm, const AST::Parameter& param);
	static void bridge_param (std::ofstream& stm, const AST::Type& t, AST::Parameter::Attribute att = AST::Parameter::Attribute::IN);

	static void client_ret (std::ofstream& stm, const AST::Type& t);
	static void client_param (std::ofstream& stm, const AST::Parameter& param);
	static void client_param (std::ofstream& stm, const AST::Type& t, AST::Parameter::Attribute att = AST::Parameter::Attribute::IN);

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

	static std::string qualified_name (const AST::NamedItem& item, bool fully = true);
	static std::string qualified_parent_name (const AST::NamedItem& item, bool fully = true);

private:
	static bool pred (const char* l, const char* r)
	{
		return strcmp (l, r) < 0;
	}

	static Members get_members (const AST::ItemContainer& cont, AST::Item::Kind member_kind);

protected:
	static const char internal_namespace_ [];

private:
	static const char* const protected_names_ [];
};

std::ostream& operator << (std::ostream& stm, const AST::Identifier& id);

#endif
