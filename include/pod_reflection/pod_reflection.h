﻿#pragma once

#include <array>
#include <cstddef>
#include <limits>
#include <tuple>
#include <type_traits>

namespace eld
{
    /*!
     * Tag for a type that couldn't be deduced from provided tuple feed
     */
    struct undeduced
    {
    };

    // TODO: add pointers?
    using basic_feed = std::tuple<bool, unsigned,
            signed,
            char, signed char, unsigned char,
            short, unsigned short,
            int, unsigned int,
            long, unsigned long,
            long long, unsigned long long,
            float, double, long double, long double
    >;

    // TODO: filter duplicates
    template<typename ... ArgsT>
    using extend_feed = decltype(std::tuple_cat(basic_feed(),
                                                std::tuple<ArgsT>()...));

    constexpr size_t invalid_offset = std::numeric_limits<size_t>::max();

    struct ignore_enums_t
    {
    };

    constexpr ignore_enums_t ignore_enums{};

    namespace detail
    {

        template<size_t>
        struct tag_s
        {
        };

        template<typename... Ts> struct make_void
        {
            typedef void type;
        };
        template<typename... Ts> using void_t = typename make_void<Ts...>::type;

        /*!
         * \TODO: description
         */
        struct implicitly_convertible
        {
            template<typename Any>
            constexpr operator Any() const noexcept;
        };

        template<size_t s>
        using implicitly_convertible_s = implicitly_convertible;

        // stop case
        template<typename POD, typename ... T>
        constexpr size_t count_args(void_t<>)
        {
            return sizeof...(T) - 1;
        }

        template<typename POD, typename ... T>
        constexpr size_t count_args(void_t<decltype(POD{T()...})>)
        {
            return count_args<POD, T..., implicitly_convertible>(
                    void_t<decltype(POD{T()..., implicitly_convertible()})>());
        }

        template<typename T, bool /*false*/ = std::is_enum<T>::value>
        struct unwrap_enum
        {
            using type = T;
        };

        template<typename T>
        struct unwrap_enum<T, true>
        {
            using type = typename std::underlying_type<T>::type;
        };

        template<typename T>
        using unwrap_enum_t = typename unwrap_enum<T>::type;

        /*!
 * \TODO: description
 * @tparam Allowed
 */
        template<typename Allowed, typename /*ignore_enums_t*/= void>
        struct explicitly_convertible
        {
            template<typename To, typename = typename
            std::enable_if<std::is_same<To, Allowed>::value>::type>
            constexpr operator To() const noexcept;
        };

        template<typename Allowed>
        struct explicitly_convertible<Allowed, ignore_enums_t>
        {
            template<typename To, typename = typename
            std::enable_if<std::is_same<unwrap_enum_t<To>, Allowed>::value>::type>
            constexpr operator To() const noexcept;
        };

        // index sequence only
        template<size_t ...>
        struct index_sequence
        {
        };

        template<size_t N, size_t ... Next>
        struct index_sequence_helper : public index_sequence_helper<N - 1U, N - 1U, Next...>
        {
        };

        template<size_t ... Next>
        struct index_sequence_helper<0U, Next ...>
        {
            using type = index_sequence<Next ...>;
        };

        template<size_t N>
        using make_index_sequence = typename index_sequence_helper<N>::type;

        /**
         * Check if T is aggregate initialisable from given types.
         * Yields True if T{From()...} is well-formed
         * @tparam T Type for aggregate initialisation
         * @tparam From Types
         */
        template<typename, typename T, typename ...>
        struct is_aggregate_initialisable_ : std::false_type
        {
        };

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

        /**
         * Check if T is aggregate initialisable from given types.
         * Yields True if T{From()...} is well-formed
         * @tparam T Type for aggregate initialisation
         * @tparam From Types
         */
        template<typename T, typename ... From>
        struct is_aggregate_initialisable_<
                void_t<decltype(T{{std::declval<From>()}...})>,
                T,
                From...> : std::true_type
        {
        };

