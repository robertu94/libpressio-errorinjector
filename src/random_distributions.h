#include <cstdint>
#include <random>
#include <utility>
#include <libpressio_ext/cpp/pressio.h>
#include <vector>

class polymorphic_generator {
  public:
    using result_type = std::uint64_t;

    virtual ~polymorphic_generator()=default;

    virtual result_type min()=0;
    virtual result_type max()=0;
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
  virtual void configure(std::vector<result_type> const&)=0;
  virtual std::unique_ptr<polymorphic_distribution> clone()=0;
};


pressio_registry<std::unique_ptr<polymorphic_generator>>& generator_registry();
pressio_registry<std::unique_ptr<polymorphic_distribution<float>>>& distribution_float_registry();
pressio_registry<std::unique_ptr<polymorphic_distribution<double>>>& distribution_double_registry();
