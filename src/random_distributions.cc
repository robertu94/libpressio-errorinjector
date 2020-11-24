#include <cstdint>
#include <stdexcept>
#include <memory>
#include <utility>
#include <random>
#include <stdexcept>
#include <std_compat/std_compat.h>
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
make_param(std::vector<double> const& params)
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
make_param(std::vector<double> const& params)
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

  void configure(std::vector<double> const& parameters) final {
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

template <class T>
pressio_registry<std::unique_ptr<polymorphic_distribution<T>>>& get_distribution_registry() {
  assert(false);
}

#define CREATE_REGISTRY(type) template <> \
    pressio_registry<std::unique_ptr<polymorphic_distribution<type>>>& get_distribution_registry<type>() { \
    static pressio_registry<std::unique_ptr<polymorphic_distribution<type>>> registry; \
    return registry; \
  } 
CREATE_REGISTRY(float)
CREATE_REGISTRY(double)
CREATE_REGISTRY(int8_t)
CREATE_REGISTRY(int16_t)
CREATE_REGISTRY(int32_t)
CREATE_REGISTRY(int64_t)
CREATE_REGISTRY(uint8_t)
CREATE_REGISTRY(uint16_t)
CREATE_REGISTRY(uint32_t)
CREATE_REGISTRY(uint64_t)



#define RANDOM_REGISTER_STD_GENERATOR(type) \
  pressio_register type (generator_registry(), #type, []{return compat::make_unique<polymorphic_generator_impl<std::type>>(); })
#define RANDOM_REGISTER_REAL_STD_DISTRIBUTION(type) \
  pressio_register f_##type (get_distribution_registry<float>(), #type, []{return compat::make_unique<polymorphic_distribution_impl<std::type<float>>>(); }); \
  pressio_register d_##type (get_distribution_registry<double>(), #type, []{return compat::make_unique<polymorphic_distribution_impl<std::type<double>>>(); });
#define RANDOM_REGISTER_INT_STD_DISTRIBUTION(type) \
  pressio_register i8_##type (get_distribution_registry<int8_t>(), #type, []{return compat::make_unique<polymorphic_distribution_impl<std::type<int8_t>>>(); }); \
  pressio_register i16_##type (get_distribution_registry<int16_t>(), #type, []{return compat::make_unique<polymorphic_distribution_impl<std::type<int16_t>>>(); }); \
  pressio_register i32_##type (get_distribution_registry<int32_t>(), #type, []{return compat::make_unique<polymorphic_distribution_impl<std::type<int32_t>>>(); }); \
  pressio_register i64_##type (get_distribution_registry<int64_t>(), #type, []{return compat::make_unique<polymorphic_distribution_impl<std::type<int64_t>>>(); }); \
  pressio_register u8_##type  (get_distribution_registry<uint8_t>(), #type, []{return compat::make_unique<polymorphic_distribution_impl<std::type<uint8_t>>>(); }); \
  pressio_register u16_##type (get_distribution_registry<uint16_t>(), #type, []{return compat::make_unique<polymorphic_distribution_impl<std::type<uint16_t>>>(); }); \
  pressio_register u32_##type (get_distribution_registry<uint32_t>(), #type, []{return compat::make_unique<polymorphic_distribution_impl<std::type<uint32_t>>>(); }); \
  pressio_register u64_##type (get_distribution_registry<uint64_t>(), #type, []{return compat::make_unique<polymorphic_distribution_impl<std::type<uint64_t>>>(); });

namespace {
  RANDOM_REGISTER_STD_GENERATOR(minstd_rand);
  RANDOM_REGISTER_STD_GENERATOR(mt19937_64);
  RANDOM_REGISTER_STD_GENERATOR(ranlux48_base);
  RANDOM_REGISTER_STD_GENERATOR(knuth_b);
  RANDOM_REGISTER_REAL_STD_DISTRIBUTION(cauchy_distribution);
  RANDOM_REGISTER_REAL_STD_DISTRIBUTION(chi_squared_distribution);
  RANDOM_REGISTER_REAL_STD_DISTRIBUTION(extreme_value_distribution);
  RANDOM_REGISTER_REAL_STD_DISTRIBUTION(fisher_f_distribution);
  RANDOM_REGISTER_REAL_STD_DISTRIBUTION(gamma_distribution);
  RANDOM_REGISTER_REAL_STD_DISTRIBUTION(lognormal_distribution);
  RANDOM_REGISTER_REAL_STD_DISTRIBUTION(normal_distribution);
  RANDOM_REGISTER_REAL_STD_DISTRIBUTION(student_t_distribution);
  RANDOM_REGISTER_REAL_STD_DISTRIBUTION(weibull_distribution);
  RANDOM_REGISTER_REAL_STD_DISTRIBUTION(uniform_real_distribution);
  RANDOM_REGISTER_INT_STD_DISTRIBUTION(uniform_int_distribution);
  RANDOM_REGISTER_INT_STD_DISTRIBUTION(binomial_distribution);
  RANDOM_REGISTER_INT_STD_DISTRIBUTION(negative_binomial_distribution);
  RANDOM_REGISTER_INT_STD_DISTRIBUTION(geometric_distribution);
  RANDOM_REGISTER_INT_STD_DISTRIBUTION(poisson_distribution);

}
