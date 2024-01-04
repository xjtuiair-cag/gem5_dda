#include "mem/cache/prefetch/diff_matching.hh"
#include "mem/cache/mshr.hh"
#include "mem/cache/base.hh"

#include "debug/HWPrefetch.hh"
#include "debug/DMP.hh"
#include "params/DiffMatchingPrefetcher.hh"

namespace gem5
{

GEM5_DEPRECATED_NAMESPACE(Prefetcher, prefetch);
namespace prefetch
{

DiffMatching::DiffMatching(const DiffMatchingPrefetcherParams &p)
  : Stride(p),
    iddt_ent_num(p.iddt_ent_num),
    tadt_ent_num(p.tadt_ent_num),
    rt_ent_num(p.rt_ent_num),
    indir_range(p.indir_range),
    iddt_diff_num(p.iddt_diff_num),
    tadt_diff_num(p.tadt_diff_num),
    iddt_ptr(0),
    tadt_ptr(0),
    range_unit_param(p.range_unit),
    range_level_param(p.range_level),
    rt_ptr(0),
    statsDMP(this)
{
    // init IDDT
    if (!p.index_pc_init.empty()) {
        for (auto index_pc : p.index_pc_init) {
            indexDataDeltaTable.emplace_back(index_pc, 0, iddt_diff_num);
            indexDataDeltaTable.back().validate();
            iddt_ptr++;
        }
    }

    // init TADT
    if (!p.target_pc_init.empty()) {
        for (auto target_pc : p.target_pc_init) {
            targetAddrDeltaTable.emplace_back(target_pc, 0, tadt_diff_num);
            targetAddrDeltaTable.back().validate();
            tadt_ptr++;
        }
    }

    // init RangeTable
    if (!p.range_pc_init.empty()) {
        for (auto range_pc : p.range_pc_init) {
            for (unsigned int shift_try: shift_v) {
                rangeTable.push_back(
                    new RangeTableEntry(
                        range_pc, 0x0, shift_try, range_level_param, range_unit_param
                    )
                );
            }
        }
    }
}

DiffMatching::~DiffMatching()
{
    for (auto range_ent : rangeTable) {
        delete range_ent;
    }
}

DiffMatching::DMPStats::DMPStats(statistics::Group *parent)
    : statistics::Group(parent),
    ADD_STAT(dmp_pfIdentified, statistics::units::Count::get(),
             "number of DMP prefetch candidates identified")
{
}

void
DiffMatching::diffMatching(const DiffMatching::tadt_ent_t& tadt_ent)
{
    // ready flag check 
    assert(tadt_ent.isValid() && tadt_ent.isReady());

    ContextID tadt_ent_cID = tadt_ent.getContextId();

    // try to match all valid and ready index data diff-sequence 
    for (const auto& iddt_ent : indexDataDeltaTable) {
        if (!iddt_ent.isValid() || !iddt_ent.isReady()) continue;
        if (tadt_ent_cID != iddt_ent.getContextId()) continue;
        
        // a specific index data diff-sequence may have multiple matching point  
        for (int i_start = 0; i_start < iddt_diff_num-tadt_diff_num+1; i_start++) {

            // try different shift values
            for (unsigned int shift_try: shift_v) {
                int t_start = 0;
                while (t_start < tadt_diff_num) {
                    if (iddt_ent[i_start+t_start] != (tadt_ent[t_start] >> shift_try)) {
                        break;
                    }
                    t_start++;
                }

                if (t_start == tadt_diff_num) {
                    // match success
                    // insert pattern to RelationTable
                    insertRTE(iddt_ent, tadt_ent, i_start+tadt_diff_num, shift_try, tadt_ent_cID);
                }

            }
        }

    }
}

bool
DiffMatching::findRTE(Addr index_pc, Addr target_pc, ContextID cID)
{
    for (const auto& rte : relationTable)
    {
        // only allow one index for each target
        if ( rte.target_pc == target_pc && 
             rte.cID == cID) {
            return true; 
        }

        // avoid ring
        if ( rte.index_pc == target_pc && 
             rte.target_pc == index_pc && 
             rte.cID == cID) {
            return true; 
        }

    }
    return false;
}

DiffMatching::RTEntry*
DiffMatching::insertRTE(
    const DiffMatching::iddt_ent_t& iddt_ent_match, 
    const DiffMatching::tadt_ent_t& tadt_ent_match,
    int iddt_match_point, unsigned int shift, ContextID cID)
{
    Addr new_index_pc = iddt_ent_match.getPC();
    Addr new_target_pc = tadt_ent_match.getPC();

    // check if pattern already exist
    if (findRTE(new_index_pc, new_target_pc, cID)) {
        return nullptr;
    }

    // calculate the target base address
    IndexData data_match = iddt_ent_match.getLast();
    for (int i = iddt_match_point; i < iddt_diff_num; i++) {
        data_match -= iddt_ent_match[i];
    }

    TargetAddr addr_match = tadt_ent_match.getLast();

    int64_t base_addr_tmp = addr_match - (data_match << shift);

    DPRINTF(DMP, "Matched: LastData %llx Data %llx Addr %llx Shift %d\n", 
               iddt_ent_match.getLast(), data_match, addr_match, shift);
    
    assert(base_addr_tmp <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()));
    Addr target_base_addr = static_cast<uint64_t>(base_addr_tmp);

