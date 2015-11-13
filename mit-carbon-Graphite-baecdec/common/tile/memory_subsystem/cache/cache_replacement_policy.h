#pragma once

#include <string>
using std::string;

#include "fixed_types.h"
#include "caching_protocol_type.h"
#include "../pr_l1_sh_l2_msi/shmem_msg.h"
#include "../pr_l1_sh_l2_msi/shmem_req.h"
#include "../pr_l1_sh_l2_msi/dram_cntlr.h"
#include "compressed_cache.h"
//#include "../shmem_msg.h"
//#include "clru_replacement_policy.h"
#include<vector>
class CacheLineInfo;

class CacheReplacementPolicy
{
public:
   enum Type
   {
      ROUND_ROBIN = 0,
      LRU,
      CLRU,
      NUM_TYPES
   };

   CacheReplacementPolicy(UInt32 cache_size, UInt32 associativity, UInt32 cache_line_size);
   //virtual ~CacheReplacementPolicy();
   ~CacheReplacementPolicy();
   static CacheReplacementPolicy* create(string policy_str, UInt32 cache_size, UInt32 associativity, UInt32 cache_line_size);
   static Type parse(string policy_str);
   
   //virtual vector<UInt32> getReplacementWay(CacheLineInfo** cache_line_info_array, UInt32 set_num ,/*ShmemMsg::*/ ShmemMsg* shmem_msg ) ;  // compressed size  must be transfered into
   virtual vector<UInt32> getReplacementWay(CacheLineInfo** cache_line_info_array, UInt32 set_num ,/*ShmemMsg::*/PrL1ShL2MSI:: ShmemMsg* shmem_msg ) =0 ;
   virtual UInt32 getReplacementWay(CacheLineInfo** cache_line_info_array, UInt32 set_num)  =0;
   //virtual UInt32 getReplacementWay(CacheLineInfo** cache_line_info_array, UInt32 set_num) = 0; 
   //virtual void update(CacheLineInfo** cache_line_info_array, UInt32 set_num, vector<UInt32> accessed_way) = 0;
   virtual void update(CacheLineInfo** cache_line_info_array, UInt32 set_num, vector<UInt32> accessed_way) =0;
   virtual void update(CacheLineInfo** cache_line_info_array, UInt32 set_num, UInt32 accessed_way) =0 ;
   //virtual void update(CacheLineInfo** cache_line_info_array, UInt32 set_num, UInt32 accessed_way) = 0;
protected:
   UInt32 _num_sets;
   UInt32 _associativity;
//private:
//   DramCntlr* _dc ;  // used to trace dram data while computing compress ratio
//   CompressedCache* _compressed_cache_ptr;
};
