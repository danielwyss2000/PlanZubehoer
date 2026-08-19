#include "StdAfx.h"
#include "PlanZubehoer/ExtPlanZubehoer.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace PlanZubehoer;

namespace
{
    constexpr const char* kPluginVersion = "0.20.0";
    constexpr short kTool2DSymbol = -209;
    constexpr short kTool3DSymbol = -309;

    struct SResourceSpec
    {
        short       fListType;
        const char* fLabel;
    };

    static const SResourceSpec kResourceSpecs[] = {
        { 16,  "Symbole/Objektstile" },
        { 18,  "Tabellen/Legenden" },
        { 19,  "Materialien" },
        { 47,  "Datenbanken/Datensatzformate" },
        { -47, "Skizzenstile" },
        { 49,  "Scripts" },
        { 66,  "Schraffuren" },
        { 96,  "Linienarten" },
        { 97,  "Texturen" },
        { 102, "Dachstile" },
        { 107, "Boden-/Deckenstile" },
        { 108, "Mosaike" },
        { 109, "Textstile" },
        { 115, "Renderworks-Hintergruende" },
        { 119, "Bildfuellungen" },
        { 120, "Farbverlaeufe" },
        { 127, "Wandstile" },
    };

    constexpr short kTypeScriptPalette = 51;
    constexpr short kTypeResourceFolder = 92;

    struct SDirectAction
    {
        std::string fCode;
        bool fEnabled = false;
    };

    std::string ToUTF8(const TXString& value)
    {
        return std::string(static_cast<const char*>(value));
    }

    bool IsResourceContainer(MCObjectHandle h)
    {
        if (h == nullptr)
            return false;

        const short type = gSDK->GetObjectTypeN(h);
        return type == kTypeResourceFolder || type == kTypeScriptPalette;
    }

    TXString GetObjectNameSafe(MCObjectHandle h)
    {
        TXString name;
        if (h != nullptr)
            gSDK->GetObjectName(h, name);
        return name;
    }

    std::vector<TXString> GetContainerPath(MCObjectHandle h)
    {
        std::vector<TXString> reversed;
        std::set<std::uintptr_t> visited;
        MCObjectHandle current = gSDK->ParentObject(h);

        for (size_t depth = 0; depth < 64 && IsResourceContainer(current); ++depth)
        {
            const std::uintptr_t key = reinterpret_cast<std::uintptr_t>(current);
            if (key == 0 || visited.find(key) != visited.end())
                break;

            visited.insert(key);
            TXString name = GetObjectNameSafe(current);
            if (!name.IsEmpty())
                reversed.push_back(name);

            current = gSDK->ParentObject(current);
        }

        std::reverse(reversed.begin(), reversed.end());
        return reversed;
    }

    nlohmann::json ToJsonArray(const TXStringArray& strings)
    {
        nlohmann::json result = nlohmann::json::array();
        for (const TXString& value : strings)
            result.push_back(ToUTF8(value));
        return result;
    }

    nlohmann::json ToJsonArray(const std::vector<TXString>& strings)
    {
        nlohmann::json result = nlohmann::json::array();
        for (const TXString& value : strings)
            result.push_back(ToUTF8(value));
        return result;
    }

    SDirectAction GetDirectAction(MCObjectHandle h)
    {
        SDirectAction result;
        if (h == nullptr)
        {
            result.fCode = "missing";
            return result;
        }

        const short objectType = gSDK->GetObjectTypeN(h);

        if (objectType == kSymDefNode)
        {
            // Intelligente Objektstile sind ebenfalls Symboldefinitionen und werden
            // bewusst nicht wie normale Symbole eingesetzt.
            if (gSDK->GetSymbolDefSubType(h) > 0)
            {
                result.fCode = "object-style";
                return result;
            }

            result.fCode = "insert-symbol";
            result.fEnabled = true;
            return result;
        }

        if (objectType == kHatchDefNode ||
            objectType == kImageDefNode ||
            objectType == kGradientDefNode ||
            objectType == kTileDefNode)
        {
            result.fCode = "set-fill";
            result.fEnabled = true;
            return result;
        }

        if (objectType == kLineTypeDefNode)
        {
            result.fCode = "set-line-type";
            result.fEnabled = true;
            return result;
        }

        result.fCode = "unsupported";
        return result;
    }

    struct SCollectedResource
    {
        MCObjectHandle fHandle = nullptr;
        TXString fName;
        std::set<std::string> fTypes;
        TXStringArray fTags;
        std::vector<TXString> fPath;
        short fObjectType = 0;
        SDirectAction fAction;
    };

