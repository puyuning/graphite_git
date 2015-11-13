#pragma once

#include "../cache/cache_replacement_policy.h"
#include "hash_map_list.h"
#include "shmem_req.h"
#include <vector>
#include "shmem_msg.h"
namespace PrL1ShL2MSI
{

class L2ComCacheReplacementPolicy : public CacheReplacementPolicy
{
public:
   L2ComCacheReplacementPolicy(UInt32 cache_size, UInt32 associativity, UInt32 cache_line_size,
                            HashMapList<IntPtr,ShmemReq*>& L2_cache_req_list);
   ~L2ComCacheReplacementPolicy();

   vector<UInt32> getReplacementWay(CacheLineInfo** cache_line_info_array, UInt32 set_num,/*ShememMsg::*/ShmemMsg* shmem_msg);
   void update(CacheLineInfo** cache_line_info_array, UInt32 set_num, vector<UInt32> accessed_way);

private:
   HashMapList<IntPtr,ShmemReq*>& _L2_cache_req_list;
   UInt32 _log_cache_line_size;
   
   IntPtr getAddressFromTag(IntPtr tag) const;
};

}
