#include <iostream>
#include <getopt.h>
#include <cstring>
#include <random>
#include <array>
#include <fstream>
#include <cmath>

enum class eOperatingMode {
  Null,
  Normal,
  Cluster
};

const char * opmode_to_str(eOperatingMode om) {
  switch (om) {
  case eOperatingMode::Null: return "Null";
  case eOperatingMode::Normal: return "Normal";
  case eOperatingMode::Cluster: return "Cluster";
  default: return "unknown";
  }
}

eOperatingMode get_op_mode(std::string op_mode) {
  for (int i = 0; i < op_mode.size(); i++) {
    auto c = std::tolower(op_mode[i]);
    op_mode[i] = c;
  }
  if (op_mode == "normal") {
    return eOperatingMode::Normal;
  } else if (op_mode == "cluster") {
    return eOperatingMode::Cluster;
  }
  throw std::invalid_argument("Operating mode can be only one of these two: [normal, cluster]");
}

struct Config {
  int n_pairs_to_generate = 1000;
  unsigned long seed = 0;
  eOperatingMode mode = eOperatingMode::Null;
};

std::ostream &operator<<(std::ostream &os, const Config &config) {
  return os << "n_pairs: " << config.n_pairs_to_generate
	    << ", seed: " << config.seed
            << ", mode: " << opmode_to_str(config.mode);
}

Config parse_command_line(int argc, char **argv) {
  if (argc < 1)
    throw std::invalid_argument("usage: "
				+ std::string(basename(argv[0]))
				+ " --mode [normal|cluster] [--n_pairs(1000) <pairs_amount> --seed(0) <the_seed>]>");
  Config config;
  const static option long_options[] = {
    {"n_pairs", required_argument, 0, 'n'},
    {"seed", required_argument, 0, 's'},
    {"mode", required_argument, 0, 'm'},
    {0, 0, 0, 0}
  };
  int option_index = 0;
  int c = 0;
  while (true) {
    c = getopt_long(argc, argv, "n:s:m:h", long_options, &option_index);
    if (c == -1)
      break;
    switch (c) {
    case 'n': config.n_pairs_to_generate = std::stod(optarg); break;
    case 's': config.seed = std::stoul(optarg); break;
    case 'm': {
      config.mode = get_op_mode(optarg);
    } break;
    default: {
      
    }
    }
  }
  if (config.mode == eOperatingMode::Null)
    throw std::invalid_argument("Operating mode can be only one of these two: [normal, cluster]");
  return config;
}

struct Point {
  double x, y;
};

struct PointPair {
  Point first, second;
};

static double Square(double A) {
  double Result = A*A;
  return Result;
}

static double RadiansFromDegrees(double Degrees) {
  double Result = 0.01745329251994329577f * Degrees;
  return Result;
}

static double ReferenceHaversine(double X0, double Y0, double X1, double Y1, double EarthRadius = 6372.8d) {
  double lat1 = Y0, lat2 = Y1, lon1 = X0, lon2 = X1;
  double dLat = RadiansFromDegrees(lat2 - lat1);
  double dLon = RadiansFromDegrees(lon2 - lon1);
  lat1 = RadiansFromDegrees(lat1);
  lat2 = RadiansFromDegrees(lat2);

  double a = Square(sin(dLat / 2.0)) + cos(lat1) * cos(lat2) * Square(sin(dLon / 2.0));
  double c = 2.0 * asin(sqrt(a));

  double Result = EarthRadius * c;

  return Result;
}

int normal_mode_part(std::array<PointPair, 1000> &pairs, int n_generated, int n_total, std::default_random_engine &re) {
  static std::uniform_real_distribution<double> xg(-180.0d, 180.0d);
  static std::uniform_real_distribution<double> yg(-1.0d, 1.0d);
  int n = 0;
  for (; n < std::min(n_total - n_generated, 1000); n++) {
    PointPair pp;
    pp.first.x = xg(re);
    pp.first.y = asin(yg(re));
    pp.second.x = xg(re);
    pp.second.y = asin(yg(re));
    pairs[n] = pp;
  }
  return n;
}