    std::map<std::uintptr_t, SCollectedResource> CollectResources()
    {
        std::map<std::uintptr_t, SCollectedResource> collected;

        for (const SResourceSpec& spec : kResourceSpecs)
        {
            Sint32 count = 0;
            const FolderSpecifier currentDocument = static_cast<FolderSpecifier>(0);
            const Sint32 listID = gSDK->BuildResourceList(spec.fListType, currentDocument, TXString(), count);

            if (listID <= 0 || count <= 0)
            {
                if (listID > 0)
                    gSDK->DisposeResourceList(listID);
                continue;
            }

            for (Sint32 index = 1; index <= count; ++index)
            {
                MCObjectHandle h = gSDK->GetResourceFromList(listID, index);

                if (h == nullptr)
                {
                    TXString actualName;
                    gSDK->GetActualNameFromResourceList(listID, index, actualName);
                    if (!actualName.IsEmpty())
                        h = gSDK->GetNamedObject(actualName);
                }

                if (h == nullptr || IsResourceContainer(h))
                    continue;

                const std::uintptr_t key = reinterpret_cast<std::uintptr_t>(h);
                if (key == 0)
                    continue;

                auto it = collected.find(key);
                if (it == collected.end())
                {
                    SCollectedResource item;
                    item.fHandle = h;
                    item.fName = GetObjectNameSafe(h);
                    if (item.fName.IsEmpty())
                    {
                        TXString actualName;
                        gSDK->GetActualNameFromResourceList(listID, index, actualName);
                        item.fName = actualName;
                    }

                    gSDK->GetResourceTags(h, item.fTags);
                    if (item.fTags.IsEmpty())
                        gSDK->GetObjectTags(h, item.fTags);

                    item.fPath = GetContainerPath(h);
                    item.fObjectType = gSDK->GetObjectTypeN(h);
                    item.fAction = GetDirectAction(h);
                    it = collected.emplace(key, std::move(item)).first;
                }

                it->second.fTypes.insert(spec.fLabel);
            }

            gSDK->DisposeResourceList(listID);
        }

        return collected;
    }

    MCObjectHandle ResolveResource(const TXString& name, short expectedObjectType)
    {
        if (name.IsEmpty())
            return nullptr;

        MCObjectHandle h = gSDK->GetNamedObject(name);
        if (h == nullptr)
            return nullptr;

        if (expectedObjectType != 0 && gSDK->GetObjectTypeN(h) != expectedObjectType)
            return nullptr;

        return h;
    }

    bool SetResourceAsCurrentFill(MCObjectHandle h)
    {
        if (h == nullptr)
            return false;

        InternalIndex index = gSDK->GetObjectInternalIndex(h);
        if (index == 0)
            return false;

        if (index > 0)
            index = -index;

        gSDK->SetDefaultFillPat(index);
        return true;
    }

    bool SetResourceAsCurrentLineType(MCObjectHandle h)
    {
        if (h == nullptr)
            return false;

        InternalIndex index = gSDK->GetObjectInternalIndex(h);
        if (index == 0)
            return false;

        if (index > 0)
            index = -index;

        gSDK->SetDefaultPenPatN(index);
        return true;
    }

    bool ActivateSymbolForInsertion(MCObjectHandle h)
    {
        if (h == nullptr || !VWSymbolDefObj::IsSymbolDefObject(h))
            return false;

        if (gSDK->GetSymbolDefSubType(h) > 0)
            return false;

        VWSymbolDefObj symbol(h);
        symbol.SetAsActiveSymbolDef();

        short insertMode = kSymbolToolRegularInsert;
        gSDK->SetProgramVariable(varSymbolToolInsertMode, &insertMode);

        const short toolIndex = symbol.GetType() == kSymbolDefType_3D ? kTool3DSymbol : kTool2DSymbol;
        gSDK->SetToolByIndex(toolIndex);
        return true;
    }
}

BEGIN_WebPalette_DISPATCH_MAP(CPaletteJSProvider)
ADD_WebPalette_FUNCTION("getSnapshot", OnGetSnapshot)
ADD_WebPalette_FUNCTION("useResource", OnUseResource)
END_WebPalette_DISPATCH_MAP

CPaletteJSProvider::CPaletteJSProvider(IVWUnknown* parent)
    : VWExtensionPaletteJSProvider(parent)
{
}

CPaletteJSProvider::~CPaletteJSProvider()
{
}

void CPaletteJSProvider::OnInit(IInitContext* context)
{
    fWebFrame = context->GetWebFrame();
    context->AddReourceAccessFunction("vwAPI", DefaultPluginVWRIdentifier());
    context->AddFunctionPromiseSync("vwAPI.getSnapshot");
    context->AddFunctionPromiseSync("vwAPI.useResource");
}

void CPaletteJSProvider::OnGetSnapshot(const TXString& objName,
                                       const TXString& functionName,
                                       const std::vector<nlohmann::json>& args,
                                       VectorWorks::UI::IJSFunctionCallbackContext* context)
{
    (void)objName;
    (void)functionName;
    (void)args;

    nlohmann::json result;
    result["version"] = kPluginVersion;
    result["source"] = "active-document";
    result["resources"] = nlohmann::json::array();

    const auto resources = CollectResources();
    for (const auto& pair : resources)
    {
        const SCollectedResource& source = pair.second;

        nlohmann::json item;
        item["name"] = ToUTF8(source.fName);
        item["tags"] = ToJsonArray(source.fTags);
        item["pathParts"] = ToJsonArray(source.fPath);
        item["objectType"] = source.fObjectType;
        item["actionCode"] = source.fAction.fCode;
        item["actionEnabled"] = source.fAction.fEnabled;

        nlohmann::json types = nlohmann::json::array();
        for (const std::string& type : source.fTypes)
            types.push_back(type);
        item["types"] = std::move(types);

        result["resources"].push_back(std::move(item));
    }

    result["count"] = result["resources"].size();
    context->Resolve(result);
}

