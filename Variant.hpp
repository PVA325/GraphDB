#include <iostream>

template<typename T, typename... Types>
struct VariantAlternative;
template<typename... Types>
class Variant;


namespace variant_traits {
  template<typename... Types>
  struct max_type_align {};
  template<typename Head>
  struct max_type_align<Head> {
    static constexpr size_t value = alignof(Head);
  };
  template<typename Head, typename... Tail>
  struct max_type_align<Head, Tail...> {
    static constexpr size_t value = std::max(alignof(Head), max_type_align<Tail...>::value);
  };
  template<typename... Types>
  struct max_type_sizeof {};
  template<typename Head>
  struct max_type_sizeof<Head> {
    static constexpr size_t value = sizeof(Head);
  };
  template<typename Head, typename... Tail>
  struct max_type_sizeof<Head, Tail...> {
    static constexpr size_t value = std::max(sizeof(Head), max_type_sizeof<Tail...>::value);
  };

  template<typename... Types>
  struct get_index_by_type {};
  template<typename T, typename Head, typename... Tail>
  struct get_index_by_type<T, Head, Tail...> {
    static constexpr size_t value = (get_index_by_type<T, Tail...>::value + 1);
  };
  template<typename T, typename... Tail>
  struct get_index_by_type<T, T, Tail...> {
    static constexpr size_t value = 0;
  };
  template<size_t Idx, typename... Types>
  struct get_type_by_index {};
  template<typename Head, typename... Tail>
  struct get_type_by_index<0, Head, Tail...> {
    using type = Head;
  };
  template<size_t Idx, typename Head, typename... Tail>
  struct get_type_by_index<Idx, Head, Tail...> {
    using type = typename get_type_by_index<Idx - 1, Tail...>::type;
  };
  template<typename... Types>
  struct is_type_in_pack {};
  template<typename T, typename... Tail>
  struct is_type_in_pack<T, T, Tail...> {
    static constexpr bool value = true;
  };
  template<typename T, typename Head, typename... Tail>
  struct is_type_in_pack<T, Head, Tail...> {
    static constexpr bool value = is_type_in_pack<T, Tail...>::value;
  };
  template<typename T>
  struct is_type_in_pack<T> {
    static constexpr bool value = false;
  };
  template<typename T>
  struct is_variant : std::false_type {};
  template<typename... Types>
  struct is_variant<Variant<Types...>> : std::true_type {};
  template<typename T, typename U>
  struct is_type_in_variant {};
  template<typename T, typename... Types>
  struct is_type_in_variant<T, Variant<Types...>> : is_type_in_pack<T, Types...> {};
  template<typename T, typename U>
  struct get_index_in_variant_by_type {};
  template<typename T, typename... Types>
  struct get_index_in_variant_by_type<T, Variant<Types...>> : get_index_by_type<T, Types...> {};
  template<typename... Types>
  struct cnt_of_variant_types {};
  template<typename... Types>
  struct cnt_of_variant_types<Variant<Types...>> {
    static constexpr size_t value = sizeof...(Types);
  };
  template<size_t Idx, typename... Types>
  struct get_type_in_variant_by_index {};
  template<size_t Idx, typename... Types>
  struct get_type_in_variant_by_index<Idx, Variant<Types...>> : get_type_by_index<Idx, Types...> {};

  template<typename... Types>
  constexpr bool is_type_in_pack_v = is_type_in_pack<Types...>::value;
  template<typename... Types>
  constexpr size_t max_type_align_v = max_type_align<Types...>::value;
  template<typename... Types>
  constexpr size_t max_type_sizeof_v = max_type_sizeof<Types...>::value;
  template<typename Head, typename... Types>
  requires (is_type_in_pack_v<Head, Types...>)
  constexpr size_t get_index_by_type_v = get_index_by_type<Head, Types...>::value;
  template<size_t Idx, typename... Types>
  requires (Idx < sizeof...(Types))
  using get_type_by_index_t = typename get_type_by_index<Idx, Types...>::type;
  template<typename T>
  constexpr bool is_variant_v = is_variant<T>::value;
  template<typename T, typename U>
  constexpr bool is_type_in_variant_v = is_type_in_variant<T, U>::value;
  template<typename T, typename U>
  constexpr size_t get_index_in_variant_by_type_v = get_index_in_variant_by_type<T, U>::value;
  template<typename T>
  requires variant_traits::is_variant_v<std::remove_cvref_t<T>>
  constexpr size_t cnt_of_variant_types_v = cnt_of_variant_types<std::remove_cvref_t<T>>::value;
  template<size_t Idx, typename T>
  requires variant_traits::is_variant_v<std::remove_cvref_t<T>>
  using get_type_in_variant_by_index_t = typename get_type_in_variant_by_index<Idx, std::remove_cvref_t<T>>::type;

}


