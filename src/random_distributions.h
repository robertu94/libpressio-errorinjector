#include <cstdint>
#include <random>
#include <utility>
#include <libpressio_ext/cpp/pressio.h>
#include <vector>

class polymorphic_generator {
  public:
    //in libstdcxx, all random geneators have the same result type, use it
    using result_type = std::minstd_rand::result_type;

    virtual ~polymorphic_generator()=default;

    static constexpr result_type min() {
      return std::minstd_rand::min();
    }
    static constexpr result_type max() {
      return std::minstd_rand::max();
    }
    virtual result_type operator()()=0;
    virtual void seed(std::seed_seq& seed)=0;
    virtual std::unique_ptr<polymorphic_generator> clone()=0;
};

template <class ResultType>
class polymorphic_distribution {
  public:
  using result_type = ResultType;

  virtual ~polymorphic_distribution()=default;

  virtual result_type operator()(polymorphic_generator &g)=0;
  virtual result_type min()=0;
  virtual result_type max()=0;
  virtual bool operator==(polymorphic_distribution const&)const=0 ;
  virtual bool operator!=(polymorphic_distribution const&)const=0;
  virtual void reset()=0;
  virtual void configure(std::vector<double> const&)=0;
  virtual std::unique_ptr<polymorphic_distribution> clone()=0;
};


pressio_registry<std::unique_ptr<polymorphic_generator>>& generator_registry();

template <class T> pressio_registry<std::unique_ptr<polymorphic_distribution<T>>>& get_distribution_registry();
template <> pressio_registry<std::unique_ptr<polymorphic_distribution<double>>>& get_distribution_registry<double>();
template <> pressio_registry<std::unique_ptr<polymorphic_distribution<float>>>& get_distribution_registry<float>();
template <> pressio_registry<std::unique_ptr<polymorphic_distribution<int8_t>>>& get_distribution_registry<int8_t>();
template <> pressio_registry<std::unique_ptr<polymorphic_distribution<int16_t>>>& get_distribution_registry<int16_t>();
template <> pressio_registry<std::unique_ptr<polymorphic_distribution<int32_t>>>& get_distribution_registry<int32_t>();
template <> pressio_registry<std::unique_ptr<polymorphic_distribution<int64_t>>>& get_distribution_registry<int64_t>();
template <> pressio_registry<std::unique_ptr<polymorphic_distribution<uint8_t>>>& get_distribution_registry<uint8_t>();
template <> pressio_registry<std::unique_ptr<polymorphic_distribution<uint16_t>>>& get_distribution_registry<uint16_t>();
template <> pressio_registry<std::unique_ptr<polymorphic_distribution<uint32_t>>>& get_distribution_registry<uint32_t>();
template <> pressio_registry<std::unique_ptr<polymorphic_distribution<uint64_t>>>& get_distribution_registry<uint64_t>();
template <> pressio_registry<std::unique_ptr<polymorphic_distribution<bool>>>& get_distribution_registry<bool>();
