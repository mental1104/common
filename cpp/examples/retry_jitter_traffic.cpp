#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct Config {
  int clients = 10000;
  int attempts = 5;
  double base_ms = 100.0;
  double factor = 2.0;
  double max_ms = 1600.0;
  double jitter_ratio = 0.2;
  double bin_ms = 10.0;
  std::uint64_t seed = 12345;
  std::string output = "/tmp/retry_jitter_traffic.svg";
};

struct Scenario {
  std::string name;
  bool exponential = false;
  bool jitter = false;
};

struct Series {
  Scenario scenario;
  std::vector<int> bins;
  int peak = 0;
  int min_nonzero = 0;
  int empty_bins = 0;
  double total_requests = 0.0;
};

void print_usage(const char *program) {
  std::cout
      << "Usage: " << program << " [options]\n"
      << "  --clients=N       clients that fail at the same time (default 10000)\n"
      << "  --attempts=N      retry attempts per client, excluding t=0 (default 5)\n"
      << "  --base-ms=N       base delay in ms (default 100)\n"
      << "  --factor=N        exponential factor (default 2)\n"
      << "  --max-ms=N        maximum delay in ms (default 1600)\n"
      << "  --jitter=N        symmetric jitter ratio in [0,1] (default 0.2)\n"
      << "  --bin-ms=N        measurement bucket width in ms (default 10)\n"
      << "  --seed=N          deterministic random seed (default 12345)\n"
      << "  --output=PATH     SVG output path (default /tmp/retry_jitter_traffic.svg)\n";
}

std::string option_value(const std::string &arg, const std::string &name) {
  const auto prefix = name + "=";
  if (arg.rfind(prefix, 0) != 0) {
    return {};
  }
  return arg.substr(prefix.size());
}

Config parse_args(int argc, char **argv) {
  Config cfg;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--help" || arg == "-h") {
      print_usage(argv[0]);
      std::exit(0);
    }

    auto set_int = [&](const std::string &name, int &target) {
      auto value = option_value(arg, name);
      if (!value.empty()) {
        target = std::stoi(value);
        return true;
      }
      return false;
    };
    auto set_double = [&](const std::string &name, double &target) {
      auto value = option_value(arg, name);
      if (!value.empty()) {
        target = std::stod(value);
        return true;
      }
      return false;
    };

    if (set_int("--clients", cfg.clients) ||
        set_int("--attempts", cfg.attempts) ||
        set_double("--base-ms", cfg.base_ms) ||
        set_double("--factor", cfg.factor) ||
        set_double("--max-ms", cfg.max_ms) ||
        set_double("--jitter", cfg.jitter_ratio) ||
        set_double("--bin-ms", cfg.bin_ms)) {
      continue;
    }
    if (auto value = option_value(arg, "--seed"); !value.empty()) {
      cfg.seed = static_cast<std::uint64_t>(std::stoull(value));
      continue;
    }
    if (auto value = option_value(arg, "--output"); !value.empty()) {
      cfg.output = value;
      continue;
    }
    throw std::invalid_argument("unknown option: " + arg);
  }

  if (cfg.clients <= 0 || cfg.attempts <= 0 || cfg.base_ms <= 0 ||
      cfg.factor < 1.0 || cfg.max_ms < cfg.base_ms || cfg.bin_ms <= 0 ||
      cfg.jitter_ratio < 0.0 || cfg.jitter_ratio > 1.0) {
    throw std::invalid_argument("invalid parameter range");
  }
  return cfg;
}

double base_delay(const Config &cfg, int attempt, bool exponential) {
  if (!exponential) {
    return cfg.base_ms;
  }
  return std::min(cfg.max_ms, cfg.base_ms * std::pow(cfg.factor, attempt));
}

double maybe_jitter(double delay, double ratio, bool enabled, std::mt19937_64 &rng) {
  if (!enabled || ratio == 0.0) {
    return delay;
  }
  std::uniform_real_distribution<double> dist(1.0 - ratio, 1.0 + ratio);
  return delay * dist(rng);
}

Series simulate(const Config &cfg, const Scenario &scenario) {
  std::vector<double> event_times;
  event_times.reserve(static_cast<std::size_t>(cfg.clients) * cfg.attempts);
  std::mt19937_64 rng(cfg.seed + (scenario.exponential ? 17 : 0) +
                      (scenario.jitter ? 101 : 0));

  for (int client = 0; client < cfg.clients; ++client) {
    double t = 0.0;
    for (int attempt = 0; attempt < cfg.attempts; ++attempt) {
      const double delay = base_delay(cfg, attempt, scenario.exponential);
      t += maybe_jitter(delay, cfg.jitter_ratio, scenario.jitter, rng);
      event_times.push_back(t);
    }
  }

  const auto max_it = std::max_element(event_times.begin(), event_times.end());
  const int bin_count =
      static_cast<int>(std::floor(*max_it / cfg.bin_ms)) + 2;
  Series series;
  series.scenario = scenario;
  series.bins.assign(bin_count, 0);

  for (double t : event_times) {
    const int bin = static_cast<int>(std::floor(t / cfg.bin_ms));
    ++series.bins.at(static_cast<std::size_t>(bin));
  }

  series.peak = *std::max_element(series.bins.begin(), series.bins.end());
  series.empty_bins = static_cast<int>(std::count(series.bins.begin(),
                                                 series.bins.end(), 0));
  series.min_nonzero = std::numeric_limits<int>::max();
  for (int count : series.bins) {
    if (count > 0) {
      series.min_nonzero = std::min(series.min_nonzero, count);
    }
  }
  if (series.min_nonzero == std::numeric_limits<int>::max()) {
    series.min_nonzero = 0;
  }
  series.total_requests =
      std::accumulate(series.bins.begin(), series.bins.end(), 0.0);
  return series;
}

