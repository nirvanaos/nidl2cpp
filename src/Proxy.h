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
	virtual void begin (const AST::Interface& itf);
	virtual void leaf (const AST::Operation& op);
	virtual void leaf (const AST::Attribute& att);
	virtual void end (const AST::Interface& itf);

private:
	void abi_members (const std::vector <const AST::Parameter*>& params);
	std::ostream& abi_member (const AST::Type& t);

private:
	Code cpp_;
};

#endif
