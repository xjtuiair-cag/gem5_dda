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
    dpp_req(nullptr)
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
    relationTable.push_back(RelationTableEntry(0x4009c8, 0x4009e0, 0x80000000 + p.stream_ahead_dist, 4, false, 0));
    relationTable.push_back(RelationTableEntry(0x4009e0, 0x4009ec, 0xc0000000, 8, true, p.indir_range));

    rt_ptr = 2;
}

DiffMatching::~DiffMatching()
{
    delete dpp_req;
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

    if (pkt->req->hasPC()) {

        if (!pkt->validData()) {
            DPRINTF(HWPrefetch, "notifyFill: PC %llx, PAddr %llx, no Data, %s\n", pkt->req->getPC(), pkt->req->getPaddr(), pkt->cmdString());
            return;
        }
        assert(pkt->getSize() <= blkSize); 
        uint8_t fill_data[blkSize];
        pkt->writeData(fill_data); 

        unsigned data_offset = pkt->req->getPaddr() & (blkSize-1);
        do {
        int64_t resp_data = (int64_t) ((uint64_t)fill_data[data_offset]
                                + (((uint64_t)fill_data[data_offset+1]) << 8)
                                + (((uint64_t)fill_data[data_offset+2]) << 16)
                                + (((uint64_t)fill_data[data_offset+3]) << 24));
        DPRINTF(HWPrefetch, "notifyFill: PC %llx, PAddr %llx, Data_offset %d, Data %llx\n", pkt->req->getPC(), pkt->req->getPaddr(), data_offset, resp_data);

        data_offset += 4;

        } while (data_offset < blkSize);

        Addr pc = pkt->req->getPC();
        for (auto rt_ent: relationTable) { 
            if (rt_ent.index_pc == pc) {
                // Assume response data is a int and always occupies 4 bytes 
                unsigned data_offset = pkt->req->getPaddr() & (blkSize-1);
                assert(data_offset % 4 == 0);

                unsigned range_end = std::min(data_offset + 4 * rt_ent.range_degree, blkSize);
                do {
                    int64_t resp_data = (int64_t) ((uint64_t)fill_data[data_offset]
                                           + (((uint64_t)fill_data[data_offset+1]) << 8)
                                           + (((uint64_t)fill_data[data_offset+2]) << 16)
                                           + (((uint64_t)fill_data[data_offset+3]) << 24));

                    Addr pf_addr = blockAddress( resp_data*rt_ent.shift + rt_ent.target_base_addr);
                    DPRINTF(HWPrefetch, "notifyFill: PC %llx, pkt_addr %llx, pkt_offset %d, pkt_data %d, pf_addr %llx\n", 
                               pc, pkt->getAddr(), data_offset, resp_data, pf_addr);

                    // if (!rt_ent.pfi) continue;

                    // rt_ent.pfi->setAddr(pf_addr);
                    // insertDMP(rt_ent);

                    if (dpp_req) {
                        Tick pf_time = curTick() + clockPeriod() * latency;
                        dpp_req->createPkt(pf_addr, blkSize, requestorId, true, pf_time);
                        dpp_req->pkt->req->setPC(rt_ent.target_pc);
                        addDMPToQueue(pfq, *dpp_req);
                    }
                
                    data_offset += 4;
                } while (rt_ent.range && data_offset < range_end);
            }
        }
    } else {
       DPRINTF(HWPrefetch, "notifyFill: no PC\n");
    }
}

void
DiffMatching::notify (const PacketPtr &pkt, const PrefetchInfo &pfi)
{
    if (!dpp_req) {
        dpp_req = new DeferredPacket(this, pfi, 0, 0);
    }
    //if (pkt->req->hasPC()) {
    //    Addr pc = pkt->req->getPC();
    //    unsigned offset = (unsigned) (pfi.getAddr() & (blkSize-1));
    //    if (access_offset.count(pc) == 0) {
    //        std::queue<unsigned> temp;
    //        temp.push(offset);
    //        access_offset[pc] = temp;
    //    } else {
    //        access_offset[pc].push(offset);
    //    }
    //    //DPRINTF(HWPrefetch, "Notify: offset register: %d\n", offset);
    //}
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

void
DiffMatching::addDMPToQueue(std::list<DeferredPacket> &queue,
                             DeferredPacket &dpp)
{
    /* Verify prefetch buffer space for request */
    if (queue.size() == queueSize) {
        statsQueued.pfRemovedFull++;
        /* Lowest priority packet */
        iterator it = queue.end();
        panic_if (it == queue.begin(),
            "Prefetch queue is both full and empty!");
        --it;
        /* Look for oldest in that level of priority */
        panic_if (it == queue.begin(),
            "Prefetch queue is full with 1 element!");
        iterator prev = it;
        bool cont = true;
        /* While not at the head of the queue */
        while (cont && prev != queue.begin()) {
            prev--;
            /* While at the same level of priority */
            cont = prev->priority == it->priority;
            if (cont)
                /* update pointer */
                it = prev;
        }
        DPRINTF(HWPrefetch, "Prefetch queue full, removing lowest priority "
                            "oldest packet, addr: %#x\n",it->pfInfo.getAddr());
        delete it->pkt;
        queue.erase(it);
    }

    if ((queue.size() == 0) || (dpp <= queue.back())) {
        queue.emplace_back(dpp);
    } else {
        iterator it = queue.end();
        do {
            --it;
        } while (it != queue.begin() && dpp > *it);
        /* If we reach the head, we have to see if the new element is new head
         * or not */
        if (it == queue.begin() && dpp <= *it)
            it++;
        queue.insert(it, dpp);
    }
}

} // namespace prefetch

} // namespace gem5

