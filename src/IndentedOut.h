#ifndef NIDL2CPP_INDENTEDOUT_H_
#define NIDL2CPP_INDENTEDOUT_H_

// TODO: Move to idlfe library as an utility class.
class IndentedOut : public std::ofstream
{
public:
	IndentedOut ();

	IndentedOut (const std::filesystem::path& file);

	~IndentedOut ();

	void open (const std::filesystem::path& file);

	void indent ()
	{
		isbuf_.indent ();
	}

	void unindent ()
	{
		isbuf_.unindent ();
	}

	void empty_line ()
	{
		isbuf_.empty_line ();
	}

	unsigned indentation () const
	{
		return isbuf_.indentation ();
	}

private:
	class IndentedStreambuf : public std::streambuf
	{
	public:
		IndentedStreambuf () :
			out_ (nullptr),
			indentation_ (0),
			bol_ (true),
			empty_line_ (false)
		{}

		void init (std::ostream& s)
		{
			out_ = s.rdbuf ();
			s.rdbuf (this);
		}

		void term (std::ostream& s)
		{
			s.rdbuf (out_);
		}

		void indent ()
		{
			++indentation_;
		}

		void unindent ()
		{
			assert (indentation_ > 0);
			if (indentation_ > 0)
				--indentation_;
		}

		unsigned indentation () const
		{
			return indentation_;
		}

		void empty_line ();

	protected:
		virtual int overflow (int c);

	private:
		std::streambuf* out_;
		unsigned indentation_;
		bool bol_;
		bool empty_line_;
	};

	IndentedStreambuf isbuf_;
};

#endif
