#pragma once

#include "combotree_config.h"
#include "kvbuffer.h"
#include "clevel.h"
#include "pmem.h"

#include "bentry.h"
#include "common_time.h"
#include "pointer_bentry.h"
// #include "learnindex/learn_index.h"
#include "rmi_model.h"
#include "statistic.h"
#include "nvm_alloc.h"

namespace combotree
{
    static const size_t max_entry_count = 4096;
    static const size_t min_entry_count = 64;
    typedef combotree::PointerBEntry bentry_t;

    /**
 * @brief 根模型，采用的是两层RMI模型，
 * 1. 目前实现需要首先 Load一定的数据作为初始化数据；
 * 2. EXPAND_ALL 宏定义控制采用每次扩展所有EntryGroup，还是采用重复指针一次扩展一个EntryGroup
 */
    class letree;
    class __attribute__((aligned(64))) group
    {
        // class  group {
    public:
        class Iter;
        class BEntryIter;
        class EntryIter;
        friend class letree;
        group() : nr_entries_(0), next_entry_count(0)
        {
            // clevel_mem_ = new CLevel::MemControl(CLEVEL_PMEM_FILE, CLEVEL_PMEM_FILE_SIZE);
        }

        group(CLevel::MemControl *clevel_mem)
        // : clevel_mem_(clevel_mem)
        {
        }

        ~group()
        {
            if (entry_space)
                NVM::data_alloc->Free(entry_space, nr_entries_ * sizeof(bentry_t));
        }

        void Init(CLevel::MemControl *mem)
        {

            nr_entries_ = 1;
            entry_space = (bentry_t *)NVM::data_alloc->alloc_aligned(nr_entries_ * sizeof(bentry_t));

            new (&entry_space[0]) bentry_t(0, 8, mem);

            NVM::Mem_persist(entry_space, nr_entries_ * sizeof(bentry_t));
            model.init<bentry_t *, bentry_t>(entry_space, 1, 1, get_entry_key);

            next_entry_count = 1;
            NVM::Mem_persist(this, sizeof(*this));
        }

        void bulk_load(std::vector<std::pair<uint64_t, uint64_t>> &data, CLevel::MemControl *mem)
        {
            nr_entries_ = data.size();
            bentry_t *new_entry_space = (bentry_t *)NVM::data_alloc->alloc_aligned(nr_entries_ * sizeof(bentry_t));
            size_t new_entry_count = 0;
            for (size_t i = 0; i < data.size(); i++)
            {
                new (&new_entry_space[new_entry_count++]) bentry_t(data[i].first, data[i].second, mem);
            }
            NVM::Mem_persist(new_entry_space, nr_entries_ * sizeof(bentry_t));
            model.init<bentry_t *, bentry_t>(new_entry_space, new_entry_count,
                                             std::ceil(1.0 * new_entry_count / 100), get_entry_key);
            entry_space = new_entry_space;
            next_entry_count = nr_entries_;
        }

        void bulk_load(std::vector<std::pair<uint64_t, uint64_t>> &data, size_t start, size_t count, CLevel::MemControl *mem)
        {
            nr_entries_ = count;
            size_t new_entry_count = 0;
            for (size_t i = 0; i < count; i++)
            {
                new (&entry_space[new_entry_count++]) bentry_t(data[start + i].first,
                                                               data[start + i].second, 0, mem);
            }
            min_key = data[0].first;
            NVM::Mem_persist(entry_space, nr_entries_ * sizeof(bentry_t));
            model.init<bentry_t *, bentry_t>(entry_space, new_entry_count,
                                             std::ceil(1.0 * new_entry_count / 100), get_entry_key);
            next_entry_count = nr_entries_;
        }

        void bulk_load(const std::pair<uint64_t, uint64_t> data[], size_t start, size_t count, CLevel::MemControl *mem)
        {
            nr_entries_ = count;
            size_t new_entry_count = 0;
            for (size_t i = 0; i < count; i++)
            {
                new (&entry_space[new_entry_count++]) bentry_t(data[start + i].first,
                                                               data[start + i].second, 0, mem);
            }
            min_key = data[0].first;
            NVM::Mem_persist(entry_space, nr_entries_ * sizeof(bentry_t));
            model.init<bentry_t *, bentry_t>(entry_space, new_entry_count,
                                             std::ceil(1.0 * new_entry_count / 100), get_entry_key);
            next_entry_count = nr_entries_;
        }

