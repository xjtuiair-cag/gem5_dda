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

    typedef int64_t IndexData;
    typedef int64_t TargetAddr;

    const int iddt_ent_num;
    const int tadt_ent_num;
    const int rt_ent_num;

    // indirect range prefetch length
    int indir_range;

    // possiable shift values
    const unsigned int shift_v[4] = {0, 1, 2, 3};

    /** IDDT and TDDT */
    const int iddt_diff_num;
    const int tadt_diff_num;

    template <typename T>
    class DiffSeqCollection
    {
        Addr pc;
        bool valid;
        bool ready;
        ContextID cID;
        T last;

        int diff_ptr;
        const int diff_size;
        std::vector<T> diff;

      public:
        DiffSeqCollection(Addr pc, T last, int diff_size)
          : pc(pc), valid(false), ready(false), cID(0), 
            last(last), diff_ptr(0), diff_size(diff_size) {};
        ~DiffSeqCollection() = default;

        void validate() { valid = true; };

        void invalidate ()
        {
            diff_ptr = 0;
            cID = 0;
            valid = false;
            diff.clear();
        };

        void fill (T last_in, ContextID cID_in)
        {
            if (cID_in != cID) return;

            if (ready) {
                diff_ptr = (diff_ptr+1) % diff_size;
                diff[diff_ptr] = last_in - last;
            } else {
                diff.push_back(last_in - last);
                if (diff.size() == diff_size) ready = true;
            }
            last = last_in;
        };

        bool isReady() const { return ready; };

        bool isValid() const { return valid; };

        Addr getPC() const { return pc; }; 

        ContextID getContextId() const { return cID; };

        T getLast() const {return last; };

        T operator[](int index) const { return diff[ (diff_ptr+index) % diff_size ]; };

        void renew (Addr pc_new, T last_new, ContextID cID_new)
        {
            pc = pc_new;
            last = last_new;
            cID = cID_new;
            ready = false;
            valid = false;
            diff_ptr = 0;
            diff.clear();
        };
    };

    typedef DiffSeqCollection<IndexData> iddt_ent_t;
    typedef DiffSeqCollection<TargetAddr> tadt_ent_t;

    std::vector<iddt_ent_t> indexDataDeltaTable;
    std::vector<tadt_ent_t> targetAddrDeltaTable;

    int iddt_ptr;
    int tadt_ptr;

    iddt_ent_t* allocateIDDTEntry(Addr index_pc);
    tadt_ent_t* allocateTADTEntry(Addr target_pc);

    /** RangeTable related */
    struct RangeTableEntry
    {
        Addr target_PC; // range base on req address
        Addr cur_tail;
        int cur_count;

        const int shift_times; // 0 (byte) / 2 (int) / 3 (double)
        const int range_quant_unit; // quantify true range to several units
        const int range_quant_level; // total levels of range quant unit 

        // NOTE: Range prefetch distence should coorparate wit StreamPrefetch
        // NOTE: [TODO] more suitable RangePrefetc schedule policy
        std::vector<int> sample_count;

        RangeTableEntry(
                Addr target_PC, Addr req_addr, int shift_times, int rql, int rqu
            ) :  target_PC(target_PC), cur_tail(req_addr), shift_times(shift_times),
                cur_count(0), range_quant_level(rql), range_quant_unit(rqu),
                sample_count(rql, 0) {}

        bool updateSample(Addr addr_in); 

        int getPredLevel() {
            return std::distance( sample_count.begin(),
                std::max_element(sample_count.begin(), sample_count.end()));
        }

    };

    std::unordered_map<Addr, RangeTableEntry> RT_shift_0;
    std::unordered_map<Addr, RangeTableEntry> RT_shift_2;
    std::unordered_map<Addr, RangeTableEntry> RT_shift_3;

    /** Range quantification method
    * eg. unit=8, level=4
    * level:   |  1  |  2  |  3  |  4  |
    * unti:    |  u  |  u  |  u  |  u  |
    * range:   0     8     16    24    32 
    */
    const int range_unit_param; // quantify true range to several units
    const int range_level_param; // total levels of range quant unit 

    bool rangeFilter(Addr PC_in, Addr addr_in);


    /** IndirectCandidateScoreboard related*/
    struct ICSEntry
    {
        bool valid;
        Addr subscribe_pc;
    };
    std::vector<ICSEntry> indirectCandidateScoreboard;


    /** RelationTable related */
    struct RTEntry
    {
        Addr index_pc;
        Addr target_pc;
        Addr target_base_addr;
        unsigned int shift;
        bool range;
        int range_degree;
        ContextID cID;
        Tick birth_time;

        // manually allocate
        RTEntry(
            Addr index_pc, Addr target_pc, Addr target_base_addr, 
            unsigned int shift, bool range, int range_degree, ContextID cID
        ) : index_pc(index_pc), target_pc(target_pc), target_base_addr(target_base_addr),
            shift(shift), range(range), range_degree(range_degree), cID(cID)
        { birth_time = curTick(); }

        RTEntry* update(const RTEntry& new_entry)
        {
            index_pc = new_entry.index_pc;
            target_pc = new_entry.target_pc;
            target_base_addr = new_entry.target_base_addr;
            shift = new_entry.shift;
            range = new_entry.range;
            range_degree = new_entry.range_degree;
            cID = new_entry.cID;
            birth_time = new_entry.birth_time;
            
            return this;
        }
    };
    std::vector<RTEntry> relationTable;

    // point to the next available RTE (index in RT)
    int rt_ptr; 

    bool findRTE(Addr index_pc, Addr target_pc, ContextID cID);

    RTEntry* insertRTE(
        const iddt_ent_t& iddt_ent_match, const tadt_ent_t& tadt_ent_match,
        int iddt_match_point, unsigned int shift, ContextID cID
    );


    /** DMP specific stats */
    struct DMPStats : public statistics::Group
    {
        DMPStats(statistics::Group *parent);
        // STATS
        statistics::Scalar dmp_pfIdentified;
    } statsDMP;


    /** DMP functions */

  protected:

    void diffMatching(const tadt_ent_t& tadt_ent);

  public:

    DiffMatching(const DiffMatchingPrefetcherParams &p);
    ~DiffMatching();

    // Base notify for Cache access (Hit or Miss)
    void notify(const PacketPtr &pkt, const PrefetchInfo &pfi) override;
    
    // Probe DataResp from Memory for prefetch generation
    void notifyFill(const PacketPtr &pkt) override;

    // Probe AddrReq to L1 for prefetch detection
    void notifyL1Req(const PacketPtr &pkt) override;
    // Probe DataResp from L1 for prefetch detection
    void notifyL1Resp(const PacketPtr &pkt) override;

    void calculatePrefetch(const PrefetchInfo &pfi,
                           std::vector<AddrPriority> &addresses) override;
};

} // namespace prefetch
} // namespace gem5

#endif // __MEM_CACHE_PREFETCH_DIFF_MATCHING_HH__