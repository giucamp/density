
//   Copyright Giuseppe Campana (giu.campana@gmail.com) 2016-2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <density/density_common.h>
#include <cstdint>
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    #include <intrin.h>
#endif

namespace density
{
    /** Similar to <a href="http://en.cppreference.com/w/cpp/atomic/atomic_load">std::atomic_load</a> but acts on primitive types.
        This function has a limited support: its availability depends on the compiler, the target architecture, and the type of the variable.
        \n Overloads not available are declared as deleted.
        <table>
        <caption id="multi_row">Availability</caption>
        <tr>
            <th style="width:130px">Compiler</th>
            <th style="width:130px">Platform</th>
            <th>Types</th>
        </tr>
        <tr>
            <td>Msc</td>
            <td>x86</td>
            <td>uint32_t</td>
        </tr>
        <tr>
            <td>Msc</td>
            <td>x64</td>
            <td>uint32_t, uint64_t</td>
        </tr>
        </table> */
    template <typename TYPE>
        TYPE raw_atomic_load(TYPE const volatile * i_atomic, std::memory_order i_memory_order = std::memory_order_seq_cst) noexcept = delete;

    /** Similar to <a href="http://en.cppreference.com/w/cpp/atomic/atomic_store">std::atomic_store</a> but acts on primitive types.
        This function has a limited support: its availability depends on the compiler, the target architecture, and the type of the variable.
        \n Overloads not available are declared as deleted.
        <table>
        <caption id="multi_row">Availability</caption>
        <tr>
            <th style="width:130px">Compiler</th>
            <th style="width:130px">Platform</th>
            <th>Types</th>
        </tr>
        <tr>
            <td>Msc</td>
            <td>x86</td>
            <td>uint32_t</td>
        </tr>
        <tr>
            <td>Msc</td>
            <td>x64</td>
            <td>uint32_t, uint64_t</td>
        </tr>
        </table> */
    template <typename TYPE>
        void raw_atomic_store(TYPE volatile * i_atomic, TYPE i_value, std::memory_order i_memory_order = std::memory_order_seq_cst) noexcept = delete;

    /** Similar to <a href="http://en.cppreference.com/w/cpp/atomic/atomic/compare_exchange">std::atomic_compare_exchange_strong</a> but acts on primitive types.
        This function has a limited support: its availability depends on the compiler, the target architecture, and the type of the variable.
        \n Overloads not available are declared as deleted.
        <table>
        <caption id="multi_row">Availability</caption>
        <tr>
            <th style="width:130px">Compiler</th>
            <th style="width:130px">Platform</th>
            <th>Types</th>
        </tr>
        <tr>
            <td>Msc</td>
            <td>x86</td>
            <td>uint32_t</td>
        </tr>
        <tr>
            <td>Msc</td>
            <td>x64</td>
            <td>uint32_t, uint64_t</td>
        </tr>
        </table> */
    template <typename TYPE>
        bool raw_atomic_compare_exchange_strong(TYPE volatile * i_atomic,
            TYPE * i_expected, TYPE i_desired, std::memory_order i_success, std::memory_order i_failure) noexcept = delete;

    /** Similar to <a href="http://en.cppreference.com/w/cpp/atomic/atomic/compare_exchange">std::atomic_compare_exchange_weak</a> but acts on primitive types.
        This function has a limited support: its availability depends on the compiler, the target architecture, and the type of the variable.
        \n Overloads not available are declared as deleted.
        <table>
        <caption id="multi_row">Availability</caption>
        <tr>
            <th style="width:130px">Compiler</th>
            <th style="width:130px">Platform</th>
            <th>Types</th>
        </tr>
        <tr>
            <td>Msc</td>
            <td>x86</td>
            <td>uint32_t</td>
        </tr>
        <tr>
            <td>Msc</td>
            <td>x64</td>
            <td>uint32_t, uint64_t</td>
        </tr>
        </table> */
    template <typename TYPE>
        bool raw_atomic_compare_exchange_weak(TYPE volatile * i_atomic,
            TYPE * i_expected, TYPE i_desired, std::memory_order i_success, std::memory_order i_failure) noexcept = delete;

