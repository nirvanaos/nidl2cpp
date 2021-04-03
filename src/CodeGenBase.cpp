#include "pch.h"
#include "CodeGenBase.h"

using namespace std;
using namespace std::filesystem;
using namespace AST;

const char CodeGenBase::internal_namespace_ [] =
"namespace CORBA {\n"
"namespace Nirvana {\n";

const char* const CodeGenBase::protected_names_ [] = {
	"FALSE",
	"TRUE",
	"alignas",
	"alignof",
	"and",
	"and_eq",
	"asm",
	"auto",
	"bitand",
	"bitor",
	"bool",
	"break",
	"case",
	"catch",
	"char",
	"char16_t",
	"char32_t",
	"class",
	"compl",
	"const",
	"const_cast",
	"constexpr",
	"continue",
	"decltype",
	"default",
	"delete",
	"do",
	"double",
	"dynamic_cast",
	"else",
	"enum",
	"explicit",
	"export",
	"extern",
	"float",
	"for",
	"friend",
	"goto",
	"if",
	"inline",
	"int",
	"int16_t",
	"int32_t",
	"int64_t",
	"long",
	"mutable",
	"namespace",
	"new",
	"noexcept",
	"not",
	"not_eq",
	"operator",
	"or",
	"or_eq",
	"private",
	"protected",
	"public",
	"register",
	"reinterpret_cast",
	"return",
	"short",
	"signed",
	"sizeof",
	"static",
	"static_cast",
	"struct",
	"switch",
	"template",
	"this",
	"throw",
	"thread_local",
	"try",
	"typedef",
	"typeid",
	"typename",
	"uint16_t",
	"uint32_t",
	"uint64_t",
	"uint8_t",
	"union",
	"unsigned",
	"what",
	"using",
	"virtual",
	"void",
	"volatile",
	"wchar_t",
	"while",
	"xor",
	"xor_eq"
};

#define PROTECTED_PREFIX "_cxx_"

inline bool CodeGenBase::is_keyword (const Identifier& id)
{
	return binary_search (protected_names_, std::end (protected_names_), id.c_str (), pred);
}

ostream& operator << (ostream& stm, const Identifier& id)
{
	if (CodeGenBase::is_keyword (id))
		stm << PROTECTED_PREFIX;
	stm << static_cast <const string&> (id);
	return stm;
}

ostream& operator << (ostream& stm, const Type& t)
{
	static const char* const basic_types [(size_t)BasicType::ANY + 1] = {
		"bool",
		"uint8_t",
		"char",
		"wchar_t",
		"uint16_t",
		"uint32_t",
		"uint64_t",
		"int16_t",
		"int32_t",
		"int64_t",
		"float",
		"double",
		"long double",
		"::CORBA::Object_var",
		"::CORBA::ValueBase_var",
		"::CORBA::Any"
	};

	switch (t.tkind ()) {
		case Type::Kind::VOID:
			stm << "void";
			break;
		case Type::Kind::BASIC_TYPE:
			stm << basic_types [(size_t)t.basic_type ()];
			break;
		case Type::Kind::NAMED_TYPE:
			stm << CodeGenBase::qualified_name (*t.named_type ());
			switch (t.named_type ()->kind ()) {
				case Item::Kind::INTERFACE:
				case Item::Kind::VALUE_TYPE:
				case Item::Kind::VALUE_BOX:
					stm << "_var";
			}
			break;
		case Type::Kind::STRING:
			stm << "std::string";
			break;
		case Type::Kind::WSTRING:
			stm << "std::wstring";
			break;
		case Type::Kind::FIXED:
			stm << "::CORBA::" << "Fixed";
			if (t.fixed_digits ())
				stm << " <" << t.fixed_digits () << ", " << t.fixed_scale () << '>';
			break;
		case Type::Kind::SEQUENCE:
			stm << "std::vector <" << static_cast <const Type&> (t.sequence ()) << '>';
			break;
		case Type::Kind::ARRAY: {
			const Array& arr = t.array ();
			for (size_t cnt = arr.dimensions ().size (); cnt; --cnt) {
				stm << "std::array <";
			}
			stm << static_cast <const Type&> (arr);
			for (auto dim = arr.dimensions ().rbegin (); dim != arr.dimensions ().rend (); ++dim) {
				stm << ", " << *dim << '>';
			}
		} break;
		default:
			assert (false);
	}

	return stm;
}

string CodeGenBase::qualified_name (const NamedItem& item, bool fully)
{
	if (is_keyword (item.name ()))
		return qualified_parent_name (item) + PROTECTED_PREFIX + item.name ();
	else
		return qualified_parent_name (item) + item.name ();
}