    // get indexPC Range type, only matter when current indexPC as other pattern's target
    bool new_range_type = false;
    for (auto range_ent : rangeTable) {
        if (range_ent->target_PC != new_index_pc || range_ent->cID != cID) continue;

        new_range_type = new_range_type || range_ent->getRangeType();
    } 

    DPRINTF(DMP, "Insert RelationTable: "
        "indexPC %llx targetPC %llx target_addr %llx shift %d cID %d rangeType %d \n",
        new_index_pc, new_target_pc, target_base_addr, shift, cID, new_range_type
    );

    RTEntry new_rte (
        new_index_pc,
        new_target_pc,
        target_base_addr,
        shift,
        new_range_type, 
        16, // TODO: dynamic detection
        cID
    );

    if (relationTable.size() < rt_ent_num) {
        relationTable.push_back(new_rte);
        return &relationTable.back();
    } else {
        RTEntry* brand_new_ent = relationTable[rt_ptr].update(new_rte);
        rt_ptr = (rt_ptr+1) % rt_ent_num;
        return brand_new_ent;
    }
}

bool
DiffMatching::RangeTableEntry::updateSample(Addr addr_in)
{
    // assert(target_PC == PC_in);

    // continuity check
    Addr addr_shifted = addr_in >> shift_times;

    if (addr_shifted == cur_tail + 1) {
        cur_count++;
        cur_tail = addr_shifted;
        return false;
    } 

    if (cur_count > 0) {
        // generate a range sample to range count
        int sampled_level;
        if (cur_count >= range_quant_level * range_quant_unit) {
            sampled_level = range_quant_level;
        } else {
            sampled_level = (cur_count + range_quant_unit) 
                                / range_quant_unit;
        }
        sample_count[sampled_level-1]++;
        cur_count = 0;
    }

    cur_tail = addr_shifted;
    return true;
}

bool
DiffMatching::RangeTableEntry::getRangeType() const
{
    int sum = 0;
    for(int sample : sample_count) {
        sum += sample;
    }
    return (sum > 0);
}

bool
DiffMatching::rangeFilter(Addr PC_in, Addr addr_in, ContextID cID_in)
{
    bool ret = true;

    for (auto range_ent : rangeTable) {
        if (range_ent->target_PC != PC_in || range_ent->cID != cID_in) continue;

        DPRINTF(DMP, "updateSample: PC %llx addr %llx cur_tail %llx\n", 
                    PC_in, addr_in, range_ent->cur_tail);
        bool update_ret = range_ent->updateSample(addr_in);
        ret = ret && update_ret;
    }

    return ret;
}

void
DiffMatching::notifyL1Req(const PacketPtr &pkt) 
{  

    // update TADT
    if (!pkt->req->hasPC() || !pkt->req->hasVaddr()) {
        return;
    }

    Addr req_addr = pkt->req->getVaddr();

    for (auto& tadt_ent: targetAddrDeltaTable) {

        Addr target_pc = tadt_ent.getPC();
        
        // tadt_ent validation check
        if (target_pc != pkt->req->getPC() || !tadt_ent.isValid()) continue;
        assert(req_addr <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()));

        // repeation check
        if (tadt_ent.getLast() == req_addr) continue;

        // range check
        if (!rangeFilter(target_pc, req_addr, 
                        pkt->req->hasContextId() ? pkt->req->contextId() : 0))
            continue;

        DPRINTF(DMP, "notifyL1Req: [filter pass] PC %llx, cID %d, Addr %llx, PAddr %llx, VAddr %llx\n",
                            pkt->req->hasPC() ? pkt->req->getPC() : 0x0,
                            pkt->req->hasContextId() ? pkt->req->contextId() : 0,
                            pkt->getAddr(), 
                            pkt->req->getPaddr(), 
                            pkt->req->hasVaddr() ? pkt->req->getVaddr() : 0x0 );

        // check passed, fill in
        tadt_ent.fill(
            static_cast<TargetAddr>(pkt->req->getVaddr()),
            pkt->req->hasContextId() ? pkt->req->contextId() : 0
        ); 

        // try matching
        if (tadt_ent.isReady()) {
            diffMatching(tadt_ent);
        }
    }