    #if defined(_MSC_VER) && (defined(_M_X64) | defined(_M_IX86))

        inline uint32_t raw_atomic_load(uint32_t const volatile * i_atomic, std::memory_order i_memory_order = std::memory_order_seq_cst) noexcept
        {
            DENSITY_ASSERT(address_is_aligned((void*)i_atomic, alignof(decltype(i_atomic))));

            switch (i_memory_order)
            {
                case std::memory_order_relaxed:
                    return *i_atomic;

                default:
                    DENSITY_ASSERT(false); // invalid memory order
                case std::memory_order_consume:
                case std::memory_order_acquire:
                case std::memory_order_seq_cst:
                {
                    auto const value = *i_atomic;
                    _ReadWriteBarrier();
                    return value;
                }
            }
        }

        #if defined(_M_X64)
            inline uint64_t raw_atomic_load(uint64_t const volatile * i_atomic, std::memory_order i_memory_order = std::memory_order_seq_cst) noexcept
            {
                DENSITY_ASSERT(address_is_aligned((void*)i_atomic, alignof(decltype(i_atomic))));

                switch (i_memory_order)
                {
                    case std::memory_order_relaxed:
                        return *i_atomic;

                    default:
                        DENSITY_ASSERT(false); // invalid memory orders
                    case std::memory_order_consume:
                    case std::memory_order_acquire:
                    case std::memory_order_seq_cst:
                    {
                        auto const value = *i_atomic;
                        _ReadWriteBarrier();
                        return value;
                    }
                }
            }
        #endif

        inline void raw_atomic_store(uint32_t volatile * i_atomic, uint32_t i_value, std::memory_order i_memory_order = std::memory_order_seq_cst) noexcept
        {
            DENSITY_ASSERT(address_is_aligned((void*)i_atomic, alignof(decltype(i_atomic))));

            switch (i_memory_order)
            {
                case std::memory_order_relaxed:
                    *i_atomic = i_value;
                    break;

                case std::memory_order_release:
                    _ReadWriteBarrier();
                    *i_atomic = i_value;
                    break;

                case std::memory_order_seq_cst:
                    _InterlockedExchange(reinterpret_cast<long volatile*>(i_atomic), static_cast<long>(i_value));
                    break;

                default:
                    DENSITY_ASSERT(false); // invalid memory order
            }
        }

        #if defined(_M_X64)
            inline void raw_atomic_store(uint64_t volatile * i_atomic, uint64_t i_value, std::memory_order i_memory_order = std::memory_order_seq_cst) noexcept
            {
                DENSITY_ASSERT(address_is_aligned((void*)i_atomic, alignof(decltype(i_atomic))));

                switch (i_memory_order)
                {
                    case std::memory_order_relaxed:
                        *i_atomic = i_value;
                        break;

                    case std::memory_order_release:
                        _ReadWriteBarrier();
                        *i_atomic = i_value;
                        break;

                    case std::memory_order_seq_cst:
                        _InterlockedExchange64(reinterpret_cast<long long volatile*>(i_atomic), static_cast<long long>(i_value));
                        break;

                    default:
                        DENSITY_ASSERT(false); // invalid memory order
                }
            }
        #endif

        inline bool raw_atomic_compare_exchange_strong(uint32_t volatile * i_atomic,
            uint32_t * i_expected, uint32_t i_desired, std::memory_order i_success, std::memory_order i_failure) noexcept
        {
            DENSITY_ASSERT(address_is_aligned((void*)i_atomic, alignof(decltype(i_atomic))));
            (void)i_success;
            (void)i_failure;
            long const prev_val = _InterlockedCompareExchange(reinterpret_cast<volatile long*>(i_atomic), (long)i_desired, *(long*)i_expected);
            if (prev_val == *(long*)i_expected)
            {
                return true;
            }
            else
            {
                *i_expected = (long)prev_val;
                return false;
            }
        }

