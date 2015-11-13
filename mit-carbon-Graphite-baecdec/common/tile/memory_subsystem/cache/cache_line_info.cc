#include "cache_line_info.h"
#include "pr_l1_pr_l2_dram_directory_msi/cache_line_info.h"
#include "pr_l1_pr_l2_dram_directory_mosi/cache_line_info.h"
#include "pr_l1_sh_l2_msi/cache_line_info.h"
#include "log.h"

CacheLineInfo::CacheLineInfo(IntPtr tag, CacheState::Type cstate)
   : _tag(tag)
   , _cstate(cstate)
   , _com_size()
   , _start_address()
   , _is_com(false)
{
  _com_size = 0; // Here fixed the block size  uncompressed means 0 compressed mean 1~64
  _start_address = 0x00; // not used
  _is_com = false ;
}

CacheLineInfo::~CacheLineInfo()
{}

CacheLineInfo*
CacheLineInfo::create(CachingProtocolType caching_protocol_type, SInt32 cache_level)
{
   switch (caching_protocol_type)
   {
   case PR_L1_PR_L2_DRAM_DIRECTORY_MSI:
      return PrL1PrL2DramDirectoryMSI::createCacheLineInfo(cache_level);

   case PR_L1_PR_L2_DRAM_DIRECTORY_MOSI:
      return PrL1PrL2DramDirectoryMOSI::createCacheLineInfo(cache_level);

   case PR_L1_SH_L2_MSI:
      return PrL1ShL2MSI::createCacheLineInfo(cache_level);

   default:
      LOG_PRINT_ERROR("Unrecognized caching protocol type(%u)", caching_protocol_type);
      return NULL;
   }
}

void
CacheLineInfo::invalidate()
{
   _tag = ~0;
   _cstate = CacheState::INVALID;
   // remain _com_size _start_address _is_com
}

void
CacheLineInfo::assign(CacheLineInfo* cache_line_info)
{
   _tag = cache_line_info->getTag();
   _cstate = cache_line_info->getCState();
   // compressed cache used

   _com_size = cache_line_info->getcomsize();
   _start_address = cache_line_info->getstartaddress();
   _is_com = cache_line_info->getcom();
}
