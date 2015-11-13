#pragma once

#include "../cache/cache_replacement_policy.h"
#include "hash_map_list.h"
#include "shmem_req.h"
#include "shmem_msg.h"
#include <vector>
#include "dram_cntlr.h"
#include "../cache/compressed_cache.h"
#include "../dram_cntlr.h"
namespace PrL1ShL2MSI
{

class L2CacheReplacementPolicy : public CacheReplacementPolicy
{
public:
   L2CacheReplacementPolicy(UInt32 cache_size, UInt32 associativity, UInt32 cache_line_size,
                            HashMapList<IntPtr,ShmemReq*>& L2_cache_req_list/*,DramCntlr* dmcontrol, CompressedCache* compressed_cache_ptr*/);
   ~L2CacheReplacementPolicy();

   vector<UInt32> getReplacementWay(CacheLineInfo** cache_line_info_array, UInt32 set_num,/*ShmemMsg::*/ShmemMsg* shmem_msg );
   UInt32 getReplacementWay(CacheLineInfo** cache_line_info_array, UInt32 set_num);
   void update(CacheLineInfo** cache_line_info_array, UInt32 set_num,vector<UInt32> accessed_way);
   void update(CacheLineInfo** cache_line_info_array, UInt32 set_num, UInt32 accessed_way);
   
   //void set_dc(DramCntlr* dmcontrol);
   UInt32 compress_set( Byte *line_buf);
   void setdc(DramCntlr* dramcontrol);
   void setccp(CompressedCache* compressed_cache_ptr);
   //UInt32 compress_word(UInt32 dword);
private:
   HashMapList<IntPtr,ShmemReq*>& _L2_cache_req_list;
   UInt32 _log_cache_line_size;
   
   IntPtr getAddressFromTag(IntPtr tag) const;

   vector<vector<UInt8> > _lru_bits_vec; // compressed cache lru used
   DramCntlr* _dc ;  // used to trace dram data while computing compress ratio
   CompressedCache* _compressed_cache_ptr;
};

}
