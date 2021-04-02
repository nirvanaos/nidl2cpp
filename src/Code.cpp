#include "pch.h"
#include "Code.h"

using namespace std;
using namespace std::filesystem;
using namespace AST;

Code::Code () :
	module_namespace_ (nullptr),
	spec_namespace_ (nullptr)
{}

Code::Code (const path& file, const Root& root) :
	module_namespace_ (nullptr),
	spec_namespace_ (nullptr)
{
	open (file, root);
}

void Code::open (const std::filesystem::path& file, const Root& root)
{
	IndentedOut::open (file);
	*this << "// This file was generated from " << root.file ().filename () << endl;
	*this << "// Nirvana IDL compiler version 1.0\n";
}

void Code::close ()
{
	namespace_close ();
	IndentedOut::close ();
}

void Code::namespace_open (const NamedItem& item)
{
	if (spec_namespace_)
		namespace_close ();
	auto p = item.parent ();
	while (p && p->kind () != Item::Kind::MODULE)
		p = p->parent ();

	if (p != module_namespace_) {
		Namespaces cur_namespaces, req_namespaces;
		get_namespaces (module_namespace_, cur_namespaces);
		get_namespaces (p, req_namespaces);
		Namespaces::const_iterator cur = cur_namespaces.begin (), req = req_namespaces.begin ();
		for (; cur != cur_namespaces.end () && req != req_namespaces.end (); ++cur, ++req) {
			if (*cur != *req)
				break;
		}
		empty_line ();
		for (; cur != cur_namespaces.end (); ++cur) {
			*this << "}\n";
		}
		empty_line ();
		for (; req != req_namespaces.end (); ++req) {
			*this << "namespace " << (*req)->name () << " {\n";
		}
		module_namespace_ = p;
		*this << endl;
	}
}

void Code::get_namespaces (const NamedItem* item, Namespaces& namespaces)
{
	if (item) {
		get_namespaces (item->parent (), namespaces);
		namespaces.push_back (item);
	}
}

void Code::namespace_open (const char* spec_ns)
{
	if (module_namespace_)
		namespace_close ();
	if (spec_ns != spec_namespace_) {
		empty_line ();
		*this << spec_ns << endl;
		spec_namespace_ = spec_ns;
	}
}

void Code::namespace_close ()
{
	if (module_namespace_) {
		do {
			empty_line ();
			*this << "}\n";
			module_namespace_ = static_cast <const Module*> (module_namespace_->parent ());
		} while (module_namespace_);
		*this << endl;
	} else if (spec_namespace_) {
		empty_line ();
		for (const char* s = spec_namespace_, *open; open = strchr (s, '{'); s = open + 1) {
			*this << "}\n";
		}
		spec_namespace_ = nullptr;
		*this << endl;
	}
}
