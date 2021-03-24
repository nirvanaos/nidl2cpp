#ifndef NIDL2CPP_CLIENT_H_
#define NIDL2CPP_CLIENT_H_

#include "CodeGenBase.h"

// Client code generator.
class Client : public CodeGenBase
{
public:
	Client (const std::filesystem::path& file) :
		CodeGenBase (file)
	{}

protected:
	virtual void leaf (const AST::Include& item);
	virtual void leaf (const AST::TypeDef& item);

	virtual void begin (const AST::Interface& item);
	virtual void end (const AST::Interface& item);

private:
	IndentedOut cpp_out_; // Optional .cpp file.
};

#endif
