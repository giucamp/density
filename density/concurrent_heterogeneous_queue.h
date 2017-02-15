
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/heterogeneous_queue.h>
#include <mutex> // for std::lock_guard and std::unique_lock

namespace density
{
	/** \brief Concurrent heterogeneous FIFO container-like class template. 
	
	*/
	template < typename COMMON_TYPE = void, typename TYPE_ERASER_TYPE = runtime_type<COMMON_TYPE>, typename ALLOCATOR_TYPE = void_allocator >
		class concurrent_heterogeneous_queue : private heterogeneous_queue<COMMON_TYPE, TYPE_ERASER_TYPE, ALLOCATOR_TYPE>
	{
	private:
		using heter_queue = heterogeneous_queue<COMMON_TYPE, TYPE_ERASER_TYPE, ALLOCATOR_TYPE>;

	public:
		
		static_assert(std::is_same<COMMON_TYPE, typename TYPE_ERASER_TYPE::common_type>::value,
			"COMMON_TYPE and TYPE_ERASER_TYPE::common_type must be the same type (did you try to use a type like heter_cont<A,runtime_type<B>>?)");

		static_assert(std::is_same<COMMON_TYPE, typename std::decay<COMMON_TYPE>::type>::value,
			"COMMON_TYPE can't be cv-qualified, an array or a reference");

		using common_type = COMMON_TYPE;
		using type_eraser_type = TYPE_ERASER_TYPE;
		using value_type = std::pair<const type_eraser_type &, common_type* const>;
		using allocator_type = ALLOCATOR_TYPE;
		using pointer = value_type *;
		using const_pointer = const value_type *;
		using reference = value_type;
		using const_reference = const value_type&;
		using size_type = std::size_t;
		using difference_type = std::ptrdiff_t;

		class put_transaction;
		template <typename TYPE> class typed_put_transaction;
		class reentrant_put_transaction;
		template <typename TYPE> class reentrant_typed_put_transaction;
		class iterator;
		class const_iterator;

		/** Same to heterogeneous_queue::min_alignment. */
		constexpr static size_t min_alignment = heter_queue::min_alignment;

        /** Default constructor. Same to heterogeneous_queue::heterogeneous_queue(). 
		
			\n <b>Thread safeness</b>: NOT thread safe. */
		concurrent_heterogeneous_queue() = default;

        /** Move constructor. Same to heterogeneous_queue::heterogeneous_queue(heterogeneous_queue &&).

			\n <b>Thread safeness</b>: NOT thread safe. */
		concurrent_heterogeneous_queue(concurrent_heterogeneous_queue && i_source) noexcept
			: heter_queue(std::move(i_source))
		{
		}

        /** Copy constructor. Same to heterogeneous_queue::heterogeneous_queue(const heterogeneous_queue &).
		
			\n <b>Thread safeness</b>: NOT thread safe. */
        concurrent_heterogeneous_queue(const concurrent_heterogeneous_queue & i_source)
			: heter_queue(i_source)
        {
        }

        /** Move assignment. Same to heterogeneous_queue::operator = (heterogeneous_queue &&).
		
			\n <b>Thread safeness</b>: NOT thread safe. */
        concurrent_heterogeneous_queue & operator = (concurrent_heterogeneous_queue && i_source) noexcept
        {
			heter_queue::operator = std::move(i_source);
			return *this;
        }

        /** Copy assignment. Same to heterogeneous_queue::operator = (const heterogeneous_queue &).
		
			\n <b>Thread safeness</b>: NOT thread safe. */
        concurrent_heterogeneous_queue & operator = (const concurrent_heterogeneous_queue & i_source)
        {
			heter_queue::operator = (i_source);
			return *this;
        }

        /** Swaps the content of this queue and another one. Same to heterogeneous_queue::swap.
		
			\n <b>Thread safeness</b>: NOT thread safe. */
        void swap(concurrent_heterogeneous_queue & i_other) noexcept
        {
			heter_queue::swap(i_source);
        }

        /** Destructor.

            \n <b>Thread safeness</b>: NOT thread safe. */
		~concurrent_heterogeneous_queue() noexcept = default;

