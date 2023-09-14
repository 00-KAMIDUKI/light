#include <array>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <optional>
#include <ostream>
#include <span>
#include <filesystem>
#include <string_view>
#include <cmath>

// #define DEBUG

const char* help = 
  "Usage:\n"
  "  light [OPT] [ARGS]\n"
  "Options:\n"
  "  -C          [dev] Print brightness\n"
  "  -M          [dev] Print maximum brightness\n"
  "  -I          [dev] [val] [min_brightness] Increase brightness by percentage\n"
  "  -D          [dev] [val] [min_brightness] Decrease brightness by percentage\n"
  "  --help, -h  Print this help and exit\n"
  ;

struct conf {
  std::filesystem::path dev_path;
  float value;
  std::uint32_t min_brightness;
  enum {
    inc, dec, brightness, max_brightness, help
  } type;
};


using domain = std::array<float, 2>;
using range = std::array<std::uint32_t, 2>;

auto scale(domain domain, range range) {
  float x1 = domain[0], x2 = domain[1], y1 = range[0], y2 = range[1];
  float k = std::pow(y2, x1 / (x1 - x2)) * std::pow(y1, x2 / (x2 - x1));
  float a = std::pow(y1 / y2, 1 / (x1 - x2));

#ifdef DEBUG
  std::cout << "k = " << k << std::endl;
  std::cout << "a = " << a << std::endl;
#endif

  struct {
    float k;
    float a;
    auto operator()(float x) -> std::uint32_t {
      return k * std::pow(a, x);
    }
    auto reverse(std::uint32_t y) -> float {
      return std::log(y / k) / std::log(a);
    }
  } res{.k=k, .a=a};
  return res;
}

auto read_dev(std::filesystem::path const& dev_path, const char* filename) -> std::uint32_t {
  std::ifstream f{dev_path / filename};
  std::uint32_t res;
  f >> res;
  f.close();
  return res;
}

auto max_brightness(std::filesystem::path const& dev_path) {
  return read_dev(dev_path, "max_brightness");
}

auto brightness(std::filesystem::path const& dev_path) -> std::uint32_t {
  return read_dev(dev_path, "brightness");
}

auto change(std::filesystem::path const& dev_path, float val, std::uint32_t min_brightness) {
#ifdef DEBUG
  std::cout << "y0 = " << brightness(dev_path) << std::endl;
#endif
  auto f = scale({0, 100}, {min_brightness, max_brightness(dev_path)});
  auto x = f.reverse(brightness(dev_path)) + val;
  x = x < 0 ? 0 : x > 100 ? 100 : x;

  auto file = std::fstream{dev_path / "brightness"};
  file.clear();

#ifdef DEBUG
  std::cout << "x = " << x << std::endl;
  std::cout << "y = " << f(x) << std::endl;
#endif

  file << f(x) << '\0';
  file.close();
}

auto app(conf const& conf) {
  switch (conf.type) {
    case conf::help:
      std::cout << help << std::flush;
      return;
    case conf::max_brightness:
      std::cout << max_brightness(conf.dev_path) << std::endl;
      return;
    case conf::brightness:
      std::cout << brightness(conf.dev_path) << std::endl;
      return;
    case conf::inc:
      change(conf.dev_path, conf.value, conf.min_brightness);
      return;
    case conf::dec:
      change(conf.dev_path, -conf.value, conf.min_brightness);
      return;
  }
}


auto parse_conf(std::span<const char*> args) -> conf {
  conf conf;
  if (args.size() == 1) {
    conf.type = conf::help;
    return conf;
  }

  auto opt = std::string_view(args[1]); 

  if (opt == "-h" || opt == "--help") {
    conf.type = conf::help;
    return conf;
  }

  if (args.size() < 3) {
    std::cerr << "Not enough arguments." << std::endl;
    std::exit(1);
  }

  conf.dev_path = std::filesystem::path(args[2]);

  if (opt == "-C") {
    conf.type = conf::brightness;
    return conf;
  }

  if (opt == "-M") {
    conf.type = conf::max_brightness;
    return conf;
  }

  if (args.size() < 5) {
    std::cerr << "Not enough arguments." << std::endl;
    std::exit(1);
  }

  float val;
  float min_brightness;

  auto parse_number = [](std::string_view sv, auto& value) {
    if (std::from_chars(sv.data(), sv.data() + sv.size(), value).ec != std::errc{}) {
      std::cerr << "Cannot parse number from " << sv << std::endl;
      std::exit(1);
    }
  };
  
  parse_number(args[3], val);
  parse_number(args[4], min_brightness);
  
  conf.value = val;
  conf.min_brightness = min_brightness;

  if (opt == "-I") {
    conf.type = conf::inc;
    return conf;
  }

  if (opt == "-D") {
    conf.type = conf::dec;
    return conf;
  }

  std::cerr << "Unexpected option";
  std::exit(1);
}

int main(int argc, char const** argv) {
  auto args = std::span(argv, argc);
  auto conf = parse_conf(args);
  app(conf);
}

