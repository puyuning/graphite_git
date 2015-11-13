#include "cache_replacement_policy.h"
#include "round_robin_replacement_policy.h"
#include "lru_replacement_policy.h"
#include "clru_replacement_policy.h"
#include "cache_line_info.h"
#include "log.h"
#include "vector"
CacheReplacementPolicy::CacheReplacementPolicy(UInt32 cache_size, UInt32 associativity, UInt32 cache_line_size)
   : _associativity(associativity)
{
   _num_sets = cache_size * k_KILO / (cache_line_size * _associativity);
}

CacheReplacementPolicy::~CacheReplacementPolicy()
{}

CacheReplacementPolicy*
CacheReplacementPolicy::create(string policy_str, UInt32 cache_size, UInt32 associativity, UInt32 cache_line_size)
{
   Type policy = parse(policy_str);

   switch (policy)
   {
   case ROUND_ROBIN:
      return new RoundRobinReplacementPolicy(cache_size, associativity, cache_line_size);
   case LRU:
      return new LRUReplacementPolicy(cache_size, associativity, cache_line_size);
   case CLRU:
      return new CLRUReplacementPolicy(cache_size, associativity ,cache_line_size);
   default:
      LOG_PRINT_ERROR("Unrecognized Replacement Policy(%u)", policy);
      return (CacheReplacementPolicy*) NULL;
   }
}

CacheReplacementPolicy::Type
CacheReplacementPolicy::parse(string policy_str)
{
   if (policy_str == "round_robin")
      return ROUND_ROBIN;
   if (policy_str == "lru")
      return LRU;
   if (policy_str == "clru")
      return CLRU;  
   else
   {
      LOG_PRINT_ERROR("Unrecognized Cache Replacement Policy(%s)", policy_str.c_str());
      return NUM_TYPES;
   }
}
//vector<UInt32> CacheReplacementPolicy::CacheReplacementPolicy:: getReplacementWay(CacheLineInfo** cache_line_info_array, UInt32 set_num ,/*ShmemMsg::*/ ShmemMsg* shmem_msg ) {LOG_PRINT_WARNING("wrong replacementpolicy route. \n"); vector<UInt32> s; return s;}
//UInt32 CacheReplacementPolicy::getReplacementWay(CacheLineInfo** cache_line_info_array, UInt32 set_num){LOG_PRINT_WARNING("wrong replacementpolicy route. \n");UInt32 x=0; return x;}
//void CacheReplacementPolicy:: update(CacheLineInfo** cache_line_info_array, UInt32 set_num, vector<UInt32> accessed_way){LOG_PRINT_WARNING("wrong replacementpolicy route. \n");}
//void CacheReplacementPolicy::update(CacheLineInfo** cache_line_info_array, UInt32 set_num, UInt32 accessed_way){LOG_PRINT_WARNING("wrong replacementpolicy route. \n");}



