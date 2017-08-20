
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/density_common.h>
#include <atomic>
#include <thread>
#include <type_traits>
#include <density/raw_atomic.h>

namespace density
{
    namespace detail
    {
        /** Class template to safely and efficiently handle a singleton.
            SingletonPtr is an empty class (no non-static data members) that provides thread-safe ref-counted access to a
            singleton object allocated in a fixed static storage. \n
            The access to the singleton (operators * and ->, function get) is basically a no-op, as it returns the address
            of the static storage: no initialization guard is necessary. All the cost of handling the lifetime of the singleton
            is paid in the constructor and destructor. \n
            Internally SingletonPtr declares a global instance of itself, to guarantee that the singleton is constructed during 
            the initialization of global objects, and destroyed during the destruction of global objects. Anyway instances of
            SingletonPtr can be safely created and destroyed during the construction or destruction of globals.
            Anyway, in case of global object with asymmetric lifetimes, the singleton may be created and destroyed more than once,
            with at most one instance existing in any moment. In case of instances constructed and destroyed concurrently during
            the construction or destruction of global objects, a thread may wait in a busy loop while another thread is constructing
            or destroying the singleton.\n
            
            The singleton class should be uncopyable, unmovable, and should have private constructor and destructor. SingletonPtr<T>
            should be declared friend of t. */
        template <typename SINGLETON>
            class SingletonPtr
        {
        public:

            /** Constructs the SingletonPtr, possibly constructing the singleton. */
            SingletonPtr() noexcept(std::is_nothrow_default_constructible<SINGLETON>::value)
            {
                /*volatile void * p = &s_global_ptr_instance;
                (void)p;*/
                add_ref();
            }

            /** Copy constructs the SingletonPt. This function will never construct the singleton. */
            SingletonPtr(const SingletonPtr &) noexcept
            {
                add_ref_noconstruct();
            }

            /** Copy assigns the SingletonPt. This is actually a no-operation. */
            SingletonPtr & operator = (const SingletonPtr &) noexcept
            {
                return *this;
            }
            
            /** Destroys the SingletonPt, possibly destroying the singleton. */
            ~SingletonPtr()
            {
                release();
            }

            /** Provides access to the singleton. */
            SINGLETON * operator -> () const noexcept
            {
                return get_singleton();
            }

            /** Provides access to the singleton. */
            SINGLETON & operator * () const noexcept
            {
                return *get_singleton();
            }

            /** Provides access to the singleton. */
            SINGLETON get() const noexcept
            {
                return get_singleton();
            }

        private:

            static SINGLETON * get_singleton() noexcept
            {
                return reinterpret_cast<SINGLETON*>(&s_singleton_storage);
            }

            static void add_ref() noexcept(std::is_nothrow_default_constructible<SINGLETON>::value)
            {
                auto ref_count = raw_atomic_load(&s_ref_count);
                for (;;)
                {
                    if (ref_count == 0)
                    {
                        // the singleton must be constructed
                        if (raw_atomic_compare_exchange_weak(&s_ref_count, &ref_count, static_cast<uintptr_t>(1)))
                        {
                            new(&s_singleton_storage) SINGLETON();

                            raw_atomic_store(&s_ref_count, 2);
                            break;
                        }
                    }
                    else if (ref_count == 1)
                    {
                        // another thread is constructing the singleton, spin wait
                        std::this_thread::yield();
                        ref_count = raw_atomic_load(&s_ref_count);
                    }
                    else
                    {
                        // just increment the refcount
                        if (raw_atomic_compare_exchange_weak(&s_ref_count, &ref_count, ref_count + 1))
                        {
                            break;
                        }
                    }
                }
            }

            static void add_ref_noconstruct() noexcept
            {
                auto ref_count = raw_atomic_load(&s_ref_count);
                while (!raw_atomic_compare_exchange_weak(&s_ref_count, &ref_count, ref_count + 1));
            }

            static void release() noexcept
            {
                auto ref_count = raw_atomic_load(&s_ref_count);
                for (;;)
                {
                    DENSITY_ASSERT_INTERNAL(ref_count > 0);
                    if (ref_count == 2)
                    {
                        // the singleton must be destroyed
                        if (raw_atomic_compare_exchange_weak(&s_ref_count, &ref_count, static_cast<uintptr_t>(1)))
                        {
                            get_singleton()->~SINGLETON();

                            raw_atomic_store(&s_ref_count, 0);
                            break;
                        }
                    }
                    else if (ref_count == 1)
                    {
                        // another thread is destroying the singleton, spin wait
                        std::this_thread::yield();
                        ref_count = raw_atomic_load(&s_ref_count);
                    }
                    else
                    {
                        // just decrement the refcount
                        if (raw_atomic_compare_exchange_weak(&s_ref_count, &ref_count, ref_count - 1))
                        {
                            break;
                        }
                    }
                }
            }

        private:
            static typename std::aligned_storage<sizeof(SINGLETON), alignof(SINGLETON)>::type s_singleton_storage;
            static volatile uintptr_t s_ref_count;
            static SingletonPtr<SINGLETON> s_global_ptr_instance;
        };

        template <typename SINGLETON>
            typename std::aligned_storage<sizeof(SINGLETON), alignof(SINGLETON)>::type SingletonPtr<SINGLETON>::s_singleton_storage;
        
        template <typename SINGLETON>
            volatile uintptr_t SingletonPtr<SINGLETON>::s_ref_count;

        template <typename SINGLETON>
            SingletonPtr<SINGLETON> SingletonPtr<SINGLETON>::s_global_ptr_instance;

    } // namespace detail

} // namespace density
