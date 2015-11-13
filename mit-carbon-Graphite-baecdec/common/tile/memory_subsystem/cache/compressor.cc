/*
 * CompressedCacheDictionary.cpp
 *
 *  Created on: Oct 25, 2015
 *      Author: deuso
 */

#include "compressor.h"
#include "log.h"

#include <algorithm>

#define FIFO_SIZE 16

Compressor::Compressor()
{
   // TODO Auto-generated constructor stub
   _dictionary.clear();
   //_fifo.clear();
   //_head=_tail=&_fifo.front();
   assert(_dictionary.size()==0);
}

Compressor::~Compressor()
{
   // TODO Auto-generated destructor stub
}

UInt32 Compressor::compress(UInt32 dword)  // c-pack
{
   //LOG_PRINT_WARNING("begin to compress in compressor \n");
   static UInt32 cnt = 0;
   if(cnt==1000)
   {
      cnt = 0;
      LOG_PRINT_WARNING("dictionary size: %d", _dictionary.size());  //this out put a d size
   }
   cnt++;
   //1. basic pattern
   //[00] zzzz
   //[01] xxxx
   //[10] mmmm
   //[1100] mmxx
   //[1101] zzzx
   //[1110] mmmx
   if(dword ==0)//[00]
   {
      return 2;
   }
   if((dword&~0xff) ==0)//[1101]
   {
      return 12;
   }
   map<UInt32,UInt32>::iterator i;
   if((i=_dictionary.find(dword))!=_dictionary.end())//[10]
   {
      //found,insert value into the scoreboard
      i->second++;
      return 6;
   }
   if((i=_dictionary.find(dword&~0xff))!=_dictionary.end())//[1110]
   {
      i->second++;
      return 16;
   }
   if((i=_dictionary.find(dword&~0xffff))!=_dictionary.end())//[1100]
   {
      i->second++;
      return 24;
   }
   else
   {
      assert(_dictionary.find(dword) == _dictionary.end());
      _dictionary.insert(make_pair(dword,1));
      if((dword&0xff)!=0)
      {
         if(_dictionary.find(dword&~0xff) == _dictionary.end())
            if((dword&~0xff)!=0)
               _dictionary.insert(make_pair((dword&~0xff),1));
      }
      if((dword&0xff00)!=0)
      {
         if(_dictionary.find(dword&~0xffff) == _dictionary.end())
            if((dword&~0xffff)!=0)
               _dictionary.insert(make_pair((dword&~0xffff),1));
      }

      return 34;
   }


}


UInt32 Compressor::compress_fpc(UInt32 dword)   //FPC
{
   //static UInt32 cnt = 0;
   UInt32 firsthalf = 0;
   UInt32 secondhalf = 0;
   //if(cnt==1000)
   //{
   //   cnt = 0;
   //   LOG_PRINT_WARNING("dictionary size: %d", _dictionary.size());  //this out put a d size
   //}
   //cnt++;
   //1. basic pattern
   //[00] zzzz
   //[01] xxxx
   //[10] mmmm
   //[1100] mmxx
   //[1101] zzzx
   //[1110] mmmx
   firsthalf = dword >> 16;
   secondhalf = (dword << 16)>>16;
   if(dword ==0)//[00]
   {
      return 0;
   }
   if((dword&~0xf) ==0)//[1101]
   {
      return 4;
   }
     if((dword&~0xff)==0)//[00]
   {
      return 8;
   } 
   if(((firsthalf&0xff)==(firsthalf>>8))&&((secondhalf&0xff)==(secondhalf>>8)))
   {
      return 8;
   }
   if((dword&~0xffff) ==0)//[1101]
   {
      return 16;
   }
   if((dword&~0xffff0000)==0)
   {
      return 16;
   }

   if((firsthalf&~0xff)==0 && (secondhalf&~0xff)==0)
   {
      return 16;
   }
  
   return 32;
}
UInt32 Compressor::compress_sdfpc(UInt32 dword, UInt32 threshold)   //FPC
{
   //static UInt32 cnt = 0;
   UInt32 firsthalf = 0;
   UInt32 secondhalf = 0;
   //if(cnt==1000)
   //{
   //   cnt = 0;
   //   LOG_PRINT_WARNING("dictionary size: %d", _dictionary.size());  //this out put a d size
   //}
   //cnt++;
   //1. basic pattern
   //[00] zzzz
   //[01] xxxx
   //[10] mmmm
   //[1100] mmxx
   //[1101] zzzx
   //[1110] mmmx
   firsthalf = dword >> 16;
   secondhalf = (dword << 16)>>16;
   if(dword ==0)
   {
      return 0;
   }
   if((dword&~0xf) ==0)
   {
      return 4;
   }
     if((dword&~0xff)==0)
   {
      return 8;
   } 
   if(((firsthalf&0xff)==(firsthalf>>8))&&((secondhalf&0xff)==(secondhalf>>8)))
   {
      return 8;
   }
   if((dword&~0xffff) ==0)
   {
      return 16;
   }
   if((dword&~0xffff0000)==0)
   {
      return 16;
   }

   if((firsthalf&~0xff)==0 && (secondhalf&~0xff)==0)
   {
      return 16;
   }
  
   return 32;
}
void Compressor::decompress(UInt32 dword)
{
   //1. basic pattern
   //[00] zzzz
   //[01] xxxx
   //[10] mmmm
   //[1100] mmxx
   //[1101] zzzx
   //[1110] mmmx
   if((dword ==0) || ((dword&~0xff) ==0))//[00]
   {
      return;
   }

   map<UInt32,UInt32>::iterator i;
   if((i=_dictionary.find(dword))!=_dictionary.end())//[10]
   {
      //found,insert value into the scoreboard
      i->second++;
   }
   if((i=_dictionary.find(dword&~0xff))!=_dictionary.end())//[1110]
   {
      i->second++;
   }
   if((i=_dictionary.find(dword&~0xffff))!=_dictionary.end())//[1100]
   {
      i->second++;
   }
   static UInt32 count = 0;
   count++;
   if(count==100)
   {
      count=0;
      //log 10 most used entry
      map<UInt32, UInt32>::iterator i = _dictionary.begin();
      map<UInt32, UInt32>::iterator end = _dictionary.end();
      for(;i!=end;i++)
      {
         pair_vec.push_back(make_pair(i->first, i->second));
      }
         sort(pair_vec.begin(),pair_vec.end(),mapcmp);
         int j=0;
         LOG_PRINT_DATA("10 most used entry:");
         for(vector<PAIR>::iterator curr = pair_vec.begin();curr!=pair_vec.end()&& j<10;curr++,j++)
         {
            LOG_PRINT_DATA("0x%0x\t\t%d\n", curr->first, curr->second);
         }
         LOG_PRINT_DATA("");
         pair_vec.clear();
   }

   return;
}
void Compressor::decompress_fpc(UInt32 dword)
{

}
void Compressor::decompress_sdfpc(UInt32 dword)
{

}



