#include <bits/stdc++.h>

// Loads values from binary file into vector.
template <typename T>
static std::vector<T> load_data(const std::string &filename,const std::string output,
                                size_t max_size = 1e10,
                                bool print = true,int used_size = 400000000)
{
    std::vector<T> data;
    std::vector<T> mini_data;
    std::ifstream in(filename, std::ios::binary);
    if (!in.is_open())
    {
        std::cerr << "unable to open " << filename << std::endl;
        exit(EXIT_FAILURE);
    }
    // Read size.
    uint64_t size = 0;

    in.read(reinterpret_cast<char *>(&size), sizeof(uint64_t));
    std::cerr << "Data size: " << size << std::endl;
    size = used_size;
    data.resize(size);
    // Read values.
    in.read(reinterpret_cast<char *>(data.data()), size * sizeof(T));
    in.close();
    sort(data.begin(),data.end());
    int step = size/10000;
    std::ofstream out(output);
    for(int i = 0;i<size;i+=step){
        out << data[i] << std::endl;
    }
    out.close();
    std::cout << "finished " << output << std::endl;
    return data;
}

int main()
{
    // CDF
    load_data<unsigned long>("/home/wjy/generate_random_osm_longtitudes.dat","/home/wjy/ltd_cdf.txt",200000000);
    load_data<unsigned long>("/home/wjy/generate_random_osm_longlat.dat","/home/wjy/llt_cdf.txt",400000000);
    load_data<unsigned long>("/home/wjy/lognormal.dat","/home/wjy/lgn_cdf.txt",190000000);
    load_data<unsigned long>("/home/wjy/generate_random_ycsb.dat","/home/wjy/ycsb_cdf.txt",400000000);
    return 0;
}