        ALWAYS_INLINE void append_entry(const PointerBEntry::entry *entry)
        {
            new (&entry_space[nr_entries_++]) bentry_t(entry);
        }

        ALWAYS_INLINE void inc_entry_count()
        {
            next_entry_count++;
        }

        ALWAYS_INLINE void reserve_space()
        {
            entry_space = (bentry_t *)NVM::data_alloc->alloc_aligned(next_entry_count * sizeof(bentry_t));
        }

        ALWAYS_INLINE void re_tarin()
        {
            assert(nr_entries_ <= next_entry_count);
            pmem_persist(entry_space, nr_entries_ * sizeof(bentry_t));
            model.init<bentry_t *, bentry_t>(entry_space, nr_entries_,
                                             std::ceil(1.0 * nr_entries_ / 100), get_entry_key);
            min_key = entry_space[0].entry_key;
            // NVM::Mem_persist(entry_space, nr_entries_ * sizeof(bentry_t));
        }

        int find_entry(const uint64_t &key) const
        {
            int m = model.predict(key);
            m = std::min(std::max(0, m), (int)nr_entries_ - 1);

            return exponential_search_upper_bound(m, key);
            // return linear_search_upper_bound(m, key);
        }

        inline int exponential_search_upper_bound(int m, const uint64_t &key) const
        {
            int bound = 1;
            int l, r; // will do binary search in range [l, r)
            if (entry_space[m].entry_key > key)
            {
                int size = m;
                while (bound < size && (entry_space[m - bound].entry_key > key))
                {
                    bound *= 2;
                }
                l = m - std::min<int>(bound, size);
                r = m - bound / 2;
            }
            else
            {
                int size = nr_entries_ - m;
                while (bound < size && (entry_space[m + bound].entry_key <= key))
                {
                    bound *= 2;
                }
                l = m + bound / 2;
                r = m + std::min<int>(bound, size);
            }
            if (r - l < 6)
            {
                return std::max(linear_search_upper_bound(l, r, key) - 1, 0);
            }
            return std::max(binary_search_upper_bound(l, r, key) - 1, 0);
        }

        // inline int linear_search_upper_bound(int m, const uint64_t & key) const {
        //     int bound = 1;
        //     int l, r;  // will do binary search in range [l, r)
        //     if(entry_space[m].entry_key > key) {
        //         int size = m;
        //         while (bound < size && (entry_space[m - bound].entry_key > key)) {
        //             bound ++;
        //         }
        //         return std::max<int>(m - bound, 0);
        //     } else {
        //         int size = nr_entries_ - m;
        //         while (bound < size && (entry_space[m + bound].entry_key <= key)) {
        //             bound ++;
        //         }
        //         return std::min<int>(m + bound - 1, nr_entries_ - 1);
        //     }
        //     return 0;
        // }

        inline int binary_search_upper_bound(int l, int r, const uint64_t &key) const
        {
            while (l < r)
            {
                int mid = l + (r - l) / 2;
                if (entry_space[mid].entry_key <= key)
                {
                    l = mid + 1;
                }
                else
                {
                    r = mid;
                }
            }
            return l;
        }

        inline int linear_search_upper_bound(int l, int r, const uint64_t &key) const
        {
            while (l < r && entry_space[l].entry_key <= key)
                l++;
            return l;
        }

        inline int binary_search_lower_bound(int l, int r, const uint64_t &key) const
        {
            while (l < r)
            {
                int mid = l + (r - l) / 2;
                if (entry_space[mid].entry_key < key)
                {
                    l = mid + 1;
                }
                else
                {
                    r = mid;
                }
            }
            return l;
        }
        /**
     * @brief 插入KV对，
     * 
     * @param key 
     * @param value 
     * @return status 
     */
        status Put(CLevel::MemControl *mem, uint64_t key, uint64_t value)
        {
        retry0:
            int entry_id = find_entry(key);
            bool split = false;

            auto ret = entry_space[entry_id].Put(mem, key, value, &split);

            if (split)
            {
                next_entry_count++;
            }

            if (ret == status::Full)
            { // LearnGroup数组满了，需要扩展
                if (next_entry_count > max_entry_count)
                {
                    // LOG(Debug::INFO, "Need expand tree: group entry count %d.", next_entry_count);
                    return ret;
                }
                expand(mem);
                split = false;
                goto retry0;
            }
            return ret;
        }

