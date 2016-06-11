#pragma once
#include "density_common.h"

namespace density
{
	class Overflow : public std::exception
    {
    public:
        using std::exception::exception;
    };

	namespace detail
	{
		inline void handle_pointer_overflow(bool i_overflow)
		{
			if (i_overflow)
			{
				throw Overflow("pointer overflow");
			}
		}
	}

    template <typename UINT>
        class BasicMemSize
    {
    private:
        UINT m_value;

    public:

        static_assert( std::is_integral<UINT>::value && std::is_unsigned<UINT>::value, "UINT must be an unsigned integer" );

        BasicMemSize() DENSITY_NOEXCEPT : m_value(0) { }

        explicit BasicMemSize(UINT i_value) DENSITY_NOEXCEPT : m_value(i_value) { }

        bool operator == (const BasicMemSize & i_source) const DENSITY_NOEXCEPT { return m_value == i_source.m_value; }
        bool operator != (const BasicMemSize & i_source) const DENSITY_NOEXCEPT { return m_value != i_source.m_value; }
        bool operator > (const BasicMemSize & i_source) const DENSITY_NOEXCEPT { return m_value > i_source.m_value; }
        bool operator >= (const BasicMemSize & i_source) const DENSITY_NOEXCEPT { return m_value >= i_source.m_value; }
        bool operator < (const BasicMemSize & i_source) const DENSITY_NOEXCEPT { return m_value < i_source.m_value; }
        bool operator <= (const BasicMemSize & i_source) const DENSITY_NOEXCEPT { return m_value <= i_source.m_value; }

                // arithmetic operations

        BasicMemSize operator + (const BasicMemSize & i_source) const DENSITY_NOEXCEPT_IF(!DENSITY_POINTER_OVERFLOW_SAFE)
        {
            const UINT result = m_value + i_source.m_value;
            DENSITY_OVERFLOW_IF(result < m_value);
            return BasicMemSize(result);
        }

        BasicMemSize operator - (const BasicMemSize & i_source) const    DENSITY_NOEXCEPT_IF(!DENSITY_POINTER_OVERFLOW_SAFE)
        {
            DENSITY_OVERFLOW_IF(m_value < i_source.m_value);
            return BasicMemSize(m_value - i_source.m_value);
        }

        BasicMemSize operator * (UINT i_source) const DENSITY_NOEXCEPT_IF(!DENSITY_POINTER_OVERFLOW_SAFE)
        {
            /* see http://stackoverflow.com/questions/1815367/multiplication-of-large-numbers-how-to-catch-overflow
                using the approach of umull_overflow5, as most times the operands will be small. */
            const auto max_op = ( static_cast<UINT>(1) << (std::numeric_limits<UINT>::digits / 2) ) - 1;
            const auto max_uint = std::numeric_limits<UINT>::max();
            DENSITY_OVERFLOW_IF( ( m_value >= max_op || i_source >= max_op) &&
                i_source != 0 && max_uint / i_source < m_value );
            const UINT result = static_cast<UINT>(m_value * i_source);
            return BasicMemSize<UINT>(result);
        }

        BasicMemSize operator / (UINT i_source) const DENSITY_NOEXCEPT_IF(!DENSITY_POINTER_OVERFLOW_SAFE)
        {
            DENSITY_ASSERT(i_source != 0);
            DENSITY_OVERFLOW_IF( (m_value % i_source) != 0);
            return BasicMemSize(m_value / i_source);
        }


                // compound assignment

        BasicMemSize operator += (const BasicMemSize & i_source) DENSITY_NOEXCEPT_IF(!DENSITY_POINTER_OVERFLOW_SAFE)
        {
            const UINT result = m_value + i_source.m_value;
            DENSITY_OVERFLOW_IF(result < m_value);
            m_value = result;
            return *this;
        }
        BasicMemSize operator -= (const BasicMemSize & i_source) DENSITY_NOEXCEPT_IF(!DENSITY_POINTER_OVERFLOW_SAFE)
        {
            DENSITY_OVERFLOW_IF(m_value < i_source.m_value);
            m_value -= i_source.m_value;
            return *this;
        }
        BasicMemSize operator *= (UINT i_source) DENSITY_NOEXCEPT_IF(!DENSITY_POINTER_OVERFLOW_SAFE)
        {
            /* see http://stackoverflow.com/questions/1815367/multiplication-of-large-numbers-how-to-catch-overflow
                using the approach of umull_overflow5, as most times the operands will be small. */
            const auto max_op = (static_cast<UINT>(1) << (std::numeric_limits<UINT>::digits / 2)) - 1;
            const auto max_uint = std::numeric_limits<UINT>::max();
            DENSITY_OVERFLOW_IF((m_value >= max_op || i_source >= max_op) &&
                i_source != 0 && max_uint / i_source < m_value);
            m_value *= i_source;
            return *this;
        }

