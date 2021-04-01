#include "pch.h"
#include "Header.h"

using namespace std;
using namespace std::filesystem;

inline
string Header::get_guard_macro (const path& file)
{
	string name = file.filename ().replace_extension ("").string ();
	string ext = file.extension ().string ().substr (1);
	to_upper (name);
	to_upper (ext);
	string ret = "IDL_";
	ret += name;
	ret += '_';
	ret += ext;
	ret += '_';
	return ret;
}

void Header::to_upper (string& s)
{
	for (auto& c : s)
		c = toupper (c);
}

Header::Header (const path& file, const AST::Root& root) :
	Code (file, root)
{
	string guard = get_guard_macro (file);
	*this << "#ifndef " << guard << endl;
	*this << "#define " << guard << endl;
}

void Header::close ()
{
	namespace_close ();
	empty_line ();
	*this << "#endif\n";
	Code::close ();
}