        #if defined(_M_X64)
            inline bool raw_atomic_compare_exchange_strong(uint64_t volatile * i_atomic,
                uint64_t * i_expected, uint64_t i_desired, std::memory_order i_success, std::memory_order i_failure) noexcept
            {
                DENSITY_ASSERT(address_is_aligned((void*)i_atomic, alignof(decltype(i_atomic))));
                (void)i_success;
                (void)i_failure;
                long long const prev_val = _InterlockedCompareExchange64(reinterpret_cast<volatile long long*>(i_atomic), (long long)i_desired, *(long long*)i_expected);
                if (prev_val == *(long long*)i_expected)
                {
                    return true;
                }
                else
                {
                    *i_expected = (long long)prev_val;
                    return false;
                }
            }
        #endif

        inline bool raw_atomic_compare_exchange_weak(uint32_t volatile * i_atomic,
            uint32_t * i_expected, uint32_t i_desired, std::memory_order i_success, std::memory_order i_failure) noexcept
        {
            return raw_atomic_compare_exchange_strong(i_atomic, i_expected, i_desired, i_success, i_failure);
        }

        #if defined(_M_X64)

            inline bool raw_atomic_compare_exchange_weak(uint64_t volatile * i_atomic,
                uint64_t * i_expected, uint64_t i_desired, std::memory_order i_success, std::memory_order i_failure) noexcept
            {
                return raw_atomic_compare_exchange_strong(i_atomic, i_expected, i_desired, i_success, i_failure);
            }

        #endif
    
    #elif defined(__GNUG__) // gcc and clang
        
        inline uintptr_t raw_atomic_load(uintptr_t const volatile * i_atomic, std::memory_order i_memory_order = std::memory_order_seq_cst) noexcept
        {
            DENSITY_ASSERT_ALIGNED((void*)i_atomic, alignof(decltype(i_atomic)));

            switch(i_memory_order)
            {
                case std::memory_order_relaxed:
                    return __atomic_load_n(i_atomic, __ATOMIC_RELAXED);
                case std::memory_order_acquire:
                    return __atomic_load_n(i_atomic, __ATOMIC_ACQUIRE);
                case std::memory_order_seq_cst:
                    return __atomic_load_n(i_atomic,  __ATOMIC_SEQ_CST);
                default:
                    DENSITY_ASSERT(false);
                    return 0;
            }            
        }

        inline void raw_atomic_store(uintptr_t volatile * i_atomic, uintptr_t i_value, std::memory_order i_memory_order = std::memory_order_seq_cst) noexcept
        {
            DENSITY_ASSERT_ALIGNED((void*)i_atomic, alignof(decltype(i_atomic)));

            switch(i_memory_order)
            {
                case std::memory_order_relaxed:
                    __atomic_store_n(i_atomic, i_value, __ATOMIC_RELAXED);
                    break;
                case std::memory_order_release:
                    __atomic_store_n(i_atomic, i_value, __ATOMIC_RELEASE);
                    break;
                case std::memory_order_seq_cst:
                    __atomic_store_n(i_atomic, i_value, __ATOMIC_SEQ_CST);
                    break;
                default:
                    DENSITY_ASSERT(false);
            }
        }

