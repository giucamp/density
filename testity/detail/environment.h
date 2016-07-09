
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)


#pragma once
#include <string>
#include <chrono>

namespace testity
{
	namespace detail
	{
		class Environment
		{
		public:

			Environment();

			const std::string & operating_sytem() const { return m_operating_sytem; }
			const std::string & compiler() const { return m_compiler; }
			const std::string & system_info() const { return m_system_info; }
			size_t sizeof_pointer() const { return sizeof(void*); }
			const std::chrono::system_clock::time_point & startup_clock() const { return m_startup_clock; }

		private:
			std::string m_operating_sytem;
			std::string m_compiler;
			std::string m_system_info;
			std::chrono::system_clock::time_point m_startup_clock;
		};

	} // namespace detail

} // namespace testity



