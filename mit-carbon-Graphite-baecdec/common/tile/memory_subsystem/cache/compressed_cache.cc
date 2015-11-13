#include "simulator.h"
#include "compressed_cache.h"
#include "cache_set.h"
#include "cache_line_info.h"
#include "cache_replacement_policy.h"
#include "cache_hash_fn.h"
#include "mcpat_cache_interface.h"
#include "utils.h"
#include "log.h"
#include "memory_manager.h"
#include "compressed_cache_set.h"
#include "shmem_msg.h"
// Cache class
// constructors/destructors
CompressedCache::CompressedCache(string name,
             CachingProtocolType caching_protocol_type,
             CacheCategory cache_category,
             SInt32 cache_level,
             WritePolicy write_policy,
             UInt32 cache_size,
             UInt32 associativity,
             UInt32 line_size,
             UInt32 num_banks,
             CacheReplacementPolicy* replacement_policy,
             CacheHashFn* hash_fn,
             UInt32 data_access_latency,
             UInt32 tags_access_latency,
             //UInt32 compresssed_latency,
             //UInt32 decompressed_latency,
             string perf_model_type,
             bool track_miss_types,
             ShmemPerfModel* shmem_perf_model)
   : _enabled(false)
   , _name(name)
   , _cache_category(cache_category)
   , _write_policy(write_policy)
   , _cache_size(k_KILO * cache_size)
   , _associativity(associativity)
   , _line_size(line_size)
   , _num_banks(num_banks)
   , _replacement_policy(replacement_policy)
   , _hash_fn(hash_fn)
   , _track_miss_types(track_miss_types)
   , _mcpat_cache_interface(NULL)
   , _shmem_perf_model(shmem_perf_model)
{
   _num_sets = _cache_size / (_associativity * _line_size);
   _log_line_size = floorLog2(_line_size);
   _comp = new Compressor();
   crt = 0;
   crr = 0.0;

   // Instantiate cache sets 
   _sets = new CompressedCacheSet*[_num_sets];
   for (UInt32 i = 0; i < _num_sets; i++)
   {
      _sets[i] = new CompressedCacheSet(i, caching_protocol_type, cache_level, _replacement_policy, _associativity, _line_size, this);
   }

   // Initialize DVFS variables
   initializeDVFS();

   // Instantiate performance model
   _perf_model = CachePerfModel::create(perf_model_type, data_access_latency, tags_access_latency, _frequency);

   // Instantiate area/power model
   //if (Config::getSingleton()->getEnablePowerModeling() || Config::getSingleton()->getEnableAreaModeling())
   //{
   //   _mcpat_cache_interface = new McPATCacheInterface(this);
   //}

   // Initialize Cache Counters
   // Hit/miss counters
   initializeMissCounters();
   // Initialize eviction counters
   initializeEvictionCounters();
   // Tracking tag/data read/writes
   initializeTagAndDataArrayCounters();
   // Cache line state counters
   initializeCacheLineStateCounters();

}

CompressedCache::~CompressedCache()
{
   for (SInt32 i = 0; i < (SInt32) _num_sets; i++)
      delete _sets[i];
   delete [] _sets;

   delete _comp;
}

void
CompressedCache::accessCacheLine(IntPtr address, AccessType access_type, Byte* buf, UInt32 num_bytes)
{
   LOG_PRINT("accessCacheLine: Address(%#lx), AccessType(%s), Num Bytes(%u) start",
             address, (access_type == 0) ? "LOAD": "STORE", num_bytes);
   assert((buf == NULL) == (num_bytes == 0));

   CompressedCacheSet* set = getSet(address);
   IntPtr tag = getTag(address);
   UInt32 line_offset = getLineOffset(address);
   UInt32 line_index = -1;

   __attribute__((unused)) CacheLineInfo* cache_line_info = set->find(tag, &line_index);
   LOG_ASSERT_ERROR(cache_line_info, "Address(%#lx)", address);

   if (access_type == LOAD)
      set->read_line(line_index, line_offset, buf, num_bytes);
   else
      set->write_line(line_index, line_offset, buf, num_bytes, tag);

   if (_enabled)
   {
      // Update data array reads/writes
      OperationType operation_type = (access_type == LOAD) ? DATA_ARRAY_READ : DATA_ARRAY_WRITE;
      _event_counters[operation_type] ++;
   }
   LOG_PRINT("accessCacheLine: Address(%#lx), AccessType(%s), Num Bytes(%u) end",
             address, (access_type == 0) ? "LOAD": "STORE", num_bytes);
}

