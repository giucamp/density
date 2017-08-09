

#include "density_test_common.h"
#include "easy_random.h"
#include <iostream>
#include <assert.h>
#include <thread>
#include <memory>

namespace density_tests
{
	namespace detail
	{
		void assert_failed(const char * i_source_file, const char * i_function, int i_line, const char * i_expr)
		{
			std::cerr << "assert failed in " << i_source_file << " (" << i_line << ")\n";
			std::cerr << "function: " << i_function << "\n";
			std::cerr << "expression: " << i_expr << std::endl;
			
			assert(false);
		}
	}

	thread_local ThreadArtificialDelay * ThreadArtificialDelay::t_artificial_delay;

	ThreadArtificialDelay::ThreadArtificialDelay(size_t i_initial_progressive,
			size_t i_max_period, std::chrono::microseconds i_max_delay, EasyRandom * i_random)
		: m_progressive(i_initial_progressive), m_max_period(i_max_period), m_max_delay(i_max_delay), m_random(i_random)
	{
		DENSITY_TEST_ASSERT(t_artificial_delay == nullptr); // can't nest ThreadArtificialDelay
		if (m_max_period != 0)
		{
			t_artificial_delay = this;
		}
	}

	ThreadArtificialDelay::~ThreadArtificialDelay()
	{
		DENSITY_TEST_ASSERT( (m_max_period != 0) ? (t_artificial_delay == this) : (t_artificial_delay == nullptr));
		t_artificial_delay = nullptr;
	}

	void ThreadArtificialDelay::step()
	{
		if (t_artificial_delay != nullptr)
		{
			t_artificial_delay->step_impl();
		}
	}

	void ThreadArtificialDelay::step_impl()
	{
		++m_progressive;
		if ((m_progressive % m_max_period) == 0)
		{
			auto const delay_reduction = m_random->get_int<decltype(m_max_delay.count())>(0, m_max_delay.count() / 2);
			auto const delay = m_max_delay - std::chrono::microseconds( delay_reduction );
			std::this_thread::sleep_for(delay);
			m_progressive += m_random->get_int<size_t>(0, m_max_period / 2);
		}
	}
}

namespace density
{
	#if DENSITY_TEST_ENABLE_ARTIFICIAL_DELAY
		void test_artificial_delay()
		{
			density_tests::ThreadArtificialDelay::step();
		}
	#endif
}
