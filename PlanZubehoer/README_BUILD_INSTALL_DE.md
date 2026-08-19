# Plan Zubehoer - Vectorworks 2026 SDK Plug-in

Version 0.10.0

## Name des Plug-ins

Plan Zubehoer

## Was macht dieses Plug-in?

Das Plug-in stellt eine dauerhaft andockbare Web-Palette in Vectorworks 2026 bereit.
Es liest die Zubehoerressourcen des aktiven Dokuments, wertet deren Tags aus und
fasst alle Ressourcen mit Plan-* Tags planbezogen zusammen.

Damit koennen z. B. in derselben Ansicht gemeinsam angezeigt werden:

- Symbole und Objektstile
- Schraffuren
- Materialien
- Linienarten
- Textstile
- Tabellen und Legenden
- Datenbanken/Datensatzformate
- Texturen
- Dachstile
- Boden-/Deckenstile
- Wandstile
- weitere im Quellcode definierte Ressourcentypen

Die Palette besitzt drei Filter:

1. Plan
2. Zubehoertyp
3. freie Textsuche

Der Planfilter wird automatisch aus den vorhandenen Plan-* Tags erzeugt. Bei
nummerierten Plan-Tags versucht die Web-Oberflaeche den ausgeschriebenen
Plannamen aus dem Zubehoerordner abzuleiten. Beispiel:

Plan-08 + Ordner "08 - Contractor Plan - Schraffuren"
-> Anzeige "08 - Contractor Plan"

## Was ist zu beachten?

- Version 0.10.0 ist ein echtes Vectorworks-SDK-Plug-in, kein Python-Script.
- Es ist fuer Vectorworks 2026 unter Windows vorbereitet.
- Es arbeitet nur mit dem aktiven Dokument.
- Es veraendert keine Ressourcen und keine Tags.
- Zum Kompilieren wird die offizielle Vectorworks-SDK-2026-Struktur benoetigt.
- Vectorworks 2026 behandelt SDK-Plug-ins ohne Entwickler-Credentials als
  unbekannten Entwickler. Fuer die interne Entwicklung kann das Plug-in nach
  der Vectorworks-Warnung manuell freigegeben werden. Fuer eine saubere
  Verteilung sollte eine passende Credentials-*.vst-Datei neben dem Plug-in
  liegen.

## Welche Parameter koennen geaendert werden?

### C++

Datei:
`Source/PlanZubehoer/ExtPlanZubehoer.cpp`

`kResourceSpecs`
: Definiert die unterstuetzten Zubehoertypen.

`GetInitialSize()` / `GetMinimalSize()`
: Definiert Start- und Minimalgroesse der Palette.

### Web-Oberflaeche

Datei:
`PlanZubehoer.vwr/html/app.js`

`PLAN_TAG_PREFIX`
: Standard `Plan-`.

`KNOWN_PLAN_NAMES`
: Optionale feste Anzeigenamen fuer Tags, die nicht aus der Ordnerstruktur
  abgeleitet werden sollen.

`STORAGE_KEY`
: Name fuer die lokal gespeicherte letzte Filterauswahl.

---

# Installation zum Entwickeln und Testen

## 1. Offizielles SDKExamples bereitstellen

Besorge die offizielle Vectorworks-SDKExamples-Struktur fuer 2026.

Der Ordner dieses Projekts muss danach hier liegen:

