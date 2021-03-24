#ifndef NIDL2CPP_CODEGENBASE_H_
#define NIDL2CPP_CODEGENBASE_H_

#include "IndentedOut.h"

class CodeGenBase : public AST::CodeGen
{
public:
	CodeGenBase (const std::filesystem::path& file);

protected:
	virtual void leaf (const AST::Native&) {}
	virtual void begin (const AST::ModuleItems&) {}
	virtual void end (const AST::ModuleItems&) {}

	IndentedOut& out ()
	{
		return out_;
	}

	void namespace_enter (const AST::NamedItem& item);
	void namespace_enter (const char* ns);
	void namespace_leave ();

private:
	typedef std::vector <const AST::NamedItem*> Namespaces;
	void get_namespaces (const AST::NamedItem* item, Namespaces& namespaces);

private:

	IndentedOut out_;
	const AST::Module* module_namespace_;
	const char* spec_namespace_;
};

#endif
