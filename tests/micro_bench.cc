#include <iostream>
#include <fstream>
#include <cassert>
#include <iomanip>
#include <sstream>
#include <thread>
#include <getopt.h>
#include <unistd.h>
#include <cmath>
#include <vector>
#include <set>
#include <map>
#include <algorithm>

#include "db_interface.h"
#include "timer.h"
#include "util.h"
#include "random.h"

using combotree::ComboTree;
using combotree::Random;
using combotree::Timer;
using ycsbc::KvDB;
using namespace dbInter;

struct operation
{
  /* data */
  uint64_t op_type;
  uint64_t op_len; // for only scan
  uint64_t key_num;
};

uint64_t get_cellid(const std::string &line)
{
  uint64_t id;
  double lat, lon;
  std::stringstream strin(line);
  strin >> id >> lon >> lat;
  return id;
}

uint64_t get_longtitude(const std::string &line)
{
  uint64_t id;
  double lat, lon;
  std::stringstream strin(line);
  strin >> id >> lon >> lat;
  return lon * 1e7;
}

double get_lat(const std::string &line)
{
  uint64_t id;
  double lat, lon;
  std::stringstream strin(line);
  strin >> id >> lon >> lat;
  return lat;
}

uint64_t get_longlat(const std::string &line)
{
  uint64_t id;
  double lat, lon;
  std::stringstream strin(line);
  strin >> id >> lon >> lat;
  return (lon * 180 + lat) * 1e7;
}

template <typename T>
std::vector<T> read_data_from_osm(
    const std::string load_file,
    T (*get_data)(const std::string &) = []
    { return static_cast<T>(0); },
    const std::string output = "/home/wjy/generate_random_osm_longtitudes.dat")
{
  std::vector<T> data;
  std::set<T> unique_keys;
  std::cout << "Use: " << __FUNCTION__ << std::endl;
  const uint64_t ns = util::timing([&]
                                   {
                                     std::ifstream in(load_file);
                                     if (!in.is_open())
                                     {
                                       std::cerr << "unable to open " << load_file << std::endl;
                                       exit(EXIT_FAILURE);
                                     }
                                     uint64_t id, size = 0;
                                     double lat, lon;
                                     while (!in.eof())
                                     {
                                       /* code */
                                       std::string tmp;
                                       getline(in, tmp); // 去除第一行
                                       while (getline(in, tmp))
                                       {
                                         T key = get_data(tmp);
                                         unique_keys.insert(key);
                                         size++;
                                         if (size % 10000000 == 0)
                                           std::cerr << "Load: " << size << "\r";
                                       }
                                     }
                                     in.close();
                                     std::cerr << "Finshed loads ......\n";
                                     data.assign(unique_keys.begin(), unique_keys.end());
                                     std::random_shuffle(data.begin(), data.end());
                                     size = data.size();
                                     std::cerr << "Finshed random ......\n";
                                     std::ofstream out(output, std::ios::binary);
                                     out.write(reinterpret_cast<char *>(&size), sizeof(uint64_t));
                                     out.write(reinterpret_cast<char *>(data.data()), data.size() * sizeof(uint64_t));
                                     out.close();
                                     std::cout << "read size: " << size << ", unique data: " << unique_keys.size() << std::endl;
                                   });
  const uint64_t ms = ns / 1e6;
  std::cout << "generate " << data.size() << " values in "
            << ms << " ms (" << static_cast<double>(data.size()) / 1000 / ms
            << " M values/s)" << std::endl;
  return data;
}

template <typename T>
std::vector<T> load_data_from_osm(
    const std::string dataname = "/home/sbh/generate_random_osm_cellid.dat")
{
  return util::load_data<T>(dataname);
}

std::vector<uint64_t> generate_random_ycsb(size_t op_num)
{
  std::vector<uint64_t> data;
  data.resize(op_num);
  std::cout << "Use: " << __FUNCTION__ << std::endl;
  const uint64_t ns = util::timing([&]
                                   {
                                     Random rnd(0, op_num - 1);
                                     for (size_t i = 0; i < op_num; ++i)
                                       data[i] = utils::Hash(i);
                                     // for (size_t i = 0; i < op_num; ++i)
                                     //   std::swap(data[i], data[rnd.Next()]);
                                   });
  const uint64_t ms = ns / 1e6;
  uint64_t size = data.size();
  std::ofstream out("/home/wjy/generate_random_ycsb.dat", std::ios::binary);
  out.write(reinterpret_cast<char *>(&size), sizeof(uint64_t));
  out.write(reinterpret_cast<char *>(data.data()), data.size() * sizeof(uint64_t));
  out.close();
  std::cout << "generate " << data.size() << " values in "
            << ms << " ms (" << static_cast<double>(data.size()) / 1000 / ms
            << " M values/s)" << std::endl;
  for (int i = 0; i < 20; i++)
  {
    std::cout << data[i] << endl;
  }
  return data;
}

