#include "l2_cache_replacement_policy.h"
#include "utils.h"
#include "config.h"
#include "cache_line_info.h"  // upper directory cache_line_info.h
#include "log.h"
#include <vector>
//#include "shmem_msg.h"
namespace PrL1ShL2MSI
{

L2CacheReplacementPolicy::L2CacheReplacementPolicy(UInt32 cache_size, UInt32 associativity, UInt32 cache_line_size,
                                                   HashMapList<IntPtr,ShmemReq*>& L2_cache_req_list/*,DramCntlr* dmcontrol,CompressedCache* compressed_cache_ptr*/)
   : CacheReplacementPolicy(cache_size, associativity, cache_line_size)
   , _L2_cache_req_list(L2_cache_req_list)
   //, _dc(dmcontrol)
   //, _compressed_cache_ptr(compressed_cache_ptr)
{
   _log_cache_line_size = floorLog2(cache_line_size);
   // compressed cache lru initialized
   _lru_bits_vec.resize(_num_sets);
   for (UInt32 set_num = 0; set_num < _num_sets; set_num ++)
   {
      vector<UInt8>& lru_bits = _lru_bits_vec[set_num];
      lru_bits.resize(_associativity);
      for (UInt32 way_num = 0; way_num < _associativity; way_num ++)
      {
         lru_bits[way_num] = way_num;
      }
   }
   // each associativity is a cache_info_block
}

L2CacheReplacementPolicy::~L2CacheReplacementPolicy()
{}

vector<UInt32> L2CacheReplacementPolicy::getReplacementWay(CacheLineInfo** cache_line_info_array, UInt32 set_num ,/*ShmemMsg::*/ShmemMsg* shmem_msg)
{
   //LOG_PRINT_WARNING("get into replacement processure \n"); //////
   vector<UInt32> s;
   //UInt32 min=8; // fixed size
   IntPtr address =  shmem_msg->getAddress();/*getAddressFromTag(L2_cache_line_info->getTag())*/;  
   Byte* data = new Byte[64];
   //Byte data[64];
   //UInt32* dword;
   if(shmem_msg->getType()==ShmemMsg::EX_REQ || shmem_msg->getType()==ShmemMsg::SH_REQ)
   { 
       //LOG_PRINT_WARNING("fetch_data \n");//////
       _dc->getDataFD(address,data);
       //LOG_PRINT_WARNING("fetch_data finished \n");//////
   }
   assert(shmem_msg->getType()==ShmemMsg::EX_REQ || shmem_msg->getType()==ShmemMsg::SH_REQ);
   if(shmem_msg->getType()!=ShmemMsg::EX_REQ && shmem_msg->getType()!=ShmemMsg::SH_REQ)
   LOG_PRINT_WARNING("wrong mem access type \n");//////
   //else //  (shmem_msg->getType()==EX_REQ || shmem_msg->getType()==SH_REQ)       INV_REP,FLUSH_REP,WB_REP,      //l1
   //{  
   //  data = shmem_msg->getDataBuf();
   //}
   UInt32 csize = compress_set(data);  //compress a line
   
   if(csize<=0 || csize>64)
   LOG_PRINT_WARNING("compress error \n");//////
   //SInt32 min_num_sharers = (SInt32) Config::getSingleton()->getTotalTiles() + 1; // 4 tiles? 5 sharers
   UInt32 lru_last = -1;//_associativity + 1 ; // _associativity + 1 9
   UInt32 lru_second = -1;
   UInt32 way = UINT32_MAX_;
   UInt32 way_second = UINT32_MAX_;
   UInt32 way_last = UINT32_MAX_;
   UInt32 max_size = 0;
   UInt32 max_size_way ;
   UInt32 last_size;
   UInt32 rs; // remain_space;
   vector<UInt8>& lru_bits = _lru_bits_vec[set_num];
   UInt32 current_wsize = 0;
    for (UInt32 i = 0; i < _associativity; i++)
   {
      ShL2CacheLineInfo* L2_cache_line_info = dynamic_cast<ShL2CacheLineInfo*>(cache_line_info_array[i]);
      if (L2_cache_line_info->getCState() != CacheState::INVALID)
      {
        if(/*L2_cach_line_info->getcom()*/L2_cache_line_info->getcomsize()!=0)
        current_wsize+= L2_cache_line_info->getcomsize(); // 
        else
        current_wsize+= 64; // fixed the block size
      }
   }
   //LOG_PRINT_WARNING("finished computing while size \n");//////
   assert(current_wsize <= _associativity * 64/2);
   if(current_wsize > _associativity * 64/2 )
   {
      LOG_PRINT_WARNING("SPILLOVER CACHE PHYSICAL SIZE \n");
      current_wsize = _associativity * 64/2;
   }
   rs = _associativity * 64/2 - current_wsize;
   // Here we get the current wsize
   LOG_PRINT_WARNING("wsize is %d  csize is %d \n",current_wsize,csize);
   if(csize+current_wsize<=_associativity * 64/2)  // enough space
   { 
   for (UInt32 i = 0; i < _associativity; i++)
   {
      ShL2CacheLineInfo* L2_cache_line_info = dynamic_cast<ShL2CacheLineInfo*>(cache_line_info_array[i]);

      if (L2_cache_line_info->getCState() == CacheState::INVALID)
      {
         way = i;
         s.push_back(way);
         return s;
      }
   }
   if (way == UINT32_MAX_) // no invalid block
    {
       for (UInt32 i = 0; i < _associativity; i++)
   {
      ShL2CacheLineInfo* L2_cache_line_info = dynamic_cast<ShL2CacheLineInfo*>(cache_line_info_array[i]);

      if (L2_cache_line_info->getCState() != CacheState::INVALID && lru_bits[i]>lru_last)
      {
         way_last = i;
         lru_last = lru_bits[i];
         
      }
   }
      s.push_back(way_last);
         return s;
    }  // no invalid block
}
  else // not enough space 
{
for (UInt32 i = 0; i < _associativity; i++)
   {
      ShL2CacheLineInfo* L2_cache_line_info = dynamic_cast<ShL2CacheLineInfo*>(cache_line_info_array[i]);

      if (L2_cache_line_info->getCState() != CacheState::INVALID && lru_bits[i]>lru_last)
      {
         way_last = i;
         lru_last = lru_bits[i];
         
      }
   }
ShL2CacheLineInfo* L2_cache_line_info = dynamic_cast<ShL2CacheLineInfo*>(cache_line_info_array[way_last]);
last_size = L2_cache_line_info->getcomsize();
if(L2_cache_line_info->getcomsize()+rs>=csize)
{
  s.push_back(way_last);
  return s;  
}
else
{
  s.push_back(way_last);
for (UInt32 i = 0; i < _associativity; i++)
   {
      ShL2CacheLineInfo* L2_cache_line_info = dynamic_cast<ShL2CacheLineInfo*>(cache_line_info_array[i]);

      if (L2_cache_line_info->getCState() != CacheState::INVALID  && i!=way_last && L2_cache_line_info->getcomsize()+last_size+rs>=csize)
      {
         if(lru_bits[i]>lru_second)
         way_second= i;
         lru_second=lru_bits[i];
         //lru_last = lru_bits[i];
         
      }
   }
if(way_second!= UINT32_MAX_)
{
s.push_back(way_second);
return s;
}
else
{
assert(false);
LOG_PRINT_WARNING("No actual physical space \n");
for (UInt32 i = 0; i < _associativity; i++)
   {
      ShL2CacheLineInfo* L2_cache_line_info = dynamic_cast<ShL2CacheLineInfo*>(cache_line_info_array[i]);

      if (L2_cache_line_info->getCState() != CacheState::INVALID  && i!=way_last )
      {
         if(L2_cache_line_info->getcomsize()>max_size)
         max_size = L2_cache_line_info->getcomsize();
         max_size_way = i;
         //lru_last = lru_bits[i];
         
      }
   }
assert(max_size_way>=0&&max_size_way<=_associativity-1);
if(max_size_way<0||max_size_way>_associativity-1||max_size_way == way_last)
LOG_PRINT_WARNING("wrong max_size_way! \n");

s.push_back(max_size_way);
return s;
}

}
}  // not enough space 


//////////////////////////////////////////////////////////////////////////////////////
    /*else
      {
         

         DirectoryEntry* directory_entry = L2_cache_line_info->getDirectoryEntry();
         if (directory_entry->getNumSharers() < min_num_sharers && 
             _L2_cache_req_list.empty(address))
         {
            min_num_sharers = directory_entry->getNumSharers();
            way = i;
         }
      }*/
/////////////////////////////////////////////////////////////////////////////////////
  //vector<UInt8>& lru_bits = _lru_bits_vec[set_num]; 
 /* for (UInt32 i = 0; i < _associativity; i++)
  {
      ShL2CacheLineInfo* L2_cache_line_info = dynamic_cast<ShL2CacheLineInfo*>(cache_line_info_array[i]);
      if(lru_bits[i]<min&&L2_cache_line_info->getCState() != CacheState::INVALID)
       min = lru_bits[i];
    }
   s.push_back(min);
   return s;
   }
else
{
   vector<UInt8>& lru_bits = _lru_bits_vec[set_num];
  for (UInt32 i = 0; i < _associativity; i++)
  {

  }


}*/
   /*if (way == UINT32_MAX_)
   {
      for (UInt32 i = 0; i < _associativity; i++)
      {
         ShL2CacheLineInfo* L2_cache_line_info = dynamic_cast<ShL2CacheLineInfo*>(cache_line_info_array[i]);
         assert(L2_cache_line_info->getCState() != CacheState::INVALID);
         IntPtr address = getAddressFromTag(L2_cache_line_info->getTag());
         DirectoryEntry* directory_entry = L2_cache_line_info->getDirectoryEntry();
         assert(_L2_cache_req_list.count(address) > 0);
         fprintf(stderr, "i(%u), Address(%#lx), CState(%u), DState(%u), Num Waiters(%u)\n",
                 i, address, L2_cache_line_info->getCState(),
                 directory_entry->getDirectoryBlockInfo()->getDState(),
                 (UInt32) _L2_cache_req_list.count(address));
      }
   }*/
   //LOG_ASSERT_ERROR(way != UINT32_MAX_, "Could not find a replacement candidate");
////////////////////////////////////////////////////////////////////////////////////////
   //return way;
   return s;
}

void
L2CacheReplacementPolicy::update(CacheLineInfo** cache_line_info_array, UInt32 set_num, vector<UInt32> accessed_way)
{
vector<UInt8>& lru_bits = _lru_bits_vec[set_num];
   for (UInt32 i = 0; i < _associativity; i++)
   {
      if (lru_bits[i] < lru_bits[accessed_way[0]])
         lru_bits[i] ++;
   }
   lru_bits[accessed_way[0]] = 0;  // changing involving invalid way
}

IntPtr
L2CacheReplacementPolicy::getAddressFromTag(IntPtr tag) const
{
   return tag << _log_cache_line_size;
}
UInt32 L2CacheReplacementPolicy::compress_set( Byte *line_buf)
{
//assert(line_size==64);
   UInt32 *dwordptr=NULL;
   //double compressed_bits=0;
   UInt32 compressed_bits = 0;
//   double cr =1;
   UInt32 bits;
//   //UInt32 comp_size;
//   LOG_PRINT_DATA("DATA @%d/%d 0x%llx", set_num, set_index, tag);
   //LOG_PRINT_WARNING("begin compressed \n");
   for(int i = 0; i<16; i++)
   {
      dwordptr = (UInt32 *)line_buf+i;
      bits = _compressed_cache_ptr->compress(*dwordptr);
//      LOG_PRINT_DATA("%08x %d",*dwordptr, bits);
      compressed_bits+= bits;
   }
   //LOG_PRINT_WARNING("finished compressed \n");
//   cr = compressed_bits/512;

//   LOG_PRINT_DATA("CR: %f\n", cr);
   compressed_bits = compressed_bits/8;
   if(compressed_bits == 0)
   {compressed_bits = 1;}
   if(compressed_bits > 64)
   {compressed_bits = 64;}
   //return compressed_bits/8; compressed_bits [1,64]
   return compressed_bits;

}

UInt32 L2CacheReplacementPolicy:: getReplacementWay(CacheLineInfo** cache_line_info_array, UInt32 set_num)
{ UInt32 way = 0; return way;}
void L2CacheReplacementPolicy:: update(CacheLineInfo** cache_line_info_array, UInt32 set_num, UInt32 accessed_way) 
{}


void L2CacheReplacementPolicy:: setdc(DramCntlr* dramcontrol)
{
_dc = dramcontrol;
}
void L2CacheReplacementPolicy:: setccp(CompressedCache* compressed_cache_ptr)
{
_compressed_cache_ptr = compressed_cache_ptr;
}
//DramCntlr* _dc ;  // used to trace dram data while computing compress ratio
//CompressedCache* _compressed_cache_ptr;
}

