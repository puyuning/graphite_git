#include <cstring>
#include "compressed_cache_set.h"
#include "compressed_cache.h"
#include "log.h"
#include <vector>
CompressedCacheSet::CompressedCacheSet(UInt32 set_num, CachingProtocolType caching_protocol_type, SInt32 cache_level,
                   CacheReplacementPolicy* replacement_policy, UInt32 associativity, UInt32 line_size, CompressedCache *compressed_cache_ptr)
   : _set_num(set_num)
   , _replacement_policy(replacement_policy)
   , _associativity(associativity)
   , _line_size(line_size)
   , _compressed_cache_ptr(compressed_cache_ptr)
{
      //LOG_PRINT_WARNING("%s:%d:%s", __FILE__, __LINE__, __func__);
   _cache_line_info_array = new CacheLineInfo*[_associativity];
   for (UInt32 i = 0; i < _associativity; i++)
   {
      _cache_line_info_array[i] = CacheLineInfo::create(caching_protocol_type, cache_level);
   }
   _lines = new char[_associativity * _line_size];
   
   memset(_lines, 0x00, _associativity * _line_size);

//   _comp_size_vec = vector<UInt32>(_associativity, _line_size);
   _comp_size_vec = vector<UInt32>(_associativity, 0);

   _comp_method = FPC;//the default method, TODO
   
   threshold = 4;
}

CompressedCacheSet::~CompressedCacheSet()
{
   for (UInt32 i = 0; i < _associativity; i++)
      delete _cache_line_info_array[i];
   delete [] _cache_line_info_array;
   delete [] _lines;
}

void 
CompressedCacheSet::read_line(UInt32 line_index, UInt32 offset, Byte *out_buf, UInt32 bytes)
{
      //LOG_PRINT_WARNING("%s:%d:%s", __FILE__, __LINE__, __func__);
   assert(offset + bytes <= _line_size);
   assert((out_buf == NULL) == (bytes == 0));

   if (out_buf != NULL)
      memcpy((void*) out_buf, &_lines[line_index * _line_size + offset], bytes);

   decompress(_comp_size_vec[line_index], out_buf);

   vector<UInt32> s;
   s.push_back(line_index);
   // Update replacement policy
   _replacement_policy->update(_cache_line_info_array, _set_num, s);  // rereference
}

//Compressed cache only support full line write
void 
CompressedCacheSet::write_line(UInt32 line_index, UInt32 offset, Byte *in_buf, UInt32 bytes, IntPtr tag)
{
      //LOG_PRINT_WARNING("%s:%d:%s", __FILE__, __LINE__, __func__);
   UInt32 comp_size;
   vector<UInt32> s;
   s.push_back(line_index);
   assert((offset == 0) && (in_buf != NULL) && (bytes == _line_size));

   memcpy(&_lines[line_index * _line_size + offset], (void*) in_buf, bytes);
   
   comp_size = compress(bytes, in_buf, _set_num, line_index, tag);  // bimod compress
   //if(_comp_size_vec[line_index] < comp_size)   // Here set 
   _comp_size_vec[line_index] = comp_size;
   _cache_line_info_array[line_index]->setcomsize(comp_size);
   // Update replacement policy
   //_replacement_policy->update(_cache_line_info_array, _set_num, line_index);
   _replacement_policy->update(_cache_line_info_array, _set_num, s);
}

CacheLineInfo* 
CompressedCacheSet::find(IntPtr tag, UInt32* line_index)
{
      //LOG_PRINT_WARNING("%s:%d:%s", __FILE__, __LINE__, __func__);
   for (SInt32 index = _associativity-1; index >= 0; index--)
   {
      if (_cache_line_info_array[index]->getTag() == tag)
      {
         if (line_index != NULL)
            *line_index = index;
         return (_cache_line_info_array[index]);
      }
   }
   return NULL;
}

