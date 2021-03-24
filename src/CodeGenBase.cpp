#include "pch.h"
#include "CodeGenBase.h"

using namespace std;
using namespace AST;

CodeGenBase::CodeGenBase (const filesystem::path& file) :
	out_ (file),
	module_namespace_ (nullptr),
	spec_namespace_ (nullptr)
{}

void CodeGenBase::namespace_enter (const NamedItem& item)
{
	if (spec_namespace_)
		namespace_leave ();
	auto p = item.parent ();
	if ((!p || p->kind () == Item::Kind::MODULE)) {
		Namespaces cur_namespaces, namespaces;
		get_namespaces (module_namespace_, cur_namespaces);
		get_namespaces (p, namespaces);
		Namespaces::const_iterator cur = cur_namespaces.begin (), req = namespaces.begin ();
		for (; cur != cur_namespaces.end () && req != namespaces.end (); ++cur, ++req) {
			if (*cur != *req)
				break;
		}
	}
}

void CodeGenBase::namespace_leave ()
{
	if (module_namespace_) {
		do {
			out () << "}\n";
			module_namespace_ = static_cast <const Module*> (module_namespace_->parent ());
		} while (module_namespace_);
	} else if (spec_namespace_) {
		for (const char* s = spec_namespace_, *open; open = strchr (s, '{');) {
			out () << "}\n";
			s = open + 1;
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