        /**
         * Check if T is aggregate initialisable from given types.
         * Yields True if T{From()...} is well-formed
         * @tparam T Type for aggregate initialisation
         * @tparam From Types
         */
        template<typename T, typename ... From>
        using is_aggregate_initialisable = is_aggregate_initialisable_<void_t<>, T, From...>;


        // stop case
        template<typename POD, typename ... Args>
        constexpr size_t count_args(std::false_type)
        {
            return sizeof...(Args) ? sizeof...(Args) - 1 : 0;
        }

        // general recursion
        template<typename POD, typename ... Args>
        constexpr size_t count_args(std::true_type)
        {
            return count_args<POD, Args...,
                    implicitly_convertible>(is_aggregate_initialisable<POD, Args..., implicitly_convertible>());
        }
    }

    // TODO: write tests for pod_size
    /*!
     * Provides access to the number of elements in a POD type as a compile-time constant expression
     * @tparam POD
     */
    template<typename POD>
    struct pod_size : public std::integral_constant<size_t,
            detail::count_args<POD>(detail::is_aggregate_initialisable<POD>())>
    {
    };

    namespace detail
    {

        /**
         * Helper class to check if POD member is initialisable from T
         * @tparam POD
         * @tparam T
         * @tparam PODMemberIndex
         * @todo finish documentation
         */
        template<typename POD, typename T, size_t PODMemberIndex,
                typename = void,
                typename = make_index_sequence<PODMemberIndex>>
        struct is_pod_member_initialisable_from_t : std::false_type
        {
        };

        /**
         * Helper class to check if POD member is initialisable from T
         * @tparam POD
         * @tparam T
         * @tparam PODMemberIndex
         * @todo finish documentation
         */
        template<typename POD, typename T, size_t PODMemberIndex,
                size_t ... PrevArgs>
        struct is_pod_member_initialisable_from_t<POD,
                T,
                PODMemberIndex,
                void_t<decltype(POD{implicitly_convertible_s<PrevArgs>()..., explicitly_convertible<T>()})>,
                index_sequence<PrevArgs...>> : std::true_type
        {
        };


        // TODO: make standalone function
        /**
         * Gets index within the TupleFeed of the Ith POD member<br>
         * Complexity: N
         * @tparam I POD member index
         * @tparam POD
         * @tparam TupleFeed tuple of types to iterate through
         */
        template<size_t I, typename POD, typename TupleFeed>
        class tuple_index_from_pod_member
        {
            static_assert(std::tuple_size<TupleFeed>(), "TupleFeed must not be empty!");

        public:

            // TODO: remove
            template<typename T>
            using is_initializable = std::integral_constant<bool,
                    (bool) detail::is_pod_member_initialisable_from_t<POD, T, I>()>;

            constexpr static size_t value()
            {
                return find_type(detail::tag_s<0>(),
                                 is_initializable<typename std::tuple_element<0, TupleFeed>::type>());
            }

        private:

            // stop case on failure when tuple index exceeded
            constexpr static size_t find_type(detail::tag_s<std::tuple_size<TupleFeed>::value - 1>,
                                              std::false_type)
            {
                return size_t() - 1;
            }

            // stop case on success
            template<size_t tupleIndx>
            constexpr static size_t find_type(detail::tag_s<tupleIndx>, std::true_type)
            {
                return tupleIndx;
            }

            // general recursion
            template<size_t TupleIndex>
            constexpr static size_t find_type(detail::tag_s<TupleIndex>, std::false_type)
            {
                return find_type(detail::tag_s<TupleIndex + 1>(),
                                 is_initializable<typename std::tuple_element<TupleIndex + 1, TupleFeed>::type>());
            }
        };

        template<size_t I, typename TupleFeed, bool = I >= std::tuple_size<TupleFeed>::value>
        struct deduced_tuple_elem
        {
            using type = undeduced;
        };

