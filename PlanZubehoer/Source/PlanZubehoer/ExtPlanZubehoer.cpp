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
    struct SResourceSpec
    {
        short       fListType;
        const char* fLabel;
    };

    // Die Typen entsprechen der bereits im Python-Prototyp getesteten Liste.
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

    struct SCollectedResource
    {
        MCObjectHandle       fHandle = nullptr;
        TXString             fName;
        std::set<std::string> fTypes;
        TXStringArray        fTags;
        std::vector<TXString> fPath;
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

                // Fallback ueber den tatsaechlichen Namen, falls eine Liste keinen Handle liefert.
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
                    {
                        // Bei einzelnen Ressourcen liefert die Object-Tag-API dieselben
                        // Informationen robuster. Nur als lesender Fallback verwenden.
                        gSDK->GetObjectTags(h, item.fTags);
                    }

                    item.fPath = GetContainerPath(h);
                    it = collected.emplace(key, std::move(item)).first;
                }

                it->second.fTypes.insert(spec.fLabel);
            }

            gSDK->DisposeResourceList(listID);
        }

        return collected;
    }
}

BEGIN_WebPalette_DISPATCH_MAP(CPaletteJSProvider)
ADD_WebPalette_FUNCTION("getSnapshot", OnGetSnapshot)
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

    // Erstellt das vwAPI-Objekt im Web-Frontend und gibt Zugriff auf die
    // Ressourcen dieses Plug-ins. PromiseSync laeuft im Vectorworks-Hauptthread
    // und darf daher die SDK-Funktionen fuer das aktive Dokument verwenden.
    context->AddReourceAccessFunction("vwAPI", DefaultPluginVWRIdentifier());
    context->AddFunctionPromiseSync("vwAPI.getSnapshot");
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
    result["version"] = "0.10.0";
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

        nlohmann::json types = nlohmann::json::array();
        for (const std::string& type : source.fTypes)
            types.push_back(type);
        item["types"] = std::move(types);

        result["resources"].push_back(std::move(item));
    }

    result["count"] = result["resources"].size();
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
    outCX = 520;
    outCY = 720;
    return true;
}

bool CExtPlanZubehoer::GetMinimalSize(ViewCoord& outCX, ViewCoord& outCY)
{
    outCX = 360;
    outCY = 420;
    return true;
}

// Neue, projekt-eigene UUID: {7055984D-57CA-433D-B8C8-3545B33D984E}
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

// Neue, projekt-eigene UUID: {B4C113C1-9DA4-4364-AD1F-61C13B31E744}
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