string CodeGenBase::qualified_parent_name (const NamedItem& item, bool fully)
{
	const NamedItem* parent = item.parent ();
	string qn;
	if (parent) {
		Item::Kind pk = parent->kind ();
		if (fully || pk != Item::Kind::MODULE) {
			if (Item::Kind::INTERFACE == pk || Item::Kind::VALUE_TYPE == pk) {
				qn = "::CORBA::Nirvana::Definitions <";
				if (fully)
					qn += " ::";
				qn += qualified_name (*parent, fully);
				qn += '>';
				if (is_keyword (item.name ()))
					qn += PROTECTED_PREFIX;
				qn += item.name ();
			} else {
				if (fully)
					qn = "::";
				qn += qualified_name (*parent, fully);
			}
			qn += "::";
		}
	}
	return qn;
}

void CodeGenBase::bridge_ret (ofstream& stm, const Type& t)
{
	if (t.tkind () != Type::Kind::VOID) {
		stm << "ABI_ret <" << t << ">";
	} else {
		stm << "void";
	}
}

void CodeGenBase::bridge_param (ofstream& stm, const Parameter& param)
{
	bridge_param (stm, param, param.attribute ());
	stm << ' ' << param.name ();
}

void CodeGenBase::bridge_param (ofstream& stm, const Type& t, Parameter::Attribute att)
{
	switch (att) {
		case Parameter::Attribute::IN:
			stm << "ABI_in <";
			break;
		case Parameter::Attribute::OUT:
			stm << "ABI_out <";
			break;
		case Parameter::Attribute::INOUT:
			stm << "ABI_inout <";
			break;
	}
	stm << t << '>';
}

CodeGenBase::Members CodeGenBase::get_members (const ItemContainer& cont, Item::Kind member_kind)
{
	Members ret;
	for (auto it = cont.begin (); it != cont.end (); ++it) {
		const Item& item = **it;
		if (item.kind () == member_kind)
			ret.push_back (&static_cast <const Member&> (item));
	}
	return ret;
}

bool CodeGenBase::is_var_len (const Type& type)
{
	const Type& t = type.dereference_type ();
	switch (t.tkind ()) {
		case Type::Kind::BASIC_TYPE:
			switch (t.basic_type ()) {
				case BasicType::ANY:
				case BasicType::OBJECT:
				case BasicType::VALUE_BASE:
					return true;
				default:
					return false;
			}

		case Type::Kind::STRING:
		case Type::Kind::WSTRING:
		case Type::Kind::SEQUENCE:
			return true;

		case Type::Kind::ARRAY:
			return is_var_len (t.array ());

		case Type::Kind::NAMED_TYPE: {
			const NamedItem& item = *t.named_type ();
			Members members;
			switch (item.kind ()) {
				case Item::Kind::INTERFACE:
				case Item::Kind::INTERFACE_DECL:
				case Item::Kind::VALUE_TYPE:
				case Item::Kind::VALUE_TYPE_DECL:
				case Item::Kind::VALUE_BOX:
					return true;
				case Item::Kind::STRUCT:
					members = get_members (static_cast <const Struct&> (item));
					break;
				case Item::Kind::UNION:
					members = get_members (static_cast <const Union&> (item));
					break;
				default:
					return false;
			}
			for (auto member : members) {
				if (is_var_len (*member))
					return true;
			}
		}
	}
	return false;
}

void CodeGenBase::leaf (const UnionDecl&)
{
	throw runtime_error ("Not yet implemented");
}

void CodeGenBase::begin (const Union&)
{
	throw runtime_error ("Not yet implemented");
}

void CodeGenBase::end (const Union&)
{
	throw runtime_error ("Not yet implemented");
}

void CodeGenBase::leaf (const UnionElement&)
{
	throw runtime_error ("Not yet implemented");
}

void CodeGenBase::leaf (const ValueTypeDecl&)
{
	throw runtime_error ("Not yet implemented");
}

void CodeGenBase::begin (const ValueType&)
{
	throw runtime_error ("Not yet implemented");
}

void CodeGenBase::end (const ValueType&)
{
	throw runtime_error ("Not yet implemented");
}

void CodeGenBase::leaf (const StateMember&)
{
	throw runtime_error ("Not yet implemented");
}

void CodeGenBase::leaf (const ValueFactory&)
{
	throw runtime_error ("Not yet implemented");
}

void CodeGenBase::leaf (const ValueBox&)
{
	throw runtime_error ("Not yet implemented");
}

