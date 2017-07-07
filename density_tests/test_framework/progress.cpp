
#include "progress.h"
#include <iterator>
#include <cmath>

namespace density_tests
{
	Progress::Progress(size_t i_target_count) noexcept
		: m_target_count(i_target_count), m_target_count_reciprocal_times_100(100.0 / i_target_count)
	{

	}

	void Progress::write_to_stream(std::ostream & i_ostream) const
	{
		// compute the completion percentage
		auto const curr_count = m_curr_count.load(std::memory_order_relaxed);
		auto const percentage = static_cast<int>(curr_count * m_target_count_reciprocal_times_100);

		// compute the elapsed time (is seconds)
		using FpSeconds = std::chrono::duration<double, std::chrono::seconds::period>;
		auto const elapsed = static_cast<FpSeconds>(std::chrono::high_resolution_clock::now() - m_start_time);

		if (curr_count < m_target_count)
		{
			/* compute a linear estimate of the remaining time, based on the equation:
					percentage / 100 = elapsed / (remaining + elapsed)						*/
			auto const remaining = (percentage < 0.0001) ? -1. : elapsed.count() * (100. / percentage - 1.);

			// actual write
			i_ostream << percentage;
			if (remaining == -1.)
				i_ostream << "%";
			else
			{
				i_ostream << "%, remaining time estimate: ";
				write_duration(i_ostream, remaining);
			}
		}
		else
		{
			i_ostream << "completed in ";
			write_duration(i_ostream, elapsed.count());
		}
	}

	PrintScopeDuration::PrintScopeDuration(std::ostream & i_ostream, const char * i_label)
		: m_ostream(i_ostream), m_label(i_label)
	{
		m_ostream << "starting " << m_label << "..." << std::endl;
	}

	PrintScopeDuration::~PrintScopeDuration()
	{
		// compute the elapsed time (is seconds)
		using FpSeconds = std::chrono::duration<double, std::chrono::seconds::period>;
		auto const elapsed = static_cast<FpSeconds>(std::chrono::high_resolution_clock::now() - m_start_time);
		
		// write to the stream
		m_ostream << m_label << " completed in ";
		write_duration(m_ostream, elapsed.count());
		m_ostream << std::endl;
	}

	void write_duration(std::ostream & i_ostream, double i_seconds)
	{
		static const struct Unit
		{
			double m_seconds;
			const char * m_name;
		} units[] = {
			{60. * 60. * 24. * 365., " years"},
			{60. * 60. * 24., " days"},
			{60. * 60., " hours"},
			{60., " mins"},			
		};

		bool done = false;
		for (auto unit : units)
		{
			if (i_seconds > unit.m_seconds)
			{
				i_ostream << i_seconds / unit.m_seconds << unit.m_name;
				done = true;
				break;
			}
		}

		if (!done)
		{
			i_ostream << i_seconds << " secs";
		}
	}

} // namespace density_tests