        BasicMemSize operator /= (UINT i_source) DENSITY_NOEXCEPT_IF(!DENSITY_POINTER_OVERFLOW_SAFE)
        {
            DENSITY_ASSERT(i_source != 0);
            DENSITY_OVERFLOW_IF((m_value % i_source) != 0);
            m_value /= i_source;
            return *this;
        }

        UINT value() const DENSITY_NOEXCEPT { return m_value; }

        bool is_valid_alignment() const DENSITY_NOEXCEPT
        {
            return m_value > 0 && (m_value & (m_value - 1)) == 0;
        }
    };


    using MemSize = BasicMemSize<size_t>;

    template <typename TYPE, typename UINT_REPRESENTATION>
        class BasicArithmeticPointer;

    template <typename UINT_REPRESENTATION>
        class BasicArithmeticPointer<void, UINT_REPRESENTATION>
    {
    private:
        UINT_REPRESENTATION m_value;

    public:

        BasicArithmeticPointer() DENSITY_NOEXCEPT
            : m_value(0) {}

        explicit BasicArithmeticPointer(nullptr_t) DENSITY_NOEXCEPT
            : m_value(0) {}

        explicit BasicArithmeticPointer(void * i_value) DENSITY_NOEXCEPT_IF((std::is_same<UINT_REPRESENTATION, uintptr_t>::value))
            : m_value(reinterpret_cast<UINT_REPRESENTATION>(i_value))
        {
            auto const is_safe = std::is_same<UINT_REPRESENTATION, uintptr_t>::value;
            if (!is_safe)
            {
                DENSITY_OVERFLOW_IF(reinterpret_cast<void*>(m_value) != i_value);
            }
        }

        BasicArithmeticPointer & operator = (nullptr_t) DENSITY_NOEXCEPT
        {
            m_value = 0;
            return *this;
        }

        bool operator == (const BasicArithmeticPointer & i_source) const DENSITY_NOEXCEPT { return m_value == i_source.m_value; }
        bool operator != (const BasicArithmeticPointer & i_source) const DENSITY_NOEXCEPT { return m_value != i_source.m_value; }
        bool operator > (const BasicArithmeticPointer & i_source) const DENSITY_NOEXCEPT { return m_value > i_source.m_value; }
        bool operator >= (const BasicArithmeticPointer & i_source) const DENSITY_NOEXCEPT { return m_value >= i_source.m_value; }
        bool operator < (const BasicArithmeticPointer & i_source) const DENSITY_NOEXCEPT { return m_value < i_source.m_value; }
        bool operator <= (const BasicArithmeticPointer & i_source) const DENSITY_NOEXCEPT { return m_value <= i_source.m_value; }

        BasicArithmeticPointer operator + (BasicMemSize<UINT_REPRESENTATION> i_value) const DENSITY_NOEXCEPT_IF(!DENSITY_POINTER_OVERFLOW_SAFE)
        {
            auto result = *this;
            result += i_value;
            return result;
        }

        BasicArithmeticPointer operator - (BasicMemSize<UINT_REPRESENTATION> i_value) const DENSITY_NOEXCEPT_IF(!DENSITY_POINTER_OVERFLOW_SAFE)
        {
            auto result = *this;
            result -= i_value;
            return result;
        }

        BasicArithmeticPointer & operator += (BasicMemSize<UINT_REPRESENTATION> i_value) DENSITY_NOEXCEPT_IF(!DENSITY_POINTER_OVERFLOW_SAFE)
        {
            auto const result = m_value + i_value.value();
            DENSITY_OVERFLOW_IF(result < m_value);
            m_value = result;
            return *this;
        }

        BasicArithmeticPointer & operator -= (BasicMemSize<UINT_REPRESENTATION> i_value) DENSITY_NOEXCEPT_IF(!DENSITY_POINTER_OVERFLOW_SAFE)
        {
            auto const result = m_value - i_value.value();
            DENSITY_OVERFLOW_IF(result > m_value);
            m_value = result;
            return *this;
        }

        BasicMemSize<UINT_REPRESENTATION> operator - (BasicArithmeticPointer i_pointer) const DENSITY_NOEXCEPT_IF(!DENSITY_POINTER_OVERFLOW_SAFE)
        {
            DENSITY_OVERFLOW_IF(m_value < i_pointer.m_value);
            return BasicMemSize<UINT_REPRESENTATION>(m_value - i_pointer.m_value);
        }

        void * value() const DENSITY_NOEXCEPT
        {
            return reinterpret_cast<void*>(m_value);
        }

        bool is_null() const DENSITY_NOEXCEPT
        {
            return m_value == 0;
        }

        BasicArithmeticPointer lower_align(BasicMemSize<UINT_REPRESENTATION> i_alignment) const DENSITY_NOEXCEPT
        {
            DENSITY_ASSERT(i_alignment.is_valid_alignment());

            auto const alignment_mask = i_alignment.value() - 1;

            return BasicArithmeticPointer(reinterpret_cast<void*>(
                m_value & (~alignment_mask) ) );
        }

