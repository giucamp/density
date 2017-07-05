#pragma once
#include <ostream>
#include <sstream>
#include <utility>
#include <string>
#include <type_traits>

namespace density_tests
{
	/** Adapter to print and edit a line of output multiple times.
		At construction time a LineUpdaterStreamAdapter is assigned a destination output stream.
		The operator << redirects the output to an internal buffer. The std::endl modifiers
		deletes the previous line and flushes the output on the destination stream
		
		Example of usage:
		\code
		density_tests::LineUpdaterStreamAdapter line(std::cout);
		for (int i = 100000; i > 0; i /= 8)
		{
			line << "progress: " << i << std::endl;
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
		\endcode
	*/
	class LineUpdaterStreamAdapter
	{
	public:

		LineUpdaterStreamAdapter(std::ostream & i_dest_stream);

		LineUpdaterStreamAdapter(const LineUpdaterStreamAdapter &) = delete;
		
		LineUpdaterStreamAdapter & operator = (const LineUpdaterStreamAdapter &) = delete;

		~LineUpdaterStreamAdapter();

		template <typename TYPE>
			LineUpdaterStreamAdapter & operator << (TYPE && i_object)
		{
			m_line << std::forward<TYPE>(i_object);
			return *this;
		}

		LineUpdaterStreamAdapter & operator << (std::ostream & (*i_manipulator)(std::ostream &))
		{
			if (i_manipulator == &std::endl<char, std::char_traits<char>>)
				end_line_impl();
			else
				m_line << i_manipulator;
			return *this;
		}

	private:
		void end_line_impl();

	private:
		std::ostream & m_dest_stream;
		std::string m_spaces;
		std::ostringstream m_line;
		size_t m_prev_line_len = 0;
	};
}