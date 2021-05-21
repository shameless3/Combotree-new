#include <iostream>
#include <fstream>
#include <cassert>
#include <iomanip>
#include <sstream>
#include <thread>
#include <getopt.h>
#include <unistd.h>
#include <cmath>

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
    uint32_t op_type;
    uint32_t op_len; // for only scan
    uint64_t key_num;
};

std::vector<uint64_t> read_data_from_map(const std::string load_file, 
    const std::string filename = "/home/sbh/generate_random_osm_longlat.dat") {
    std::vector<uint64_t> data;
    std::set<uint64_t> unique_keys;
    std::cout << "Use: " << __FUNCTION__ << std::endl;
    const uint64_t ns = util::timing([&] { 
      std::ifstream in(load_file);
      if (!in.is_open()) {
        std::cerr << "unable to open " << load_file << std::endl;
        exit(EXIT_FAILURE);
      }
      uint64_t id, size = 0;
      double lat, lon;
      std::cout << "Run hear" << std::endl;
      while (!in.eof())
      {
        /* code */
        std::string tmp;
        getline(in, tmp); // 去除第一行
        while(getline(in, tmp)) {
          stringstream strin(tmp);
          strin >> id >> lat >> lon;
          uint64_t key = (((uint64_t)(lat * 1e6)) << 32) + (uint64_t)(lon * 1e6);
          // data.push_back(key);
          unique_keys.insert(key);
          size ++;
        }
      }
      in.close();
      data.assign(unique_keys.begin(), unique_keys.end());
      std::random_shuffle(data.begin(), data.end()); 
      std::ofstream out(filename, std::ios::binary);
      out.write(reinterpret_cast<char*>(&size), sizeof(uint64_t));
      out.write(reinterpret_cast<char*>(data.data()), data.size() * sizeof(uint64_t));
      out.close(); 
      std::cout << "read size: " << size << ", unique data: " << unique_keys.size() << std::endl;
  });
  const uint64_t ms = ns/1e6;
  std::cout << "generate " << data.size() << " values in "
            << ms << " ms (" << static_cast<double>(data.size())/1000/ms
            << " M values/s)" << std::endl;   
  return data;
}

std::vector<uint64_t> generate_random_ycsb(size_t op_num)
{
  std::vector<uint64_t> data; 
  data.resize(op_num);
  std::cout << "Use: " << __FUNCTION__ << std::endl;
  const uint64_t ns = util::timing([&] { 
    Random rnd(0, op_num - 1);
    for (size_t i = 0; i < op_num; ++i)
      data[i] = utils::Hash(i);
    for (size_t i = 0; i < op_num; ++i)
      std::swap(data[i], data[rnd.Next()]);
  });
  const uint64_t ms = ns/1e6;
  std::cout << "generate " << data.size() << " values in "
            << ms << " ms (" << static_cast<double>(data.size())/1000/ms
            << " M values/s)" << std::endl;   
  return data;
}

std::vector<uint64_t> generate_uniform_random(size_t op_num)
{
  std::vector<uint64_t> data; 
  data.resize(op_num);
  std::cout << "Use: " << __FUNCTION__ << std::endl;
  const uint64_t ns = util::timing([&] { 
    Random rnd(0, UINT64_MAX);
    for (size_t i = 0; i < op_num; ++i)
      data[i] = rnd.Next();
  });
  const uint64_t ms = ns/1e6;
  std::cout << "generate " << data.size() << " values in "
            << ms << " ms (" << static_cast<double>(data.size())/1000/ms
            << " M values/s)" << std::endl;  
  return data;
}

