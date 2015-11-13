#pragma once

#include <vector>
using std::vector;

#include "cache_replacement_policy.h"
#include "../pr_l1_sh_l2_msi/shmem_msg.h"
class RoundRobinReplacementPolicy : public CacheReplacementPolicy
{
public:
   RoundRobinReplacementPolicy(UInt32 cache_size, UInt32 associativity, UInt32 cache_line_size);
   ~RoundRobinReplacementPolicy();

   vector<UInt32> getReplacementWay(CacheLineInfo** cache_line_info_array, UInt32 set_num,/*ShememMsg::*/PrL1ShL2MSI:: ShmemMsg* shmem_msg);
   UInt32 getReplacementWay(CacheLineInfo** cache_line_info_array, UInt32 set_num);
   void update(CacheLineInfo** cache_line_info_array, UInt32 set_num, vector<UInt32> accessed_way);
   void update(CacheLineInfo** cache_line_info_array, UInt32 set_num, UInt32 accessed_way);
  
private: 
   vector<UInt32> _replacement_index_vec;
};