        bool Get(CLevel::MemControl *mem, uint64_t key, uint64_t &value) const
        {
            // Common::g_metic.tracepoint("None");
            int entry_id = find_entry(key);
            // Common::g_metic.tracepoint("FindEntry");
            auto ret = entry_space[entry_id].Get(mem, key, value);
            if (!ret)
            {
                //std::cout << "Key: " << key << std::endl;
                //entry_space[entry_id].Show(mem);
                //entry_space[entry_id + 1].Show(mem);
                //entry_space[entry_id + 2].Show(mem);
                //assert(0);
            }
            // Common::g_metic.tracepoint("EntryGet");
            return ret;
        }

        bool Scan(CLevel::MemControl *mem, uint64_t start_key, int &len, std::vector<std::pair<uint64_t, uint64_t>> &results, bool if_first) const
        {
            int entry_id;
            bool ret;
            if (if_first)
            {
                entry_id = 0;
                ret = entry_space[entry_id].Scan(mem, start_key, len, results,if_first);
            }
            else
            {
                entry_id = find_entry(start_key);
                ret = entry_space[entry_id].Scan(mem, start_key, len, results,false);
            }
            while (!ret && entry_id < nr_entries_ - 1)
            {
                ++entry_id;
                ret = entry_space[entry_id].Scan(mem, start_key, len, results,false);
            }
            //cout << "in group:" << len << endl;
            return ret;
        }

        bool fast_fail(CLevel::MemControl *mem, uint64_t key, uint64_t &value)
        {
            if (nr_entries_ <= 0 || key < min_key)
                return false;
            return Get(mem, key, value);
        }

        bool scan_fast_fail(CLevel::MemControl *mem, uint64_t key)
        {
            if (nr_entries_ <= 0 || key < min_key)
                return false;
            return true;
        }

        bool Update(CLevel::MemControl *mem, uint64_t key, uint64_t value)
        {
            int entry_id = find_entry(key);
            auto ret = entry_space[entry_id].Update(mem, key, value);
            return ret;
        }

        bool Delete(CLevel::MemControl *mem, uint64_t key)
        {
            int entry_id = find_entry(key);
            auto ret = entry_space[entry_id].Delete(mem, key, nullptr);
            return ret;
        }

        static inline uint64_t get_entry_key(const bentry_t &entry)
        {
            return entry.entry_key;
        }

        ALWAYS_INLINE void AdjustEntryKey(CLevel::MemControl *mem)
        {
            entry_space[0].AdjustEntryKey(mem);
        }

        void expand(CLevel::MemControl *mem)
        {
            bentry_t::EntryIter it;
            // Meticer timer;
            // timer.Start();
            bentry_t *new_entry_space = (bentry_t *)NVM::data_alloc->alloc_aligned(next_entry_count * sizeof(bentry_t));
            size_t new_entry_count = 0;
            entry_space[0].AdjustEntryKey(mem);
            for (size_t i = 0; i < nr_entries_; i++)
            {
                new (&it) bentry_t::EntryIter(&entry_space[i]);
                while (!it.end())
                {
                    //std::cout << entry_space[i].buf.entries << std::endl;
                    new (&new_entry_space[new_entry_count++]) bentry_t(&(*it));
                    //std::cout << entry_space[i].buf.entries << std::endl;
                    it.next();
                }
            }
            if(next_entry_count != new_entry_count){
                std::cout << "next_entry_count = " << next_entry_count << " new_entry_count = " << new_entry_count << " nr_entries = " << nr_entries_<< std::endl;
                // assert(next_entry_count == new_entry_count);
            }
            

            NVM::Mem_persist(new_entry_space, new_entry_count * sizeof(bentry_t));

            model.init<bentry_t *, bentry_t>(new_entry_space, new_entry_count,
                                             std::ceil(1.0 * new_entry_count / 100), get_entry_key);
            entry_space = new_entry_space;
            nr_entries_ = new_entry_count;
            next_entry_count = nr_entries_;
            mem->expand_times++;
            // uint64_t expand_time = timer.End();
            // LOG(Debug::INFO, "Finish expanding group, new entry count %ld,  expansion time is %lfs",
            //         nr_entries_, (double)expand_time/1000000.0);
        }

