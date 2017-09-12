
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/conc_heter_queue.h>
#include <density/detail/function_runtime_type.h>

namespace density
{
    template < typename CALLABLE, typename ALLOCATOR_TYPE = void_allocator, function_type_erasure ERASURE = function_standard_erasure >
        class conc_function_queue;

    /** Thread-safe heterogeneous FIFO pseudo-container specialized to hold callable objects. conc_function_queue is an adaptor for conc_heter_queue.

        @tparam CALLABLE Signature required to the callable objects. Must be in the form RET_VAL (PARAMS...)
        @tparam ALLOCATOR_TYPE Allocator type to be used. This type must meet the requirements of both \ref UntypedAllocator_concept
                "UntypedAllocator" and \ref PagedAllocator_concept "PagedAllocator". The default is density::void_allocator.
        @tparam ERASURE Type erasure to use the callable objects. Must be a member of density::function_type_erasure. 

        conc_function_queue is basically a function_queue protected from data-races by a mutex. Non-reentrant operations (put or
        consumes) lock the mutex only once. Reentrant operations lock the mutex once when starting, and a second time to commit or cancel. Anyway
        in the middle of the operation the queue is not locked, and it can be used by the same thread or by other threads.
        \n <code>reentrant_put_transaction::commit</code>, <code>reentrant_put_transaction::cancel</code>, <code>reentrant_consume_operation::commit</code> and 
        <code>reentrant_consume_operation::cancel</code> are all noexcept functions that lock a mutex in a controlled context (no reentrancy, no callbacks,
        no possibility of deadlocks). The lock of the mutex should never throw in this context, but if for some reasons it does, the runtime will call std::terminate.
        
        If ERASURE == function_manual_clear, conc_function_queue is not able to destroy the callable objects without invoking them.
            This produces a performance benefit, but:
            - The function conc_function_queue::clear can't be used (calling it causes undefined behavior)
            - When the destructor of conc_function_queue is called, the queue must be already empty

        \n Implementation note: If ERASURE == function_manual_clear, a runtime type associated with a value is actually a pointer to a function 
            that invokes and destroys the callable object. In this case the size of a value is a pair of pointers, plus the capture (if any).
            \n If ERASURE == function_standard_erasure the runtime type has an additional pointer to a function that destroys the callable object
            without invoking it.

        \n <b>Thread safeness</b>: Put and consumes can be executed concurrently.
        \n <b>Exception safeness</b>: Any function of conc_function_queue is noexcept or provides the strong exception guarantee.
    */
    #ifndef DOXYGEN_DOC_GENERATION
        template < typename RET_VAL, typename... PARAMS, typename ALLOCATOR_TYPE, function_type_erasure ERASURE >
            class conc_function_queue<RET_VAL (PARAMS...), ALLOCATOR_TYPE, ERASURE>
    #else
        template < typename CALLABLE, typename ALLOCATOR_TYPE = void_allocator, function_type_erasure ERASURE = function_standard_erasure >
                class conc_function_queue
    #endif
    {    
    private:
        using UnderlyingQueue = conc_heter_queue<void, detail::FunctionRuntimeType<ERASURE, RET_VAL (PARAMS...)>, ALLOCATOR_TYPE>;
        UnderlyingQueue m_queue;

    public:

        /** Whether multiple threads can do put operations on the same queue without any further synchronization. */
        static constexpr bool concurrent_puts = true;

        /** Whether multiple threads can do consume operations on the same queue without any further synchronization. */
        static constexpr bool concurrent_consumes = true;

        /** Whether puts and consumes can be done concurrently without any further synchronization. In any case unsynchronized concurrency is
            constrained by concurrent_puts and concurrent_consumes. */
        static constexpr bool concurrent_put_consumes = true;

        /** Whether this queue is sequential consistent. */
        static constexpr bool is_seq_cst = true;

        /** Default constructor.
        
        \snippet conc_func_queue_examples.cpp conc_function_queue default construct example 1 */
        conc_function_queue() noexcept = default;

        /** Move constructor.
        
        \snippet conc_func_queue_examples.cpp conc_function_queue move construct example 1 */
        conc_function_queue(conc_function_queue && i_source) noexcept = default;

        /** Move assignment.
        
        \snippet conc_func_queue_examples.cpp conc_function_queue move assign example 1 */
        conc_function_queue & operator = (conc_function_queue && i_source) noexcept = default;

        /** Swaps two function queues. 
        
        \snippet conc_func_queue_examples.cpp conc_function_queue swap example 1 */
        friend void swap(conc_function_queue & i_first, conc_function_queue & i_second) noexcept
        {
            using std::swap;
            swap(i_first.m_queue, i_second.m_queue);
        }

        /** Destructor */
        ~conc_function_queue()
        {
            auto erasure = ERASURE;
            if (erasure == function_manual_clear)
            {
                DENSITY_ASSERT(empty());
            }
        }
        
        /** Alias to conc_heter_queue::put_transaction. */
        template <typename ELEMENT_COMPLETE_TYPE>
            using put_transaction = typename UnderlyingQueue::template put_transaction<ELEMENT_COMPLETE_TYPE>; 

        /** Alias to conc_heter_queue::reentrant_put_transaction. */
        template <typename ELEMENT_COMPLETE_TYPE>
            using reentrant_put_transaction = typename UnderlyingQueue::template reentrant_put_transaction<ELEMENT_COMPLETE_TYPE>; 

        /** Adds at the end of the queue a callable object.
            
        See conc_heter_queue::push for a detailed description.
            
        \snippet conc_func_queue_examples.cpp conc_function_queue push example 1
        \snippet conc_func_queue_examples.cpp conc_function_queue push example 2
        \snippet conc_func_queue_examples.cpp conc_function_queue push example 3 */
        template <typename ELEMENT_COMPLETE_TYPE>
            void push(ELEMENT_COMPLETE_TYPE && i_source)
        {
            m_queue.push(std::forward<ELEMENT_COMPLETE_TYPE>(i_source));
        }

        /** Adds at the end of the queue a callable object of type <code>ELEMENT_COMPLETE_TYPE</code>, inplace-constructing it from
                a perfect forwarded parameter pack.
            \n <i>Note</i>: the template argument <code>ELEMENT_COMPLETE_TYPE</code> can't be deduced from the parameters so it must explicitly specified.

            See conc_heter_queue::emplace for a detailed description.
            
        \snippet conc_func_queue_examples.cpp conc_function_queue emplace example 1 */
        template <typename ELEMENT_COMPLETE_TYPE, typename... CONSTRUCTION_PARAMS>
            void emplace(CONSTRUCTION_PARAMS && ... i_construction_params)
        {
            m_queue.template emplace<ELEMENT_COMPLETE_TYPE>(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...);
        }

        /** Begins a transaction that appends an element of type <code>ELEMENT_TYPE</code>, copy-constructing
                or move-constructing it from the source.

            See conc_heter_queue::start_push for a detailed description.

            <b>Examples</b>
            \snippet conc_func_queue_examples.cpp conc_function_queue start_push example 1 */
        template <typename ELEMENT_TYPE>
            put_transaction<typename std::decay<ELEMENT_TYPE>::type> start_push(ELEMENT_TYPE && i_source)
        {
            return m_queue.start_push(std::forward<ELEMENT_TYPE>(i_source));
        }


        /** Begins a transaction that appends an element of a type <code>ELEMENT_TYPE</code>,
            inplace-constructing it from a perfect forwarded parameter pack.

            See conc_heter_queue::start_emplace for a detailed description.

            <b>Examples</b>
            \snippet conc_func_queue_examples.cpp conc_function_queue start_emplace example 1 */
        template <typename ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
            put_transaction<ELEMENT_TYPE> start_emplace(CONSTRUCTION_PARAMS && ... i_construction_params)
        {
            return m_queue.template start_emplace<ELEMENT_TYPE>(std::forward<ELEMENT_TYPE>(i_construction_params)...);
        }

        /** Adds at the end of the queue a callable object.
            
        See conc_heter_queue::reentrant_push for a detailed description.
            
        \snippet conc_func_queue_examples.cpp conc_function_queue reentrant_push example 1 */
        template <typename ELEMENT_COMPLETE_TYPE>
            void reentrant_push(ELEMENT_COMPLETE_TYPE && i_source)
        {
            m_queue.reentrant_push(std::forward<ELEMENT_COMPLETE_TYPE>(i_source));
        }

        /** Adds at the end of the queue a callable object of type <code>ELEMENT_COMPLETE_TYPE</code>, inplace-constructing it from
                a perfect forwarded parameter pack.
            \n <i>Note</i>: the template argument <code>ELEMENT_COMPLETE_TYPE</code> can't be deduced from the parameters so it must explicitly specified.

            See conc_heter_queue::reentrant_emplace for a detailed description.
            
        \snippet conc_func_queue_examples.cpp conc_function_queue reentrant_emplace example 1 */
        template <typename ELEMENT_COMPLETE_TYPE, typename... CONSTRUCTION_PARAMS>
            void reentrant_emplace(CONSTRUCTION_PARAMS && ... i_construction_params)
        {
            m_queue.template reentrant_emplace<ELEMENT_COMPLETE_TYPE>(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...);
        }

        /** Begins a transaction that appends an element of type <code>ELEMENT_TYPE</code>, copy-constructing
                or move-constructing it from the source.

            See conc_heter_queue::start_reentrant_push for a detailed description.

            <b>Examples</b>
            \snippet conc_func_queue_examples.cpp conc_function_queue start_reentrant_push example 1 */
        template <typename ELEMENT_TYPE>
            reentrant_put_transaction<typename std::decay<ELEMENT_TYPE>::type> start_reentrant_push(ELEMENT_TYPE && i_source)
        {
            return m_queue.start_reentrant_push(std::forward<ELEMENT_TYPE>(i_source));
        }


        /** Begins a transaction that appends an element of a type <code>ELEMENT_TYPE</code>,
            inplace-constructing it from a perfect forwarded parameter pack.

            See conc_heter_queue::start_reentrant_emplace for a detailed description.

            <b>Examples</b>
            \snippet conc_func_queue_examples.cpp conc_function_queue start_reentrant_emplace example 1 */
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
            is undefined. Use conc_function_queue::try_reentrant_consume if you are not sure about what the callable object may do.

            \b Throws: unspecified
            \n <b>Exception guarantee</b>: strong (in case of exception the function has no observable effects). 
            
            \snippet conc_func_queue_examples.cpp conc_function_queue try_consume example 1 */
        typename std::conditional<std::is_void<RET_VAL>::value, bool, optional<RET_VAL>>::type 
            try_consume(PARAMS... i_params)
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
            
            \snippet conc_func_queue_examples.cpp conc_function_queue try_reentrant_consume example 1 */
        typename std::conditional<std::is_void<RET_VAL>::value, bool, optional<RET_VAL>>::type 
            try_reentrant_consume(PARAMS... i_params)
        {
            return try_reentrant_consume_impl(std::is_void<RET_VAL>(), std::forward<PARAMS>(i_params)...);
        }

        /** Deletes all the callable objects in the queue.
            
            \pre The behavior is undefined if either:
                - ERASURE is function_manual_clear
            
            \n<b> Effects on iterators </b>: all the iterators are invalidated
            \n\b Throws: nothing
            \n\b Complexity: linear. 
            
        \snippet conc_func_queue_examples.cpp conc_function_queue clear example 1 */
        void clear() noexcept
        {
            auto erasure = ERASURE;
            if (erasure == function_manual_clear)
            {
                DENSITY_ASSERT(false);
            }
            else
            {
                m_queue.clear();
            }
        }

        /** Returns whether this container is empty */
        bool empty() noexcept
        {
            return m_queue.empty();
        }

    private:

        optional<RET_VAL> try_consume_impl(std::false_type, PARAMS... i_params)
        {
            auto cons = m_queue.try_start_consume();
            if (cons)
            {
                auto && result = cons.complete_type().align_invoke_destroy(
                    cons.unaligned_element_ptr(), std::forward<PARAMS>(i_params)...);
                cons.commit_nodestroy();
                return optional<RET_VAL>(std::move(result));
            }
            else
            {
                return optional<RET_VAL>();
            }
        }

        bool try_consume_impl(std::true_type, PARAMS... i_params)
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

        optional<RET_VAL> try_reentrant_consume_impl(std::false_type, PARAMS... i_params)
        {
            auto cons = m_queue.try_start_reentrant_consume();
            if (cons)
            {
                auto && result = cons.complete_type().align_invoke_destroy(
                    cons.unaligned_element_ptr(), std::forward<PARAMS>(i_params)...);
                cons.commit_nodestroy();
                return optional<RET_VAL>(std::move(result));
            }
            else
            {
                return optional<RET_VAL>();
            }
        }

        bool try_reentrant_consume_impl(std::true_type, PARAMS... i_params)
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