        BasicArithmeticPointer upper_align(BasicMemSize<UINT_REPRESENTATION> i_alignment) const DENSITY_NOEXCEPT(!DENSITY_POINTER_OVERFLOW_SAFE)
        {
            DENSITY_ASSERT(i_alignment.is_valid_alignment());

            auto const alignment_mask = i_alignment.value() - 1;

            return BasicArithmeticPointer(reinterpret_cast<void*>(
                (m_value + alignment_mask) & (~alignment_mask)));
        }

        BasicArithmeticPointer linear_alloc(
            BasicMemSize<UINT_REPRESENTATION> i_size,
            BasicMemSize<UINT_REPRESENTATION> i_alignment) DENSITY_NOEXCEPT_IF(!DENSITY_POINTER_OVERFLOW_SAFE)
        {
            DENSITY_ASSERT(i_alignment.is_valid_alignment());

            auto const result = upper_align(i_alignment);
            *this = result + i_size;
            return result;
        }

        BasicArithmeticPointer linear_alloc(
            BasicMemSize<UINT_REPRESENTATION> i_size,
            BasicMemSize<UINT_REPRESENTATION> i_alignment,
            BasicArithmeticPointer i_end_address) DENSITY_NOEXCEPT_IF(!DENSITY_POINTER_OVERFLOW_SAFE)
        {
            DENSITY_ASSERT(i_alignment.is_valid_alignment() && m_value <= i_end_address.m_value );

            BasicArithmeticPointer result = upper_align(i_alignment);
            BasicArithmeticPointer new_ptr = result;
            new_ptr += i_size;
            if (new_ptr <= i_end_address)
            {
                m_value = new_ptr.m_value;
            }
            else
            {
                result.m_value = nullptr;
            }
            return result;
        }
    };

    template <typename TYPE>
        using ArithmeticPointer = BasicArithmeticPointer<TYPE, uintptr_t>;

    template <typename UINT>
        std::ostream & operator << (std::ostream & i_dest, const BasicMemSize<UINT> & i_source)
    {
        const char * suffixes[] = { " KiB", " MiB", " GiB", " TiB" };
        const char * suffixes_p[] = { " KiB(+", " MiB(+", " GiB(+", " TiB(+" };
        const char * suffixes_n[] = { " KiB(-", " MiB(-", " GiB(-", " TiB(-" };
        const double mults[] = { 1024., 1024.*1024., 1024.*1024.*1024., 1024.*1024.*1024.*1024. };

        unsigned prefix_index = 0;
        UINT value = i_source.value();
        while ((value >> 9) != 0 && prefix_index < 3)
        {
            value >>= 10;
            prefix_index++;
        }

        if (prefix_index == 0)
        {
            i_dest << value << "B";
        }
        else
        {
            prefix_index--;
            double d_val = round(i_source.value() / mults[prefix_index] * 100.) / 100.;
            const UINT as_uint = static_cast<UINT>( d_val * mults[prefix_index] );
            if (as_uint == i_source.value())
            {
                i_dest << d_val << suffixes[prefix_index];
            }
            else if (as_uint < i_source.value())
            {
                i_dest << d_val << suffixes_p[prefix_index] << i_source.value() - as_uint << ')';
            }
            else
            {
                i_dest << d_val << suffixes_n[prefix_index] << as_uint - i_source.value() << ')';
            }
        }
        return i_dest;
    }

    class MemStats
    {
    public:

        /** Total memory size requested to the allocator. This is similar to the capacity of an std::vector (except that it is expressed
            in bytes rather than in element count). */
        const MemSize & reserved_capacity() const    { return m_reserved_capacity; }

        /** Total memory size used to store the elements and the required overhead (like the space for types types) and padding (usualy to respect
            the alignment). The used size is always less than or equal to the reserved_capacity, and it is similar to the size of a vector
            (except that it is expressed in bytes rather than in element count). Adding new elements to the container makes the used size
            increase. If the new used size would exceed the reserved capacity, a reallocation will occur. */
        const MemSize & used_size() const            { return m_used_size; }

        /** Total space used for overhead (headers, footers, types). This size is a part of the used size. */
        const MemSize & overhead() const            { return m_overhead; }

        /** Total space wasted to respect the alignment of elements and overhead data. */
        const MemSize & padding() const                { return m_padding; }

        MemStats(const MemSize & i_reserved_capacity,
            const MemSize & i_used_size,
            const MemSize & i_overhead,
            const MemSize & i_padding)
            : m_reserved_capacity(i_reserved_capacity), m_used_size(i_used_size), m_overhead(i_overhead), m_padding(i_padding)
        {
        }

        MemStats & operator += (const MemStats & i_source)
        {
            m_reserved_capacity += i_source.m_reserved_capacity;
            m_used_size += i_source.m_used_size;
            m_overhead += i_source.m_overhead;
            m_padding += i_source.m_padding;
            return *this;
        }

        MemStats operator + (const MemStats & i_source) const
        {
            MemStats result = *this;
            result += i_source;
            return result;
        }

    private:
        MemSize m_reserved_capacity;
        MemSize m_used_size;
        MemSize m_overhead;
        MemSize m_padding;
    };

} // namespace density
