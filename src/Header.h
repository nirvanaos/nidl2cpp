#ifndef NIDL2CPP_HEADER_H_
#define NIDL2CPP_HEADER_H_

#include "Code.h"

// C++ header file output.
class Header : public Code
{
public:
	Header (const std::filesystem::path& file, const AST::Root& root);

	void close ();

private:
	static void to_upper (std::string& s);
	static std::string get_guard_macro (const std::filesystem::path& file);
};

#endif
