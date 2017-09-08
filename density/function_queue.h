
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/heter_queue.h>
#include <density/detail/function_queue_impl.h>

namespace density
{
    template < typename CALLABLE, typename ALLOCATOR_TYPE = void_allocator, function_type_erasure MODE = function_standard_erasure >
        class function_queue;

    /** Function queue */
    template < typename RET_VAL, typename... PARAMS, typename ALLOCATOR_TYPE, function_type_erasure MODE >
        class function_queue<RET_VAL (PARAMS...), ALLOCATOR_TYPE, MODE>
    {    
    private:
        using UnderlyingQueue = heter_queue<void, detail::FunctionRuntimeType<MODE, RET_VAL (PARAMS...)>, ALLOCATOR_TYPE>;
        UnderlyingQueue m_queue;

    public:

        /** Alias to heter_queue::put_transaction. */
        template <typename ELEMENT_COMPLETE_TYPE>
            using put_transaction = typename UnderlyingQueue::template put_transaction<ELEMENT_COMPLETE_TYPE>; 

        /** Alias to heter_queue::reentrant_put_transaction. */
        template <typename ELEMENT_COMPLETE_TYPE>
            using reentrant_put_transaction = typename UnderlyingQueue::template reentrant_put_transaction<ELEMENT_COMPLETE_TYPE>; 

        /** Adds at the end of the queue a callable object.
            
        See heter_queue::push for a detailed description.
            
        \snippet func_queue_examples.cpp function_queue push example 1
        \snippet func_queue_examples.cpp function_queue push example 2 */
        template <typename ELEMENT_COMPLETE_TYPE>
            void push(ELEMENT_COMPLETE_TYPE && i_source)
        {
            m_queue.push(std::forward<ELEMENT_COMPLETE_TYPE>(i_source));
        }

        /** Adds at the end of the queue a callable object of type <code>ELEMENT_COMPLETE_TYPE</code>, inplace-constructing it from
                a perfect forwarded parameter pack.
            \n <i>Note</i>: the template argument <code>ELEMENT_COMPLETE_TYPE</code> can't be deduced from the parameters so it must explicitly specified.

            See heter_queue::emplace for a detailed description.
            
        \snippet func_queue_examples.cpp function_queue emplace example 1 */
        template <typename ELEMENT_COMPLETE_TYPE, typename... CONSTRUCTION_PARAMS>
            void emplace(CONSTRUCTION_PARAMS && ... i_construction_params)
        {
            m_queue.template emplace<ELEMENT_COMPLETE_TYPE>(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...);
        }

        /** Begins a transaction that appends an element of type <code>ELEMENT_TYPE</code>, copy-constructing
                or move-constructing it from the source.

            See heter_queue::start_push for a detailed description.

            <b>Examples</b>
            \snippet func_queue_examples.cpp function_queue start_push example 1 */
        template <typename ELEMENT_TYPE>
            put_transaction<typename std::decay<ELEMENT_TYPE>::type> start_push(ELEMENT_TYPE && i_source)
        {
            return m_queue.start_push(std::forward<ELEMENT_TYPE>(i_source));
        }


        /** Begins a transaction that appends an element of a type <code>ELEMENT_TYPE</code>,
            inplace-constructing it from a perfect forwarded parameter pack.

            See heter_queue::start_emplace for a detailed description.

            <b>Examples</b>
            \snippet func_queue_examples.cpp function_queue start_emplace example 1 */
        template <typename ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
            put_transaction<ELEMENT_TYPE> start_emplace(CONSTRUCTION_PARAMS && ... i_construction_params)
        {
            return m_queue.template start_emplace<ELEMENT_TYPE>(std::forward<ELEMENT_TYPE>(i_construction_params)...);
        }

        /** Adds at the end of the queue a callable object.
            
        See heter_queue::reentrant_push for a detailed description.
            
        \snippet func_queue_examples.cpp function_queue reentrant_push example 1 */
        template <typename ELEMENT_COMPLETE_TYPE>
            void reentrant_push(ELEMENT_COMPLETE_TYPE && i_source)
        {
            m_queue.reentrant_push(std::forward<ELEMENT_COMPLETE_TYPE>(i_source));
        }

        /** Adds at the end of the queue a callable object of type <code>ELEMENT_COMPLETE_TYPE</code>, inplace-constructing it from
                a perfect forwarded parameter pack.
            \n <i>Note</i>: the template argument <code>ELEMENT_COMPLETE_TYPE</code> can't be deduced from the parameters so it must explicitly specified.

            See heter_queue::reentrant_emplace for a detailed description.
            
        \snippet func_queue_examples.cpp function_queue reentrant_emplace example 1 */
        template <typename ELEMENT_COMPLETE_TYPE, typename... CONSTRUCTION_PARAMS>
            void reentrant_emplace(CONSTRUCTION_PARAMS && ... i_construction_params)
        {
            m_queue.template reentrant_emplace<ELEMENT_COMPLETE_TYPE>(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...);
        }

