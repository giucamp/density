
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)


#pragma once
#include "paged_queue.h"

namespace density
{
    template < typename FUNCTION > class paged_function_queue;

	/** \brief Queue of callable objects (or function object).
		Every element in the queue is a type-erased callable object (like a std::function). Elements that can be added to
		the queue include:
			- lambda expressions
			- classes overloading the function call operator
			- the result a std::bind		
		
		This container is similar to a std::deque of std::function objects, but has more specialized storage strategy:
		the state of all the callable object is stored tightly and linearly in the memory.
        dense_function_queue internally uses a void paged_queue (a queue that can contain elements of any type).
		A dense_function_queue allocates one monolithic memory buffer with the provided allocator, and sub-allocates
        inplace its elements. This buffer is eventually reallocated to accomplish push and emplace requests.
        The memory management of this container is similar to an std::vector: since all the elements are
        stored in the same memory block, when a reallocation is performed, all the elements have to be moved.

        \n\b Thread safeness: None. The user is responsible to avoid race conditions.
        \n<b>Exception safeness</b>: Any function of dense_queue is dense_function_queue or provides the strong exception guarantee.
 
        There is not constant time function that gives the number of elements in a dense_queue in constant time,
        but std::distance will do (in linear time). Anyway dense_queue::mem_size, dense_queue::mem_capacity and
        dense_queue::empty work in constant time.
        Insertion is allowed only at the end (with the methods dense_queue::push or dense_queue::emplace).
        Removal is allowed only at the begin (with the methods dense_queue::pop or dense_queue::consume). */
    template < typename RET_VAL, typename... PARAMS >
        class paged_function_queue<RET_VAL (PARAMS...)> final
    {
    public:

        using value_type = RET_VAL(PARAMS...);

        template <typename ELEMENT_COMPLETE_TYPE>
            void push(ELEMENT_COMPLETE_TYPE && i_source)
        {
            m_queue.push(std::forward<ELEMENT_COMPLETE_TYPE>(i_source));
        }

        RET_VAL invoke_front(PARAMS... i_params)
        {
            auto first = m_queue.begin();
            return first.complete_type().template get_feature<typename detail::FeatureInvoke<value_type>>()(first.element(), i_params...);
        }

        RET_VAL consume_front(PARAMS... i_params)
        {
            return m_queue.manual_consume([&i_params...](const runtime_type<void, Features > & i_complete_type, void * i_element) {
                return i_complete_type.template get_feature<typename detail::FeatureInvokeDestroy<value_type>>()(i_element, i_params...);
            } );
        }

        bool empty() DENSITY_NOEXCEPT
        {
            return m_queue.empty();
        }

        void clear() DENSITY_NOEXCEPT
        {
            m_queue.clear();
        }

		void pop() DENSITY_NOEXCEPT
		{
			m_queue.pop();
		}

    private:
        using Features = detail::FeatureList<
            density::detail::FeatureSize, density::detail::FeatureAlignment,
            density::detail::FeatureCopyConstruct, density::detail::FeatureMoveConstruct,
            density::detail::FeatureDestroy, typename detail::FeatureInvoke<value_type>, typename detail::FeatureInvokeDestroy<value_type> >;
        paged_queue<void, page_allocator<std::allocator<char>>, runtime_type<void, Features > > m_queue;
    };

} // namespace density
