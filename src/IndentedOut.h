#ifndef NIDL2CPP_INDENTEDOUT_H_
#define NIDL2CPP_INDENTEDOUT_H_

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

private:
	class IndentedStreambuf : public std::streambuf
	{
	public:
		IndentedStreambuf () :
			out_ (nullptr),
			indentation_ (0),
			bol_ (true)
		{}

		void init (std::ostream& s)
		{
			out_ = s.rdbuf ();
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

	protected:
		virtual int overflow (int c);

	private:
		std::streambuf* out_;
		unsigned indentation_;
		bool bol_;
	};

	IndentedStreambuf isbuf_;
};

#endif
