#ifndef NIDL2CPP_PROXY_H_
#define NIDL2CPP_PROXY_H_

#include "CodeGenBase.h"
#include "Code.h"

// Proxy code generator
class Proxy : public CodeGenBase
{
public:
	Proxy (const std::filesystem::path& file, const std::filesystem::path& servant, const AST::Root& root) :
		cpp_ (file, root)
	{
		cpp_ << "#include " << servant.filename () << std::endl
			<< "#include <CORBA/Proxy/Proxy.h>\n\n";
	}

protected:
	virtual void end (const AST::Root&);

	virtual void leaf (const AST::TypeDef& item);
	
	virtual void begin (const AST::Interface& itf);
	virtual void leaf (const AST::Operation& op);
	virtual void leaf (const AST::Attribute& att);
	virtual void end (const AST::Interface& itf);

	virtual void end (const AST::Exception& item);
	virtual void end (const AST::Struct& item);
	virtual void leaf (const AST::Enum& item);

private:
	void abi_members (const std::vector <const AST::Parameter*>& params);
	std::ostream& abi_member (const AST::Type& t);
	void proxy_param (const AST::Parameter& param);

	typedef std::vector <const AST::Parameter*> Parameters;
	static void get_parameters (const AST::Operation& op, Parameters& params_in, Parameters& params_out);
	static bool need_marshal (const Parameters& params);

	static std::string export_name (const AST::NamedItem& item);

	void implement (const AST::TypeDef& item);
	void implement (const AST::Exception& item);
	void implement (const AST::Struct& item);
	void implement (const AST::Enum& item);

	void marshal_traits (const std::string& name, const Members& members);

	void parameter (const AST::Member& m);
	void tc_name (const AST::Type& t);

private:
	Code cpp_;
};

#endif