void CPaletteJSProvider::OnUseResource(const TXString& objName,
                                       const TXString& functionName,
                                       const std::vector<nlohmann::json>& args,
                                       VectorWorks::UI::IJSFunctionCallbackContext* context)
{
    (void)objName;
    (void)functionName;

    nlohmann::json result;
    result["success"] = false;
    result["code"] = "invalid-request";

    if (args.empty() || !args[0].is_object())
    {
        context->Resolve(result);
        return;
    }

    const nlohmann::json& request = args[0];
    const std::string nameUTF8 = request.value("name", std::string());
    const short expectedObjectType = static_cast<short>(request.value("objectType", 0));
    const std::string requestedAction = request.value("actionCode", std::string());

    TXString name(nameUTF8, ETXEncoding::eUTF8);
    MCObjectHandle h = ResolveResource(name, expectedObjectType);
    if (h == nullptr)
    {
        result["code"] = "resource-not-found";
        context->Resolve(result);
        return;
    }

    const SDirectAction actualAction = GetDirectAction(h);
    if (!actualAction.fEnabled || actualAction.fCode != requestedAction)
    {
        result["code"] = actualAction.fCode.empty() ? "unsupported" : actualAction.fCode;
        context->Resolve(result);
        return;
    }

    bool success = false;
    if (actualAction.fCode == "insert-symbol")
        success = ActivateSymbolForInsertion(h);
    else if (actualAction.fCode == "set-fill")
        success = SetResourceAsCurrentFill(h);
    else if (actualAction.fCode == "set-line-type")
        success = SetResourceAsCurrentLineType(h);

    result["success"] = success;
    result["code"] = success ? actualAction.fCode : "action-failed";
    result["name"] = nameUTF8;
    context->Resolve(result);
}

CExtPlanZubehoer::CExtPlanZubehoer(CallBackPtr cbp)
{
    (void)cbp;
}

CExtPlanZubehoer::~CExtPlanZubehoer()
{
}

void CExtPlanZubehoer::DefineSinks()
{
    this->DefineSink<CPaletteJSProvider>(IID_WebJavaScriptProvider);
}

TXString CExtPlanZubehoer::GetTitle()
{
    return TXResStr("ExtPlanZubehoer", "paletteName");
}

bool CExtPlanZubehoer::GetInitialSize(ViewCoord& outCX, ViewCoord& outCY)
{
    outCX = 480;
    outCY = 720;
    return true;
}

bool CExtPlanZubehoer::GetMinimalSize(ViewCoord& outCX, ViewCoord& outCY)
{
    outCX = 340;
    outCY = 400;
    return true;
}

IMPLEMENT_VWPaletteExtension(
    CExtPlanZubehoer,
    "STT.PlanZubehoer.Palette",
    1,
    0x7055984d, 0x57ca, 0x433d, 0xb8, 0xc8, 0x35, 0x45, 0xb3, 0x3d, 0x98, 0x4e);

static SMenuDef gMenuDef = {
    EMenuEnableFlags::DocIsActive,
    EMenuEnableFlags::None,
    {"ExtMenuShowPlanZubehoer", "menu_title"},
    {"ExtMenuShowPlanZubehoer", "menu_category"},
    {"ExtMenuShowPlanZubehoer", "menu_helptext"},
    31,
    0,
    0,
    " "
};

IMPLEMENT_VWMenuExtension(
    CExtMenuShowPlanZubehoer,
    CExtMenuShowPlanZubehoer_EventSink,
    "STT.PlanZubehoer.ShowPalette",
    1,
    0xb4c113c1, 0x9da4, 0x4364, 0xad, 0x1f, 0x61, 0xc1, 0x3b, 0x31, 0xe7, 0x44);

CExtMenuShowPlanZubehoer::CExtMenuShowPlanZubehoer(CallBackPtr cbp)
    : VWExtensionMenu(cbp, gMenuDef)
{
}

CExtMenuShowPlanZubehoer::~CExtMenuShowPlanZubehoer()
{
}

CExtMenuShowPlanZubehoer_EventSink::CExtMenuShowPlanZubehoer_EventSink(IVWUnknown* parent)
    : VWMenu_EventSink(parent)
{
}

CExtMenuShowPlanZubehoer_EventSink::~CExtMenuShowPlanZubehoer_EventSink()
{
}

void CExtMenuShowPlanZubehoer_EventSink::DoInterface()
{
    gSDK->SetWebPaletteVisibility(CExtPlanZubehoer::_GetIID(), true);
}