std::vector<uint64_t> load_random_osm(const std::string load_file, 
    size_t data_size, size_t op_num,
	  const std::string filename = "/home/sbh/generate_random_osm_longlat.dat")
{
  std::vector<uint64_t> data; 
  uint64_t size;
  util::FastRandom ranny(2021);
  std::cout << "Use: " << __FUNCTION__ << std::endl;
  const uint64_t ns = util::timing([&] { 
      std::ifstream in(filename, std::ios::binary);
      if (!in.is_open()) {
        std::cerr << "unable to open " << filename << std::endl;
        const DataType type = util::resolve_type(load_file);
        assert(type == DataType::UINT64);

        data = util::load_data<uint64_t>(load_file, data_size);
        std::random_shuffle(data.begin(), data.end()); 

        size = data.size();
        std::ofstream out(filename, std::ios::binary);
        out.write(reinterpret_cast<char*>(&size), sizeof(uint64_t));
        out.write(reinterpret_cast<char*>(data.data()), op_num*sizeof(uint64_t));
        out.close(); 
        data.resize(op_num);
      } else {
        in.read(reinterpret_cast<char*>(&size), sizeof(uint64_t));
        std::cerr << "Data size: " << size << std::endl;
        size = std::min(size, op_num);
        data.resize(op_num);
        // Read values.
        in.read(reinterpret_cast<char*>(data.data()), size*sizeof(uint64_t));
        in.close();
      }
      // std::set<uint64_t> unique_sets;
      // for(size_t i = 0; i < data.size(); i ++) {
      //  unique_sets.insert(data[i]);
      // }
      // std::cout << "Unique data: " << unique_sets.size() << std::endl;
  });
  const uint64_t ms = ns/1e6;
  std::cout << "generate " << data.size() << " values in "
            << ms << " ms (" << static_cast<double>(data.size())/1000/ms
            << " M values/s)" << std::endl;   
  return data;
}

void show_help(char* prog) {
  std::cout <<
    "Usage: " << prog << " [options]" << std::endl <<
    std::endl <<
    "  Option:" << std::endl <<
    "    --thread[-t]             thread number" << std::endl <<
    "    --load-size              LOAD_SIZE" << std::endl <<
    "    --put-size               PUT_SIZE" << std::endl <<
    "    --get-size               GET_SIZE" << std::endl <<
    "    --workload               WorkLoad" << std::endl <<
    "    --help[-h]               show help" << std::endl;
}

int thread_num = 1;
size_t LOAD_SIZE   = 10000000;
size_t PUT_SIZE    = 6000000;
size_t GET_SIZE    = 1000000;
size_t DELETE_SIZE = 1000000;
int Loads_type = 0;

