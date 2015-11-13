#pragma once

#include <string>
using std::string;

#include "fixed_types.h"
#include "time_types.h"

class CachePerfModel
{
public:
   enum AccessType
   {
      ACCESS_DATA_AND_TAGS = 0,
      ACCESS_DATA,
      ACCESS_TAGS,
      NUM_ACCESS_TYPES,
      UNCOMPRESSED,//// compressed reference
      WUNCOMPRESSED, //// week uncompressed
      COMPRESSED,//// compressed reference
   };

   enum ModelType
   {
      PARALLEL = 0,
      SEQUENTIAL,
      NUM_MODEL_TYPES
   };

public:
   CachePerfModel(UInt64 data_access_latency, UInt64 tags_access_latency, float frequency);
   virtual ~CachePerfModel();

   static CachePerfModel* create(string perf_model_type, UInt64 data_access_cycles, UInt64 tags_access_cycles, float frequency);
   static ModelType parseModelType(string model_type);

   virtual Time getLatency(AccessType access_type) = 0;

   void setDVFS(double frequency);

   Time getSynchronizationDelay(){ return _synchronization_delay; }

protected:
   UInt64 _data_access_cycles;
   UInt64 _tags_access_cycles;
   UInt64 _uncompress_cycles;  // uncompress delay
   UInt64 _wuncompress_cycles;
   Time _data_access_latency;
   Time _tags_access_latency;
   Time _synchronization_delay;
   Time _uncompress_latency;
   Time _wuncompress_latency;
};
