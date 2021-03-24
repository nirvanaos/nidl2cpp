#include "pch.h"
#include "Client.h"

using namespace std;
using namespace AST;

void Client::leaf (const Include& item)
{
	out () << "#include ";
	out () << (item.system () ? '<' : '"');
	out () << filesystem::path (item.file ()).replace_extension (".h").string ();
	out () << (item.system () ? '>' : '"');
	out () << endl;
}

void Client::leaf (const TypeDef& item)
{}

void Client::begin (const Interface& item)
{

}

void Client::end (const Interface& item)
{

}
