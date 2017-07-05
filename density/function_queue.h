
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/heterogeneous_queue.h>
#include <density/detail/function_queue_impl.h>

namespace density
{
	template < typename CALLABLE, typename ALLOCATOR_TYPE = void_allocator >
		using function_queue = detail::FunctionQueueImpl< heterogeneous_queue<void, detail::FunctionRuntimeType<CALLABLE>, ALLOCATOR_TYPE>, CALLABLE >;

#if 0

    /** Queue of callable objects (or function object).

        Every element in the queue is a type-erased callable object (like a std::function). Elements that can be added to
        the queue include:
            - lambda expressions
            - classes overloading the function call operator
            - the result a std::bind

        This container is similar to a std::deque of std::function objects, but has more specialized storage strategy:
        the state of all the callable object is stored tightly and linearly in the memory.

        The template argument FUNCTION is the function type used as signature for the function object contained in the queue.
        For example:

        \code{.cpp}
            function_queue_impl<void()> queue_1;
            queue_1.push([]() { std::cout << "hello!" << std::endl; });
            queue_1.consume_front();

            function_queue_impl<int(double, double)> queue_2;
            queue_2.push([](double a, double b) { return static_cast<int>(a + b); });
            std::cout << queue_2.consume_front(40., 2.) << std::endl;
        \endcode

        small_function_queue internally uses a void heterogeneous_queue (a queue that can contain elements of any type).
        function_queue_impl never moves or reallocates its elements, and has both better performances and better behavior
        respect to small_function_queue when the number of elements is not small.

        \n<b> Thread safeness</b>: None. The user is responsible to avoid race conditions.
        \n<b>Exception safeness</b>: Any function of function_queue_impl is noexcept or provides the strong exception guarantee.

        There is not a constant time function that gives the number of elements in a function_queue_impl in constant time,
        but std::distance will do (in linear time). Anyway function_queue_impl::mem_size, function_queue_impl::mem_capacity and
        function_queue_impl::empty work in constant time.
        Insertion is allowed only at the end (with the methods function_queue_impl::push or function_queue_impl::emplace).
        Removal is allowed only at the begin (with the methods function_queue_impl::pop or function_queue_impl::consume). */
    template < typename QUEUE, typename RET_VAL, typename... PARAMS >
        class function_queue_impl<RET_VAL (PARAMS...)> final
    {
    public:

        using value_type = RET_VAL(PARAMS...);

        /** Adds a new function at the end of queue.
            @param i_source object to be used as source to construct of new element.
                - If this argument is an l-value, the new element copy-constructed (and the source object is left unchanged).
                - If this argument is an r-value, the new element move-constructed (and the source object will have an undefined but valid content).

            \n\b Requires:
                - ELEMENT_COMPLETE_TYPE must be a callable object that has RET_VAL as return type and PARAMS... as parameters
                - the type ELEMENT_COMPLETE_TYPE must copy constructible (in case of l-value) or move constructible (in case of r-value)

            \n<b> Effects on iterators </b>: all the iterators are invalidated
            \n\b Throws: unspecified
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).
            \n\b Complexity: constant amortized (a reallocation may be required). */
        template <typename ELEMENT_COMPLETE_TYPE>
            void push(ELEMENT_COMPLETE_TYPE && i_source)
        {
            m_queue.push(std::forward<ELEMENT_COMPLETE_TYPE>(i_source));
        }

        /** Invokes the first function object of the queue and then deletes it from the queue.
            This function is equivalent to a call to invoke_front followed by a call to pop, but has better performance.
            If the queue is empty, the behavior is undefined.
                @param i_params... parameters to be passed to the function object
                @return the value returned by the function object.

            \n<b> Effects on iterators </b>: all the iterators are invalidated
            \n\b Throws: anything that the function object invocation throws
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).
            \n\b Complexity: constant. */
        optional_or_bool<RET_VAL> consume_front(PARAMS... i_params)
        {
			return consume_front_impl(std::is_void<RET_VAL>(), std::forward<PARAMS>(i_params)...);
        }

        /** Deletes all the function objects in the queue.
            \n<b> Effects on iterators </b>: all the iterators are invalidated
            \n\b Throws: nothing
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).
            \n\b Complexity: linear. */
        void clear() noexcept
        {
            m_queue.clear();
        }

        /** Returns whether this container is empty */
        bool empty() noexcept
        {
            return m_queue.empty();
        }

	private:

		optional<RET_VAL> consume_front_impl(std::false_type, PARAMS... i_params)
		{
			auto cons = m_queue.start_consume();
			if (cons)
			{
				auto && result = cons.complete_type().align_invoke_destroy(
					cons.unaligned_element_ptr(), std::forward<PARAMS>(i_params)...);
				cons.commit_nodestroy();
				return std::forward<RET_VAL>(result);
			}
			else
			{
				return optional<RET_VAL>();
			}
		}

		bool consume_front_impl(std::true_type, PARAMS... i_params)
		{
			auto cons = m_queue.start_consume();
			if (cons)
			{
				cons.complete_type().align_invoke_destroy(
					cons.unaligned_element_ptr(), std::forward<PARAMS>(i_params)...);
				cons.commit_nodestroy();
				return true;
			}
			else
			{
				return false;
			}
		}

    private:
        QUEUE m_queue;
    };

#endif

} // namespace density