template<typename T, typename... Types>
struct VariantAlternative {
  static constexpr size_t type_index = variant_traits::get_index_by_type_v<T, Types...>;
  VariantAlternative() {}

  VariantAlternative(const T& val) {
    auto* ptr = static_cast<Variant<Types...>*>(this);
    new (std::launder(ptr->storage_)) T(val);
    ptr->active_index_ = type_index;
  }
  VariantAlternative(T&& val) {
    auto* ptr = static_cast<Variant<Types...>*>(this);
    new (std::launder(ptr->storage_)) T(std::move(val));
    ptr->active_index_ = type_index;
  }

  void CopyConstruct(const char* val_ptr, size_t idx) {
    auto* ptr = static_cast<Variant<Types...>*>(this);
    if (idx != type_index) {
      return;
    }
    auto storage_ptr = std::launder(reinterpret_cast<T*>(ptr->storage_));
    if (ptr->active_index_ != type_index) {
      (ptr->VariantAlternative<Types, Types...>::Destroy(), ...);
      ptr->active_index_ = Variant<Types...>::npos;
      new(storage_ptr) T(*reinterpret_cast<const T*>(val_ptr));
      ptr->active_index_ = type_index;
    } else {
      *storage_ptr = (*reinterpret_cast<const T*>(val_ptr));
    }
  }
  void MoveConstruct(char* val_ptr, size_t idx) {
    auto* ptr = static_cast<Variant<Types...>*>(this);
    if (idx != type_index) {
      return;
    }
    auto storage_ptr = std::launder(reinterpret_cast<T*>(ptr->storage_));
    if (ptr->active_index_ != type_index) {
      (ptr->VariantAlternative<Types, Types...>::Destroy(), ...);
      ptr->active_index_ = Variant<Types...>::npos;
      new(storage_ptr) T(std::move(*reinterpret_cast<T*>(val_ptr)));
      ptr->active_index_ = type_index;
    } else {
      *storage_ptr = std::move(*reinterpret_cast<T*>(val_ptr));
    }
  }

  Variant<Types...>& operator=(const T& val) {
    auto* ptr = static_cast<Variant<Types...>*>(this);
    auto storage_ptr = std::launder(reinterpret_cast<T*>(ptr->storage_));
    if (ptr->active_index_ == type_index) {
      *storage_ptr = val;
    } else {
      try {
        (ptr->VariantAlternative<Types, Types...>::Destroy(), ...);
        new(storage_ptr) T(val);
        ptr->active_index_ = type_index;
      } catch (...) {
        ptr->active_index_ = Variant<Types...>::npos;
        throw;
      }
    }
    return *ptr;
  }
  Variant<Types...>& operator=(T&& val) {
    auto* ptr = static_cast<Variant<Types...>*>(this);
    auto storage_ptr = std::launder(reinterpret_cast<T*>(ptr->storage_));
    if (ptr->active_index_ == type_index) {
      *storage_ptr = std::move(val);
    } else {
      try {
        (ptr->VariantAlternative<Types, Types...>::Destroy(), ...);
        new(storage_ptr) T(std::move(val));
        ptr->active_index_ = type_index;
      } catch (...) {
        ptr->active_index_ = Variant<Types...>::npos;
        throw;
      }
    }
    return *ptr;
  }

  template<typename... Args>
  T& Emplace(Args&&... args) {
    auto* ptr = static_cast<Variant<Types...>*>(this);
    try {
      (ptr->VariantAlternative<Types, Types...>::Destroy(), ...);
      new(std::launder(ptr->storage_)) T(std::forward<Args>(args)...);
      ptr->active_index_ = type_index;
    } catch (...) {
      ptr->active_index_ = Variant<Types...>::npos;
      throw;
    }
    return *std::launder(reinterpret_cast<T*>(ptr->storage_));;
  }

  void Destroy() {
    auto* ptr = static_cast<Variant<Types...>*>(this);
    if (ptr->active_index_ == type_index) {
      std::launder(reinterpret_cast<T*>(ptr->storage_))->~T();
    }
  }

