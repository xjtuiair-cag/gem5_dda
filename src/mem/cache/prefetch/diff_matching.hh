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
    class DeltaTableEntry
    {
        Addr pc;
        bool valid;
        T last;

        int diff_ptr;
        const int diff_size;
        std::vector<T> diff;

      public:
        DeltaTableEntry(Addr pc, T last, int diff_size, bool valid)
          : pc(pc), last(last), diff_ptr(0), diff_size(diff_size), 
            valid(valid), diff(diff_size) {};
        ~DeltaTableEntry() = default;

        void invalidate ()
        {
            diff_ptr = 0; valid = false;
        };

        void fill (T last_in)
        {
            if (diff.size() < diff_size) {
                diff.push_back(last_in - last);
            } else {
                diff[diff_ptr] = last_in - last;
                diff_ptr = (diff_ptr+1) % diff_size;
            }
            last = last_in;
        };

        T& operator[](int index) { return diff[ (diff_ptr+index) % diff_size ]; };

        void renew (Addr pc_new, T last_new, bool valid_new)
        {
            pc = pc_new;
            last = last_new;
            valid = valid_new;
            diff_ptr = 0;
        };
    };

    std::vector<DeltaTableEntry<IndexData>> indexDataDeltaTable;
    std::vector<DeltaTableEntry<Addr>> targetAddrDeltaTable;

    struct IndirectCandidateSelectionEntry
    {
        bool valid;
        Addr subscribe_pc;
        

    };
    std::vector<IndirectCandidateSelectionEntry> indirectCandidateSelection;


    struct RelationTableEntry
    {
        Addr index_pc;
        Addr target_pc;
        Addr target_base_addr;
        unsigned int shift;
        bool range;
        int range_degree;
        ContextID cID;

        RelationTableEntry(
            Addr index_pc, Addr target_pc, Addr target_base_addr, 
            unsigned int shift, bool range, int range_degree, ContextID cID
        ) : index_pc(index_pc), target_pc(target_pc), target_base_addr(target_base_addr),
            shift(shift), range(range), range_degree(range_degree), cID(cID)
        {}

        ~RelationTableEntry() 
        {}
    };
    std::vector<RelationTableEntry> relationTable;

    // Capture a normal request packet to generate prefetch with resp data.
    // Otherwise there will be a segfault using resp packet 
    // TODO: Self built request
    DeferredPacket * dpp_req;

    struct DMPStats : public statistics::Group
    {
        DMPStats(statistics::Group *parent);
        // STATS
        statistics::Scalar dmp_pfIdentified;
    } statsDMP;

  public:

    DiffMatching(const DiffMatchingPrefetcherParams &p);
    ~DiffMatching();

    // Base notify for Cache access (Hit or Miss)
    void notify (const PacketPtr &pkt, const PrefetchInfo &pfi) override;
    
    // Probe DataResp from Memory for prefetch generation
    void notifyFill(const PacketPtr &pkt) override;

    // Probe AddrReq to L1 for prefetch detection
    void notifyL1Req(const PacketPtr &pkt) override;
    // Probe DataResp from L1 for prefetch detection
    void notifyL1Resp(const PacketPtr &pkt) override;

    void calculatePrefetch(const PrefetchInfo &pfi,
                           std::vector<AddrPriority> &addresses) override;
    
    //void addDMPToQueue(std::list<DeferredPacket> &queue, DeferredPacket &dpp);
};

} // namespace prefetch
} // namespace gem5

#endif // __MEM_CACHE_PREFETCH_DIFF_MATCHING_HH__