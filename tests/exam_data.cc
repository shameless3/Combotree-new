#include <bits/stdc++.h>
// Loads values from binary file into vector.
template <typename T>
static std::vector<T> load_data(const std::string &filename,
                                size_t max_size = 1e10,
                                bool print = true)
{
    std::vector<T> data;
    std::vector<uint64_t> new_data;
    std::ifstream in(filename, std::ios::binary);
    if (!in.is_open())
    {
        std::cerr << "unable to open " << filename << std::endl;
        exit(EXIT_FAILURE);
    }
    // Read size.
    uint64_t size = 0;
    if (filename == "/home/wjy/lognormal-190M.bin.data")
    {
        std::cout << "lognormal data size 190M" << std::endl;
        size = 190000000;
    }
    else
    {
        in.read(reinterpret_cast<char *>(&size), sizeof(uint64_t));
        std::cerr << "Data size: " << size << std::endl;
    }
    size = std::min(size, max_size);
    data.resize(size);
    // Read values.
    in.read(reinterpret_cast<char *>(data.data()), size * sizeof(T));
    in.close();
    int count = 0;
    for (int i = 0; i < data.size(); i++)
    {
        if (data[i] <= 0)
        {
            count++;
            continue;
        }else{
            new_data.push_back((uint64_t)data[i]);
        }
        if (i % 10000000 == 0)
        {
            std::cout << "operate " << i << std::endl;
        }
    }
    size = new_data.size();
    std::ofstream out("/home/wjy/lognormal.dat", std::ios::binary);
    out.write(reinterpret_cast<char *>(&size), sizeof(uint64_t));
    out.write(reinterpret_cast<char *>(new_data.data()), data.size() * sizeof(uint64_t));
    out.close();
    std::cout << "count = " << count << std::endl;
    std::cout << "new data size " << new_data.size() << std::endl;
    return data;
}

int main()
{
    load_data<long>("/home/wjy/lognormal-190M.bin.data");
    return 0;
}