        /** Begins a transaction that appends an element of type <code>ELEMENT_TYPE</code>, copy-constructing
                or move-constructing it from the source.

            See heter_queue::start_reentrant_push for a detailed description.

            <b>Examples</b>
            \snippet func_queue_examples.cpp function_queue start_reentrant_push example 1 */
        template <typename ELEMENT_TYPE>
            reentrant_put_transaction<typename std::decay<ELEMENT_TYPE>::type> start_reentrant_push(ELEMENT_TYPE && i_source)
        {
            return m_queue.start_reentrant_push(std::forward<ELEMENT_TYPE>(i_source));
        }


        /** Begins a transaction that appends an element of a type <code>ELEMENT_TYPE</code>,
            inplace-constructing it from a perfect forwarded parameter pack.

            See heter_queue::start_reentrant_emplace for a detailed description.

            <b>Examples</b>
            \snippet func_queue_examples.cpp function_queue start_reentrant_emplace example 1 */
        template <typename ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
            reentrant_put_transaction<ELEMENT_TYPE> start_reentrant_emplace(CONSTRUCTION_PARAMS && ... i_construction_params)
        {
            return m_queue.template start_reentrant_emplace<ELEMENT_TYPE>(std::forward<ELEMENT_TYPE>(i_construction_params)...);
        }

        /** If the queue is not empty, invokes the first function object of the queue and then deletes it 
            from the queue. Otherwise no operation is performed.

            @param i_params... parameters to be forwarded to the function object
            @return If RET_VAL is void, the return value is a boolean indicating whether a callable object was consumed.
                Otherwise the return value is an optional that contains the value returned by the callable object, or 
                an empty optional in case the queue was empty.

            This function is not reentrant: if the callable object accesses in any way this queue, the behavior
            is undefined. Use function_queue::try_reentrant_consume if you are not sure about what the callable object may do.

            \b Throws: unspecified
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects). 
            
            \snippet func_queue_examples.cpp function_queue try_consume example 1 */
        typename std::conditional<std::is_void<RET_VAL>::value, bool, optional<RET_VAL>>::type 
            try_consume(PARAMS &&... i_params)
        {
            return try_consume_impl(std::is_void<RET_VAL>(), std::forward<PARAMS>(i_params)...);
        }

        /** If the queue is not empty, invokes the first function object of the queue and then deletes it 
            from the queue. Otherwise no operation is performed.

            @param i_params... parameters to be forwarded to the function object
            @return If RET_VAL is void, the return value is a boolean indicating whether a callable object was consumed.
                Otherwise the return value is an optional that contains the value returned by the callable object, or 
                an empty optional in case the queue was empty.

            This function is reentrant: the callable object may access in any way this queue.

            \b Throws: unspecified
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).
            
            \snippet func_queue_examples.cpp function_queue try_reentrant_consume example 1 */
        typename std::conditional<std::is_void<RET_VAL>::value, bool, optional<RET_VAL>>::type 
            try_reentrant_consume(PARAMS &&... i_params)
        {
            return try_reentrant_consume_impl(std::is_void<RET_VAL>(), std::forward<PARAMS>(i_params)...);
        }

        /** Deletes all the function objects in the queue.
            \n<b> Effects on iterators </b>: all the iterators are invalidated
            \n\b Throws: nothing
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects).
            \n\b Complexity: linear. */
        void clear() noexcept
        {
            DENSITY_ASSERT(MODE != function_manual_clear);
            m_queue.clear();
        }

        /** Returns whether this container is empty */
        bool empty() noexcept
        {
            return m_queue.empty();
        }

    private:

        optional<RET_VAL> try_consume_impl(std::false_type, PARAMS &&... i_params)
        {
            auto cons = m_queue.try_start_consume();
            if (cons)
            {
                auto && result = cons.complete_type().align_invoke_destroy(
                    cons.unaligned_element_ptr(), std::forward<PARAMS>(i_params)...);
                cons.commit_nodestroy();
                return make_optional<RET_VAL>(std::forward<RET_VAL>(result));
            }
            else
            {
                return optional<RET_VAL>();
            }
        }

        bool try_consume_impl(std::true_type, PARAMS &&... i_params)
        {
            auto cons = m_queue.try_start_consume();
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

        optional<RET_VAL> try_reentrant_consume_impl(std::false_type, PARAMS &&... i_params)
        {
            auto cons = m_queue.try_start_reentrant_consume();
            if (cons)
            {
                auto && result = cons.complete_type().align_invoke_destroy(
                    cons.unaligned_element_ptr(), std::forward<PARAMS>(i_params)...);
                cons.commit_nodestroy();
                return make_optional<RET_VAL>(std::forward<RET_VAL>(result));
            }
            else
            {
                return optional<RET_VAL>();
            }
        }

        bool try_reentrant_consume_impl(std::true_type, PARAMS &&... i_params)
        {
            auto cons = m_queue.try_start_reentrant_consume();
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
    };

} // namespace density