void normal_mode(std::default_random_engine &re, std::array<PointPair, 1000> &pairs, int n_total, const std::string &output_filename) {
  int n_generated = 0;
  std::ofstream f(output_filename);
  std::ofstream ff(output_filename + ".answers.f64", std::ios_base::out);
  if (f.bad() || !f.is_open())
    throw std::runtime_error("can't create/open a file " + output_filename);
  if (ff.bad() || !ff.is_open())
    throw std::runtime_error("can't create/open an answer file " + output_filename);
  double hav_avg = 0;
  f << "{\"pairs\":[\n";
  for (int i = 0; i + 1000 < n_total; i += 1000) {
    const auto iteration_gen= normal_mode_part(pairs, i, n_total, re);
    for (int j = 0; j < iteration_gen; j++) {
      const auto &p = pairs[j];
      const auto &p0 = p.first;
      const auto &p1 = p.second;      
      f << "\t{\"x0\":" << p0.x << ", \"y0\":" << p0.y << ", ";
      f << "\"x1\":"  << p1.x << ", \"y1\":" << p1.y << "}";
      if (iteration_gen + n_generated - 1 != n_total) f << ',';
      f << '\n';
      const auto result = (ReferenceHaversine(p0.x, p0.y, p1.x, p1.y) / n_total);
      ff << result;
      hav_avg += result;
    }
    n_generated += iteration_gen;
  }
  if (n_generated != n_total) {
    const auto iteration_gen= normal_mode_part(pairs, n_generated, n_total, re);
    for (int j = 0; j < iteration_gen; j++) {
      const auto &p = pairs[j];
      const auto &p0 = p.first;
      const auto &p1 = p.second;      
      f << "\t{\"x0\":" << p0.x << ", \"y0\":" << p0.y << ", ";
      f << "\"x1\":"  << p1.x << ", \"y1\":" << p1.y << "}";
      if (iteration_gen - 1 != j) f << ',';
      f << '\n';
      const auto result = (ReferenceHaversine(p0.x, p0.y, p1.x, p1.y) / n_total);
      ff << result;
      hav_avg += result;
    }
  }
  f << "]}\n";
  std::cout << "ReferenceHaversine avg: " << hav_avg << std::endl;
  return;
}

static std::uniform_real_distribution<double> gxg[12];
static std::uniform_real_distribution<double> gyg[12];
void init_cluster_arr() {
  int i = 0;
  double x0 = 10;
  double x1 = 30;
  double y0 = -20;
  double y1 = -10;
  gxg[i] = std::uniform_real_distribution<double>(x0, x1);
  gyg[i] = std::uniform_real_distribution<double>(y0, y1);

  i = 1;
  x0 = 5;
  x1 = 15;
  y0 = -2;
  y1 = 10;
  gxg[i] = std::uniform_real_distribution<double>(x0, x1);
  gyg[i] = std::uniform_real_distribution<double>(y0, y1);

  i = 2;
  x0 = 80;
  x1 = 120;
  y0 = 20;
  y1 = 40;
  gxg[i] = std::uniform_real_distribution<double>(x0, x1);
  gyg[i] = std::uniform_real_distribution<double>(y0, y1);

  i = 3;
  x0 = -90;
  x1 = -75;
  y0 = -90;
  y1 = -70;
  gxg[i] = std::uniform_real_distribution<double>(x0, x1);
  gyg[i] = std::uniform_real_distribution<double>(y0, y1);

  i = 4;
  x0 = -60;
  x1 = -5;
  y0 = 0;
  y1 = 20;
  gxg[i] = std::uniform_real_distribution<double>(x0, x1);
  gyg[i] = std::uniform_real_distribution<double>(y0, y1);

  i = 5;
  x0 = -6;
  x1 = -1;
  y0 = 55;
  y1 = 60;
  gxg[i] = std::uniform_real_distribution<double>(x0, x1);
  gyg[i] = std::uniform_real_distribution<double>(y0, y1);

  i = 6;
  x0 = -180;
  x1 = -65;
  y0 = 33;
  y1 = 40;
  gxg[i] = std::uniform_real_distribution<double>(x0, x1);
  gyg[i] = std::uniform_real_distribution<double>(y0, y1);

  i = 7;
  x0 = 75;
  x1 = 80;
  y0 = -30;
  y1 = 30;
  gxg[i] = std::uniform_real_distribution<double>(x0, x1);
  gyg[i] = std::uniform_real_distribution<double>(y0, y1);

  i = 8;
  x0 = -140;
  x1 = -15;
  y0 = 50;
  y1 = 55;
  gxg[i] = std::uniform_real_distribution<double>(x0, x1);
  gyg[i] = std::uniform_real_distribution<double>(y0, y1);

  i = 9;
  x0 = 150;
  x1 = 155;
  y0 = 0;
  y1 = 20;
  gxg[i] = std::uniform_real_distribution<double>(x0, x1);
  gyg[i] = std::uniform_real_distribution<double>(y0, y1);

  i = 10;
  x0 = -77;
  x1 = 77;
  y0 = -77;
  y1 = 77;
  gxg[i] = std::uniform_real_distribution<double>(x0, x1);
  gyg[i] = std::uniform_real_distribution<double>(y0, y1);

  i = 11;
  x0 = -160;
  x1 = -152;
  y0 = -90;
  y1 = -70;
  gxg[i] = std::uniform_real_distribution<double>(x0, x1);
  gyg[i] = std::uniform_real_distribution<double>(y0, y1);  
}

