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
	
	virtual void begin (const AST::Interface& itf) {}
	virtual void end (const AST::Interface& itf);

	virtual void end (const AST::Exception& item);
	virtual void end (const AST::Struct& item);
	virtual void leaf (const AST::Enum& item);

private:
	void abi_members (const Members& members);
	std::ostream& abi_member (const AST::Type& t);
	void proxy_param (const AST::Parameter& param);

	static void get_parameters (const AST::Operation& op, Members& params_in, Members& params_out);

	static std::string export_name (const AST::NamedItem& item);

	void implement (const AST::Operation& op);
	void implement (const AST::Attribute& att);

	void marshal_traits (const std::string& name, const Members& members);

	void md_members (const Members& members);
	void md_member (const AST::Member& m);
	void md_member (const AST::Type& t, const std::string& name);
	void tc_name (const AST::Type& t);

	struct OpMetadata
	{
		std::string name;
		const AST::Type* type;
		Members params_in;
		Members params_out;
	};

	typedef std::list <OpMetadata> Metadata;

	void md_operation (const AST::Interface& itf, const OpMetadata& op);

	void rep_id_of (const AST::RepositoryId& rid);

private:
	Code cpp_;
};

#endif
