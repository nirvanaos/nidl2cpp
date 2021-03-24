#include "pch.h"
#include "IndentedOut.h"

using namespace std;

IndentedOut::IndentedOut ()
{
	exceptions (ofstream::failbit | ofstream::badbit);
}

IndentedOut::IndentedOut (const filesystem::path& file) :
	IndentedOut ()
{
	open (file);
	isbuf_.init (*this);
}

IndentedOut::~IndentedOut ()
{
	if (is_open ())
		isbuf_.term (*this);
}

void IndentedOut::open (const std::filesystem::path& file)
{
	assert (!is_open ());
}

int IndentedOut::IndentedStreambuf::overflow (int c)
{
	if (c != '\n') {
		if (bol_ && indentation_) {
			for (unsigned cnt = indentation_; cnt; --cnt) {
				out_->sputc ('\t');
			}
			bol_ = false;
		}
	} else
		bol_ = true;
	return out_->sputc (c);
}
