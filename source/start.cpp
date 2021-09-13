#include "BPCoreEngine.hpp"
#include "platform/platform.hpp"


void start(Platform& pf)
{
    BPCoreEngine bpcore(pf);

    bpcore.run();
}
