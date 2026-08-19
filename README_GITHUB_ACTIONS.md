# Plan ZubehÃ¶r â€“ automatischer Windows-Build Ã¼ber GitHub Actions

## Ziel
Nach einem GitHub-Actions-Lauf erhÃ¤ltst du ein ZIP-Artefakt mit genau:

- `PlanZubehoer.vlb`
- `PlanZubehoer.vwr`

Diese beiden Dateien kommen danach in den Vectorworks-2026-Benutzerordner unter `Plug-Ins\PlanZubehoer`.

## Einmalige Einrichtung

1. Auf GitHub ein neues, leeres Repository erstellen, z. B. `PlanZubehoer`.
2. Den Inhalt dieses ZIP-Pakets in das Repository hochladen. Wichtig: `.github` muss im Repository auf oberster Ebene liegen.
3. Commit nach `main` ausfÃ¼hren.
4. Im GitHub-Repository auf **Actions** wechseln.
5. Workflow **Build PlanZubehoer for Vectorworks 2026** Ã¶ffnen.
6. **Run workflow** anklicken und bestÃ¤tigen.
7. Nach erfolgreichem Lauf den Bereich **Artifacts** Ã¶ffnen.
8. `PlanZubehoer-Vectorworks2026-Windows` herunterladen.
9. ZIP entpacken. Es enthÃ¤lt `PlanZubehoer.vlb` und `PlanZubehoer.vwr`.

## Installation in Vectorworks 2026

Kopiere beide Dateien nach:

`%APPDATA%\Nemetschek\Vectorworks\2026\Plug-Ins\PlanZubehoer\`

Danach Vectorworks komplett neu starten.

## Was GitHub Actions macht

- Windows Server 2022 starten
- dieses Plug-in auschecken
- die offiziellen Vectorworks SDK Examples von `VectorworksDeveloper/SDKExamples` laden
- das Plug-in in `Examples2026\PlanZubehoer` einsetzen
- mit Visual Studio/MSBuild als `Release | x64` kompilieren
- `PlanZubehoer.vlb` und `PlanZubehoer.vwr` als fertiges Artefakt bereitstellen

## Hinweis zu Vectorworks 2026 Credentials

Der automatische Build erzeugt die BinÃ¤rdateien. Die Vectorworks-2026-Credentials-Pflicht fÃ¼r SDK-Plug-ins wird dadurch nicht aufgehoben. FÃ¼r die saubere Verteilung im BÃ¼ro ist zusÃ¤tzlich die von Vectorworks ausgestellte Credentials-Datei erforderlich.