        void Show(CLevel::MemControl *mem)
        {
            std::cout << "Entry count:" << nr_entries_ << std::endl;
            for (int i = 0; i < nr_entries_; i++)
            {
                entry_space[i].Show(mem);
            }
        }

        void Info()
        {
            std::cout << "nr_entrys: " << nr_entries_ << "\t";
            std::cout << "entry size:" << sizeof(bentry_t) << "\t";
            //clevel_mem_->Usage();
        }

    private:
        int nr_entries_;
        int next_entry_count;
        uint64_t min_key;
        bentry_t *entry_space;
        // RMI::LinearModel<RMI::Key_64> model;
        // RMI::TwoStageRMI<RMI::Key_64, 3, 2> model;
        // LearnModel::rmi_model<uint64_t> model;
        // uint8_t reserve[16];
        LearnModel::rmi_line_model<uint64_t> model;
        uint8_t reserve[24];
        // CLevel::MemControl *clevel_mem_;
    };

    static_assert(sizeof(group) == 64);

    class group::BEntryIter
    {
    public:
        using difference_type = ssize_t;
        using value_type = const uint64_t;
        using pointer = const uint64_t *;
        using reference = const uint64_t &;
        using iterator_category = std::random_access_iterator_tag;

        BEntryIter(group *root) : root_(root) {}
        BEntryIter(group *root, uint64_t idx) : root_(root), idx_(idx) {}
        ~BEntryIter()
        {
        }
        uint64_t operator*()
        {
            return root_->entry_space[idx_].entry_key;
        }

        BEntryIter &operator++()
        {
            idx_++;
            return *this;
        }

        BEntryIter operator++(int)
        {
            return BEntryIter(root_, idx_++);
        }

        BEntryIter &operator--()
        {
            idx_--;
            return *this;
        }

        BEntryIter operator--(int)
        {
            return BEntryIter(root_, idx_--);
        }

        uint64_t operator[](size_t i) const
        {
            if ((i + idx_) > root_->nr_entries_)
            {
                std::cout << "索引超过最大值" << std::endl;
                // 返回第一个元素
                return root_->entry_space[root_->nr_entries_ - 1].entry_key;
            }
            return root_->entry_space[i + idx_].entry_key;
        }

        bool operator<(const BEntryIter &iter) const { return idx_ < iter.idx_; }
        bool operator==(const BEntryIter &iter) const { return idx_ == iter.idx_ && root_ == iter.root_; }
        bool operator!=(const BEntryIter &iter) const { return idx_ != iter.idx_ || root_ != iter.root_; }
        bool operator>(const BEntryIter &iter) const { return idx_ < iter.idx_; }
        bool operator<=(const BEntryIter &iter) const { return *this < iter || *this == iter; }
        bool operator>=(const BEntryIter &iter) const { return *this > iter || *this == iter; }
        size_t operator-(const BEntryIter &iter) const { return idx_ - iter.idx_; }

        const BEntryIter &base() { return *this; }

    private:
        group *root_;
        uint64_t idx_;
    };

    class group::Iter
    {
    public:
        Iter(group *root, CLevel::MemControl *mem) : root_(root), mem_(mem), idx_(0)
        {
            if (root->nr_entries_ == 0)
                return;
            new (&biter_) bentry_t::Iter(&root_->entry_space[idx_], mem);
        }
        Iter(group *root, uint64_t start_key, CLevel::MemControl *mem) : root_(root), mem_(mem)
        {
            if (root->nr_entries_ == 0)
                return;
            idx_ = root->find_entry(start_key);
            new (&biter_) bentry_t::Iter(&root_->entry_space[idx_], mem, start_key);
            if (biter_.end())
            {
                next();
            }
        }
        ~Iter()
        {
        }