  ~VariantAlternative() {}
};

namespace variant {
  template<typename T, typename... Types>
  bool holds_alternative(const Variant<Types...>& v) {
    return (v.index() == variant_traits::get_index_by_type_v<T, Types...>);
  }

  template<typename T, typename Var>
  decltype(auto) get(Var&& v)
    requires(variant_traits::is_variant_v<std::remove_cvref_t<Var>> &&
    variant_traits::is_type_in_variant_v<T, std::remove_cvref_t<Var>>)
  {
    if (v.index() != variant_traits::get_index_in_variant_by_type_v<T, std::remove_cvref_t<Var>>) {
      throw std::runtime_error("Index is not active");
    }

    if constexpr (std::is_const_v<std::remove_reference_t<Var>>) {
      auto* storage_ptr = std::launder(reinterpret_cast<const T*>(v.storage_));
      if constexpr (std::is_lvalue_reference_v<Var>) {
        return *storage_ptr;
      } else {
        return std::move(*storage_ptr);
      }
    } else {
      auto* storage_ptr = std::launder(reinterpret_cast<T*>(v.storage_));
      if constexpr (std::is_lvalue_reference_v<Var>) {
        return *storage_ptr;
      } else {
        return std::move(*storage_ptr);
      }
    }
  }
  template<size_t Idx, typename Var>
  decltype(auto) get(Var&& v)
  requires(variant_traits::is_variant_v<std::remove_cvref_t<Var>> &&
           Idx < variant_traits::cnt_of_variant_types_v<Var>)
  {
    if (v.index() != Idx) {
      throw std::runtime_error("Index is not active");
    }
    using T = variant_traits::get_type_in_variant_by_index_t<Idx, Var>;
    if constexpr (std::is_const_v<std::remove_reference_t<Var>>) {
      auto* storage_ptr = std::launder(reinterpret_cast<const T*>(v.storage_));
      if constexpr (std::is_lvalue_reference_v<Var>) {
        return *storage_ptr;
      } else {
        return std::move(*storage_ptr);
      }
    } else {
      auto* storage_ptr = std::launder(reinterpret_cast<T*>(v.storage_));
      if constexpr (std::is_lvalue_reference_v<Var>) {
        return *storage_ptr;
      } else {
        return std::move(*storage_ptr);
      }
    }
  }

  template<typename Var, typename... Vars>
  void throw_if_valueless(const Var& v, const Vars&... vs) {
    if (v.valueless_by_exception()) {
      throw std::runtime_error("bad_variant_access");
    }
    if constexpr (sizeof...(vs) > 0) {
      throw_if_valueless(vs...);
    }
  }

  template<typename... Types>
  struct compute_visit_return;
  template<typename Visitor, typename... Leading>
  struct compute_visit_return<Visitor, std::tuple<Leading...>> {
    using type = std::invoke_result_t<Visitor, Leading...>;
  };
  template<typename Visitor, typename... Leading, typename First, typename... Tail>
  struct compute_visit_return<Visitor, std::tuple<Leading...>, First, Tail...> {
    static constexpr size_t cnt_of_types = variant_traits::cnt_of_variant_types_v<std::remove_cvref_t<First>>;

    using res0 = typename compute_visit_return<
      Visitor,
      std::tuple<Leading..., decltype( variant::get<0>( std::declval<First>() ))>,
      Tail...>::type;

    template<typename T>
    struct check_all_types_are_equal {};
    template<size_t... Seq>
    struct check_all_types_are_equal<std::index_sequence<Seq...>> {
      static_assert((std::is_same_v<
        res0,
        typename compute_visit_return<
          Visitor,
          std::tuple<Leading..., decltype( variant::get<Seq>( std::declval<First>() ))>,
          Tail...>::type
        > && ...),
          "visitor must return the same type for all alternative combinations" );
      using type = res0;
    };
    using type = typename check_all_types_are_equal<std::make_index_sequence<cnt_of_types>>::type;
  };
  template<typename Visitor, typename... Vars>
  requires ((variant_traits::is_variant_v<std::remove_cvref_t<Vars>> && ...))
  using compute_visit_return_t = compute_visit_return<Visitor, std::tuple<>, Vars...>::type;