std::vector<uint64_t> generate_uniform_random(size_t op_num)
{
  std::vector<uint64_t> data;
  //std::set<uint64_t> unique_keys;
  data.resize(op_num);
  std::cout << "Use: " << __FUNCTION__ << std::endl;
  const uint64_t ns = util::timing([&]
                                   {
                                     Random rnd(0, UINT64_MAX);
                                     for (size_t i = 0; i < op_num; ++i)
                                     {
                                       data[i] = rnd.Next();
                                       //unique_keys.insert(rnd.Next());
                                     }
                                   });

  // const std::string output = "/home/wjy/generate_random_10uint32.dat";
  // data.assign(unique_keys.begin(), unique_keys.end());
  // random_shuffle(data.begin(),data.end());
  // std::ofstream out(output, std::ios::binary);
  // uint64_t size = data.size();
  // out.write(reinterpret_cast<char*>(&size), sizeof(uint64_t));
  // out.write(reinterpret_cast<char*>(data.data()), data.size() * sizeof(uint64_t));
  // out.close();
  const uint64_t ms = ns / 1e6;
  std::cout << "generate " << data.size() << " values in "
            << ms << " ms (" << static_cast<double>(data.size()) / 1000 / ms
            << " M values/s)" << std::endl;
  for (int i = 0; i < 20; i++)
  {
    std::cout << data[i] << endl;
  }
  return data;
}

void show_help(char *prog)
{
  std::cout << "Usage: " << prog << " [options]" << std::endl
            << std::endl
            << "  Option:" << std::endl
            << "    --thread[-t]             thread number" << std::endl
            << "    --load-size              LOAD_SIZE" << std::endl
            << "    --put-size               PUT_SIZE" << std::endl
            << "    --get-size               GET_SIZE" << std::endl
            << "    --workload               WorkLoad" << std::endl
            << "    --help[-h]               show help" << std::endl;
}

int thread_num = 1;
size_t LOAD_SIZE = 10000000;
size_t PUT_SIZE = 6000000;
size_t GET_SIZE = 1000000;
size_t DELETE_SIZE = 1000000;
int Loads_type = 0;