void 
CompressedCacheSet::insert(CacheLineInfo* inserted_cache_line_info, Byte* fill_buf,
               int * eviction, CacheLineInfo* evicted_cache_line_info1,CacheLineInfo* evicted_cache_line_info2, Byte* writeback_buf1,Byte* writeback_buf2,/*ShmemMsg::*/PrL1ShL2MSI:: ShmemMsg* shmem_msg)
{
     //UInt32 index;
    vector<UInt32> s;
    //UInt32 l = s.size();
     // LOG_PRINT_WARNING("%s:%d:%s", __FILE__, __LINE__, __func__);
   // This replacement strategy does not take into account the fact that
   // cache lines can be voluntarily flushed or invalidated due to another write request
   
//   UInt32 comp_size;
   //const UInt32 = _replacement_policy->getReplacementWay(_cache_line_info_array, _set_num);
   //index=_replacement_policy->getReplacementWay(_cache_line_info_array, _set_num);  // return vector

   //LOG_PRINT_WARNING("get into compressedcache set insert \n");//////
   assert(shmem_msg!=NULL);
   if(shmem_msg == NULL)
   LOG_PRINT_WARNING("shmem_msg = 0x%x \n",shmem_msg);
   //LOG_PRINT_WARNING("set_num = %d , shmem_msg = 0x%x \n",_set_num,shmem_msg);//////

   s =_replacement_policy->getReplacementWay(_cache_line_info_array, _set_num, shmem_msg);
   //LOG_PRINT_WARNING("get out of  getreplacementway\n");///////
   UInt32 l = s.size();
  assert(l>=0&&l<=2);
  if(l<0||l>2)
  LOG_PRINT_WARNING("wrong return number \n");
   for(UInt32 i=0;i <l; i++)
  {
   assert(s[i] < _associativity); //further processing
   if(s[i] > _associativity)
   LOG_PRINT_WARNING("wrong return number \n");
   
  }
   assert(eviction != NULL);
   if(eviction == NULL)
   { 
     LOG_PRINT_WARNING("wrong eviction pointer \n");
   }
   //int ev = *eviction;  Here not used *eviction
   //while(ev)
   if(l==2)
   {
     UInt32 size = 0;
     *eviction = 2;
     evicted_cache_line_info1->assign(_cache_line_info_array[s[0]]);
     evicted_cache_line_info2->assign(_cache_line_info_array[s[1]]);
     if (writeback_buf1 != NULL)
         memcpy((void*) writeback_buf1, &_lines[s[0] * _line_size], _line_size);
     if (writeback_buf2 != NULL)
         memcpy((void*) writeback_buf2, &_lines[s[1] * _line_size], _line_size);
     _comp_size_vec[s[0]] = 0;
     _comp_size_vec[s[1]] = 0;
     //ShL2CacheLineInfo* L2_cache_line_info1 = dynamic_cast<ShL2CacheLineInfo*>(evicted_cache_line_info1);  //->setcomsize(size);
     //ShL2CacheLineInfo* L2_cache_line_info2 = dynamic_cast<ShL2CacheLineInfo*>(evicted_cache_line_info2);  //->setcomsize(size);
     evicted_cache_line_info1->setcomsize(size);
     evicted_cache_line_info2->setcomsize(size);
   }
   else if (l==1 && _cache_line_info_array[s[0]]->isValid())
   {
      UInt32 size = 0;
      *eviction = 1;
      evicted_cache_line_info1->assign(_cache_line_info_array[s[0]]);
      if (writeback_buf1 != NULL)
         memcpy((void*) writeback_buf1, &_lines[s[0] * _line_size], _line_size);
      _comp_size_vec[s[0]] = 0; //restore the line size
      //ShL2CacheLineInfo* L2_cache_line_info1 = dynamic_cast<ShL2CacheLineInfo*>(evicted_cache_line_info1);  //->setcomsize(size);
      evicted_cache_line_info1->setcomsize(size);
//      _comp_size_vec[index] = _line_size; //restore the line size
   }
   //else if (l==1 && !_cache_line_info_array[s[0]]->isValid())
   else
   {
      *eviction = 0;  ////
      // Get the line info for the purpose of getting the utilization and birth time
   }
   //LOG_PRINT_WARNING("%s, %x", __func__, fill_buf);
   _cache_line_info_array[s[0]]->assign(inserted_cache_line_info);
 /*  if (fill_buf != NULL)
   {
      assert(false);
//      memcpy(&_lines[index * _line_size], (void*) fill_buf, _line_size);
//
//      comp_size = get_comp_size(_line_size, fill_buf, _set_num, index);
//      _comp_size_vec[index] = comp_size;
   }*/

   // Update replacement policy
   //_replacement_policy->update(_cache_line_info_array, _set_num, index);

   //LOG_PRINT_WARNING("finished insert \n");//////

   _replacement_policy->update(_cache_line_info_array, _set_num, s);
}  // while

