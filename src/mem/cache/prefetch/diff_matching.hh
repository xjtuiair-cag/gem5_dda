/**
* Difference-based prefetcher
*/

#ifndef __MEM_CACHE_PREFETCH_DIFF_MATCHING_HH__
#define __MEM_CACHE_PREFETCH_DIFF_MATCHING_HH__

#include <vector>
#include <queue>
#include <unordered_map>

#include "base/types.hh"
#include "mem/cache/prefetch/stride.hh"
#include "mem/cache/prefetch/queued.hh"

namespace gem5
{

struct DiffMatchingPrefetcherParams;

GEM5_DEPRECATED_NAMESPACE(Prefetcher, prefetch);
namespace prefetch
{

class DiffMatching : public Stride
{

    typedef int32_t IndexData;

    std::unordered_map<Addr, std::queue<unsigned>> access_offset;

    const int iddt_ent_num;
    const int tadt_ent_num;
    const int rt_ent_num;

    const int iddt_diff_num;
    const int tadt_diff_num;

    int iddt_ptr;
    int tadt_ptr;
    int rt_ptr;

    template <typename T>
    struct DeltaTableEntry
    {
        Addr pc;
        T last;
        std::vector<T> diff;

        int diff_ptr;
        bool valid;
        const int diff_size;

        DeltaTableEntry(Addr pc, T last, int diff_size, bool valid)
          : pc(pc), last(last), diff_ptr(0), diff_size(diff_size), valid(valid)
        {}
        ~DeltaTableEntry() = default;

        void invalidate ()
        {
            diff_ptr = 0; valid = false;
        }

        void fill (T last_in)
        {
            if (diff.size() < diff_size) {
                diff.push_back(last_in - last);
            } else {
                diff[diff_ptr] = last_in - last;
                diff_ptr = (diff_ptr+1) % diff_size;
            }
            last = last_in;
        }

        void renew (Addr pc_new, T last_new, int diff_size_new, bool valid_new)
        {
            pc = pc_new;
            last = last_new;
            diff_size = diff_size_new;
            valid = valid_new;
        }
    };

    std::vector<DeltaTableEntry<IndexData>> indexDataDeltaTable;
    std::vector<DeltaTableEntry<Addr>> targetAddrDeltaTable;


    struct RelationTableEntry
    {
        Addr index_pc;
        Addr target_pc;
        Addr target_base_addr;
        unsigned int shift;
        bool range;

        RelationTableEntry(Addr index_pc, Addr target_pc, Addr target_base_addr, unsigned int shift, bool range)
          : index_pc(index_pc), target_pc(target_pc), target_base_addr(target_base_addr), shift(shift), range(range) 
        {}
    };
    std::vector<RelationTableEntry> relationTable;

    DeferredPacket * dpp_req;

  public:

    DiffMatching(const DiffMatchingPrefetcherParams &p);
    ~DiffMatching();

    void notify (const PacketPtr &pkt, const PrefetchInfo &pfi) override;
    
    void notifyFill(const PacketPtr &pkt) override;

    void calculatePrefetch(const PrefetchInfo &pfi,
                           std::vector<AddrPriority> &addresses) override;
    
    void addDMPToQueue(std::list<DeferredPacket> &queue, DeferredPacket &dpp);
};

} // namespace prefetch
} // namespace gem5

#endif // __MEM_CACHE_PREFETCH_DIFF_MATCHING_HH__