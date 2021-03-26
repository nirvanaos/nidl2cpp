#ifndef NIDL2CPP_CODE_H_
#define NIDL2CPP_CODE_H_

#include "IndentedOut.h"

// C++ code file output.
class Code : public IndentedOut
{
public:
	Code ();
	Code (const std::filesystem::path& file, const AST::Root& root);

	void open (const std::filesystem::path& file, const AST::Root& root);
	void close ();

	void namespace_open (const AST::NamedItem& item);
	void namespace_open (const char* spec_ns);
	void namespace_close ();

private:
	typedef std::vector <const AST::NamedItem*> Namespaces;
	void get_namespaces (const AST::NamedItem* item, Namespaces& namespaces);

private:
	const AST::NamedItem* module_namespace_;
	const char* spec_namespace_;
};

#endif