void
CompressedCache::insertCacheLine(IntPtr inserted_address, CacheLineInfo* inserted_cache_line_info, Byte* fill_buf,
                        int* eviction, IntPtr* evicted_address1,IntPtr* evicted_address2 ,CacheLineInfo* evicted_cache_line_info1, CacheLineInfo* evicted_cache_line_info2,Byte* writeback_buf1,Byte* writeback_buf2, /*ShmemMsg::*/PrL1ShL2MSI:: ShmemMsg* shmem_msg)
{
   //LOG_PRINT("insertCacheLine: Address(%#lx) start", inserted_address);//////
   //LOG_PRINT_WARNING("get into compressedcache insert \n");//////

   CompressedCacheSet* set = getSet(inserted_address);
   //if((_name=="L1-D")&&(_hash_fn->compute(0x909feec0) == _hash_fn->compute(inserted_address))&&(inserted_address!=0x909feec0))
   //{
   //   LOG_PRINT_WARNING("inserting address 0x%x, cstate %d, data@0x%x", inserted_address, inserted_cache_line_info->getCState(), fill_buf);
   //}

   // Write into the data array
   set->insert(inserted_cache_line_info, fill_buf,
               eviction, evicted_cache_line_info1,evicted_cache_line_info2, writeback_buf1,writeback_buf2,shmem_msg);
  
   // Evicted address
   assert(*eviction>=0&&*eviction<=2); // compressed cache most two lines
   if(*eviction == 1)
   *evicted_address1 = getAddressFromTag(evicted_cache_line_info1->getTag());
   if(*eviction == 2)
   {
    *evicted_address1 = getAddressFromTag(evicted_cache_line_info1->getTag());
    *evicted_address2 = getAddressFromTag(evicted_cache_line_info2->getTag());
   }
   //if((_name=="L1-D")&&(_hash_fn->compute(0x909feec0) == _hash_fn->compute(inserted_address))&&*eviction)
   //{
   //   LOG_PRINT_WARNING("evicting address 0x%x, cstate %d", *evicted_address, evicted_cache_line_info->getCState());
   //}
   // Update Cache Line State Counters and address set for the evicted line
   if (*eviction == 1)
   {
      // Add to evicted set for tracking miss type
      assert(*evicted_address1 != INVALID_ADDRESS);

      if (_track_miss_types)
         _evicted_address_set.insert(*evicted_address1);

      // Update exclusive/sharing counters
      updateCacheLineStateCounters(evicted_cache_line_info1->getCState(), CacheState::INVALID);
   }
   if (*eviction == 2)
   {
      // Add to evicted set for tracking miss type
      assert(*evicted_address1 != INVALID_ADDRESS && *evicted_address2 != INVALID_ADDRESS);

      if (_track_miss_types)
        {
         _evicted_address_set.insert(*evicted_address1);
         _evicted_address_set.insert(*evicted_address2);
        }
      // Update exclusive/sharing counters
      updateCacheLineStateCounters(evicted_cache_line_info1->getCState(), CacheState::INVALID);
      updateCacheLineStateCounters(evicted_cache_line_info2->getCState(), CacheState::INVALID);
   }

   // Clear the miss type tracking sets for this address
   if (_track_miss_types)
      clearMissTypeTrackingSets(inserted_address);

   // Add to fetched set for tracking miss type
   if (_track_miss_types)
      _fetched_address_set.insert(inserted_address);

   // Update exclusive/sharing counters

   //if(*eviction == 1)
   updateCacheLineStateCounters(CacheState::INVALID, inserted_cache_line_info->getCState());
   //if(*eviction == 2)
   //{
   //  updateCacheLineStateCounters(CacheState::INVALID, inserted_cache_line_info->getCState());
     //updateCacheLineStateCounters(CacheState::INVALID, inserted_cache_line_info2->getCState());
   //}

   if (_enabled)
   {
     
      if (*eviction == 1)
      {
         assert(evicted_cache_line_info1->getCState() != CacheState::INVALID);

         // Update tag/data array reads
         // Read data array only if there is an eviction
         _event_counters[TAG_ARRAY_READ] ++;
         _event_counters[DATA_ARRAY_READ] ++;
      
         // Increment number of evictions and dirty evictions
         _total_evictions ++;
         // Update number of dirty evictions
         if ( (_write_policy == WRITE_BACK) && (CacheState(evicted_cache_line_info1->getCState()).dirty()) )
            _total_dirty_evictions ++;
      }
      else if(*eviction == 2)
      { 
           assert(evicted_cache_line_info1->getCState() != CacheState::INVALID && evicted_cache_line_info2->getCState() != CacheState::INVALID);
           _event_counters[TAG_ARRAY_READ] +=2;
           _event_counters[DATA_ARRAY_READ] +=2;
      
         // Increment number of evictions and dirty evictions
           _total_evictions +=2;
         // Update number of dirty evictions
         if ( (_write_policy == WRITE_BACK) && (CacheState(evicted_cache_line_info1->getCState()).dirty()) )
            _total_dirty_evictions ++;
         if ( (_write_policy == WRITE_BACK) && (CacheState(evicted_cache_line_info2->getCState()).dirty()) )
            _total_dirty_evictions ++;
       
      }
      else // (! (*eviction))
      {
         assert(evicted_cache_line_info1->getCState() == CacheState::INVALID);
         
         // Update tag array reads
         _event_counters[TAG_ARRAY_READ] ++;
      }

      // Update tag/data array writes
      if(*eviction == 1)
      {
      _event_counters[TAG_ARRAY_WRITE] ++;
      _event_counters[DATA_ARRAY_WRITE] ++;
      }
       if(*eviction == 2)
      {
      _event_counters[TAG_ARRAY_WRITE] += 2;
      _event_counters[DATA_ARRAY_WRITE] += 2;
      }
   }
   
   //LOG_PRINT("insertCacheLine: Address(%#lx) end", inserted_address);
}

