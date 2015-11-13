#include <cstring>

#include "dram_cntlr.h"
#include "core_model.h"
#include "tile.h"
#include "memory_manager.h"
#include "log.h"
#include "constants.h"

DramCntlr::DramCntlr(Tile* tile,
      float dram_access_cost,
      float dram_bandwidth,
      bool dram_queue_model_enabled,
      string dram_queue_model_type,
      UInt32 cache_line_size)
   : _tile(tile)
   , _cache_line_size(cache_line_size)
{
   _dram_perf_model = new DramPerfModel(dram_access_cost, 
                                        dram_bandwidth,
                                        dram_queue_model_enabled,
                                        dram_queue_model_type,
                                        cache_line_size);

   _dram_access_count = new AccessCountMap[NUM_ACCESS_TYPES];
}

DramCntlr::~DramCntlr()
{
   printDramAccessCount();
   delete [] _dram_access_count;

   delete _dram_perf_model;
}

void
DramCntlr::getDataFromDram(IntPtr address, Byte* data_buf, bool modeled)
{
   //LOG_PRINT_WARNING("address is 0x%x , map size is %d \n",address, _data_map.size());
   if (_data_map[address] == NULL)
   {
      _data_map[address] = new Byte[_cache_line_size];
      memset((void*) _data_map[address], 0x00, _cache_line_size);
   }
   memcpy((void*) data_buf, (void*) _data_map[address], _cache_line_size);
   UInt32* dwordptr;
    for(int i = 0; i<16; i++)
   {
      dwordptr = (UInt32 *)data_buf+i;
      LOG_PRINT_WARNING("0x%d",*dwordptr);
      if(i==15)
      LOG_PRINT_WARNING("\n");
   }
   Latency dram_access_latency = modeled ? runDramPerfModel() : Latency(0,DRAM_FREQUENCY);
   LOG_PRINT("Dram Access Latency(%llu)", dram_access_latency.getCycles());
   getShmemPerfModel()->incrCurrTime(dram_access_latency);

   addToDramAccessCount(address, READ);
}

void DramCntlr:: getDataFD(IntPtr address, Byte* data_buf)
{
//LOG_PRINT_WARNING("fetch data from dram \n");
//if(_data_map.find(address) == _data_map.end())
//LOG_PRINT_WARNING("no such item \n");
//LOG_PRINT_WARNING("address is 0x%x , map size is %d \n",address, _data_map.size());
if (_data_map[address] == NULL)
   {
      //LOG_PRINT_WARNING("null space \n");
      _data_map[address] = new Byte[_cache_line_size];
      memset((void*) _data_map[address], 0x00, _cache_line_size);
      //LOG_PRINT_WARNING("set null space \n");
   }
//LOG_PRINT_WARNING("copy data \n");
   memcpy((void*) data_buf, (void*) _data_map[address], _cache_line_size);
//LOG_PRINT_WARNING("finished fetch data from dram \n");
}

void
DramCntlr::putDataToDram(IntPtr address, Byte* data_buf, bool modeled)
{
   LOG_ASSERT_ERROR(_data_map[address] != NULL, "Data Buffer does not exist");
   
   memcpy((void*) _data_map[address], (void*) data_buf, _cache_line_size);

   __attribute__((unused)) Latency dram_access_latency = modeled ? runDramPerfModel() : Latency(0,DRAM_FREQUENCY);
   
   addToDramAccessCount(address, WRITE);
}

Latency
DramCntlr::runDramPerfModel()
{

   Time pkt_time = getShmemPerfModel()->getCurrTime();

   UInt64 pkt_size = (UInt64) _cache_line_size;

   return _dram_perf_model->getAccessLatency(pkt_time, pkt_size);
}

void
DramCntlr::addToDramAccessCount(IntPtr address, AccessType access_type)
{
   _dram_access_count[access_type][address] = _dram_access_count[access_type][address] + 1;
}

void
DramCntlr::printDramAccessCount()
{
   for (UInt32 k = 0; k < NUM_ACCESS_TYPES; k++)
   {
      for (AccessCountMap::iterator i = _dram_access_count[k].begin(); i != _dram_access_count[k].end(); i++)
      {
         if ((*i).second > 100)
         {
            LOG_PRINT("Dram Cntlr(%i), Address(0x%x), Access Count(%llu), Access Type(%s)", 
                  _tile->getId(), (*i).first, (*i).second,
                  (k == READ)? "READ" : "WRITE");
         }
      }
   }
}

ShmemPerfModel*
DramCntlr::getShmemPerfModel()
{
   return _tile->getMemoryManager()->getShmemPerfModel();
}
