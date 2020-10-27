#include "platform/platform.hpp"
#include "BPCoreEngine.hpp"


void start(Platform& pf)
{
    BPCoreEngine bpcore(pf);

    bpcore.run();
}