    DPRINTF(HWPrefetch, "notifyL1Req: PC %llx, Addr %llx, PAddr %llx, VAddr %llx\n",
                        pkt->req->hasPC() ? pkt->req->getPC() : 0x0,
                        pkt->getAddr(), 
                        pkt->req->getPaddr(), 
                        pkt->req->hasVaddr() ? pkt->req->getVaddr() : 0x0 );
}

void
DiffMatching::notifyL1Resp(const PacketPtr &pkt) 
{
    if (!pkt->req->hasPC()) {
       DPRINTF(HWPrefetch, "notifyL1Resp: no PC\n");
       return;
    }
        
    if (!pkt->validData()) {
        DPRINTF(HWPrefetch, "notifyL1Resp: PC %llx, PAddr %llx, no Data, %s\n", 
                                pkt->req->getPC(), pkt->req->getPaddr(), pkt->cmdString());
        return;
    }

    assert(pkt->getSize() <= blkSize); 
    uint8_t data[pkt->getSize()];
    pkt->writeData(data); 

    const int data_stride = 4;
    const int byte_width = 8;
    uint64_t resp_data = 0;
    for (int i_st = data_stride-1; i_st >= 0; i_st--) {
        resp_data = resp_data << byte_width;
        resp_data += static_cast<uint64_t>(data[i_st]);
    }
    assert(resp_data <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()));

    // update IDDT
    for (auto& iddt_ent: indexDataDeltaTable) {
        if (iddt_ent.getPC() == pkt->req->getPC() && iddt_ent.isValid()) {
            IndexData new_data;
            std::memcpy(&new_data, &resp_data, sizeof(int64_t));

            // repeation check
            if (iddt_ent.getLast() == new_data) continue;

            DPRINTF(DMP, "notifyL1Resp: [filter pass] PC %llx, PAddr %llx, VAddr %llx, Size %d, Data %llx\n", 
                                pkt->req->getPC(), pkt->req->getPaddr(), 
                                pkt->req->hasVaddr() ? pkt->req->getVaddr() : 0x0,
                                pkt->getSize(), resp_data);

            iddt_ent.fill(new_data, pkt->req->hasContextId() ? pkt->req->contextId() : 0);
        }
    }

    DPRINTF(HWPrefetch, "notifyL1Resp: PC %llx, PAddr %llx, VAddr %llx, Size %d, Data %llx\n", 
                        pkt->req->getPC(), pkt->req->getPaddr(), 
                        pkt->req->hasVaddr() ? pkt->req->getVaddr() : 0x0,
                        pkt->getSize(), resp_data);
}

