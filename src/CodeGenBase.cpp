#include "pch.h"
#include "CodeGenBase.h"

using namespace std;
using namespace AST;

CodeGenBase::CodeGenBase (const filesystem::path& file) :
	out_ (file),
	module_namespace_ (nullptr),
	spec_namespace_ (nullptr)
{}

void CodeGenBase::namespace_open (const NamedItem& item)
{
	if (spec_namespace_)
		namespace_close ();
	auto p = item.parent ();
	if ((!p || p->kind () == Item::Kind::MODULE) && p != module_namespace_) {
		Namespaces cur_namespaces, req_namespaces;
		get_namespaces (module_namespace_, cur_namespaces);
		get_namespaces (p, req_namespaces);
		Namespaces::const_iterator cur = cur_namespaces.begin (), req = req_namespaces.begin ();
		for (; cur != cur_namespaces.end () && req != req_namespaces.end (); ++cur, ++req) {
			if (*cur != *req)
				break;
		}
		for (; cur != cur_namespaces.end (); ++cur) {
			out () << "}\n";
		}
		for (; req != req_namespaces.end (); ++req) {
			out () << "namespace " << (*req)->name () << " {\n";
		}
	}
}

void CodeGenBase::namespace_open (const char* spec_ns)
{
	if (module_namespace_)
		namespace_close ();
	out () << spec_ns;
	spec_namespace_ = spec_ns;
}

void CodeGenBase::namespace_close ()
{
	if (module_namespace_) {
		do {
			out () << "}\n";
			module_namespace_ = static_cast <const Module*> (module_namespace_->parent ());
		} while (module_namespace_);
	} else if (spec_namespace_) {
		for (const char* s = spec_namespace_, *open; open = strchr (s, '{'); s = open + 1) {
			out () << "}\n";
		}
		spec_namespace_ = nullptr;
	}
}

void CodeGenBase::get_namespaces (const NamedItem* item, Namespaces& namespaces)
{
	if (item) {
		get_namespaces (item->parent (), namespaces);
		namespaces.push_back (item);
	}
}
