#include "pch.h"
#include "IndentedOut.h"

using namespace std;

IndentedOut::IndentedOut ()
{}

IndentedOut::IndentedOut (const filesystem::path& file)
{
	open (file);
}

IndentedOut::~IndentedOut ()
{
	if (is_open ())
		isbuf_.term (*this);
}

void IndentedOut::open (const std::filesystem::path& file)
{
	assert (!is_open ());
	create_directories (file.parent_path ());
	ofstream::open (file);
	if (!is_open ())
		throw runtime_error (string ("Can not open file: ") + file.string ());
	isbuf_.init (*this);
}

int IndentedOut::IndentedStreambuf::overflow (int c)
{
	if (c != '\n') {
		if (bol_ && indentation_) {
			for (unsigned cnt = indentation_; cnt; --cnt) {
				int ret = out_->sputc ('\t');
				if (ret != '\t')
					return ret;
			}
		}
		empty_line_ = bol_ = false;
	} else if (bol_)
		empty_line_ = true;
	else
		bol_ = true;
	return out_->sputc (c);
}

void IndentedOut::IndentedStreambuf::empty_line ()
{
	if (!bol_) {
		out_->sputc ('\n');
		bol_ = true;
	}
	if (!empty_line_) {
		out_->sputc ('\n');
		empty_line_ = true;
	}
}
