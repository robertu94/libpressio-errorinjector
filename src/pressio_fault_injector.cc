#include <cmath>
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
#include "libpressio_ext/cpp/options.h"
#include "libpressio_ext/cpp/pressio.h"
#include "libpressio_ext/cpp/domain_manager.h"
#include "std_compat/optional.h"
#include "std_compat/memory.h"

enum class bit_action {
  flip = 1,
  unset = 2,
  set = 4
};
std::string to_string(bit_action const& action) {
  switch (action) {
    case bit_action::flip:
      return "flip";
    case bit_action::unset:
      return "unset";
    case bit_action::set:
      return "set";
  }
  throw std::logic_error("unhanded bit_action");
}
bit_action from_string(std::string const& s) {
  if(s == "flip") {
    return bit_action::flip;
  } else if (s == "unset") {
    return bit_action::unset;
  } else if (s == "set") {
    return bit_action::set;
  } else {
    throw std::invalid_argument(s);
  }
}


class fault_injector_plugin: public libpressio_compressor_plugin {

  struct pressio_options 	get_options_impl () const override {
    struct pressio_options options = pressio_options();
    set(options, "fault_injector:seed", seed);
    set(options, "fault_injector:injections", injections);
    set(options, "fault_injector:injection_mode", mode);
    set_type(options, "fault_injector:injection_mode_str", pressio_option_charptr_type);
    set_meta(options, "fault_injector:compressor", compressor_name, compressor);
    return options;
  };

  struct pressio_options 	get_documentation_impl () const override {
    struct pressio_options options = pressio_options();
    set_meta_docs(options, "fault_injector:compressor", "name of the compressor to inject errors after compression", compressor);
    set(options, "pressio:description", "injects single bit errors of specified distribution");
    set(options, "fault_injector:seed", "random number seed");
    set(options, "fault_injector:injections", "the number of injections to make");
    set(options, "fault_injector:injection_mode", "the method of performing injections");
    set(options, "fault_injector:injection_mode_str", "human interpretable mode");
    return options;
  };

  struct pressio_options 	get_configuration_impl () const override {
    pressio_options options;
    set(options, "pressio:thread_safe", pressio_thread_safety_multiple);
    set(options, "fault_injector:injection_mode_str", std::vector<std::string>{"set", "unset", "flip"});
    
        std::vector<std::string> invalidations {"fault_injector:seed", "fault_injector:injections", "fault_injector:injection_mode", "fault_injector:injection_mode_str"}; 
        std::vector<pressio_configurable const*> invalidation_children {&*compressor}; 
        
        set(options, "predictors:error_dependent", get_accumulate_configuration("predictors:error_dependent", invalidation_children, invalidations));
        set(options, "predictors:error_agnostic", get_accumulate_configuration("predictors:error_agnostic", invalidation_children, invalidations));
        set(options, "predictors:runtime", get_accumulate_configuration("predictors:runtime", invalidation_children, invalidations));

    return options;
  };

  int 	set_options_impl (struct pressio_options const& options) override {
    get_meta(options, "fault_injector:compressor", compressor_plugins(), compressor_name, compressor);
    get(options, "fault_injector:seed", &seed);
    get(options, "fault_injector:injections", &injections);
    get(options, "fault_injector:injection_mode", &mode);
    std::string mode_str;
    if(get(options, "fault_injector:injection_mode_str", &mode_str) == pressio_options_key_set) {
      try{
        mode = static_cast<unsigned int>(from_string(mode_str));
      } catch(std::invalid_argument&) {
        return set_error(1, "invalid mode_str " + mode_str);
      }
    }
    return 0;
  }

  int 	compress_impl (const pressio_data *input, struct pressio_data *output) override {
    int ret = compressor->compress(input, output);
    *output = domain_manager().make_readable(domain_plugins().build("malloc"), std::move(*output));
    uint8_t* bytes = static_cast<uint8_t*>(output->data());
    size_t len = output->size_in_bytes();

    std::mt19937_64 gen;
    gen.seed(seed.value_or(time(nullptr)));
    std::uniform_int_distribution<size_t> len_dist(0, len);
    std::uniform_int_distribution<uint8_t> bit_dist(0, 7);

    for (unsigned int i = 0; i < injections; ++i) {
      uint8_t mask = (1<<bit_dist(gen));
      uint8_t& byte = bytes[len_dist(gen)];

      switch(bit_action(mode)) {
        case bit_action::flip:
          byte ^= mask;
          break;
        case bit_action::set:
          byte |= mask;
          break;
        case bit_action::unset:
          byte &= (~mask);
          break;
      }
    }

    return ret;
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
    return "fault_injector";
  }
  std::shared_ptr<libpressio_compressor_plugin> clone() override {
    return std::make_unique<fault_injector_plugin>(*this);
  }

  private:
  std::string compressor_name = "noop";
  compat::optional<unsigned int> seed;
  unsigned int injections = 1;
  unsigned int mode = (unsigned int)bit_action::flip;
  pressio_compressor compressor = compressor_plugins().build("noop");
};

static pressio_register X(compressor_plugins(), "fault_injector", [](){ return compat::make_unique<fault_injector_plugin>(); });