        template<size_t I, typename TupleFeed>
        struct deduced_tuple_elem<I, TupleFeed, false>
        {
            using type = typename std::tuple_element<I, TupleFeed>::type;
        };
    }

    /*!
     * Provides compile-time indexed access to the types of the elements of the POD type deduced from
     * the provided TupleFeed class
     * @tparam I - index of the element
     * @tparam POD - POD type
     * @tparam TupleFeed - Tuple of types to be used as a feed to deduce an element
     */
    template<size_t I, typename POD, typename TupleFeed>
    struct pod_element
    {
        static_assert(std::tuple_size<TupleFeed>(), "TupleFeed must not be empty!");

        using type = typename
        detail::deduced_tuple_elem<detail::tuple_index_from_pod_member<I, POD, TupleFeed>::value(),
                TupleFeed>::type;
    };

    template<size_t I, typename POD, typename TupleFeed>
    using pod_element_t = typename pod_element<I, POD, TupleFeed>::type;

    namespace detail
    {
        template<typename POD, typename /*TupleFeed*/, typename = make_index_sequence<pod_size<POD>::value>>
        struct pod_to_tuple;

        template<typename POD, typename TupleFeed, size_t ... I>
        struct pod_to_tuple<POD, TupleFeed, index_sequence<I...>>
        {
            using type = std::tuple<pod_element_t<I, POD, TupleFeed>...>;
        };
    }

    template<typename POD, typename TupleFeed>
    using pod_to_tuple_t = typename detail::pod_to_tuple<POD, TupleFeed>::type;

    namespace detail
    {
        template<typename POD, typename TupleFeed = basic_feed>
        constexpr size_t pod_packing()
        {
            return sizeof(POD) % 4 ?
                   sizeof(POD) % 4 :
                   4;//alignof(POD);
        }

        template<size_t I, typename POD, typename TupleFeed>
        struct pod_elem_size_
        {
            using type = pod_element_t<I, POD, TupleFeed>;

            constexpr static size_t value()
            {
                return sizeof(type);
            }
        };


        template<typename T, size_t ... LIndices, size_t ... RIndices>
        constexpr std::array<T, sizeof...(LIndices) + sizeof...(RIndices)>
        append_arrays(const std::array<T, sizeof...(LIndices)> &lArray,
                      const std::array<T, sizeof...(RIndices)> &rArray,
                      index_sequence<LIndices...>,
                      index_sequence<RIndices...>)
        {
            return {lArray[LIndices]...,
                    rArray[RIndices]...};
        }

        template<size_t PrevSize, size_t ToAppend, typename T>
        constexpr std::array<T, PrevSize + ToAppend> append_arrays(const std::array<T, PrevSize> &prev,
                                                                   const std::array<T, ToAppend> &append)
        {
            return append_arrays(prev, append, make_index_sequence<PrevSize>(), make_index_sequence<ToAppend>());
        }

        template<size_t l, size_t r>
        struct is_equal : std::integral_constant<bool, l == r>
        {
        };

        template<typename T, size_t N>
        struct const_array
        {
            constexpr T operator[](size_t index) const
            {
                return data[index];
            }

            constexpr T back() const
            {
                return data[N - 1];
            }

            const T data[N];
        };

        // workaround for zero-sized c-arrays
        template<typename T>
        struct const_array<T, 0>
        {
            constexpr T operator[](size_t index) const
            {
                return T();// data[index];
            }

            constexpr T back() const
            {
                return T();
            }

            const T *data = nullptr;
        };

        template<size_t Index, typename POD, typename TupleFeed>
        struct calc_offset
        {
            constexpr static std::ptrdiff_t value(const const_array<std::ptrdiff_t, Index> &offsets)
            {
                return !((offsets.back() + pod_elem_size_<Index - 1, POD, TupleFeed>::value()) %
                         pod_packing<POD, TupleFeed>()) ||
                       pod_packing<POD, TupleFeed>() -
                       (offsets.back() + pod_elem_size_<Index - 1, POD, TupleFeed>::value()) %
                       pod_packing<POD, TupleFeed>() >= pod_elem_size_<Index, POD, TupleFeed>::value() ?
                       offsets.back() + pod_elem_size_<Index - 1, POD, TupleFeed>::value() :
                       offsets.back() + pod_elem_size_<Index - 1, POD, TupleFeed>::value() +
                       pod_packing<POD, TupleFeed>() -
                       (offsets.back() + pod_elem_size_<Index - 1, POD, TupleFeed>::value()) %
                       pod_packing<POD, TupleFeed>();
            }
        };