        uint64_t key()
        {
            return biter_.key();
        }

        uint64_t value()
        {
            return biter_.value();
        }

        bool next()
        {
            if (idx_ < root_->nr_entries_ && biter_.next())
            {
                return true;
            }
            // idx_ = root_->NextGroupId(idx_);
            idx_++;
            if (idx_ < root_->nr_entries_)
            {
                new (&biter_) bentry_t::Iter(&root_->entry_space[idx_], mem_);
                return true;
            }
            return false;
        }

        bool end()
        {
            return idx_ >= root_->nr_entries_;
        }

    private:
        group *root_;
        CLevel::MemControl *mem_;
        bentry_t::Iter biter_;
        uint64_t idx_;
    };

    class group::EntryIter
    {
    public:
        EntryIter(group *group) : group_(group), cur_idx(0)
        {
            new (&biter_) bentry_t::EntryIter(&group_->entry_space[cur_idx]);
        }
        const PointerBEntry::entry &operator*() { return *biter_; }

        ALWAYS_INLINE bool next()
        {
            if (cur_idx < group_->nr_entries_ && biter_.next())
            {
                return true;
            }
            cur_idx++;
            if (cur_idx >= group_->nr_entries_)
            {
                return false;
            }
            new (&biter_) bentry_t::EntryIter(&group_->entry_space[cur_idx]);
            return true;
        }

        ALWAYS_INLINE bool end() const
        {
            return cur_idx >= group_->nr_entries_;
        }

    private:
        group *group_;
        bentry_t::EntryIter biter_;
        uint64_t cur_idx;
    };

    class EntryIter;

    class letree
    {
    public:
        friend class EntryIter;
        class Iter;

    public:
        letree() : nr_groups_(0), root_expand_times(0)
        {
            clevel_mem_ = new CLevel::MemControl(CLEVEL_PMEM_FILE, CLEVEL_PMEM_FILE_SIZE);
        }

        letree(size_t groups) : nr_groups_(groups), root_expand_times(0)
        {
            clevel_mem_ = new CLevel::MemControl(CLEVEL_PMEM_FILE, CLEVEL_PMEM_FILE_SIZE);
            group_space = (group *)NVM::data_alloc->alloc_aligned(groups * sizeof(group));
        }

        ~letree()
        {
            if (clevel_mem_)
            {
                delete clevel_mem_;
            }
            if (group_space)
                NVM::data_alloc->Free(group_space, nr_groups_ * sizeof(group));
        }

        void Init()
        {
            nr_groups_ = 1;
            group_space = (group *)NVM::data_alloc->alloc_aligned(sizeof(group));
            group_space[0].Init(clevel_mem_);
        }

        static inline uint64_t first_key(const std::pair<uint64_t, uint64_t> &kv)
        {
            return kv.first;
        }

        void bulk_load(std::vector<std::pair<uint64_t, uint64_t>> &data)
        {
            size_t size = data.size();
            int group_id = 0;
            model.init<std::vector<std::pair<uint64_t, uint64_t>> &, std::pair<uint64_t, uint64_t>>(data, size, size / 256, first_key);
            nr_groups_ = size / min_entry_count;
            group_space = (group *)NVM::data_alloc->alloc_aligned(nr_groups_ * sizeof(group));
            pmem_memset_persist(group_space, 0, nr_groups_ * sizeof(group));
#ifdef TEST_PMEM_SIZE
            NVM::pmem_size += nr_groups_ * sizeof(group);
#endif
            for (int i = 0; i < size; i++)
            {
                group_id = model.predict(data[i].first) / min_entry_count;
                group_id = std::min(std::max(0, group_id), (int)nr_groups_ - 1);

                group_space[group_id].inc_entry_count();
            }
            size_t start = 0;
            for (int i = 0; i < nr_groups_; i++)
            {
                if (group_space[i].next_entry_count == 0)
                    continue;
                group_space[i].reserve_space();
                group_space[i].bulk_load(data, start, group_space[i].next_entry_count, clevel_mem_);
                start += group_space[i].next_entry_count;
            }
        }

