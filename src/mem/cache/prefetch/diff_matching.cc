#include "mem/cache/prefetch/diff_matching.hh"
#include "mem/cache/mshr.hh"
#include "mem/cache/base.hh"

#include "debug/HWPrefetch.hh"
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
    iddt_diff_num(p.iddt_diff_num),
    tadt_diff_num(p.tadt_diff_num),
    iddt_ptr(0),
    tadt_ptr(0),
    rt_ptr(0),
    dpp_req(nullptr),
    statsDMP(this)
{
    // temp for testing sssp prefetching generation
    // relationTable.push_back(RelationTableEntry(0x400a10, 0x400a1c, 0xa0000000, 4, false));
    // relationTable.push_back(RelationTableEntry(0x400a1c, 0x400a40, 0xb0000000, 4, true));
    // relationTable.push_back(RelationTableEntry(0x400a1c, 0x400a48, 0xe0000000, 8, true));
    // relationTable.push_back(RelationTableEntry(0x400a40, 0x400a54, 0x90000000, 4, false));
    // rt_ptr=4;

    // temp for testing spmv prefetching generation

    // relationTable.push_back(RelationTableEntry(0x400998, 0x4009b0, 0x80000000 + p.stream_ahead_dist, 4, false, 0));
    // relationTable.push_back(RelationTableEntry(0x4009b0, 0x4009bc, 0xc0000000, 8, true, p.indir_range));

    // relationTable.push_back(RelationTableEntry(0x4009c8, 0x4009e0, 0x80000000 + p.stream_ahead_dist, 4, false, 0));
    // relationTable.push_back(RelationTableEntry(0x4009e0, 0x4009ec, 0xc0000000, 8, true, p.indir_range));

    // spmv fs
    // relationTable.push_back(RelationTableEntry(0x400a10, 0x400a28, 0x80000000 + p.stream_ahead_dist, 4, false, 0, 0));
    relationTable.push_back(RelationTableEntry(0x400a28, 0x400a34, 0xc360270, 8, true, p.indir_range, 0));

    rt_ptr = 2;
}

DiffMatching::~DiffMatching()
{
    delete dpp_req;
}

DiffMatching::DMPStats::DMPStats(statistics::Group *parent)
    : statistics::Group(parent),
    ADD_STAT(dmp_pfIdentified, statistics::units::Count::get(),
             "number of DMP prefetch candidates identified")
{
}

void
DiffMatching::notifyL1Req(const PacketPtr &pkt) 
{  
    DPRINTF(HWPrefetch, "notifyL1Req: PC %llx, Addr %llx, PAddr %llx, VAddr %llx\n",
                        pkt->req->hasPC() ? pkt->req->getPC() : 0x0,
                        pkt->getAddr(), 
                        pkt->req->getPaddr(), 
                        pkt->req->hasVaddr() ? pkt->req->getVaddr() : 0x0 );
}

void
DiffMatching::notifyL1Resp(const PacketPtr &pkt) 
{
    if (pkt->req->hasPC()) {
        
        if (!pkt->validData()) {
            DPRINTF(HWPrefetch, "notifyL1Resp: PC %llx, PAddr %llx, no Data, %s\n", pkt->req->getPC(), pkt->req->getPaddr(), pkt->cmdString());
            return;
        }

        assert(pkt->getSize() <= blkSize); 
        uint8_t data[pkt->getSize()];
        pkt->writeData(data); 

        uint32_t resp_data = ((uint64_t)data[0]
                                + (((uint64_t)data[1]) << 8)
                                + (((uint64_t)data[2]) << 16)
                                + (((uint64_t)data[3]) << 24));

        DPRINTF(HWPrefetch, "notifyL1Resp: PC %llx, PAddr %llx, VAddr %llx, Size %d, Data %llx\n", 
                            pkt->req->getPC(), pkt->req->getPaddr(), 
                            pkt->req->hasVaddr() ? pkt->req->getVaddr() : 0x0,
                            pkt->getSize(), resp_data);
    } else {
       DPRINTF(HWPrefetch, "notifyL1Resp: no PC\n");
    }
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
            /* integrate fill_data[] to resp_data  */
            uint64_t u_resp_data = 0;
            for (int i_st = data_stride-1; i_st >= 0; i_st--) {
                u_resp_data = u_resp_data << byte_width;
                u_resp_data += static_cast<uint64_t>(fill_data[i_of + i_st]);
            }
            int64_t resp_data = static_cast<int64_t>(u_resp_data);

            /* calculate target prefetch address */
            Addr pf_addr = blockAddress(resp_data * rt_ent.shift + rt_ent.target_base_addr);
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