// Single line cache access at address
void
CompressedCache::getCacheLineInfo(IntPtr address, CacheLineInfo* cache_line_info )
{
   LOG_PRINT("getCacheLineInfo: Address(%#lx) start", address);

   CacheLineInfo* line_info = getCacheLineInfo(address);

   // Assign it to the second argument in the function (copies it over) 
   if (line_info)
      cache_line_info->assign(line_info);

   if (_enabled)
   {
      // Update tag/data array reads/writes
      _event_counters[TAG_ARRAY_READ] ++;
   }

   LOG_PRINT("getCacheLineInfo: Address(%#lx) end", address);
}

CacheLineInfo*
CompressedCache::getCacheLineInfo(IntPtr address)
{
   CompressedCacheSet* set = getSet(address);
   IntPtr tag = getTag(address);

   CacheLineInfo* line_info = set->find(tag);

   return line_info;
}

void
CompressedCache::setCacheLineInfo(IntPtr address, CacheLineInfo* updated_cache_line_info)
{
   LOG_PRINT("setCacheLineInfo: Address(%#lx) start", address);
   CacheLineInfo* cache_line_info = getCacheLineInfo(address);
   LOG_ASSERT_ERROR(cache_line_info, "Address(%#lx)", address);

   // Update exclusive/shared counters
   updateCacheLineStateCounters(cache_line_info->getCState(), updated_cache_line_info->getCState());
  
   // Update _invalidated_address_set
   if ( (updated_cache_line_info->getCState() == CacheState::INVALID) && (_track_miss_types) )
      _invalidated_address_set.insert(address);

   // Update the cache line info   
   cache_line_info->assign(updated_cache_line_info);
   
   if (_enabled)
   {
      // Update tag/data array reads/writes
      _event_counters[TAG_ARRAY_WRITE] ++;
   }
   LOG_PRINT("setCacheLineInfo: Address(%#lx) end", address);
}

