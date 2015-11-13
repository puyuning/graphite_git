/*
 * Compressor.h
 *
 *  Created on: Oct 25, 2015
 *      Author: deuso
 */

#pragma once

#include "utils.h"
#include <map>

using std::map;
class Compressor
{
public:
   Compressor();
   virtual ~Compressor();
   UInt32 compress(UInt32 dword);
   UInt32 compress_fpc(UInt32 dword);
   UInt32 compress_sdfpc(UInt32 dword , UInt32 threshold);
   void decompress(UInt32 dword);
   void decompress_fpc(UInt32 dword);
   void decompress_sdfpc(UInt32 dword);


private:
   map<UInt32, UInt32> _dictionary;
   //map<UInt32, UInt32> _score_board;
   typedef pair<UInt32, UInt32> PAIR;
   static bool mapcmp(const PAIR& x, const PAIR& y)
   {
      return x.second>y.second;
   }
   vector<PAIR> pair_vec;

   vector<PAIR> _fifo;
   PAIR *_head, *_tail;

   // while using a static dictionary we directly check the value.
};

