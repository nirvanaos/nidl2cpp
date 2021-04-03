#ifndef NIDL2CPP_SERVANT_H_
#define NIDL2CPP_SERVANT_H_

#include "CodeGenBase.h"
#include "Header.h"

// Servant code generator
class Servant : public CodeGenBase
{
public:
	Servant (const std::filesystem::path& file, const std::filesystem::path& client, const AST::Root& root) :
		h_ (file, root)
	{
		h_ << "#include <CORBA/Server.h>\n"
			"#include " << client.filename () << std::endl << std::endl;
	}

protected:
	virtual void end (const AST::Root&);
	virtual void begin (const AST::Interface& itf);
	virtual void leaf (const AST::Operation& op);
	virtual void leaf (const AST::Attribute& att);
	virtual void end (const AST::Interface& itf);

private:
	void servant_param (const AST::Parameter& param)
	{
		servant_param (param, param.name (), param.attribute ());
	}

	void servant_param (const AST::Type& t, const std::string& name, AST::Parameter::Attribute att = AST::Parameter::Attribute::IN);

	void catch_block ();

	void implementation_suffix (const AST::InterfaceKind ik);
	void implementation_parameters (const AST::Interface& primary, const AST::Interfaces& bases);

private:
	Header h_;
	std::vector<std::string> epv_;
};

#endif