void
CompressedCache::initializeMissCounters()
{
   _total_cache_accesses = 0;
   _total_cache_misses = 0;
   _total_read_accesses = 0;
   _total_read_misses = 0;
   _total_write_accesses = 0;
   _total_write_misses = 0;

   if (_track_miss_types)
      initializeMissTypeCounters();
}

void
CompressedCache::initializeMissTypeCounters()
{
   _total_cold_misses = 0;
   _total_capacity_misses = 0;
   _total_sharing_misses = 0;
}

void
CompressedCache::initializeEvictionCounters()
{
   _total_evictions = 0;
   _total_dirty_evictions = 0;   
}

void
CompressedCache::initializeTagAndDataArrayCounters()
{
   for (UInt32 i = 0; i < NUM_OPERATION_TYPES; i++)
      _event_counters[i] = 0;
}

void
CompressedCache::initializeCacheLineStateCounters()
{
   _cache_line_state_counters.resize(CacheState::NUM_STATES, 0);
}

void
CompressedCache::initializeDVFS()
{
   // Initialize asynchronous boundaries
   if (_name == "L1-I"){
      _module = L1_ICACHE;
      _asynchronous_map[CORE] = Time(0);
      _asynchronous_map[L2_CACHE] = Time(0);
      if (MemoryManager::getCachingProtocolType() == PR_L1_SH_L2_MSI){
         _asynchronous_map[NETWORK_MEMORY] = Time(0);
      }
   }
   else if (_name == "L1-D"){
      _module = L1_DCACHE;
      _asynchronous_map[CORE] = Time(0);
      _asynchronous_map[L2_CACHE] = Time(0);
      if (MemoryManager::getCachingProtocolType() == PR_L1_SH_L2_MSI){
         _asynchronous_map[NETWORK_MEMORY] = Time(0);
      }
   }
   else if (_name == "L2"){
      _module = L2_CACHE;
      _asynchronous_map[L1_ICACHE] = Time(0);
      _asynchronous_map[L1_DCACHE] = Time(0);
      _asynchronous_map[NETWORK_MEMORY] = Time(0);
      if (MemoryManager::getCachingProtocolType() != PR_L1_SH_L2_MSI){
         _asynchronous_map[DIRECTORY] = Time(0);
      }
   }

   // Initialize frequency and voltage
   int rc = DVFSManager::getInitialFrequencyAndVoltage(_module, _frequency, _voltage);
   LOG_ASSERT_ERROR(rc == 0, "Error setting initial voltage for frequency(%g)", _frequency);
}


CompressedCache::MissType
CompressedCache::updateMissCounters(IntPtr address, Core::mem_op_t mem_op_type, bool cache_miss)
{
   MissType miss_type = INVALID_MISS_TYPE;
   
   if (_enabled)
   {
      _total_cache_accesses ++;

      // Read/Write access
      if ((mem_op_type == Core::READ) || (mem_op_type == Core::READ_EX))
      {
         _total_read_accesses ++;
      }
      else // (mem_op_type == Core::WRITE)
      {
         assert(_cache_category != INSTRUCTION_CACHE);
         _total_write_accesses ++;
      }

      if (cache_miss)
      {
         _total_cache_misses ++;
         
         // Read/Write miss
         if ((mem_op_type == Core::READ) || (mem_op_type == Core::READ_EX))
            _total_read_misses ++;
         else // (mem_op_type == Core::WRITE)
            _total_write_misses ++;
         
         // Compute the miss type counters for the inserted line
         if (_track_miss_types)
         {
            miss_type = getMissType(address);
            updateMissTypeCounters(address, miss_type);
         }
      }
   }

   return miss_type;
}

CompressedCache::MissType
CompressedCache::getMissType(IntPtr address) const
{
   // We maintain three address sets to keep track of miss types
   if (_evicted_address_set.find(address) != _evicted_address_set.end())
      return CAPACITY_MISS;
   else if (_invalidated_address_set.find(address) != _invalidated_address_set.end())
      return SHARING_MISS;
   else if (_fetched_address_set.find(address) != _fetched_address_set.end())
      return SHARING_MISS;
   else
      return COLD_MISS;
}