        template<typename POD, typename TupleFeed>
        struct calc_offset<0, POD, TupleFeed>
        {
            constexpr static std::ptrdiff_t value(const const_array<std::ptrdiff_t, 0> &)
            {
                return 0;
            }
        };

        template<typename T, size_t ... Indx>
        constexpr const_array<T, sizeof...(Indx) + 1>
        combine(const const_array<T, sizeof...(Indx)> &array, const T &val, index_sequence<Indx...>)
        {
            return {array[Indx]..., val};
        }

        template<typename T, size_t N>
        constexpr const_array<T, N + 1> combine(const const_array<T, N> &array, const T &value)
        {
            return combine(array, value, make_index_sequence<N>());
        }

        // stop case: Index == pod_size
        template<typename POD, typename TupleFeed>
        constexpr const_array<std::ptrdiff_t, pod_size<POD>::value>
        get_pod_offsets(const_array<std::ptrdiff_t, pod_size<POD>::value> offsets, std::true_type /*Index == pod_size*/)
        {
            return offsets;
        }

        // general recursion
        template<typename POD, typename TupleFeed, size_t CurIndex>
        constexpr const_array<std::ptrdiff_t, pod_size<POD>::value>
        get_pod_offsets(const_array<std::ptrdiff_t, CurIndex> offsets, std::false_type)
        {
            return get_pod_offsets<POD, TupleFeed>(
                    combine(offsets, /*TODO: calculate current*/
                            calc_offset<CurIndex, POD, TupleFeed>::value(offsets)),
                    is_equal<CurIndex + 1, pod_size<POD>::value>());
        }

        template<typename POD, typename TupleFeed>
        constexpr const_array<std::ptrdiff_t, pod_size<POD>::value>
        get_pod_offsets()
        {
            return get_pod_offsets<POD, TupleFeed>(const_array<std::ptrdiff_t, 0>{},
                                                   is_equal<0, pod_size<POD>::value>());
        }

        // TODO: check this function!
        // TODO: optimize code generation, now it is n!
        template<size_t I, typename POD, typename TupleFeed>
        class pod_elem_offset
        {
            static_assert(!std::is_same<undeduced, pod_element_t<I, POD, TupleFeed>>::value,
                          "Can't get an offset for an undeduced POD element!");

        public:
            constexpr static std::ptrdiff_t value()
            {
                return get_pod_offsets<POD, TupleFeed>()[I];
            }
        };


        /*!
         * \warning Invalid implementation: alignof does not yield packing size of a struct
         * @tparam TupleFeed
         * @tparam POD
         * @return
         */
        template<typename TupleFeed, typename POD>
        constexpr size_t evaluated_pod_size()
        {
            return (pod_elem_offset<pod_size<POD>() - 1, POD, TupleFeed>::value() +
                    sizeof(pod_element_t<pod_size<POD>() - 1, POD, TupleFeed>)) % pod_packing<POD, TupleFeed>() ?
                   pod_elem_offset<pod_size<POD>() - 1, POD, TupleFeed>::value() +
                   sizeof(pod_element_t<pod_size<POD>() - 1, POD, TupleFeed>) +
                   pod_packing<POD, TupleFeed>() - (pod_elem_offset<pod_size<POD>() - 1, POD, TupleFeed>::value() +
                                                    sizeof(pod_element_t<pod_size<POD>() - 1, POD, TupleFeed>)) %
                                                   pod_packing<POD, TupleFeed>() :
                   pod_elem_offset<pod_size<POD>() - 1, POD, TupleFeed>::value() +
                   sizeof(pod_element_t<pod_size<POD>() - 1, POD, TupleFeed>);
        }

    }

// TODO: check warnings
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

