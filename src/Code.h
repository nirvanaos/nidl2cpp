#ifndef NIDL2CPP_CODE_H_
#define NIDL2CPP_CODE_H_

#include <BE/IndentedOut.h>

// C++ code file output.
class Code : public BE::IndentedOut
{
	typedef BE::IndentedOut Base;
public:
	Code ();
	Code (const std::filesystem::path& file, const AST::Root& root);

	void open (const std::filesystem::path& file, const AST::Root& root);
	void close ();

	void namespace_open (const AST::NamedItem& item);
	void namespace_open (const char* spec_ns);
	void namespace_close ();

	const AST::NamedItem* module_namespace () const
	{
		return module_namespace_;
	}

	const char* spec_namespace () const
	{
		return spec_namespace_;
	}

	bool no_namespace () const
	{
		return !module_namespace_ && !spec_namespace_;
	}

	Code& operator << (short val)
	{
		Base::operator << (val);
		return *this;
	}

	Code& operator << (unsigned short val)
	{
		Base::operator << (val);
		return *this;
	}

	Code& operator << (int val)
	{
		Base::operator << (val);
		return *this;
	}

	Code& operator << (unsigned int val)
	{
		Base::operator << (val);
		return *this;
	}

	Code& operator << (long val)
	{
		Base::operator << (val);
		return *this;
	}

	Code& operator << (unsigned long val)
	{
		Base::operator << (val);
		return *this;
	}

	Code& operator << (std::ios_base& (*func)(std::ios_base&))
	{
		Base::operator << (func);
		return *this;
	}

	Code& operator << (std::basic_ios <char, std::char_traits <char> >& (*func)(std::basic_ios <char, std::char_traits <char> >&))
	{
		Base::operator << (func);
		return *this;
	}

	Code& operator << (std::ostream& (*func)(std::ostream&))
	{
		Base::operator << (func);
		return *this;
	}

private:
	typedef std::vector <const AST::NamedItem*> Namespaces;
	void get_namespaces (const AST::NamedItem* item, Namespaces& namespaces);

private:
	const AST::NamedItem* module_namespace_;
	const char* spec_namespace_;
};

inline
Code& operator << (Code& stm, char c)
{
	static_cast <BE::IndentedOut&> (stm) << c;
	return stm;
}

inline
Code& operator << (Code& stm, signed char c)
{
	static_cast <BE::IndentedOut&> (stm) << c;
	return stm;
}

inline
Code& operator << (Code& stm, unsigned char c)
{
	static_cast <BE::IndentedOut&> (stm) << c;
	return stm;
}

inline
Code& operator << (Code& stm, const char* s)
{
	static_cast <BE::IndentedOut&> (stm) << s;
	return stm;
}

inline
Code& operator << (Code& stm, const std::string& s)
{
	static_cast <BE::IndentedOut&> (stm) << s;
	return stm;
}

Code& operator << (Code& stm, const AST::Identifier& id);
Code& operator << (Code& stm, const AST::Type& t);
Code& operator << (Code& stm, const AST::Variant& var);

#endif
