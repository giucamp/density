#pragma once
#include <atomic>
#include <chrono>
#include <ostream>
#include <string>

namespace density_tests
{
	/** This class tracks the progress of an operation. Internally is holds a constant size_t target count,
		and a mutable size_t current count. They are both size_t. The operation is considered complete when
		the current count reaches the target count. \n
		The user advance the progress with add_progress, which adds an user-supplied size_t to the current
		count. add_progress is thread safe: multiple threads can call it concurrently. */
	class Progress
	{
	private:
		std::atomic<size_t> m_curr_count{ 0 };
		size_t const m_target_count;
		double const m_target_count_reciprocal_times_100;
		std::chrono::high_resolution_clock::time_point const m_start_time = std::chrono::high_resolution_clock::now();

	public:

		/** Constructs a Progress, assigning the target count. The target count can't be changed. */
		Progress(size_t i_target_count) noexcept
			: m_target_count(i_target_count), m_target_count_reciprocal_times_100(100.0 / i_target_count)
		{

		}

		// copy and move not allowed
		Progress(const Progress &) = delete;
		Progress & operator = (const Progress &) = delete;

		/** Returns the current count. */
		size_t curr_count() const noexcept
		{
			return m_curr_count.load(std::memory_order_relaxed);
		}
		
		/** Returns the target count. */
		size_t target_count() const noexcept
		{
			return m_target_count;
		}

		/** Advance the progress. The provided parameter is added to the current count. */
		void add_progress(size_t i_count_to_add) noexcept
		{
			m_curr_count.fetch_add(i_count_to_add, std::memory_order_relaxed);
		}

		/** Returns whether the current count has reached the target count. */
		bool is_complete() const noexcept
		{
			return m_curr_count.load(std::memory_order_relaxed) >= m_target_count;
		}

		/** Returns whether the current count is greater than the target count, which may be considered
			an error, depending on the context. */
		bool did_overshot() const noexcept
		{
			return m_curr_count.load(std::memory_order_relaxed) > m_target_count;
		}

		/** Writes the progress in an human readable format. */
		void write_to_stream(std::ostream & i_ostream) const
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
					i_ostream << "%, remaining time estimate: " << remaining << " seconds";
			}
			else
			{
				i_ostream << "completed in " << elapsed.count() << " seconds";				
			}
		}

		/** Writes the progress in an human readable format. */
		template <typename OSTREAM>
			friend OSTREAM & operator << (OSTREAM & i_ostream, const Progress & i_progress)
		{
			i_progress.write_to_stream(i_ostream);
			return i_ostream;
		}
	};

	class DurationPrint
	{
	public:

		DurationPrint(std::ostream & i_ostream, const char * i_label)
			: m_ostream(i_ostream), m_label(i_label)
		{
			
		}

		DurationPrint(const DurationPrint &) = delete;
		DurationPrint & operator = (const DurationPrint &) = delete;
		
		~DurationPrint()
		{
			// compute the elapsed time (is seconds)
			using FpSeconds = std::chrono::duration<double, std::chrono::seconds::period>;
			auto const elapsed = static_cast<FpSeconds>(std::chrono::high_resolution_clock::now() - m_start_time);

			m_ostream << m_label << "(" << elapsed.count() << ")" << std::endl;
		}

	private:
		std::ostream & m_ostream;
		std::string const m_label;
		std::chrono::high_resolution_clock::time_point const m_start_time = std::chrono::high_resolution_clock::now();
	};

} // namespace density_tests