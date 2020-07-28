#include <cstdint>
#include <stdexcept>
#include <memory>
#include <utility>
#include <random>
#include <stdexcept>
#include <libpressio_ext/compat/std_compat.h>
#include "random_distributions.h"

template <class Impl>
class polymorphic_generator_impl : public polymorphic_generator {
  public: template <class... T>
  polymorphic_generator_impl(T&&... args): impl(std::forward<T>(args)...) {}

  typename polymorphic_generator::result_type min() final { return impl.min(); }
  typename polymorphic_generator::result_type max() final { return impl.max(); }
  typename polymorphic_generator::result_type operator()() final { return impl(); }
  void seed(std::seed_seq& seed) final { impl.seed(seed); } 
  std::unique_ptr<polymorphic_generator> clone() final {
    return compat::make_unique<polymorphic_generator_impl>(this->impl);
  }

  private:
  Impl impl;
};

template <typename T, typename ArgType, typename = void>
struct is_one_arg_constructable : std::false_type {};

template <typename T, typename ArgType>
struct is_one_arg_constructable<T, ArgType, compat::void_t<
  decltype(T(std::declval<ArgType>()))>>
    : std::true_type {};

template <typename T, typename ArgType, typename = void>
struct is_two_arg_constructable : std::false_type {};

template <typename T, typename ArgType>
struct is_two_arg_constructable<T, ArgType, compat::void_t<
  decltype(T(std::declval<ArgType>())),
  decltype(T(std::declval<ArgType>(), std::declval<ArgType>()))
  >>
    : std::true_type {};


template <class Distribution>
typename std::enable_if<
compat::conjunction<
is_one_arg_constructable<Distribution, typename Distribution::result_type>,
is_two_arg_constructable<Distribution, typename Distribution::result_type>
  >::value
  , typename Distribution::param_type>::type
make_param(std::vector<typename Distribution::result_type> const& params)
{
  if(params.size() == 2) {
    return typename Distribution::param_type(params[0], params[1]);
  } else if(params.size() == 1) {
    return typename Distribution::param_type(params[0]);
  } else {
    throw std::invalid_argument("this distribution type expects 1 or 2 arguments");
  }
}

template <class Distribution>
typename std::enable_if<
  compat::conjunction<
  is_one_arg_constructable<Distribution, typename Distribution::result_type>,
  compat::negation<is_two_arg_constructable<Distribution, typename Distribution::result_type>>
  >::value,
  typename Distribution::param_type>::type
make_param(std::vector<typename Distribution::result_type> const& params)
{
  if(params.size() == 1) {
    return typename Distribution::param_type(params[0]);
  } else {
    throw std::invalid_argument("this distribution type expects 1 argument");
  }
}



template <class Impl>
class polymorphic_distribution_impl: public polymorphic_distribution<typename Impl::result_type> {
  public:
  template <class... T>
  polymorphic_distribution_impl(T&&... args): impl(std::forward<T>(args)...) {}

  using result_type = typename polymorphic_distribution<typename Impl::result_type>::result_type;

  result_type operator()(polymorphic_generator &g) final {
    return impl(g);
  }

  result_type min() {return impl.min(); }

  result_type max() {return impl.max(); }

  bool operator==(polymorphic_distribution<result_type> const& rhs)const {
    try {
      auto const& rhs_impl = dynamic_cast<decltype(*this)>(rhs);
      return impl == rhs_impl.impl;
    } catch (std::bad_cast const&) {
      return false;
    }
  }

  bool operator!=(polymorphic_distribution<result_type> const& rhs)const {
    return not (*this == rhs);
  }

  void reset() {
    impl.reset();
  }

  void configure(std::vector<result_type> const& parameters) final {
    impl.param(make_param<Impl>(parameters));
  }

  std::unique_ptr<polymorphic_distribution<typename Impl::result_type>> clone() final {
    return compat::make_unique<polymorphic_distribution_impl<Impl>>(this->impl);
  };

  private:
  Impl impl;
};

pressio_registry<std::unique_ptr<polymorphic_generator>>& generator_registry() {
  static pressio_registry<std::unique_ptr<polymorphic_generator>> registry;
  return registry;
}
pressio_registry<std::unique_ptr<polymorphic_distribution<float>>>& distribution_float_registry() {
  static pressio_registry<std::unique_ptr<polymorphic_distribution<float>>> registry;
  return registry;
}
pressio_registry<std::unique_ptr<polymorphic_distribution<double>>>& distribution_double_registry() {
  static pressio_registry<std::unique_ptr<polymorphic_distribution<double>>> registry;
  return registry;
}

#define RANDOM_REGISTER_STD_GENERATOR(type) \
  pressio_register type (generator_registry(), #type, []{return compat::make_unique<polymorphic_generator_impl<std::type>>(); })
#define RANDOM_REGISTER_STD_DISTRIBUTION(type) \
  pressio_register f_##type (distribution_float_registry(), #type, []{return compat::make_unique<polymorphic_distribution_impl<std::type<float>>>(); }); \
  pressio_register d_##type (distribution_double_registry(), #type, []{return compat::make_unique<polymorphic_distribution_impl<std::type<double>>>(); });

namespace {
  RANDOM_REGISTER_STD_GENERATOR(minstd_rand);
  RANDOM_REGISTER_STD_GENERATOR(mt19937_64);
  RANDOM_REGISTER_STD_GENERATOR(ranlux48_base);
  RANDOM_REGISTER_STD_GENERATOR(knuth_b);
  RANDOM_REGISTER_STD_DISTRIBUTION(cauchy_distribution);
  RANDOM_REGISTER_STD_DISTRIBUTION(chi_squared_distribution);
  RANDOM_REGISTER_STD_DISTRIBUTION(extreme_value_distribution);
  RANDOM_REGISTER_STD_DISTRIBUTION(fisher_f_distribution);
  RANDOM_REGISTER_STD_DISTRIBUTION(gamma_distribution);
  RANDOM_REGISTER_STD_DISTRIBUTION(lognormal_distribution);
  RANDOM_REGISTER_STD_DISTRIBUTION(normal_distribution);
  RANDOM_REGISTER_STD_DISTRIBUTION(student_t_distribution);
  RANDOM_REGISTER_STD_DISTRIBUTION(weibull_distribution);
}
