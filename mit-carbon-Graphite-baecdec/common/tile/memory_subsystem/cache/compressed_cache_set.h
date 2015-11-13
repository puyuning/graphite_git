/*
 * 
 * Compressed Cache Set
 * 
 * Management of compressed cache blocks including compression, 
 * decompression, compaction and replacement candidate chosing.
 * 
 */

#pragma once

#include "fixed_types.h"
#include "cache_line_info.h"
#include "cache_replacement_policy.h"
#include "compressed_cache.h"
#include <vector>
#include "../pr_l1_sh_l2_msi/shmem_msg.h"
#include "../pr_l1_sh_l2_msi/shmem_req.h"
using namespace std;

// Everything related to cache sets
class CompressedCacheSet
{
public:
   CompressedCacheSet(UInt32 set_num, CachingProtocolType caching_protocol_type, SInt32 cache_level,
            CacheReplacementPolicy* replacement_policy, UInt32 associativity, UInt32 line_size, CompressedCache *compressed_cache_ptr);
   ~CompressedCacheSet();

   void read_line(UInt32 line_index, UInt32 offset, Byte *out_buf, UInt32 bytes);
   void write_line(UInt32 line_index, UInt32 offset, Byte *in_buf, UInt32 bytes, IntPtr tag);
   CacheLineInfo* find(IntPtr tag, UInt32* line_index = NULL);
   void insert(CacheLineInfo* inserted_cache_line_info, Byte* fill_buf,
               int* eviction, CacheLineInfo* evicted_cache_line_info1,CacheLineInfo* evicted_cache_line_info2, Byte* writeback_buf1,Byte* writeback_buf2,/*ShmemMsg::*/PrL1ShL2MSI:: ShmemMsg* shmem_msg);

//compressed cache attr
   double get_comp_ratio(UInt32 & valid_blks);

   enum CompType
   {
      FPC,
      CPACK,
      BDI
   };

private:
//conventional cache attr
   CacheLineInfo** _cache_line_info_array;
   char* _lines;
   UInt32 _set_num;
   CacheReplacementPolicy* _replacement_policy;
   UInt32 _associativity;
   UInt32 _line_size;
   CompType _comp_method;
   CompressedCache *_compressed_cache_ptr;
//compressed cache attr
   //char* _comp;
   vector<UInt32> _comp_size_vec;
   UInt32 compress(UInt32 line_size, Byte *line_buf, UInt32 set_num, UInt32 set_index, IntPtr tag);
   void decompress(UInt32 comp_line_size, Byte *line_buf);
   UInt32 threshold;
   //void compress(UInt32 uncomp_len, Byte *uncomp_buf, UInt32 comp_len, Byte *comp_buf);
   //void decompress(UInt32 uncomp_len, Byte *uncomp_buf, UInt32 comp_len, Byte *comp_buf);
};