        void bulk_load(const std::pair<uint64_t, uint64_t> data[], int size)
        {
            int group_id = 0;
            model.init<const std::pair<uint64_t, uint64_t>[], std::pair<uint64_t, uint64_t>>(data, size, size / 256, first_key);
            nr_groups_ = size / min_entry_count;
            group_space = (group *)NVM::data_alloc->alloc_aligned(nr_groups_ * sizeof(group));
            pmem_memset_persist(group_space, 0, nr_groups_ * sizeof(group));
#ifdef TEST_PMEM_SIZE
            NVM::pmem_size += nr_groups_ * sizeof(group);
#endif
            for (int i = 0; i < size; i++)
            {
                group_id = model.predict(data[i].first) / min_entry_count;
                group_id = std::min(std::max(0, group_id), (int)nr_groups_ - 1);
                group_space[group_id].inc_entry_count();
            }
            size_t start = 0;
            for (int i = 0; i < nr_groups_; i++)
            {
                if (group_space[i].next_entry_count == 0)
                    continue;
                group_space[i].reserve_space();
                group_space[i].bulk_load(data, start, group_space[i].next_entry_count, clevel_mem_);
                start += group_space[i].next_entry_count;
            }
        }

        status Put(uint64_t key, uint64_t value)
        {
        retry0:
            int group_id = find_group(key);
            auto ret = group_space[group_id].Put(clevel_mem_, key, value);
            if (ret == status::Full)
            { // LearnGroup 太大了
                root_expand_times++;
                ExpandTree();
                std::cout << "root expand " << std::endl;
                goto retry0;
            }
            return ret;
        }

        bool Update(uint64_t key, uint64_t value)
        {
            int group_id = find_group(key);
            auto ret = group_space[group_id].Update(clevel_mem_, key, value);
            return ret;
        }

        bool Get(uint64_t key, uint64_t &value) const
        {
            // Common::g_metic.tracepoint("None");
            // int group_id = find_group(key);
            // Common::g_metic.tracepoint("FindGoup");
            // auto ret = group_space[group_id].Get(clevel_mem_, key, value);
            // if(!ret) {
            //     std::cout  << "Group [" << group_id << "].\n";
            //     group_space[group_id].Show(clevel_mem_);
            //     std::cout  << "Group [" << group_id + 1 << "].\n";
            //     group_space[group_id + 1].Show(clevel_mem_);
            // }
            // return ret;
            if (find_fast(key, value))
            {
                return true;
            }
            return find_slow(key, value);
        }

        bool Scan(uint64_t start_key, int len, std::vector<std::pair<uint64_t, uint64_t>> &results)
        {
            int length = len;
            if (scan_fast(start_key, length, results))
            {
                return true;
            }
            return scan_slow(start_key, length, results);
        }

        bool Delete(uint64_t key)
        {
            int group_id = find_group(key);
            auto ret = group_space[group_id].Delete(clevel_mem_, key);
            return ret;
        }

        ALWAYS_INLINE int find_group(const uint64_t &key) const
        {
            int group_id = model.predict(key) / min_entry_count;
            group_id = std::min(std::max(0, group_id), (int)nr_groups_ - 1);
            // if(key < group_space[group_id].min_key && group_id > 0) {
            //     group_id --;
            // }
            /** 忽略空的group和边界 **/
            while (group_id > 0 && (group_space[group_id].nr_entries_ == 0 ||
                                    (key < group_space[group_id].entry_space[0].entry_key)))
            {
                group_id--;
            }
            while (group_space[group_id].nr_entries_ == 0)
                group_id++;

            return group_id;
        }

        ALWAYS_INLINE bool find_fast(uint64_t key, uint64_t &value) const
        {
            int group_id = model.predict(key) / min_entry_count;
            group_id = std::min(std::max(0, group_id), (int)nr_groups_ - 1);
            return group_space[group_id].fast_fail(clevel_mem_, key, value);
        }

