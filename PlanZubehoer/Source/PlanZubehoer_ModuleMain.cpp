#include "StdAfx.h"
#include "PlanZubehoer/ExtPlanZubehoer.h"

using namespace PlanZubehoer;

const char* DefaultPluginVWRIdentifier()
{
    return "PlanZubehoer";
}

extern "C" Sint32 GS_EXTERNAL_ENTRY plugin_module_ver()
{
    return SDK_VERSION;
}

extern "C" Sint32 GS_EXTERNAL_ENTRY plugin_module_main(Sint32 action,
                                                        void* moduleInfo,
                                                        const VWIID& iid,
                                                        IVWUnknown*& inOutInterface,
                                                        CallBackPtr cbp)
{
    ::GS_InitializeVCOM(cbp);

    Sint32 reply = 0L;
    using namespace VWFC::PluginSupport;

    REGISTER_Extension<CExtPlanZubehoer>(GROUPID_ExtensionWebPalettes,
                                         action, moduleInfo, iid, inOutInterface, cbp, reply);

    REGISTER_Extension<CExtMenuShowPlanZubehoer>(GROUPID_ExtensionMenu,
                                                  action, moduleInfo, iid, inOutInterface, cbp, reply);

    return reply;
}
