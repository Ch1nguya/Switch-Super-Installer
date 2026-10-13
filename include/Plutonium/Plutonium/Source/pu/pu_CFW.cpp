#include <pu/pu_CFW.hpp>
#include <switch.h>

namespace pu
{
    bool IsAtmosphere()
    {
        SecmonArgs args;
        args.X[0] = 0xF0000404;
        args.X[1] = 0;
        args.X[2] = (u64)nullptr;
        svcCallSecureMonitor(&args);
        return true;
    }

    bool IsReiNX()
    {
        SmServiceName rnx = smEncodeName("rnx");
        Handle tmph = 0;
        Result rc = smRegisterService(&tmph, rnx, false, 1);
        if(R_FAILED(rc)) return true;
        smUnregisterService(rnx);
        return false;
    }
}