        ALWAYS_INLINE bool scan_fast(uint64_t start_key, int &len, std::vector<std::pair<uint64_t, uint64_t>> &results) const
        {
            int group_id = model.predict(start_key) / min_entry_count;
            group_id = std::min(std::max(0, group_id), (int)nr_groups_ - 1);
            if (group_space[group_id].scan_fast_fail(clevel_mem_, start_key))
            {
                scan_slow(start_key, len, results, group_id);
                return true;
            }
            return false;
        }

        ALWAYS_INLINE bool find_slow(uint64_t key, uint64_t &value) const
        {
            int group_id = find_group(key);
            return group_space[group_id].Get(clevel_mem_, key, value);
        }

        ALWAYS_INLINE bool scan_slow(uint64_t start_key, int &len, std::vector<std::pair<uint64_t, uint64_t>> &results, int fast_group_id = 0) const
        {
            int group_id = 0;
            if (fast_group_id != 0)
            {
                group_id = fast_group_id;
            }
            else
            {
                group_id = find_group(start_key);
            }
            auto ret = group_space[group_id].Scan(clevel_mem_, start_key, len, results,true);
            while (!ret && group_id < nr_groups_ - 1)
            {
                ++group_id;
                while (group_space[group_id].nr_entries_ == 0)
                {
                    ++group_id;
                }
                ret = group_space[group_id].Scan(clevel_mem_, start_key, len, results,false);
            }
            // cout << len << endl;
            if (len > 0)
            {
                std::cout << len << std::endl;
            }
            return 0;
        }

        void ExpandTree()
        {
            size_t entry_count = 0;
            int entry_seq = 0;

            // Show();
            // Meticer timer;
            // timer.Start();
            {
                /*采用一层线性模型*/
                LearnModel::rmi_line_model<uint64_t>::builder_t bulder(&model);
                for (int i = 0; i < nr_groups_; i++)
                {
                    if (group_space[i].next_entry_count == 0)
                        continue;
                    entry_count += group_space[i].next_entry_count;
                    group_space[i].AdjustEntryKey(clevel_mem_);
                    group::EntryIter e_iter(&group_space[i]);
                    // int sample = std::ceil(1.0 * group_space[i].next_entry_count / min_entry_count);
                    while (!e_iter.end())
                    {
                        bulder.add((*e_iter).entry_key, entry_seq);
                        e_iter.next();
                        entry_seq++;
                    }
                }
                bulder.build();
            }

            // {
            //     std::vector<uint64_t> entry_keys;
            //     for(int i = 0; i < nr_groups_; i++) {
            //         if(group_space[i].next_entry_count == 0) continue;
            //         entry_count += group_space[i].next_entry_count;
            //         group_space[i].AdjustEntryKey(clevel_mem_);
            //         group::EntryIter e_iter(&group_space[i]);
            //         while (!e_iter.end())
            //         {
            //         entry_keys.push_back((*e_iter).entry_key);
            //         e_iter.next();
            //         }
            //     }
            //     model.init(entry_keys, entry_keys.size(), entry_keys.size() / 256);
            // }
            // std::cout << "Entry count: " <<
            //LOG(Debug::INFO, "Entry_count: %ld, %d", entry_count, entry_seq);
            // int new_nr_groups = std::ceil(1.0 * entry_count / min_entry_count);
            int new_nr_groups = std::ceil(1.0 * entry_count / min_entry_count);
            group *new_group_space = (group *)NVM::data_alloc->alloc_aligned(new_nr_groups * sizeof(group));
            pmem_memset_persist(new_group_space, 0, new_nr_groups * sizeof(group));
#ifdef TEST_PMEM_SIZE
            NVM::pmem_size += nr_groups_ * sizeof(group);
#endif
            int group_id = 0;

            // int prev_group_id  = 0;
            // for(int  i = 1; i < entry_keys.size(); i ++) {
            //     assert(entry_keys[i] > entry_keys[i-1]);
            //     group_id = model.predict(entry_keys[i]) / min_entry_count;
            //     group_id = std::min(std::max(0, group_id), (int)new_nr_groups - 1);
            //     if(group_id < prev_group_id) {
            //         model.predict(entry_keys[i-1]);
            //         model.predict(entry_keys[i]);
            //         assert(0);
            //     }
            //     prev_group_id = group_id;
            // }

            for (int i = 0; i < nr_groups_; i++)
            {
                group::EntryIter e_iter(&group_space[i]);
                while (!e_iter.end())
                {
                    group_id = model.predict((*e_iter).entry_key) / min_entry_count;
                    group_id = std::min(std::max(0, group_id), (int)new_nr_groups - 1);
                    new_group_space[group_id].inc_entry_count();
                    e_iter.next();
                }
            }

            for (int i = 0; i < new_nr_groups; i++)
            {
                if (new_group_space[i].next_entry_count == 0)
                    continue;
                new_group_space[i].reserve_space();
            }

            // uint64_t prev_key = 0;
            for (int i = 0; i < nr_groups_; i++)
            {
                group::EntryIter e_iter(&group_space[i]);
                while (!e_iter.end())
                {
                    group_id = model.predict((*e_iter).entry_key) / min_entry_count;
                    group_id = std::min(std::max(0, group_id), (int)new_nr_groups - 1);
                    new_group_space[group_id].append_entry(&(*e_iter));
                    // assert((*e_iter).entry_key >= prev_key);
                    // prev_key = (*e_iter).entry_key;
                    e_iter.next();
                }
            }

            for (int i = 0; i < new_nr_groups; i++)
            {
                // std::cerr << "Group [" << i << "]: entry count " << new_group_space[i].next_entry_count << ".\n";
                if (new_group_space[i].next_entry_count == 0)
                    continue;
                new_group_space[i].re_tarin();
            }
            pmem_persist(new_group_space, new_nr_groups * sizeof(group));
            nr_groups_ = new_nr_groups;
            group_space = new_group_space;
            // uint64_t expand_time = timer.End();
            // LOG(Debug::INFO, "Finish expanding group, new group count %ld,  expansion time is %lfs",
            //nr_groups_, (double)expand_time/1000000.0);
            // Show();
        }

