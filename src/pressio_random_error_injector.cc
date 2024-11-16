#include <iterator>
#include <stdexcept>
#include <time.h>
#include <cstdint>
#include <random>
#include <memory>
#include <string>
#include <sstream>
#include <algorithm>
#include <type_traits>
#include "pressio_data.h"
#include "pressio_compressor.h"
#include "libpressio_ext/cpp/data.h"
#include "libpressio_ext/cpp/compressor.h"
#include "libpressio_ext/cpp/domain_manager.h"
#include "libpressio_ext/cpp/options.h"
#include "libpressio_ext/cpp/pressio.h"
#include "std_compat/optional.h"
#include "std_compat/memory.h"
#include "random_distributions.h"

extern "C" 
void libpressio_register_error_injector() {
}

namespace {
  template <class Registry>
  std::vector<std::string> plugin_names(Registry const& plugins) {
    std::vector<std::string> names;
    std::transform(std::begin(plugins), std::end(plugins),
                   std::back_inserter(names),
                   [](auto const& it) { return it.first; });
    return names;
}


  struct inject_error {
    inject_error( pressio_data& dist_args,
          compat::optional<unsigned int>& seed,
          std::string& gen_name,
          std::string& dist_name
        ): dist_args(dist_args), seed(seed), gen_name(gen_name), dist_name(dist_name)  {}

    template <class T>
    int operator()(T* begin, T* end) {
      auto gen = generator_registry().build(gen_name);
      if(!gen) {
        throw std::runtime_error("invalid generator " + gen_name);
      }
      auto gen_seed = std::seed_seq{seed.value_or(time(nullptr))};
      gen->seed(gen_seed);
      auto dist = get_distribution_registry<T>().build(dist_name);
      if(!dist) {
        throw std::runtime_error("invalid distribution " + dist_name);
      }
      dist->configure(dist_args.to_vector<double>());

           
      while(begin != end) {
        *begin += (*dist)(*gen);
        begin++;
      }
      return 0;
    }
    private:
    pressio_data& dist_args;
    compat::optional<unsigned int>& seed;
    std::string& gen_name;
    std::string& dist_name;
  };
}

class random_error_injector_plugin: public libpressio_compressor_plugin {

  struct pressio_options 	get_options_impl () const override {
    struct pressio_options options = pressio_options();
    set_meta(options, "random_error_injector:compressor", compressor_name, compressor);
    set(options, "random_error_injector:seed", seed);
    set(options, "random_error_injector:dist_name", dist_name);
    set(options, "random_error_injector:gen_name", gen_name);
    set(options, "random_error_injector:dist_args", dist_args);
    return options;
  };

  struct pressio_options 	get_documentation_impl () const override {
    struct pressio_options options = pressio_options();
    set_meta_docs(options, "random_error_injector:compressor", "compressor to use after applying errors", compressor);
    set(options, "pressio:description", "injects errors according to the specified distribution");
    set(options, "random_error_injector:seed", "random seed to use");
    set(options, "random_error_injector:dist_name", "name of the error distribution to use");
    set(options, "random_error_injector:gen_name", "name of the random number generator to use");
    set(options, "random_error_injector:dist_args", "the distribution arguments");
    set(options, "random_error_injector:real_distributions", "available distributions for real numbers");
    set(options, "random_error_injector:int_distributions", "available distributions for integer numbers");
    set(options, "random_error_injector:generators", "available distribution generators");
    return options;
  };


  struct pressio_options 	get_configuration_impl () const override {
    pressio_options options;
    set(options, "pressio:thread_safe", pressio_thread_safety_multiple);
    set(options, "random_error_injector:real_distributions", plugin_names(get_distribution_registry<float>()));
    set(options, "random_error_injector:int_distributions", plugin_names(get_distribution_registry<int32_t>()));
    set(options, "random_error_injector:generators", plugin_names(generator_registry()));
    
        std::vector<std::string> invalidations {"random_error_injector:seed", "random_error_injector:dist_args", "random_error_injector:dist_name", "random_error_injector:gen_name"}; 
        std::vector<pressio_configurable const*> invalidation_children {&*compressor}; 
        
        set(options, "predictors:error_dependent", get_accumulate_configuration("predictors:error_dependent", invalidation_children, invalidations));
        set(options, "predictors:error_agnostic", get_accumulate_configuration("predictors:error_agnostic", invalidation_children, invalidations));
        set(options, "predictors:runtime", get_accumulate_configuration("predictors:runtime", invalidation_children, invalidations));

    return options;
  };

  int 	set_options_impl (struct pressio_options const& options) override {
    get(options, "random_error_injector:seed", &seed);
    std::string tmp_name;
    if(get(options, "random_error_injector:dist_name", &tmp_name) == pressio_options_key_set) {
      if(get_distribution_registry<float>().contains(tmp_name) || get_distribution_registry<uint64_t>().contains(tmp_name)) {
        dist_name = std::move(tmp_name);
      }
    }

    if(get(options, "random_error_injector:gen_name", &tmp_name) == pressio_options_key_set) {
      if(generator_registry().contains(tmp_name)) {
        gen_name = std::move(tmp_name);
      }

    }

    get(options, "random_error_injector:dist_args", &dist_args);
    get_meta(options, "random_error_injector:compressor", compressor_plugins(), compressor_name, compressor);

    return 0;
  }

  int 	compress_impl (const pressio_data *input, struct pressio_data *output) override {
    pressio_data tmp = pressio_data::clone(domain_manager().make_readable(domain_plugins().build("malloc"),*input));
    try {
    pressio_data_for_each<int>(tmp, inject_error(dist_args, seed, gen_name, dist_name));
    } catch (std::invalid_argument const&) {
      return set_error(1, "invalid number of arguments passed " + std::to_string(dist_args.num_elements()));
    } catch (std::runtime_error const& e) {
      return set_error(2, e.what());
    }
    return compressor->compress(&tmp, output);
  };

   int 	decompress_impl (const pressio_data *input, struct pressio_data *output) override {
     return compressor->decompress(input, output);
   }

  public:

  int	major_version () const override {
    return 0;
  }

  int minor_version () const override {
    return 0;
  }

  int patch_version () const override {
    return 0;
  }

  const char* version() const override {
    return "0.0.0";
  }
  const char* prefix() const noexcept override {
    return "random_error_injector";
  }
  std::shared_ptr<libpressio_compressor_plugin> clone() override {
    return std::make_unique<random_error_injector_plugin>(*this);
  }

  private:
  std::string dist_name="uniform_real_distribution", gen_name="mt19937_64", compressor_name = "noop";
  pressio_data dist_args;
  compat::optional<unsigned int> seed;
  pressio_compressor compressor = compressor_plugins().build("noop");
};

static pressio_register X(compressor_plugins(), "random_error_injector", [](){ return compat::make_unique<random_error_injector_plugin>(); });