void
DiffMatching::notifyFill(const PacketPtr &pkt)
{
    /** use virtual address for prefetch */
    assert(tlb != nullptr);

    if (!pkt->req->hasPC()) {
       DPRINTF(HWPrefetch, "notifyFill: no PC\n");
       return;
    }

    if (!pkt->validData()) {
        DPRINTF(HWPrefetch, "notifyFill: PC %llx, PAddr %llx, no Data, %s\n", 
                    pkt->req->getPC(), pkt->req->getPaddr(), pkt->cmdString());
        return;
    }

    /* get response data */
    assert(pkt->getSize() <= blkSize); 
    uint8_t fill_data[blkSize];
    pkt->writeData(fill_data); 

    /* prinf response data in bytes */
    if (debug::HWPrefetch) {
        unsigned data_offset_debug = pkt->req->getPaddr() & (blkSize-1);
        do {
        int64_t resp_data = (int64_t) ((uint64_t)fill_data[data_offset_debug]
                                + (((uint64_t)fill_data[data_offset_debug+1]) << 8)
                                + (((uint64_t)fill_data[data_offset_debug+2]) << 16)
                                + (((uint64_t)fill_data[data_offset_debug+3]) << 24));
        DPRINTF(HWPrefetch, "notifyFill: PC %llx, PAddr %llx, DataOffset %d, Data %llx\n", 
                        pkt->req->getPC(), pkt->req->getPaddr(), data_offset_debug, resp_data);
        data_offset_debug += 4;
        } while (data_offset_debug < blkSize);
    }

    Addr pc = pkt->req->getPC();
    for (const auto& rt_ent: relationTable) { 

        if (rt_ent.index_pc != pc) continue;

        /* Assume response data is a int and always occupies 4 bytes */
        const int data_stride = 4;
        const int byte_width = 8;
        const int32_t priority = 0;

        /* set range_end, only process one data if not range type */
        unsigned range_end;
        unsigned data_offset = pkt->req->getPaddr() & (blkSize-1);
        if (rt_ent.range) {
            range_end = std::min(data_offset + data_stride * rt_ent.range_degree, blkSize);
        } else {
            range_end = data_offset + data_stride;
        }

        /* loop for range prefetch */
        for (unsigned i_of = data_offset; i_of < range_end; i_of += data_stride)
        {
            /* integrate fill_data[] to resp_data (considered as unsigned)  */
            uint64_t resp_data = 0;
            for (int i_st = data_stride-1; i_st >= 0; i_st--) {
                resp_data = resp_data << byte_width;
                resp_data += static_cast<uint64_t>(fill_data[i_of + i_st]);
            }

            /* calculate target prefetch address */
            Addr pf_addr = blockAddress((resp_data << rt_ent.shift) + rt_ent.target_base_addr);
            DPRINTF(HWPrefetch, 
                    "notifyFill: PC %llx, pkt_addr %llx, pkt_offset %d, pkt_data %d, pf_addr %llx\n", 
                    pc, pkt->getAddr(), data_offset, resp_data, pf_addr);

            /** get a fake pfi, generator pc is target_pc for chain-trigger */
            PrefetchInfo fake_pfi(pf_addr, rt_ent.target_pc, requestorId);
            
            /* filter repeat request */
            if (queueFilter) {
                if (alreadyInQueue(pfq, fake_pfi, priority)) {
                    /* repeat address in pfi */
                    continue;
                }
                if (alreadyInQueue(pfqMissingTranslation, fake_pfi, priority)) {
                    /* repeat address in pfi */
                    continue;
                }
            }

            /* create pkt and req for dpp, fake for later translation*/
            DeferredPacket dpp(this, fake_pfi, 0, priority);

            Tick pf_time = curTick() + clockPeriod() * latency;
            dpp.createPkt(pf_addr, blkSize, requestorId, true, pf_time);
            dpp.pkt->req->setPC(rt_ent.target_pc);
            dpp.pfInfo.setPC(rt_ent.target_pc);
            dpp.pfInfo.setAddr(pf_addr);

            /* make translation request and set PREFETCH flag*/
            RequestPtr translation_req = std::make_shared<Request>(
                pf_addr, blkSize, dpp.pkt->req->getFlags(), requestorId, 
                rt_ent.target_pc, rt_ent.cID);
            translation_req->setFlags(Request::PREFETCH);

            /* set to-be-translating request, append to pfqMissingTranslation*/
            dpp.setTranslationRequest(translation_req);
            dpp.tc = cache->system->threads[translation_req->contextId()];

            addToQueue(pfqMissingTranslation, dpp);
            statsDMP.dmp_pfIdentified++;
        }

        // try to do translation immediately
        processMissingTranslations(queueSize - pfq.size());
    }
}

void
DiffMatching::notify (const PacketPtr &pkt, const PrefetchInfo &pfi)
{
    if (pkt->req->hasPC() && pkt->req->hasContextId()) {
        for (auto& rt_ent: relationTable) { 
            if (rt_ent.index_pc == pkt->req->getPC())
            {
                rt_ent.cID = pkt->req->contextId();
            }
        }
        // DPRINTF(HWPrefetch, "notify: Request Flags %llx ContextID %d\n", pkt->req->getFlags(), pkt->req->contextId());
    }
    
    if (pfi.isCacheMiss()) {
        // Miss
        DPRINTF(HWPrefetch, "notify::CacheMiss: PC %llx, Addr %llx, PAddr %llx, VAddr %llx\n", 
                            pkt->req->hasPC() ? pkt->req->getPC() : 0x0,
                            pkt->getAddr(), 
                            pkt->req->getPaddr(), 
                            pkt->req->hasVaddr() ? pkt->req->getVaddr() : 0x0);
    } else {
        // Hit
        DPRINTF(HWPrefetch, "notify::CacheHit: PC %llx, Addr %llx, PAddr %llx, VAddr %llx\n", 
                            pkt->req->hasPC() ? pkt->req->getPC() : 0x0,
                            pkt->getAddr(), 
                            pkt->req->getPaddr(), 
                            pkt->req->hasVaddr() ? pkt->req->getVaddr() : 0x0);

        notifyFill(pkt); 
    }

    Queued::notify(pkt, pfi);
}
void
DiffMatching::calculatePrefetch(const PrefetchInfo &pfi, std::vector<AddrPriority> &addresses) 
{
    Stride::calculatePrefetch(pfi, addresses);
}

} // namespace prefetch

} // namespace gem5