`SDKExamples\Examples2026\PlanZubehoer\`

Wichtig ist insbesondere, dass relativ dazu dieser Ordner existiert:

`SDKExamples\VectorworksSDK\SDK2026\`

Die Visual-Studio-Projektdatei verwendet absichtlich dieselbe relative
SDK-Struktur wie das offizielle WebPaletteExample.

## 2. Visual Studio

Fuer Vectorworks 2026 unter Windows wird Visual Studio 2022 mit dem v143
Toolset verwendet. Oeffne:

`PlanZubehoer2026.sln`

und waehle:

- Configuration: `Release`
- Platform: `x64`

Alternativ kann `BUILD_RELEASE.bat` gestartet werden.

## 3. Ausgabe

Wenn der Ordner an der oben beschriebenen Stelle liegt, werden die fertigen
Dateien hier erzeugt:

`SDKExamples\Output\2026\_Output\Release\PlanZubehoer.vlb`

`SDKExamples\Output\2026\_Output\Release\PlanZubehoer.vwr`

Beide Dateien gehoeren zusammen.

## 4. Credentials fuer Vectorworks 2026

Im Projekt befindet sich:

`Credentials_STT_PlanZubehoer_TEMPLATE.json`

Trage dort Entwickler/Firma, Kontakt, Website und Ausgabedatum ein. Der
entscheidende Dateieintrag ist bereits vorbereitet:

`"PlanZubehoer"`

Die JSON-Datei wird gemaess dem Vectorworks-2026-Credentials-Verfahren an
Vectorworks eingereicht. Die daraus erhaltene `Credentials*.vst`-Datei muss
bei der Verteilung neben `PlanZubehoer.vlb` liegen.

Fuer einen internen Entwicklungstest ist die Credentials-Datei nicht zwingend
zum Kompilieren erforderlich. Vectorworks 2026 kann unbekannte Plug-ins nach
einer Sicherheitswarnung manuell freigeben.

## 5. Vectorworks findet das Plug-in

Kopiere fuer einen einfachen Test mindestens diese beiden Dateien in einen
Vectorworks-Plug-in-Ordner:

- `PlanZubehoer.vlb`
- `PlanZubehoer.vwr`

Bei einer signierten/sauberen Verteilung kommt dazu:

- `Credentials....vst`

Danach Vectorworks neu starten.

## 6. Menuebefehl in die Arbeitsumgebung aufnehmen

Das Plug-in registriert den Menuebefehl:

`Plan Zubehoer oeffnen...`

Fuege diesen Befehl ueber den Vectorworks-Befehl `Arbeitsumgebung anpassen`
in deine gewuenschte Arbeitsumgebung ein. Der Menuebefehl oeffnet die
Web-Palette. Die Palette kann anschliessend wie andere Vectorworks-Paletten
positioniert bzw. angedockt werden.

## 7. Funktionstest

1. Dokument mit bereits getaggtem Zubehoer oeffnen.
2. `Plan Zubehoer oeffnen...` ausfuehren.
3. In der Palette sollte `08 - Contractor Plan` erscheinen, wenn Ressourcen
   den Tag `Plan-08` besitzen.
4. `Alle` muss die verschiedenen Zubehoertypen gemeinsam anzeigen.
5. Auf `Schraffuren`, `Materialien` usw. wechseln und Trefferzahl pruefen.
6. Suchfeld testen.
7. `Aktualisieren` betaetigen, nachdem Tags im Dokument veraendert wurden.

---

# Technischer Aufbau

## C++ -> Web-Palette

`CPaletteJSProvider::OnInit()` registriert die Promise-Funktion:

`vwAPI.getSnapshot()`

Der Aufruf erfolgt synchron zum Vectorworks-Hauptthread, damit die SDK-APIs
fuer das aktive Dokument sicher verwendet werden koennen.

## Ressourcen lesen

Der C++-Teil verwendet pro Ressourcentyp:

- `BuildResourceList`
- `GetResourceFromList`
- `GetActualNameFromResourceList` als Fallback
- `GetResourceTags`
- `GetObjectTags` als lesenden Fallback
- `ParentObject` fuer den Ordnerpfad
- `DisposeResourceList`

Mehrfach in verschiedenen Ressourcenlisten auftauchende Handles werden
zusammengefuehrt.

## Web-Frontend

Der C++-Teil liefert einen Snapshot aller gefundenen Ressourcen. JavaScript
filtert diesen Snapshot anschliessend lokal. Dadurch reagieren Plan-, Typ- und
Textfilter ohne weitere SDK-Abfrage.

Der Refresh-Knopf liest das aktive Dokument erneut ein.

---

# Bewusste Grenzen von Version 0.10.0

Noch nicht enthalten:

- Ressourcen direkt einsetzen/aktivieren
- Vorschaubilder wie im Zubehoer-Manager
- Favoriten und Arbeitsgruppenbibliotheken
- automatisches Ereignis bei jeder Ressourcenaenderung
- Kontextmenue
- Drag-and-drop

Diese Funktionen sollten erst nach dem Praxistest der dockbaren Palette
hinzugefuegt werden.
