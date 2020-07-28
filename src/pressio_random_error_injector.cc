#include <iterator>
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
#include "libpressio_ext/cpp/options.h"
#include "libpressio_ext/cpp/pressio.h"
#include "libpressio_ext/compat/optional.h"
#include "libpressio_ext/compat/memory.h"
#include "random_distributions.h"

namespace {
  template <class Registry>
  std::vector<std::string> plugin_names(Registry const& plugins) {
    std::vector<std::string> names;
    std::transform(std::begin(plugins), std::end(plugins),
                   std::back_inserter(names),
                   [](auto const& it) { return it.first; });
    return names;
}


  struct inject_error_float {
    inject_error_float(polymorphic_distribution<float>& dist, polymorphic_generator& gen): dist(dist), gen(gen) {}

    template <class T>
    int operator()(T* begin, T* end) {
           
      while(begin != end) {
        *begin += dist(gen);
        begin++;
      }
      return 0;
    }
    private:
    polymorphic_distribution<float>& dist;
    polymorphic_generator& gen;
  };
}

class random_error_injector_plugin: public libpressio_compressor_plugin {

  struct pressio_options 	get_options_impl () const override {
    struct pressio_options options = pressio_options();
    set(options, "random_error_injector:seed", seed);
    set(options, "random_error_injector:dist_name", dist_name);
    set(options, "random_error_injector:gen_name", gen_name);
    set(options, "random_error_injector:dist_args", pressio_data(dist_args.begin(), dist_args.end()));
    return options;
  };

  struct pressio_options 	get_configuration_impl () const override {
    pressio_options options;
    set(options, "pressio:thread_safe", (int)pressio_thread_safety_multiple);
    set(options, "random_error_injector:distribution", plugin_names(distribution_float_registry()));
    set(options, "random_error_injector:generators", plugin_names(generator_registry()));
    return options;
  };

  int 	set_options_impl (struct pressio_options const& options) override {
    get(options, "random_error_injector:seed", &seed);
    std::string tmp_name;
    if(get(options, "random_error_injector:dist_name", &tmp_name) == pressio_options_key_set) {
      if(distribution_float_registry().contains(tmp_name)) {
        dist_name = std::move(tmp_name);
      }
    }

    if(get(options, "random_error_injector:gen_name", &tmp_name) == pressio_options_key_set) {
      if(generator_registry().contains(tmp_name)) {
        gen_name = std::move(tmp_name);
      }

    }

    pressio_data tmp_data;
    if(get(options, "random_error_injector:dist_args", &tmp_data) == pressio_options_key_set) {
      dist_args = tmp_data.to_vector<float>();
    }

    if(get(options,"random_error_injector:compressor", &tmp_name) == pressio_options_key_set) {
      if(tmp_name != compressor_name) {
        auto plugin = compressor_plugins().build(tmp_name);
        if(plugin) {
          compressor = std::move(plugin);
          compressor_name = std::move(tmp_name);
        } else {
          return set_error(1, "invalid search method");
        }
      }

    }
    return 0;
  }

  int 	compress_impl (const pressio_data *input, struct pressio_data *output) override {
    pressio_data tmp = pressio_data::clone(*input);
    auto gen = generator_registry().build(gen_name);
    auto gen_seed = std::seed_seq{seed.value_or(time(nullptr))};
    gen->seed(gen_seed);
    auto dist_f = distribution_float_registry().build(dist_name);
    dist_f->configure(dist_args);
    pressio_data_for_each<int>(tmp, inject_error_float(*dist_f, *gen));
    return compressor->compress(&tmp, output);
  };

   int 	decompress_impl (const pressio_data *input, struct pressio_data *output) override {
     *output = pressio_data::clone(*input);
     return 0;
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
    return std::make_shared<random_error_injector_plugin>(*this);
  }

  private:
  std::string dist_name, gen_name, compressor_name;
  std::vector<float> dist_args;
  compat::optional<unsigned int> seed;
  pressio_compressor compressor;
};

static pressio_register X(compressor_plugins(), "random_error_injector", [](){ return compat::make_unique<random_error_injector_plugin>(); });
