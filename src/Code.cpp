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
	*this << "// This file is generated from " << root.file () << endl;
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
		for (; cur != cur_namespaces.end (); ++cur) {
			*this << "}\n";
		}
		for (; req != req_namespaces.end (); ++req) {
			*this << "namespace " << (*req)->name () << " {\n";
		}
		module_namespace_ = p;
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
	*this << spec_ns;
	spec_namespace_ = spec_ns;
}

void Code::namespace_close ()
{
	if (module_namespace_) {
		do {
			*this << "}\n";
			module_namespace_ = static_cast <const Module*> (module_namespace_->parent ());
		} while (module_namespace_);
	} else if (spec_namespace_) {
		for (const char* s = spec_namespace_, *open; open = strchr (s, '{'); s = open + 1) {
			*this << "}\n";
		}
		spec_namespace_ = nullptr;
	}
}