std::string fmt(double value, int precision = 1) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(precision) << value;
  return out.str();
}

void write_svg(const Config &cfg, const std::vector<Series> &all) {
  const int width = 1100;
  const int panel_h = 190;
  const int left = 70;
  const int right = 30;
  const int top = 36;
  const int bottom = 34;
  const int gap = 18;
  const int height = top + static_cast<int>(all.size()) * panel_h +
                     static_cast<int>(all.size() - 1) * gap + bottom;
  const int plot_w = width - left - right;
  const int plot_h = panel_h - 58;

  std::size_t max_bins = 0;
  int global_peak = 1;
  for (const auto &series : all) {
    max_bins = std::max(max_bins, series.bins.size());
    global_peak = std::max(global_peak, series.peak);
  }

  std::ofstream svg(cfg.output);
  if (!svg) {
    throw std::runtime_error("cannot open SVG output: " + cfg.output);
  }

  svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width
      << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << " "
      << height << "\">\n";
  svg << "<rect width=\"100%\" height=\"100%\" fill=\"#f8fafc\"/>\n";
  svg << "<text x=\"24\" y=\"24\" font-family=\"Arial\" font-size=\"16\" "
         "font-weight=\"700\">Retry traffic spikes: no backoff, backoff, "
         "jitter, backoff + jitter</text>\n";
  svg << "<text x=\"560\" y=\"24\" font-family=\"Arial\" font-size=\"12\" "
         "fill=\"#475569\">clients="
      << cfg.clients << ", attempts=" << cfg.attempts
      << ", base=" << fmt(cfg.base_ms) << "ms, factor=" << fmt(cfg.factor)
      << ", max=" << fmt(cfg.max_ms) << "ms, jitter="
      << fmt(cfg.jitter_ratio, 2) << ", bin=" << fmt(cfg.bin_ms)
      << "ms</text>\n";

  const std::vector<std::string> colors = {"#dc2626", "#2563eb", "#16a34a",
                                           "#7c3aed"};
  for (std::size_t i = 0; i < all.size(); ++i) {
    const auto &series = all[i];
    const int y0 = top + static_cast<int>(i) * (panel_h + gap);
    const int plot_top = y0 + 34;
    const int plot_bottom = plot_top + plot_h;

    svg << "<text x=\"" << left << "\" y=\"" << (y0 + 18)
        << "\" font-family=\"Arial\" font-size=\"14\" font-weight=\"700\">"
        << series.scenario.name << " | peak=" << series.peak
        << ", low(nonzero)=" << series.min_nonzero
        << ", empty_bins=" << series.empty_bins << "</text>\n";
    svg << "<line x1=\"" << left << "\" y1=\"" << plot_bottom
        << "\" x2=\"" << (width - right) << "\" y2=\"" << plot_bottom
        << "\" stroke=\"#94a3b8\"/>\n";
    svg << "<line x1=\"" << left << "\" y1=\"" << plot_top
        << "\" x2=\"" << left << "\" y2=\"" << plot_bottom
        << "\" stroke=\"#94a3b8\"/>\n";
    svg << "<text x=\"12\" y=\"" << (plot_top + 12)
        << "\" font-family=\"Arial\" font-size=\"11\" fill=\"#64748b\">"
        << global_peak << "</text>\n";
    svg << "<text x=\"30\" y=\"" << (plot_bottom - 2)
        << "\" font-family=\"Arial\" font-size=\"11\" fill=\"#64748b\">0</text>\n";

    std::ostringstream points;
    for (std::size_t b = 0; b < series.bins.size(); ++b) {
      const double x = left + (max_bins <= 1 ? 0.0
                                             : (static_cast<double>(b) /
                                                (max_bins - 1)) *
                                                   plot_w);
      const double y =
          plot_bottom -
          (static_cast<double>(series.bins[b]) / global_peak) * plot_h;
      points << std::fixed << std::setprecision(2) << x << "," << y << " ";
    }
    svg << "<polyline fill=\"none\" stroke=\"" << colors[i % colors.size()]
        << "\" stroke-width=\"2\" points=\"" << points.str() << "\"/>\n";
  }

  svg << "</svg>\n";
}

int main(int argc, char **argv) {
  try {
    const Config cfg = parse_args(argc, argv);
    const std::vector<Scenario> scenarios = {
        {"no exponential backoff, no jitter", false, false},
        {"exponential backoff only", true, false},
        {"symmetric jitter only", false, true},
        {"exponential backoff + symmetric jitter", true, true},
    };

    std::vector<Series> all;
    for (const auto &scenario : scenarios) {
      all.push_back(simulate(cfg, scenario));
    }
    write_svg(cfg, all);

    std::cout << "SVG: " << cfg.output << "\n";
    std::cout << "scenario,peak,min_nonzero,empty_bins,total_requests\n";
    for (const auto &series : all) {
      std::cout << '"' << series.scenario.name << '"' << "," << series.peak
                << "," << series.min_nonzero << "," << series.empty_bins
                << "," << static_cast<long long>(series.total_requests)
                << "\n";
    }
  } catch (const std::exception &ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }
  return 0;
}
