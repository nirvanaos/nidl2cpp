#ifndef NIDL2CPP_CLIENT_H_
#define NIDL2CPP_CLIENT_H_

#include "CodeGenBase.h"
#include "Header.h"

// Client code generator
class Client : public CodeGenBase
{
public:
	Client (const std::filesystem::path& file_h, const std::filesystem::path& file_cpp, const AST::Root& root) :
		h_ (file_h, root),
		cpp_ (file_cpp, root)
	{
		h_ << "\n#include <CORBA/CORBA.h>\n\n";

		cpp_ << "\n#include " << file_h.filename () << "\n"
			"#include <Nirvana/OLF.h>\n\n";
	}

protected:
	virtual void end (const AST::Root&);

	virtual void leaf (const AST::Include& item);
	virtual void leaf (const AST::TypeDef& item);

	virtual void leaf (const AST::InterfaceDecl& item);
	virtual void begin (const AST::Interface& item);
	virtual void end (const AST::Interface& item);
	virtual void leaf (const AST::Operation&);
	virtual void leaf (const AST::Attribute&);

	virtual void leaf (const AST::Constant& item);

	virtual void begin (const AST::Exception& item);
	virtual void end (const AST::Exception& item);

	virtual void leaf (const AST::StructDecl& item);
	virtual void begin (const AST::Struct& item);
	virtual void end (const AST::Struct& item);

	virtual void leaf (const AST::Member& item);

	virtual void leaf (const AST::Enum& item);

private:
	void interface_forward (const AST::NamedItem& item, AST::InterfaceKind ik);
	bool constant (std::ofstream& stm, const AST::Constant& item);
	void environment (const AST::Raises& raises);
	void type_code_decl (const AST::NamedItem& item);
	void type_code_def (const AST::RepositoryId& type);
	static void value (std::ofstream& stm, const AST::Variant& var);

private:
	Header h_; // .h file
	Code cpp_; // .cpp file.
};

#endif