        inline bool raw_atomic_compare_exchange_strong(uintptr_t volatile * i_atomic,
            uintptr_t * i_expected, uintptr_t i_desired, std::memory_order i_success, std::memory_order i_failure) noexcept
        {
            DENSITY_ASSERT_ALIGNED((void*)i_atomic, alignof(decltype(i_atomic)));
            
            switch(i_success)
            {
                case std::memory_order_relaxed:
                {
                    switch(i_failure)
                    {
                        case std::memory_order_relaxed:
                            return __atomic_compare_exchange_n(i_atomic, i_expected, i_desired, false, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
                        case std::memory_order_consume:
                            return __atomic_compare_exchange_n(i_atomic, i_expected, i_desired, false, __ATOMIC_RELAXED, __ATOMIC_CONSUME);
                        case std::memory_order_acquire:
                            return __atomic_compare_exchange_n(i_atomic, i_expected, i_desired, false, __ATOMIC_RELAXED, __ATOMIC_ACQUIRE);
                        case std::memory_order_seq_cst:
                            return __atomic_compare_exchange_n(i_atomic, i_expected, i_desired, false, __ATOMIC_RELAXED, __ATOMIC_SEQ_CST);
                        default:
                            DENSITY_ASSERT(false);
                            return false;
                    }                
                }                
                    
                case std::memory_order_consume:
                {
                    switch(i_failure)
                    {
                        case std::memory_order_relaxed:
                            return __atomic_compare_exchange_n(i_atomic, i_expected, i_desired, false, __ATOMIC_CONSUME, __ATOMIC_RELAXED);
                        case std::memory_order_consume:
                            return __atomic_compare_exchange_n(i_atomic, i_expected, i_desired, false, __ATOMIC_CONSUME, __ATOMIC_CONSUME);
                        case std::memory_order_acquire:
                            return __atomic_compare_exchange_n(i_atomic, i_expected, i_desired, false, __ATOMIC_CONSUME, __ATOMIC_ACQUIRE);
                        case std::memory_order_seq_cst:
                            return __atomic_compare_exchange_n(i_atomic, i_expected, i_desired, false, __ATOMIC_CONSUME, __ATOMIC_SEQ_CST);
                        default:
                            DENSITY_ASSERT(false);
                            return false;
                    }                
                }
                    
                case std::memory_order_acquire:
                {
                    switch(i_failure)
                    {
                        case std::memory_order_relaxed:
                            return __atomic_compare_exchange_n(i_atomic, i_expected, i_desired, false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
                        case std::memory_order_consume:
                            return __atomic_compare_exchange_n(i_atomic, i_expected, i_desired, false, __ATOMIC_ACQUIRE, __ATOMIC_CONSUME);
                        case std::memory_order_acquire:
                            return __atomic_compare_exchange_n(i_atomic, i_expected, i_desired, false, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE);
                        case std::memory_order_seq_cst:
                            return __atomic_compare_exchange_n(i_atomic, i_expected, i_desired, false, __ATOMIC_ACQUIRE, __ATOMIC_SEQ_CST);
                        default:
                            DENSITY_ASSERT(false);
                            return false;
                    }                
                }
                    
                case std::memory_order_release:
                {
                    switch(i_failure)
                    {
                        case std::memory_order_relaxed:
                            return __atomic_compare_exchange_n(i_atomic, i_expected, i_desired, false, __ATOMIC_RELEASE, __ATOMIC_RELAXED);
                        case std::memory_order_consume:
                            return __atomic_compare_exchange_n(i_atomic, i_expected, i_desired, false, __ATOMIC_RELEASE, __ATOMIC_CONSUME);
                        case std::memory_order_acquire:
                            return __atomic_compare_exchange_n(i_atomic, i_expected, i_desired, false, __ATOMIC_RELEASE, __ATOMIC_ACQUIRE);
                        case std::memory_order_seq_cst:
                            return __atomic_compare_exchange_n(i_atomic, i_expected, i_desired, false, __ATOMIC_RELEASE, __ATOMIC_SEQ_CST);
                        default:
                            DENSITY_ASSERT(false);
                            return false;
                    }                
                }
                    
                case std::memory_order_acq_rel:
                {
                    switch(i_failure)
                    {
                        case std::memory_order_relaxed:
                            return __atomic_compare_exchange_n(i_atomic, i_expected, i_desired, false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED);
                        case std::memory_order_consume:
                            return __atomic_compare_exchange_n(i_atomic, i_expected, i_desired, false, __ATOMIC_ACQ_REL, __ATOMIC_CONSUME);
                        case std::memory_order_acquire:
                            return __atomic_compare_exchange_n(i_atomic, i_expected, i_desired, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
                        case std::memory_order_seq_cst:
                            return __atomic_compare_exchange_n(i_atomic, i_expected, i_desired, false, __ATOMIC_ACQ_REL, __ATOMIC_SEQ_CST);
                        default:
                            DENSITY_ASSERT(false);
                            return false;
                    }                
                }
                    
                case std::memory_order_seq_cst:
                {
                    switch(i_failure)
                    {
                        case std::memory_order_relaxed:
                            return __atomic_compare_exchange_n(i_atomic, i_expected, i_desired, false, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED);
                        case std::memory_order_consume:
                            return __atomic_compare_exchange_n(i_atomic, i_expected, i_desired, false, __ATOMIC_SEQ_CST, __ATOMIC_CONSUME);
                        case std::memory_order_acquire:
                            return __atomic_compare_exchange_n(i_atomic, i_expected, i_desired, false, __ATOMIC_SEQ_CST, __ATOMIC_ACQUIRE);
                        case std::memory_order_seq_cst:
                            return __atomic_compare_exchange_n(i_atomic, i_expected, i_desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
                        default:
                            DENSITY_ASSERT(false);
                            return false;
                    }                
                }
                
                default:
                    DENSITY_ASSERT(false);
                    return false;
            }
        }

        inline bool raw_atomic_compare_exchange_weak(uintptr_t volatile * i_atomic,
            uintptr_t * i_expected, uintptr_t i_desired, std::memory_order i_success, std::memory_order i_failure) noexcept
        {
            return raw_atomic_compare_exchange_strong(i_atomic, i_expected, i_desired, i_success, i_failure);
        }

    #endif

    /** Similar to <a href="http://en.cppreference.com/w/cpp/atomic/atomic/compare_exchange">std::atomic_compare_exchange_strong</a> but acts on primitive types.
        This function has a limited support: its availability depends on the compiler, the target architecture, and the type of the variable.
        \n Overloads not available are declared as deleted.
        <table>
        <caption id="multi_row">Availability</caption>
        <tr>
            <th style="width:130px">Compiler</th>
            <th style="width:130px">Platform</th>
            <th>Types</th>
        </tr>
        <tr>
            <td>Msc</td>
            <td>x86</td>
            <td>uint32_t</td>
        </tr>
        <tr>
            <td>Msc</td>
            <td>x64</td>
            <td>uint32_t, uint64_t</td>
        </tr>
        </table> */
    template <typename TYPE>
        bool raw_atomic_compare_exchange_strong(TYPE volatile * i_atomic,
            TYPE * i_expected, TYPE i_desired, std::memory_order i_memory_order = std::memory_order_seq_cst) noexcept
    {
        return raw_atomic_compare_exchange_strong(i_atomic, i_expected, i_desired, i_memory_order, i_memory_order);
    }

    /** Similar to <a href="http://en.cppreference.com/w/cpp/atomic/atomic/compare_exchange">std::atomic_compare_exchange_weak</a> but acts on primitive types.
        This function has a limited support: its availability depends on the compiler, the target architecture, and the type of the variable.
        \n Overloads not available are declared as deleted.
        <table>
        <caption id="multi_row">Availability</caption>
        <tr>
            <th style="width:130px">Compiler</th>
            <th style="width:130px">Platform</th>
            <th>Types</th>
        </tr>
        <tr>
            <td>Msc</td>
            <td>x86</td>
            <td>uint32_t</td>
        </tr>
        <tr>
            <td>Msc</td>
            <td>x64</td>
            <td>uint32_t, uint64_t</td>
        </tr>
        </table> */
    template <typename TYPE>
        bool raw_atomic_compare_exchange_weak(TYPE volatile * i_atomic,
            TYPE * i_expected, TYPE i_desired, std::memory_order i_memory_order = std::memory_order_seq_cst) noexcept
    {
        return raw_atomic_compare_exchange_weak(i_atomic, i_expected, i_desired, i_memory_order, i_memory_order);
    }

} // namespace density