double
CompressedCacheSet::get_comp_ratio(UInt32 &valid_blks)
{
     // LOG_PRINT_WARNING("%s:%d:%s", __FILE__, __LINE__, __func__);
   vector<UInt32>::iterator iter;  
   UInt32 comp_set_size=0;
   valid_blks = 0;
   assert(_comp_size_vec.size()==_associativity);
   for (iter=_comp_size_vec.begin();iter!=_comp_size_vec.end();iter++)  
   { 
      if(*iter>0)
      {
         comp_set_size += *iter;
         valid_blks ++;
      }
   } 
   if(valid_blks==0)return 0;
   return ((double)comp_set_size)/((double)(valid_blks*_line_size));
}

/*
 * FPC
 * 000 zero run
 * 001 4b sign-ext
 * 010 1B sign-ext
 * 011 2B sign-ext
 * 100 2B zero-padding
 * 101 2HW each 1B sign-ext
 * 110 1B repeat
 * 111 uncomp
 */
UInt32
CompressedCacheSet::compress(UInt32 line_size, Byte *line_buf, UInt32 set_num, UInt32 set_index, IntPtr tag)
{
   assert(line_size==64);
   UInt32 *dwordptr=NULL;
// double compressed_bits=0;
   UInt32 compressed_bits=0;
//   double cr =1;
   UInt32 bits;
//   //UInt32 comp_size;
//   LOG_PRINT_DATA("DATA @%d/%d 0x%llx", set_num, set_index, tag);
   for(int i = 0; i<16; i++)
   {
      dwordptr = (UInt32 *)line_buf+i;
      bits = _compressed_cache_ptr->compress(*dwordptr);
//      LOG_PRINT_DATA("%08x %d",*dwordptr, bits);
      compressed_bits+= bits;
   }
//   cr = compressed_bits/512;

//   LOG_PRINT_DATA("CR: %f\n", cr);
   compressed_bits = compressed_bits/8;
   if(compressed_bits == 0)
   {compressed_bits = 1;}
   if(compressed_bits > 64)
   {compressed_bits = 64;}
   //return compressed_bits/8; compressed_bits [1,64]
   return compressed_bits;
   //return compressed_bits/8;
}


/*UInt32
CompressedCacheSet::compress(UInt32 line_size, Byte *line_buf, UInt32 set_num, UInt32 set_index, IntPtr tag)
{
   assert(line_size==64);
   UInt32 *dwordptr=NULL;
   double compressed_bits=0;
//   double cr =1;
   UInt32 bits;
//   //UInt32 comp_size;
//   LOG_PRINT_DATA("DATA @%d/%d 0x%llx", set_num, set_index, tag);
   for(int i=0; i<16; i++)
  {
     dwordptr = (UInt32*) line_buf+i;
     if((*dwordptr)==0x00||(*dwordptr)==0x01||(*dwordptr)==0x02||(*dwordptr)==0xffffffff)
     compressed_bits+=0;
     else
     compressed_bits+=32;
  }
     compressed_bits/=8;
     if(compressed_bits<=threshold)
     return compressed_bits;
     else compressed_bits = 0;

   for(int i = 0; i<16; i++)
   {
      dwordptr = (UInt32 *)line_buf+i;
      bits = _compressed_cache_ptr->compress(*dwordptr);
//      LOG_PRINT_DATA("%08x %d",*dwordptr, bits);
      compressed_bits+= bits;
   }
//   cr = compressed_bits/512;

//   LOG_PRINT_DATA("CR: %f\n", cr);
   return compressed_bits/8;
}*/

  // return set segment;

void
CompressedCacheSet::decompress(UInt32 comp_line_size, Byte *line_buf)
{
//   UInt32 *dwordptr=NULL;
//
////   UInt32 bits;
////   //UInt32 comp_size;
//   for(int i = 0; i<16; i++)
//   {
//      dwordptr = (UInt32 *)line_buf+i;
//      _compressed_cache_ptr->decompress(*dwordptr);
//   }
}