    /*!
     * Checks if POD type is valid for accessing and manipulating elements.
     * \warning POD is not valid if it contains bitfields.
     * @tparam TupleFeed
     * @tparam POD
     * @return
     * \todo check this function
     * \todo return false if some of the elements have not been deduced
     */
    template<typename TupleFeed, typename POD>
    constexpr bool is_valid_pod()
    {
        // calculate expected POD size using offset
        return sizeof(POD) == detail::evaluated_pod_size<TupleFeed, POD>();
    }

    /*!
     * Extracts the Ith element from the POD structure
     * @tparam I - index of an element to access
     * @tparam TupleFeed - Tuple of types to be used as a feed to deduce POD elements
     * @tparam POD
     * @param pod
     * @return
     * \warning This function is UB. Need to write a workaround, i.e. - disable optimization
     */
    template<size_t I, typename TupleFeed, typename POD>
    pod_element_t<I, POD, TupleFeed> &get(POD &pod)
    {
        static_assert(is_valid_pod<TupleFeed, POD>(), "Invalid POD struct: possibly contains bitfields");
        static_assert(!std::is_same<undeduced, pod_element_t<I, POD, TupleFeed>>::value,
                      "Can't get an undeduced POD element!");
        return *reinterpret_cast<pod_element_t<I, POD, TupleFeed> *>(((std::ptrdiff_t) &pod +
                                                                      detail::pod_elem_offset<I, POD, TupleFeed>::value()));
    }

    /*!
     * Extracts the Ith element from the POD structure
     * @tparam I - index of an element to access
     * @tparam TupleFeed - Tuple of types to be used as a feed to deduce POD elements
     * @tparam POD
     * @param pod
     * @return
     * \warning This function is UB. Need to write a workaround, i.e. - disable optimization
     */
    template<size_t I, typename TupleFeed, typename POD>
    const pod_element_t<I, POD, TupleFeed> &get(const POD &pod)
    {
        static_assert(is_valid_pod<TupleFeed, POD>(), "Invalid POD struct: possibly contains bitfields");
        static_assert(!std::is_same<undeduced, pod_element_t<I, POD, TupleFeed>>::value,
                      "Can't get an undeduced POD element!");
        return *reinterpret_cast<const pod_element_t<I, POD, TupleFeed> *>(((std::ptrdiff_t) &pod +
                                                                            detail::pod_elem_offset<I, POD, TupleFeed>::value()));
    }

    namespace detail
    {
        template<typename TupleFeed, typename POD, typename F, size_t ... Indx>
        size_t for_each(POD &pod, F &&func, index_sequence<Indx...>)
        {
            auto f = std::forward<F>(func);
            return (int) std::initializer_list<int>{(f(get<Indx, TupleFeed>(pod)), 0)...}.size();
        }
    }

    /*!
     * Invokes functor of type F for each element in a given POD. TupleFeed must be
     * provided for elements deduction
     * @tparam TupleFeed
     * @tparam POD
     * @tparam F
     * @param pod
     * @param func
     * @return
     * \warning Invokes elements in reverse order
     * \todo assert that a POD does not have bitfields
     * \todo assert that a POD does not contain fixed size arrays
     */
    template<typename TupleFeed, typename POD, typename F>
    size_t for_each(POD &pod, F &&func)
    {
        return detail::for_each<TupleFeed>(pod, std::forward<F>(func),
                                           detail::make_index_sequence<pod_size<POD>::value>());
    }

    template<typename TupleFeed, typename POD, typename F>
    size_t for_each(POD &pod, F &&func, ignore_enums_t)
    {
        return detail::for_each<TupleFeed>(pod, std::forward<F>(func),
                                           detail::make_index_sequence<pod_size<POD>::value>());
    }

}