void
CompressedCache::updateMissTypeCounters(IntPtr address, MissType miss_type)
{
   assert(_enabled);
   switch (miss_type)
   {
   case COLD_MISS:
      _total_cold_misses ++;
      break;
   case CAPACITY_MISS:
      _total_capacity_misses ++;
      break;
   case SHARING_MISS:
      _total_sharing_misses ++;
      break;
   default:
      LOG_PRINT_ERROR("Unrecognized Cache Miss Type(%i)", miss_type);
      break;
   }
}

void
CompressedCache::clearMissTypeTrackingSets(IntPtr address)
{
   if (_evicted_address_set.erase(address));
   else if (_invalidated_address_set.erase(address));
   else if (_fetched_address_set.erase(address));
}

void
CompressedCache::updateCacheLineStateCounters(CacheState::Type old_cstate, CacheState::Type new_cstate)
{
   _cache_line_state_counters[old_cstate] --;
   _cache_line_state_counters[new_cstate] ++;
}

void
CompressedCache::getCacheLineStateCounters(vector<UInt64>& cache_line_state_counters) const
{
   cache_line_state_counters = _cache_line_state_counters;
}

void
CompressedCache::outputSummary(ostream& out, const Time& target_completion_time)
{
   // Cache Miss Summary
   out << "  Cache " << _name << ": "<< endl;
   out << "    Cache Accesses: " << _total_cache_accesses << endl;
   out << "    Cache Misses: " << _total_cache_misses << endl;
   if (_total_cache_accesses > 0)
      out << "    Miss Rate (%): " << 100.0 * _total_cache_misses / _total_cache_accesses << endl;
   else
      out << "    Miss Rate (%): " << endl;
   
   if (_cache_category != INSTRUCTION_CACHE)
   {
      out << "      Read Accesses: " << _total_read_accesses << endl;
      out << "      Read Misses: " << _total_read_misses << endl;
      if (_total_read_accesses > 0)
         out << "      Read Miss Rate (%): " << 100.0 * _total_read_misses / _total_read_accesses << endl;
      else
         out << "      Read Miss Rate (%): " << endl;
      
      out << "      Write Accesses: " << _total_write_accesses << endl;
      out << "      Write Misses: " << _total_write_misses << endl;
      if (_total_write_accesses > 0)
         out << "      Write Miss Rate (%): " << 100.0 * _total_write_misses / _total_write_accesses << endl;
      else
         out << "    Write Miss Rate (%): " << endl;
   }

   // Evictions
   out << "    Evictions: " << _total_evictions << endl;
   if (_write_policy == WRITE_BACK)
   {
      out << "    Dirty Evictions: " << _total_dirty_evictions << endl;
   }
   
   // Output Power and Area Summaries
   //if (Config::getSingleton()->getEnablePowerModeling() || Config::getSingleton()->getEnableAreaModeling())
   //   _mcpat_cache_interface->outputSummary(out, target_completion_time, _frequency);

   // Track miss types
   if (_track_miss_types)
   {
      out << "    Miss Types:" << endl;
      out << "      Cold Misses: " << _total_cold_misses << endl;
      out << "      Capacity Misses: " << _total_capacity_misses << endl;
      out << "      Sharing Misses: " << _total_sharing_misses << endl;
   }

   // Cache Access Counters Summary
   out << "    Event Counters:" << endl;
   out << "      Tag Array Reads: " << _event_counters[TAG_ARRAY_READ] << endl;
   out << "      Tag Array Writes: " << _event_counters[TAG_ARRAY_WRITE] << endl;
   out << "      Data Array Reads: " << _event_counters[DATA_ARRAY_READ] << endl;
   out << "      Data Array Writes: " << _event_counters[DATA_ARRAY_WRITE] << endl;
   out << "      average compress ratio:  " << crr/crt <<endl;
//   //Cache Compression Summary
//   map<UInt32, UInt32>::iterator i = _dictionary.begin();
//   map<UInt32, UInt32>::iterator end = _dictionary.end();
//   for(;i!=end;i++)
//   {
//      if(i->second>=100)
//      LOG_PRINT_DATA("0x%0x\t\t%d\n", i->first, i->second);
//   }

   // Asynchronous communication
   DVFSManager::printAsynchronousMap(out, _module, _asynchronous_map);


}