void combotree_expand_test(std::vector<uint64_t> &data_base);
int main(int argc, char *argv[]) {
    static struct option opts[] = {
  /* NAME               HAS_ARG            FLAG  SHORTNAME*/
    {"thread",          required_argument, NULL, 't'},
    {"load-size",       required_argument, NULL, 0},
    {"put-size",        required_argument, NULL, 0},
    {"get-size",        required_argument, NULL, 0},
    {"dbname",          required_argument, NULL, 0},
    {"workload",        required_argument, NULL, 0},
    {"loadstype",       required_argument, NULL, 0},
    {"help",            no_argument,       NULL, 'h'},
    {NULL, 0, NULL, 0}
  };

  int c;
  int opt_idx;
  std::string  dbName= "combotree";
  std::string  load_file= "";
  while ((c = getopt_long(argc, argv, "t:s:dh", opts, &opt_idx)) != -1) {
    switch (c) {
      case 0:
        switch (opt_idx) {
          case 0: thread_num = atoi(optarg); break;
          case 1: LOAD_SIZE = atoi(optarg); break;
          case 2: PUT_SIZE = atoi(optarg); break;
          case 3: GET_SIZE = atoi(optarg); break;
          case 4: dbName = optarg; break;
          case 5: load_file = optarg; break;
          case 6: Loads_type = atoi(optarg); break;
          case 7: show_help(argv[0]); return 0;
          default: std::cerr << "Parse Argument Error!" << std::endl; abort();
        }
        break;
      case 't': thread_num = atoi(optarg); break;
      case 'h': show_help(argv[0]); return 0;
      case '?': break;
      default:  std::cout << (char)c << std::endl; abort();
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
  case -1:
    data_base = read_data_from_map(load_file);
    break;
  case 0:
    data_base = generate_uniform_random(LOAD_SIZE + PUT_SIZE * 5);
    break;
  case 1:
    data_base = generate_random_ycsb(LOAD_SIZE + PUT_SIZE * 5);
    break;
  case 2:
    data_base = load_random_osm(load_file, 10e8, LOAD_SIZE + PUT_SIZE * 5);
    break;
  default:
    data_base = generate_uniform_random(LOAD_SIZE + PUT_SIZE * 5);
    break;
  }

  // Load 
  NVM::env_init();

  combotree_expand_test(data_base);
  return 0;
  KvDB* db = nullptr;
  if(dbName == "fastfair") {
    db = new FastFairDb();
  } else if(dbName == "pgm") {
    db = new PGMDynamicDb();
  } else if(dbName == "xindex") {
    db = new XIndexDb();
  } else if(dbName == "alex") {
    db = new AlexDB();
  } else if(dbName == "stx") {
    db = new StxDB();
  } else {
    db = new ComboTreeDb();
  }
  db->Init();
  Timer timer;
  uint64_t us_times;
  uint64_t load_pos = 0; 
  std::cout << "Start run ...." << std::endl;
  if(false){
   std::set<uint64_t> unique_keys;
   std::set<uint64_t> unique_seqs;
   for(int i = 0; i < data_base.size(); i ++) {
     unique_keys.insert(data_base[i]);
     std::cout << "key: " << data_base[i] << std::endl;
   }
   std::cout << "Unique seqs: " << unique_seqs.size() << ", " 
             << "Unique data: " << unique_keys.size() << std::endl;
  } 
  {
     // Load
    timer.Record("start");
    for(load_pos = 0; load_pos < LOAD_SIZE; load_pos ++) {
      db->Put(data_base[load_pos], data_base[load_pos]);
      if((load_pos + 1) % 10000 == 0) std::cerr << "Operate: " << load_pos + 1 << '\r';  
    }
    std::cerr << std::endl;
    timer.Record("stop");
    us_times = timer.Microsecond("stop", "start");
    std::cout << "[Metic-Load]: Load " << LOAD_SIZE << ": " 
              << "cost " << us_times/1000000.0 << "s, " 
              << "iops " << (double)(LOAD_SIZE)/(double)us_times*1000000.0 << " ." << std::endl;
  }
  
  // us_times = timer.Microsecond("stop", "start");
  // timer.Record("start");
  // Different insert_ration
  std::vector<float> insert_ratios = {0, 0.2, 0.5, 0.8, 1.0}; 
  float insert_ratio = 0;
  util::FastRandom ranny(18);
  db->Begin_trans();
  for(int i = 0; i < insert_ratios.size(); i++)
  {
    uint64_t value = 0;
    insert_ratio = insert_ratios[i];
    timer.Clear();
    timer.Record("start");
    for(uint64_t i = 0; i < GET_SIZE; i ++) {
      if(ranny.ScaleFactor() < insert_ratio) {
        db->Put(data_base[load_pos], data_base[load_pos]);
        load_pos ++;
      } else {
        uint32_t op_seq = ranny.RandUint32(0, load_pos - 1);
        db->Get(data_base[op_seq], value);
      }
    }
    timer.Record("stop");
    us_times = timer.Microsecond("stop", "start");
    std::cout << "[Metic-Operate]: Operate " << GET_SIZE << " insert_ratio "<< insert_ratio <<  ": " 
              << "cost " << us_times/1000000.0 << "s, " 
              << "iops " << (double)(GET_SIZE)/(double)us_times*1000000.0 << " ." << std::endl;
  }

  delete db;

  return 0;
}

void combotree_expand_test(std::vector<uint64_t> &data_base) {
  ComboTree *tree;
#ifdef SERVER
      tree = new ComboTree("/pmem0/", (1024*1024*1024*100UL), true);
#else
      tree = new ComboTree("/mnt/pmem0/", (1024*1024*512UL), true);
#endif
  Timer timer;
  uint64_t us_times;
  uint64_t load_pos = 0; 
  {
     // Load
    timer.Record("start");
    for(load_pos = 0; load_pos < LOAD_SIZE; load_pos ++) {
      tree->Put(data_base[load_pos], data_base[load_pos]);
      if((load_pos + 1) % 100000 == 0) std::cerr << "Operate: " << load_pos + 1 << '\r';  
    }
    std::cerr << std::endl;
    timer.Record("stop");
    us_times = timer.Microsecond("stop", "start");
    std::cout << "[Metic-Load]: Load " << LOAD_SIZE << ": " 
              << "cost " << us_times/1000000.0 << "s, " 
              << "iops " << (double)(LOAD_SIZE)/(double)us_times*1000000.0 << " ." << std::endl;
  }

  tree->ExpandComboTree_();
}