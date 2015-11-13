#include "tile.h"
#include "core.h"
#include "mem_component.h"
#include "tile_manager.h"
#include "simulator.h"
#include "config.h"

#include "carbon_user.h"
#include "fixed_types.h"

using namespace std;

int main (int argc, char *argv[])
{
   int iii;
   iii = getpid();
   printf("pid %d\n", iii);
   scanf("%d", &iii);
   printf("Starting (shared_mem_basic)\n");
   CarbonStartSim(argc, argv);

   Simulator::enablePerformanceModelsInCurrentProcess();
   
   IntPtr address = 0x1000;

   // 1) Get a tile object
   Core* core = Sim()->getTileManager()->getTileFromID(0)->getCore();

   UInt32 written_val = 0xdeadbeaf;
   UInt32 read_val = 0;

   UInt32 num_misses;

   // Read out the value
   printf("Reading from address(%#lx)\n", address);
   num_misses = (core->initiateMemoryAccess(MemComponent::L1_DCACHE, Core::NONE, Core::READ, address, (Byte*) &read_val, sizeof(read_val), true)).first;
   printf("Reading(0x%x) from address(%#lx) completed\n", read_val, address);
    LOG_ASSERT_ERROR(num_misses == 1, "num_misses(%u)", num_misses);

   Simulator::disablePerformanceModelsInCurrentProcess();
   CarbonStopSim();

   printf("Finished (shared_mem_basic) - SUCCESS\n");
   return 0;
}