void CompressedCache::computeEnergy(const Time& curr_time)
{
   //_mcpat_cache_interface->computeEnergy(curr_time, _frequency);
}

double CompressedCache::getDynamicEnergy()
{
   //return _mcpat_cache_interface->getDynamicEnergy();
   return 0.0;
}

double CompressedCache::getLeakageEnergy()
{
   //return _mcpat_cache_interface->getLeakageEnergy();
   return 0.0;
}

// Utilities
IntPtr
CompressedCache::getTag(IntPtr address) const
{
   return (address >> _log_line_size);
}

CompressedCacheSet*
CompressedCache::getSet(IntPtr address) const
{
   UInt32 set_num = _hash_fn->compute(address);
   return _sets[set_num];
}

UInt32
CompressedCache::getLineOffset(IntPtr address) const
{
   return (address & (_line_size-1));
}

IntPtr
CompressedCache::getAddressFromTag(IntPtr tag) const
{
   return tag << _log_line_size;
}

CompressedCache::MissType
CompressedCache::parseMissType(string miss_type)
{
   if (miss_type == "cold")
      return COLD_MISS;
   else if (miss_type == "capacity")
      return CAPACITY_MISS;
   else if (miss_type == "sharing")
      return SHARING_MISS;
   else
   {
      LOG_PRINT_ERROR("Unrecognized Miss Type(%s)", miss_type.c_str());
      return INVALID_MISS_TYPE;
   }
}

int
CompressedCache::getDVFS(double &frequency, double &voltage)
{
   frequency = _frequency;
   voltage = _voltage;
   return 0;
}

int
CompressedCache::setDVFS(double frequency, voltage_option_t voltage_flag, const Time& curr_time)
{
   int rc = DVFSManager::getVoltage(_voltage, voltage_flag, frequency);
   if (rc==0)
   {
      _perf_model->setDVFS(frequency);
      //if (Config::getSingleton()->getEnablePowerModeling())
      //   _mcpat_cache_interface->setDVFS(_frequency, _voltage, frequency, curr_time);
      _frequency = frequency;
   }
   return rc;
}

Time
CompressedCache::getSynchronizationDelay(module_t module)
{
   if (!DVFSManager::hasSameDVFSDomain(_module, module) && _enabled){
      _asynchronous_map[module] += _perf_model->getSynchronizationDelay();
      return _perf_model->getSynchronizationDelay();
;
   }
   return Time(0);
}

/*
 * sample the cache and calculate the CR and dictionary
 */
double
CompressedCache::calcCompRatio()
{
   // cr = SUM(cr_per_set) / set_num
   double sum_cr = 0.0;
   double set_cr = 0.0;
   double cr = 1.0;
   UInt32 valid_sets=0;
   UInt32 valid_blks=0;
   UInt32 total_blks=0;
   UInt32 i;

   if(_cache_category == COMPRESSED_CACHE)
   {
      for(i = 0; i < _num_sets; i++)
      {
         set_cr = _sets[i]->get_comp_ratio(valid_blks);
         if(set_cr!=0)
         {
            sum_cr += set_cr;
            valid_sets++;
         }
         total_blks+=valid_blks;
      }
      if(valid_sets!=0)
         cr = sum_cr/valid_sets;
      else
         cr = -1;
      LOG_PRINT_WARNING("CR: %.4f, %d blks.", cr, total_blks);
   }
   crt++; 
   crr+=cr;
   return cr;
}

//implements c-pack algorithm
UInt32
CompressedCache::compress(UInt32 dword)
{
    //LOG_PRINT_WARNING("start to compress \n");
  // return _comp->compress_fpc(dword);
    return _comp->compress(dword);
    //LOG_PRINT_WARNING("finish to compress \n"); 
}

void
CompressedCache::decompress(UInt32 dword)
{
   //_comp->decompress_fpc(dword);
   _comp->decompress(dword);
}
