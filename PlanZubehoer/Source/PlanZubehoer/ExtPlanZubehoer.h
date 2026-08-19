#pragma once

/*
Name des Plug-ins:
Plan Zubehoer

Version:
0.20.0

Was macht dieses Plug-in?
- Stellt eine dauerhaft andockbare Web-Palette fuer Vectorworks 2026 bereit.
- Liest Zubehoerressourcen aus dem aktiven Dokument und erkennt Plan-* Tags.
- Filtert planbezogen ueber alle unterstuetzten Zubehoertypen hinweg.
- Unterstuetzt direkte, sichere Standardaktionen per Doppelklick bzw. "Verwenden":
  * normale 2D-/3D-/Hybridsymbole: als aktives Symbol setzen und Symbol-Werkzeug starten.
  * Schraffuren, Bildfuellungen, Farbverlaeufe und Mosaike: als aktuelle Fuellung setzen.
  * Linienarten: als aktuelle Linienart setzen.
- Andere Zubehoertypen werden weiterhin gefunden und angezeigt, aber nicht automatisch
  veraendert, solange keine eindeutige, dokumentierte Standardaktion existiert.

Was ist zu beachten?
- Dieses Projekt ist fuer das Vectorworks SDK 2026 unter Windows aufgebaut.
- Es muss mit Visual Studio 2022 / Toolset v143 kompiliert werden.
- Vectorworks 2026 SDK-Plug-ins benoetigen die vorgesehene Entwickler-Credentials-Datei.
- Das Plug-in arbeitet mit dem aktiven Dokument.
- Direkte Aktionen werden nur fuer dokumentiert eindeutig behandelbare Ressourcentypen
  ausgefuehrt. Objektstile werden bewusst nicht als normale Symbole eingesetzt.
- Das Setzen von Fuellung/Linienart aendert die aktuellen Vorgabeattribute fuer neu
  erzeugte Objekte; bestehende Objekte werden nicht stillschweigend veraendert.

Welche Parameter koennen geaendert werden?
- kResourceSpecs in ExtPlanZubehoer.cpp: unterstuetzte Zubehoertypen.
- PLAN_TAG_PREFIX in PlanZubehoer.vwr/html/app.js: Tag-Praefix, Standard Plan-.
- KNOWN_PLAN_NAMES in app.js: optionale Anzeigenamen fuer spezielle Tags.
- Initial- und Minimalgroesse der Palette in ExtPlanZubehoer.cpp.
*/

namespace PlanZubehoer
{
    using namespace VectorWorks::Extension;
    using namespace VWFC::PluginSupport;

    class CPaletteJSProvider : public VWExtensionPaletteJSProvider
    {
    public:
        CPaletteJSProvider(IVWUnknown* parent);
        virtual ~CPaletteJSProvider();

        virtual void OnInit(IInitContext* context);

        DEFINE_WebPalette_DISPATCH_MAP;

    private:
        void OnGetSnapshot(const TXString& objName,
                           const TXString& functionName,
                           const std::vector<nlohmann::json>& args,
                           VectorWorks::UI::IJSFunctionCallbackContext* context);

        void OnUseResource(const TXString& objName,
                           const TXString& functionName,
                           const std::vector<nlohmann::json>& args,
                           VectorWorks::UI::IJSFunctionCallbackContext* context);
    };

    class CExtPlanZubehoer : public VWExtensionWebPalette
    {
        DEFINE_VWPaletteExtension;

    public:
        CExtPlanZubehoer(CallBackPtr cbp);
        virtual ~CExtPlanZubehoer();

        virtual void DefineSinks() override;
        virtual TXString GetTitle() override;
        virtual bool GetInitialSize(ViewCoord& outCX, ViewCoord& outCY) override;
        virtual bool GetMinimalSize(ViewCoord& outCX, ViewCoord& outCY) override;
    };

    class CExtMenuShowPlanZubehoer_EventSink : public VWMenu_EventSink
    {
    public:
        CExtMenuShowPlanZubehoer_EventSink(IVWUnknown* parent);
        virtual ~CExtMenuShowPlanZubehoer_EventSink();

        virtual void DoInterface();
    };

    class CExtMenuShowPlanZubehoer : public VWExtensionMenu
    {
        DEFINE_VWMenuExtension;

    public:
        CExtMenuShowPlanZubehoer(CallBackPtr cbp);
        virtual ~CExtMenuShowPlanZubehoer();
    };
}