int main(int argc, char *argv[])
{
  static struct option opts[] = {
      /* NAME               HAS_ARG            FLAG  SHORTNAME*/
      {"thread", required_argument, NULL, 't'},
      {"load-size", required_argument, NULL, 0},
      {"put-size", required_argument, NULL, 0},
      {"get-size", required_argument, NULL, 0},
      {"dbname", required_argument, NULL, 0},
      {"workload", required_argument, NULL, 0},
      {"loadstype", required_argument, NULL, 0},
      {"help", no_argument, NULL, 'h'},
      {NULL, 0, NULL, 0}};

  int c;
  int opt_idx;
  std::string dbName = "combotree";
  std::string load_file = "";
  while ((c = getopt_long(argc, argv, "t:s:dh", opts, &opt_idx)) != -1)
  {
    switch (c)
    {
    case 0:
      switch (opt_idx)
      {
      case 0:
        thread_num = atoi(optarg);
        break;
      case 1:
        LOAD_SIZE = atoi(optarg);
        break;
      case 2:
        PUT_SIZE = atoi(optarg);
        break;
      case 3:
        GET_SIZE = atoi(optarg);
        break;
      case 4:
        dbName = optarg;
        break;
      case 5:
        load_file = optarg;
        break;
      case 6:
        Loads_type = atoi(optarg);
        break;
      case 7:
        show_help(argv[0]);
        return 0;
      default:
        std::cerr << "Parse Argument Error!" << std::endl;
        abort();
      }
      break;
    case 't':
      thread_num = atoi(optarg);
      break;
    case 'h':
      show_help(argv[0]);
      return 0;
    case '?':
      break;
    default:
      std::cout << (char)c << std::endl;
      abort();
    }
  }

  std::cout << "THREAD NUMBER:         " << thread_num << std::endl;
  std::cout << "LOAD_SIZE:             " << LOAD_SIZE << std::endl;
  std::cout << "PUT_SIZE:              " << PUT_SIZE << std::endl;
  std::cout << "GET_SIZE:              " << GET_SIZE << std::endl;
  std::cout << "DB  name:              " << dbName << std::endl;
  std::cout << "Workload:              " << load_file << std::endl;

  std::vector<uint64_t> data_base;
  switch (Loads_type)
  {
  case -2:
    data_base = read_data_from_osm<uint64_t>(load_file, get_cellid);
    break;
  case -1:
    data_base = read_data_from_osm<uint64_t>(load_file, get_longtitude);
    break;
  case 0:
    data_base = generate_uniform_random(LOAD_SIZE + PUT_SIZE * 10);
    break;
  case 1:
    data_base = generate_random_ycsb(LOAD_SIZE + PUT_SIZE * 6);
    break;
  case 2:
    data_base = load_data_from_osm<uint64_t>("/home/wjy/generate_random_osm_longtitudes.dat");
    break;
  case 3:
    data_base = load_data_from_osm<uint64_t>("/home/wjy/generate_random_osm_longlat.dat");
    break;
  case 4:
    data_base = load_data_from_osm<uint64_t>("/home/wjy/generate_random_10uint32.dat");
    break;
  case 5:
    data_base = load_data_from_osm<uint64_t>("/home/wjy/lognormal.dat");
    break;
  case 6:
    data_base = load_data_from_osm<uint64_t>("/home/wjy/generate_random_ycsb.dat");
    break;
  default:
    data_base = generate_uniform_random(LOAD_SIZE + PUT_SIZE * 8);
    break;
  }

  NVM::env_init();
  KvDB *db = nullptr;
  if (dbName == "fastfair")
  {
    db = new FastFairDb();
  }
  else if (dbName == "pgm")
  {
    db = new PGMDynamicDb();
  }
  else if (dbName == "xindex")
  {
    db = new XIndexDb();
  }
  else if (dbName == "alex")
  {
    db = new AlexDB();
  }
  else if (dbName == "stx")
  {
    db = new StxDB();
  }
  else if (dbName == "letree")
  {
    db = new LetDB();
  }
  else if (dbName == "lipp")
  {
    db = new LIPPDb();
  }
  else
  {
    db = new ComboTreeDb();
  }
  db->Init();
  Timer timer;
  uint64_t us_times;
  uint64_t load_pos = 0;
  std::cout << "Start run ...." << std::endl;
  {
    int init_size = 1e3;
    std::mt19937_64 gen_payload(std::random_device{}());
    auto values = new std::pair<uint64_t, uint64_t>[init_size];
    for (int i = 0; i < init_size; i++)
    {
      values[i].first = data_base[i];
      values[i].second = static_cast<uint64_t>(gen_payload());
    }
    std::sort(values, values + init_size,
              [](auto const &a, auto const &b)
              { return a.first < b.first; });
    db->Bulk_load(values, init_size);
    load_pos = init_size;
  }
  {
    // //for alex lognormal
    // std::cout << "Start loading ...." << std::endl;
    // int init_size = 15*1e7;
    // auto values = new std::pair<uint64_t, uint64_t>[init_size];
    // for (int i = 0; i < init_size; i++)
    // {
    //   values[i].first = data_base[i];
    //   values[i].second = data_base[i];
    // }
    // std::sort(values, values + init_size,
    //           [](auto const &a, auto const &b)
    //           { return a.first < b.first; });
    // db->Bulk_load(values,init_size);
    // load_pos = init_size;
  }
  {
    // Load
    std::cout << "Start loading ...." << std::endl;
    timer.Record("start");
    for (load_pos; load_pos < LOAD_SIZE; load_pos++)
    {
      db->Put(data_base[load_pos], (uint64_t)data_base[load_pos]);
      if ((load_pos + 1) % 10000000 == 0){
        std::cerr << "Operate: " << load_pos + 1 << std::endl;
        db->Info();
      }
    }
    std::cerr << std::endl;

    timer.Record("stop");
    us_times = timer.Microsecond("stop", "start");
    load_pos = LOAD_SIZE;
    std::cout << "[Metic-Load]: Load " << LOAD_SIZE << ": "
              << "cost " << us_times / 1000000.0 << "s, "
              << "iops " << (double)(LOAD_SIZE) / (double)us_times * 1000000.0 << " ." << std::endl;
  }

  // db->Info();
  // us_times = timer.Microsecond("stop", "start");
  // timer.Record("start");
  // Different insert_ration
  std::vector<float> insert_ratios = {1};
  //std::vector<float> insert_ratios = {0,0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9,1.0};
  float insert_ratio = 0;
  util::FastRandom ranny(18);
  std::cout << "Start testing ...." << std::endl;
  for (int i = 0; i < insert_ratios.size(); i++)
  {
    uint64_t value = 0;
    insert_ratio = insert_ratios[i];
    db->Begin_trans();
    std::cout << "Data loaded: " << load_pos << std::endl;
    timer.Clear();
    timer.Record("start");
    for (uint64_t i = 0; i < GET_SIZE; i++)
    {
      if(i%1000000 == 0){
        std::cout << "i = " << i << std::endl;
      }
      if (ranny.ScaleFactor() < insert_ratio)
      {
        db->Put(data_base[load_pos], (uint64_t)data_base[load_pos]);
        load_pos++;
      }
      else
      {
        std::cout << "i1 = " << i << std::endl;
        uint64_t op_seq = ranny.RandUint32(0, load_pos - 1);
        std::cout << "i2 = " << i << std::endl;
        db->Get(data_base[op_seq], value);
        std::cout << "i3 = " << i << std::endl;
      }
    }
    timer.Record("stop");
    us_times = timer.Microsecond("stop", "start");
    std::cout << "[Metic-Operate]: Operate " << GET_SIZE << " insert_ratio " << insert_ratio << ": "
              << "cost " << us_times / 1000000.0 << "s, "
              << "iops " << (double)(GET_SIZE) / (double)us_times * 1000000.0 << " ." << std::endl;
  }

  delete db;

  NVM::env_exit();
  return 0;
}