        /** Same to heterogeneous_queue::empty.

			\n <b>Thread safeness</b>: thread safe. */
        bool empty() const noexcept
        {
			std::lock_guard<sync::mutex> lock(m_mutex);
            return heter_queue::empty();
        }

        /** Same to heterogeneous_queue::clear.

            \n <b>Thread safeness</b>: thread safe. */
        void clear() noexcept
        {
			std::lock_guard<sync::mutex> lock(m_mutex);
			heter_queue::clear();
        }

        /** Same to heterogeneous_queue::push.

            \n <b>Thread safeness</b>: thread safe. */
        template <typename ELEMENT_TYPE>
            void push(ELEMENT_TYPE && i_source)
        {
            return emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<ELEMENT_TYPE>(i_source));
        }

        /** Same to heterogeneous_queue::emplace.

            \n <b>Thread safeness</b>: thread safe. */
        template <typename ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
            void emplace(CONSTRUCTION_PARAMS && ... i_construction_params)
        {
            start_emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...).commit();
        }

        /** Same to heterogeneous_queue::dyn_push.

            \n <b>Thread safeness</b>: thread safe. */
        void dyn_push(const type_eraser_type & i_type)
        {
            start_dyn_push(i_type).commit();
        }

        /** Same to heterogeneous_queue::dyn_push_copy.

            \n <b>Thread safeness</b>: thread safe. */
        void dyn_push_copy(const type_eraser_type & i_type, const COMMON_TYPE * i_source)
        {
            start_dyn_push_copy(i_type, i_source).commit();
        }

        /** Same to heterogeneous_queue::dyn_push_move.

            \n <b>Thread safeness</b>: thread safe. */
        void dyn_push_move(const type_eraser_type & i_type, COMMON_TYPE * i_source)
        {
            start_dyn_push_move(i_type, i_source).commit();
        }

        /** Same to heterogeneous_queue::start_push.

            \n <b>Thread safeness</b>: thread safe. */
        template <typename ELEMENT_TYPE>
            typed_put_transaction<ELEMENT_TYPE> start_push(ELEMENT_TYPE && i_source)
        {
            return start_emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<ELEMENT_TYPE>(i_source));
        }

        /** Same to heterogeneous_queue::start_emplace.

            \n <b>Thread safeness</b>: thread safe. */
        template <typename ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
            typed_put_transaction<ELEMENT_TYPE> start_emplace(CONSTRUCTION_PARAMS && ... i_construction_params)
        {
			std::unique_lock<sync::mutex> lock(m_mutex);
            return typed_put_transaction<ELEMENT_TYPE>(std::move(lock), 
				heter_queue::template start_emplace<ELEMENT_TYPE>(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...));
        }

        /** Same to heterogeneous_queue::start_dyn_push.

            \n <b>Thread safeness</b>: thread safe. */
        put_transaction start_dyn_push(const type_eraser_type & i_type)
        {
			std::unique_lock<sync::mutex> lock(m_mutex);
            return put_transaction(std::move(lock), heter_queue::start_dyn_push(i_type));
        }

        /** Same to heterogeneous_queue::start_dyn_push_copy.

            \n <b>Thread safeness</b>: thread safe. */
        put_transaction start_dyn_push_copy(const type_eraser_type & i_type, const COMMON_TYPE * i_source)
        {
			std::unique_lock<sync::mutex> lock(m_mutex);
			return put_transaction(std::move(lock), heter_queue::start_dyn_push_copy(i_type, i_source));
        }

        /** Same to heterogeneous_queue::start_dyn_push_move.

            \n <b>Thread safeness</b>: thread safe. */
        put_transaction start_dyn_push_move(const type_eraser_type & i_type, COMMON_TYPE * i_source)
        {
			std::unique_lock<sync::mutex> lock(m_mutex);
			return put_transaction(std::move(lock), heter_queue::start_dyn_push_move(i_type, i_source));
        }

		/** Same to heterogeneous_queue::put_transaction. */
        class put_transaction
        {
        public:

            /** Copy construction is not allowed */
            put_transaction(const put_transaction &) = delete;

            /** Copy assignment is not allowed */
            put_transaction & operator = (const put_transaction &) = delete;

			/** Same to heterogeneous_queue::put_transaction::put_transaction(put_transaction &&). */
			put_transaction(put_transaction && i_source) noexcept = default;

			/** Same to heterogeneous_queue::put_transaction::operator = (put_transaction &&). */
			put_transaction & operator = (put_transaction && i_source) noexcept
			{
				static_assert(noexcept(m_lock = std::move(i_source.m_lock)), "1");
				static_assert(noexcept(m_put_transaction = std::move(i_source.m_put_transaction)), "2");

				m_lock = std::move(i_source.m_lock);
				m_put_transaction = std::move(i_source.m_put_transaction);
				return *this;
			}

			/** Same to heterogeneous_queue::put_transaction::raw_allocate. */
            void * raw_allocate(size_t i_size, size_t i_alignment = alignof(std::max_align_t))
            {
				DENSITY_ASSERT_INTERNAL(m_lock.owns_lock());
                return m_put_transaction.raw_allocate(i_size, i_alignment);
            }

			/** Same to heterogeneous_queue::put_transaction::raw_allocate_copy. */
            template <typename INPUT_ITERATOR>
                typename std::iterator_traits<INPUT_ITERATOR>::value_type *
                    raw_allocate_copy(INPUT_ITERATOR i_begin, INPUT_ITERATOR i_end)
            {
				DENSITY_ASSERT_INTERNAL(m_lock.owns_lock());
                return m_put_transaction.raw_allocate_copy(i_size, i_alignment);
            }

			/** Same to heterogeneous_queue::put_transaction::raw_allocate_copy. */
            template <typename INPUT_RANGE>
                auto raw_allocate_copy(const INPUT_RANGE & i_source_range)
                    -> decltype(raw_allocate_copy(std::begin(i_source_range), std::end(i_source_range)))
            {
				DENSITY_ASSERT_INTERNAL(m_lock.owns_lock());
                return raw_allocate_copy(std::begin(i_source_range), std::end(i_source_range));
            }

			/** Same to heterogeneous_queue::put_transaction::commit. */
            void commit() noexcept
            {
				DENSITY_ASSERT_INTERNAL(m_lock.owns_lock());
				m_put_transaction.commit();
				m_lock.unlock();
            }

			/** Same to heterogeneous_queue::put_transaction::cancel. */
			void cancel() noexcept
			{
				DENSITY_ASSERT_INTERNAL(m_lock.owns_lock());
				m_put_transaction.cancel();
				m_lock.unlock();
			}

			/** Same to heterogeneous_queue::put_transaction::empty. */
            bool empty() const noexcept 
			{
				bool const is_empty = m_put_transaction.empty();
				DENSITY_ASSERT_INTERNAL(is_empty == m_lock.owns_lock());
				return is_empty;
			}

			/** Same to heterogeneous_queue::put_transaction::element_ptr. */
            COMMON_TYPE * element_ptr() const noexcept
            {
				DENSITY_ASSERT_INTERNAL(m_lock.owns_lock());
                return m_put_transaction.element_ptr();
            }

			/** Same to heterogeneous_queue::put_transaction::type. */
            const TYPE_ERASER_TYPE & type() const noexcept
            {
				DENSITY_ASSERT_INTERNAL(m_lock.owns_lock());
				return m_put_transaction.type();
            }

			/** Destructor. */
            ~put_transaction()
            {
				if (!m_put_transaction.empty())
				{
					m_put_transaction.cancel();
					m_lock.unlock();
				}
            }

            #ifndef DOXYGEN_DOC_GENERATION

                // not part of the public interface
                put_transaction(std::unique_lock<sync::mutex> && i_lock, typename heter_queue::put_transaction && i_put_transaction) noexcept
                    : m_lock(std::move(i_lock)), m_put_transaction(std::move(i_put_transaction))
                {
				}

            #endif // #ifndef DOXYGEN_DOC_GENERATION

        private:			
			std::unique_lock<sync::mutex> m_lock;
			typename heter_queue::put_transaction m_put_transaction;
        };


		/** This class is used as return type from put functions with an element type known at compile time.
			
			typed_put_transaction derives from put_transaction adding just an element_ptr() that returns a
			pointer of correct the type. */
        template <typename TYPE>
            class typed_put_transaction : public put_transaction
        {
        public:

            using put_transaction::put_transaction;

            /** Returns a pointer to the object being added.
                \n <i>Note</i>: the object is constructed at the begin of the transaction, so this
                    function always returns a pointer to a valid object.

				\pre The behavior is undefined if either:
					- this transaction is empty */
            TYPE * element_ptr() const noexcept
                { return static_cast<TYPE *>(put_transaction::element_ptr()); }
        };

        /** Move-only class that holds the state of a consumes, or is empty. */
        class consume_operation
        {
        public:

            /** Copy construction is not allowed */
            consume_operation(const consume_operation &) = delete;

            /** Copy assignment is not allowed */
            consume_operation & operator = (const consume_operation &) = delete;

            /** Move constructor. The source is left empty. */
			consume_operation(consume_operation && i_source) noexcept = default;

            /** Move assignment. The source is left empty. */
            consume_operation & operator = (consume_operation && i_source) noexcept = default;

            /** Destructor: cancel the transaction. */
            ~consume_operation()
            {
                if(!m_consume_operation.empty())
                {
					m_consume_operation.cancel();
					m_lock.unlock();
                }
            }

			/** Same to heterogeneous_queue::consume_operation::empty. */
            bool empty() const noexcept
			{
				bool const is_empty = m_consume_operation.empty();
				DENSITY_ASSERT_INTERNAL(is_empty == m_lock.owns_lock());
				return is_empty;
			}

            /** Returns true whether this object does not hold the state of a transaction. Same to !consume_operation::empty. */
            explicit operator bool() const noexcept
            {
                return !empty();
            }

			/** Same to heterogeneous_queue::consume_operation::commit. */
            void commit() noexcept
            {
				DENSITY_ASSERT_INTERNAL(m_lock.owns_lock());
				m_consume_operation.commit();
				m_lock.unlock();
            }

			/** Same to heterogeneous_queue::consume_operation::complete_type. */
            const TYPE_ERASER_TYPE & complete_type() const noexcept
            {
				DENSITY_ASSERT_INTERNAL(m_lock.owns_lock());
                return m_consume_operation.complete_type();
            }

			/** Same to heterogeneous_queue::consume_operation::unaligned_element_ptr. */
            void * unaligned_element_ptr() const noexcept
            {
				DENSITY_ASSERT_INTERNAL(m_lock.owns_lock());
				return m_consume_operation.unaligned_element_ptr();
            }

			/** Same to heterogeneous_queue::consume_operation::element_ptr. */
            COMMON_TYPE * element_ptr() const noexcept
            {
				DENSITY_ASSERT_INTERNAL(m_lock.owns_lock());
				return m_consume_operation.element_ptr();
            }

            #ifndef DOXYGEN_DOC_GENERATION

                // not part of the public interface
                consume_operation(std::unique_lock<sync::mutex> && i_lock, typename heter_queue::consume_operation && i_consume_operation) noexcept
                    : m_lock(std::move(i_lock)), m_consume_operation(std::move(i_consume_operation))
                {
                }

            #endif // #ifndef DOXYGEN_DOC_GENERATION

        private:
			std::unique_lock<sync::mutex> m_lock;
			typename heter_queue::consume_operation m_consume_operation;
        };

		/** Same to heterogeneous_queue::pop. */
        void pop() noexcept
        {
			std::lock_guard<sync::mutex> lock(m_mutex);
			heter_queue::pop();
        }

		/** Same to heterogeneous_queue::pop_if_any. */
        bool pop_if_any() noexcept
        {
			std::lock_guard<sync::mutex> lock(m_mutex);
			return heter_queue::pop_if_any();
        }

		/** Same to heterogeneous_queue::consume. */
        template <typename FUNC>
            auto consume(FUNC && i_func)
                noexcept(noexcept(i_func(std::declval<const TYPE_ERASER_TYPE &>(), std::declval<COMMON_TYPE *>())))
                    -> unvoid_t<decltype(i_func(std::declval<const TYPE_ERASER_TYPE &>(), std::declval<COMMON_TYPE *>()))>
        {
			std::lock_guard<sync::mutex> lock(m_mutex);
			return heter_queue::consume(i_func);
        }

		/** Same to heterogeneous_queue::consume_if_any. */
        template <typename FUNC>
            auto consume_if_any(FUNC && i_func)
                noexcept(noexcept((i_func(std::declval<const TYPE_ERASER_TYPE>(), std::declval<COMMON_TYPE *>()))))
                    -> optional<unvoid_t<decltype(i_func(std::declval<const TYPE_ERASER_TYPE &>(), std::declval<COMMON_TYPE *>()))>>
        {
			std::lock_guard<sync::mutex> lock(m_mutex);
			return heter_queue::consume_if_any(i_func);
        }

		/** Same to heterogeneous_queue::start_consume. */
        consume_operation start_consume() noexcept
        {
			std::unique_lock<sync::mutex> lock(m_mutex);
            return consume_operation(std::move(lock), heter_queue::start_consume());
        }


                    // reentrant

		/** Same to heterogeneous_queue::reentrant_push. */
        template <typename ELEMENT_TYPE>
            void reentrant_push(ELEMENT_TYPE && i_source)
        {
            return reentrant_emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<ELEMENT_TYPE>(i_source));
        }

		/** Same to heterogeneous_queue::reentrant_emplace. */
        template <typename ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
            void reentrant_emplace(CONSTRUCTION_PARAMS &&... i_construction_params)
        {
            start_reentrant_emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...).commit();
        }

		/** Same to heterogeneous_queue::reentrant_dyn_push. */
        void reentrant_dyn_push(const type_eraser_type & i_type)
        {
            start_reentrant_dyn_push(i_type).commit();
        }

		/** Same to heterogeneous_queue::reentrant_dyn_push_copy. */
        void reentrant_dyn_push_copy(const type_eraser_type & i_type, const COMMON_TYPE * i_source)
        {
            start_reentrant_dyn_push_copy(i_type, i_source).commit();
        }

		/** Same to heterogeneous_queue::reentrant_dyn_push_move. */
        void reentrant_dyn_push_move(const type_eraser_type & i_type, COMMON_TYPE * i_source)
        {
            start_reentrant_dyn_push_move(i_type, i_source).commit();
        }

		/** Same to heterogeneous_queue::start_reentrant_push. */
        template <typename ELEMENT_TYPE>
            reentrant_typed_put_transaction<ELEMENT_TYPE> start_reentrant_push(ELEMENT_TYPE && i_source)
        {
            return start_reentrant_emplace<typename std::decay<ELEMENT_TYPE>::type>(std::forward<ELEMENT_TYPE>(i_source));
        }

		/** Same to heterogeneous_queue::start_reentrant_emplace. */
        template <typename ELEMENT_TYPE, typename... CONSTRUCTION_PARAMS>
            reentrant_typed_put_transaction<ELEMENT_TYPE> start_reentrant_emplace(CONSTRUCTION_PARAMS && ... i_construction_params)
        {
			{
				std::lock_guard<sync::mutex> lock(m_mutex);
				auto res = heter_queue::start_reentrant_emplace(std::forward<CONSTRUCTION_PARAMS>(i_construction_params)...);
			}
            return reentrant_typed_put_transaction<ELEMENT_TYPE>(res);
        }

		/** Same to heterogeneous_queue::start_reentrant_dyn_push. */
        reentrant_put_transaction start_reentrant_dyn_push(const type_eraser_type & i_type)
        {
			{
				std::lock_guard<sync::mutex> lock(m_mutex);
				auto res = heter_queue::start_reentrant_dyn_push(i_type);
			}
			return reentrant_put_transaction(res);
        }


		/** Same to heterogeneous_queue::start_reentrant_dyn_push_copy. */
        reentrant_put_transaction start_reentrant_dyn_push_copy(const type_eraser_type & i_type, const COMMON_TYPE * i_source)
        {
			{
				std::lock_guard<sync::mutex> lock(m_mutex);
				auto res = heter_queue::start_reentrant_dyn_push_copy(i_type, i_source);
			}
			return reentrant_put_transaction(res);
        }

		/** Same to heterogeneous_queue::start_reentrant_dyn_push_move. */
        reentrant_put_transaction start_reentrant_dyn_push_move(const type_eraser_type & i_type, COMMON_TYPE * i_source)
        {
			{
				std::lock_guard<sync::mutex> lock(m_mutex);
				auto res = heter_queue::start_reentrant_dyn_push_move(i_type, i_source);
			}
			return reentrant_put_transaction(res);
        }

        /** Same to heterogeneous_queue::reentrant_put_transaction. */
        class reentrant_put_transaction
        {
        public:

            /** Copy construction is not allowed */
            reentrant_put_transaction(const reentrant_put_transaction &) = delete;

            /** Copy assignment is not allowed */
            reentrant_put_transaction & operator = (const reentrant_put_transaction &) = delete;

            /** Move constructs a reentrant_put_transaction, transferring the state from the source.
                    @i_source source to move from. It becomes empty after the call. */
			reentrant_put_transaction(reentrant_put_transaction && i_source) noexcept = default;

            /** Move assigns a reentrant_put_transaction, transferring the state from the source.
                @i_source source to move from. It becomes empty after the call. */
            reentrant_put_transaction & operator = (reentrant_put_transaction && i_source) noexcept = default;

			/** Same to heterogeneous_queue::reentrant_put_transaction::raw_allocate. */
            void * raw_allocate(size_t i_size, size_t i_alignment = alignof(std::max_align_t))
            {
				std::lock_guard<sync::mutex> lock(static_cast<concurrent_heterogeneous_queue&>(m_put_transaction.queue()).m_mutex);
				return m_put_transaction.raw_allocate(i_size, i_alignment);
            }

			/** Same to heterogeneous_queue::reentrant_put_transaction::commit. */
            void commit() noexcept
            {
				std::lock_guard<sync::mutex> lock(static_cast<concurrent_heterogeneous_queue&>(m_put_transaction.queue()).m_mutex);
				m_put_transaction.commit();
            }

			/** Same to heterogeneous_queue::reentrant_put_transaction::cancel. */
			void cancel() noexcept
			{
				std::lock_guard<sync::mutex> lock(static_cast<concurrent_heterogeneous_queue&>(m_put_transaction.queue()).m_mutex);
				m_put_transaction.cancel();
			}

			/** Same to heterogeneous_queue::reentrant_put_transaction::empty. */
            bool empty() const noexcept { return m_put_transaction.empty(); }

			/** Same to heterogeneous_queue::reentrant_put_transaction::element_ptr. */
            COMMON_TYPE * element_ptr() const noexcept
            {
                return m_put_transaction.element_ptr();
            }

			/** Same to heterogeneous_queue::type. */
            const TYPE_ERASER_TYPE & type() const noexcept
            {
				return m_put_transaction.type();
            }

            /** Same to heterogeneous_queue::reentrant_put_transaction::~reentrant_put_transaction. */
            ~reentrant_put_transaction()
            {
                if (!m_put_transaction.empty())
                {
					cancel();
                }
            }

            #ifndef DOXYGEN_DOC_GENERATION

                // not part of the public interface
                reentrant_put_transaction(typename heter_queue::reentrant_put_transaction && i_put_transaction)
                    : m_put_transaction(std::move(i_put_transaction))
                        { }

            #endif // #ifndef DOXYGEN_DOC_GENERATION

        private:
			typename heter_queue::reentrant_put_transaction m_put_transaction;
        };

        template <typename TYPE>
            class reentrant_typed_put_transaction : public reentrant_put_transaction
        {
        public:

            using reentrant_put_transaction::reentrant_put_transaction;

            TYPE * element_ptr() const noexcept
                { return static_cast<TYPE *>(reentrant_put_transaction::element_ptr()); }
        };

	private:
		heter_queue & inner_queue() { return *this; }
		const heter_queue & inner_queue() const { return *this; }

	private:
		sync::mutex m_mutex;
	};

} // namespace density
