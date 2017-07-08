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
		Progress(size_t i_target_count) noexcept;

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

		/** Sets the progress. */
		void set_progress(size_t i_count) noexcept
		{
			m_curr_count.store(i_count, std::memory_order_relaxed);
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
		void write_to_stream(std::ostream & i_ostream) const;

		/** Writes the progress in an human readable format. */
		friend std::ostream & operator << (std::ostream & i_ostream, const Progress & i_progress)
		{
			i_progress.write_to_stream(i_ostream);
			return i_ostream;
		}
	};

	/** */
	class PrintScopeDuration
	{
	public:

		PrintScopeDuration(std::ostream & i_ostream, const char * i_label);

		PrintScopeDuration(const PrintScopeDuration &) = delete;
		PrintScopeDuration & operator = (const PrintScopeDuration &) = delete;
		
		~PrintScopeDuration();

	private:
		std::ostream & m_ostream;
		std::string const m_label;
		std::chrono::high_resolution_clock::time_point const m_start_time = std::chrono::high_resolution_clock::now();
	};

	void write_duration(std::ostream & i_ostream, double i_seconds);

} // namespace density_tests