int cluster_mode_part(std::array<PointPair, 1000> &pairs, int n_generated, int n_total, std::default_random_engine &re) {
  int n = 0;
  for (; n < std::min(n_total - n_generated, 1000); n++) {
    PointPair pp;
    const int i = n % 12;
    pp.first.x = gxg[i](re);
    pp.first.y = gyg[i](re);
    pp.second.x = gxg[i](re);
    pp.second.y = gyg[i](re);
    pairs[n] = pp;
  }
  return n;
}

void cluster_mode(std::default_random_engine &re, std::array<PointPair, 1000> &pairs, int n_total, const std::string &output_filename) {
  init_cluster_arr();
  int n_generated = 0;
  std::ofstream f(output_filename);
  std::ofstream ff(output_filename + ".answers.f64", std::ios_base::out);
  if (f.bad() || !f.is_open())
    throw std::runtime_error("can't create/open a file " + output_filename);
  if (ff.bad() || !ff.is_open())
    throw std::runtime_error("can't create/open an answer file " + output_filename);
  double hav_avg = 0;
  f << "{\"pairs\":[\n";
  for (int i = 0; i + 1000 < n_total; i += 1000) {
    const auto iteration_gen= cluster_mode_part(pairs, i, n_total, re);
    for (int j = 0; j < iteration_gen; j++) {
      const auto &p = pairs[j];
      const auto &p0 = p.first;
      const auto &p1 = p.second;      
      f << "\t{\"x0\":" << p0.x << ", \"y0\":" << p0.y << ", ";
      f << "\"x1\":"  << p1.x << ", \"y1\":" << p1.y << "}";
      if (iteration_gen + n_generated - 1 != n_total) f << ',';
      f << '\n';
      const auto result = (ReferenceHaversine(p0.x, p0.y, p1.x, p1.y) / n_total);
      ff << result;
      hav_avg += result;
    }
    n_generated += iteration_gen;
  }
  if (n_generated != n_total) {
    const auto iteration_gen= cluster_mode_part(pairs, n_generated, n_total, re);
    for (int j = 0; j < iteration_gen; j++) {
      const auto &p = pairs[j];
      const auto &p0 = p.first;
      const auto &p1 = p.second;      
      f << "\t{\"x0\":" << p0.x << ", \"y0\":" << p0.y << ", ";
      f << "\"x1\":"  << p1.x << ", \"y1\":" << p1.y << "}";
      if (iteration_gen - 1 != j) f << ',';
      f << '\n';
      const auto result = (ReferenceHaversine(p0.x, p0.y, p1.x, p1.y) / n_total);
      ff << result;
      hav_avg += result;
    }
  }
  f << "]}\n";
  std::cout << "ReferenceHaversine avg: " << hav_avg << std::endl;
  return;
}

int main(int argc, char **argv) try {
  const Config config = parse_command_line(argc, argv);
  std::cout << "Config: " << config << std::endl;

  std::array<PointPair, 1000> generated_pairs;
  std::default_random_engine re(config.seed);
  if (config.mode == eOperatingMode::Normal) {
    normal_mode(re, generated_pairs, config.n_pairs_to_generate, "normal.json");
  } else if (config.mode == eOperatingMode::Cluster) {
    cluster_mode(re, generated_pairs, config.n_pairs_to_generate, "cluster.json");
  }
  return 0;
} catch (const std::invalid_argument &ia) {
  std::cout << ia.what() << std::endl;
  return 1;
}