        void Show()
        {
            for (int i = 0; i < nr_groups_; i++)
            {
                std::cout << "Group [" << i << "].\n";
                group_space[i].Show(clevel_mem_);
            }
        }

        void Info() {
            std::cout << "root_expand_times : " << root_expand_times << std::endl;
            clevel_mem_->Usage();
        }

    private:
        group *group_space;
        int nr_groups_;
        LearnModel::rmi_line_model<uint64_t> model;
        CLevel::MemControl *clevel_mem_;
        int entries_per_group = min_entry_count;
        uint64_t root_expand_times;
    };

    class letree::Iter
    {
    public:
        Iter(letree *tree) : tree_(tree), group_id_(0), giter(&tree->group_space[0], tree->clevel_mem_)
        {
            while (giter.end())
            {
                group_id_++;
                if (group_id_ >= tree_->nr_groups_)
                    break;
                new (&giter) group::Iter(&tree_->group_space[group_id_], tree_->clevel_mem_);
            }
        }
        Iter(letree *tree, uint64_t start_key) : tree_(tree), group_id_(0),
                                                 giter(&tree->group_space[0], tree->clevel_mem_)
        {
            group_id_ = tree->find_group(start_key);
            new (&giter) group::Iter(&tree->group_space[group_id_], start_key, tree->clevel_mem_);
        }
        ~Iter()
        {
        }

        uint64_t key()
        {
            return giter.key();
        }

        uint64_t value()
        {
            return giter.value();
        }

        bool next()
        {
            if (group_id_ < tree_->nr_groups_ && giter.next())
            {
                return true;
            }
            // idx_ = root_->NextGroupId(idx_);
            while (giter.end())
            {
                group_id_++;
                if (group_id_ >= tree_->nr_groups_)
                    break;
                new (&giter) group::Iter(&tree_->group_space[group_id_], tree_->clevel_mem_);
            }

            return group_id_ < tree_->nr_groups_;
        }

        bool end()
        {
            return group_id_ >= tree_->nr_groups_;
        }

    private:
        letree *tree_;
        group::Iter giter;
        int group_id_;
    };

} // namespace combotree