  template<typename Return, size_t I, typename Visitor, typename Var1, typename... Tail>
  Return visit_impl(Visitor&& visitor, Var1&& var1, Tail&&... tail) {
    using Var1Type = std::remove_cvref_t<Var1>;
    if constexpr (I < variant_traits::cnt_of_variant_types_v<Var1Type>) {
      if (I == var1.index()) {
        if constexpr (sizeof...(Tail) == 0) {
          return std::forward<Visitor>(visitor)(variant::get<I>(std::forward<Var1>(var1)));
        } else {
          auto&& extracted = variant::get<I>(std::forward<Var1>(var1));

          auto new_visitor = [&extracted, &visitor](auto&&... rest_args) -> auto {
            return std::forward<Visitor>(visitor)(
              std::forward<decltype(extracted)>(extracted),
              std::forward<decltype(rest_args)>(rest_args)...
              );
          };

          return visit_impl<Return, 0>(std::move(new_visitor), std::forward<Tail>(tail)...);
        }
      } else {
        return visit_impl<Return, I + 1>(
          std::forward<Visitor>(visitor),
          std::forward<Var1>(var1),
          std::forward<Tail>(tail)...
          );
      }
    } else {
      throw std::runtime_error("Error in visit");
    }
  }


  template<typename Visitor, typename... Vars>
  auto visit(Visitor&& visitor, Vars&&... vars)
  {
    throw_if_valueless(vars...);
    using Return = typename variant::compute_visit_return_t<Visitor, Vars...>;
    return variant::visit_impl<Return, 0, Visitor, Vars...>(std::forward<Visitor>(visitor), std::forward<Vars>(vars)...);
  }
}

template<typename... Types>
class Variant : private VariantAlternative<Types, Types...>... {
private:
  static constexpr size_t npos = sizeof...(Types);
  alignas(variant_traits::max_type_align_v<Types...>) char storage_[variant_traits::max_type_sizeof_v<Types...>];
  size_t active_index_;
public:
  template<typename U, typename... UTypes>
  friend class VariantAlternative;
  template<typename T, typename Var>
  friend decltype(auto) variant::get(Var&& v)
  requires(variant_traits::is_variant_v<std::remove_cvref_t<Var>> &&
           variant_traits::is_type_in_variant_v<T, std::remove_cvref_t<Var>>);
  template<size_t Idx, typename Var>
  friend decltype(auto) variant::get(Var&& v)
  requires(variant_traits::is_variant_v<std::remove_cvref_t<Var>> &&
           Idx < variant_traits::cnt_of_variant_types_v<Var>);

  using VariantAlternative<Types, Types...>::VariantAlternative...;
  using VariantAlternative<Types, Types...>::operator=...;
  Variant(): Variant(variant_traits::get_type_by_index_t<0, Types...>{}) {}

  Variant(const Variant& other) {
    active_index_ = Variant::npos;
    (VariantAlternative<Types, Types...>::CopyConstruct(other.storage_, other.active_index_), ...);
    active_index_ = other.active_index_;

  }
  Variant(Variant&& other) {
    active_index_ = Variant::npos;
    (VariantAlternative<Types, Types...>::MoveConstruct(other.storage_, other.active_index_), ...);
    active_index_ = other.active_index_;
    other.active_index_ = Variant<Types...>::npos;
  }

  Variant& operator=(const Variant& other) {
    if (&other == this) {
      return *this;
    }
    (VariantAlternative<Types, Types...>::CopyConstruct(other.storage_, other.active_index_), ...);
    active_index_ = other.active_index_;
    return *this;
  }
  Variant& operator=(Variant&& other) {
    if (&other == this) {
      return *this;
    }
    (VariantAlternative<Types, Types...>::MoveConstruct(other.storage_, other.active_index_), ...);
    active_index_ = other.active_index_;

    other.active_index_ = Variant<Types...>::npos;
    return *this;
  }

  template<size_t I, typename... Args>
  auto emplace(Args&&... args) -> variant_traits::get_type_by_index_t<I, Types...>& {
    return VariantAlternative<variant_traits::get_type_by_index_t<I, Types...>, Types...>::template Emplace<Args...>(std::forward<Args>(args)...);
  }
  template<typename T, typename... Args>
  auto emplace(Args&&... args) -> T& {
    return VariantAlternative<T, Types...>::template Emplace<Args...>(std::forward<Args>(args)...);
  }
  size_t index() const {
    return active_index_;
  }
  bool valueless_by_exception() const {
    return (active_index_ == npos);
  }

  ~Variant() {
    (VariantAlternative<Types, Types...>::Destroy(), ...);
  }
};

