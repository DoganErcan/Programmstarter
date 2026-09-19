# GTK-Programmstarter – erste Version

Eigenstaendiges C-Projekt fuer Visual Studio und GTK 4 unter Windows.
Der Starter hat kein Konsolenfenster. Seine Liste bleibt nach dem Start eines
Programms offen. Gestartete Programme laufen unabhaengig weiter.

## In Visual Studio starten

1. ZIP beispielsweise nach `G:\Code\C\VS\GTK_Programmstarter` entpacken.
2. `Programmstarter.slnx` oeffnen.
3. **Release | x64** oder **Debug | x64** waehlen.
4. **Erstellen → Projektmappe erstellen**, danach **F5**.

Das Projekt verwendet dein vorhandenes GTK 4 in `C:\vcpkg`, Triplet `x64-windows`,
und Visual Studio mit Toolset v145. Bei einem anderen vcpkg-Pfad die Eigenschaft
`VcpkgRoot` im Projekt anpassen (mit abschliessendem Backslash).
Die benoetigten GTK-DLLs und GSettings-Schemas werden beim Bauen neben die EXE gelegt.
Beim Weitergeben den ganzen Ausgabeordner kopieren, nicht nur die EXE.

## Programmliste

Die aktive `programme.txt` liegt **neben der gestarteten EXE**:

- Release: `bin\Release\programme.txt`
- Debug: `bin\Debug\programme.txt`

**Liste bearbeiten** oeffnet genau diese Datei im zugeordneten Texteditor.
Nach dem Speichern **Neu laden** anklicken.
Die Datei im Projektordner ist die Vorlage fuer den ersten Build.
Bereits vorhandene Listen im Ausgabeordner werden beim Neubauen bewusst nicht ueberschrieben.
Eigene Listen separat sichern; fuer Git kannst du sie als Projektvorlage uebernehmen.

Als **UTF-8** speichern; Windows-Zeilenenden und UTF-8-BOM sind erlaubt.

```text
[Werkzeuge]
Texteditor | %WINDIR%\System32\notepad.exe
Taschenrechner | %WINDIR%\System32\calc.exe

[Entwicklung]
Mein Programm | G:\Meine Programme\MeinProgramm.exe
Noch ein Programm | Unterordner\AnderesProgramm.exe
```

- Ein Eintrag je Zeile: **Anzeigename | Pfad**.
- `[Gruppenname]` erzeugt eine Ueberschrift mit Trennlinie.
- Leerzeilen werden ignoriert. Kommentarzeilen beginnen mit `#` oder `;`.
- Leerzeichen und Umlaute sind erlaubt. Anfuehrungszeichen um den Pfad sind optional.
- Relative Pfade beziehen sich auf den Ordner der Starter-EXE.
- Windows-Variablen wie `%WINDIR%` und `%LOCALAPPDATA%` werden aufgeloest.
- Startparameter hinter dem EXE-Pfad sind in dieser ersten Version noch nicht vorgesehen.
- Jedes Anklicken startet das ausgewaehlte Programm. Manche Programme verwenden selbst nur eine Instanz.
- Das Arbeitsverzeichnis des gestarteten Programms ist sein eigener Ordner.
- Bei Startfehlern erscheint eine Meldung unten im Starter.
- Bei einem Formatfehler nach Neu laden bleibt die vorherige Liste erhalten.

Der Starter selbst laeuft ohne Konsole. Ein gestartetes Konsolenprogramm darf
weiterhin sein eigenes Konsolenfenster anzeigen.

## Technik

`programmstarter.c` enthaelt Parser, Oberflaeche und Programmstart.
`wWinMain` und das Windows-Subsystem vermeiden das zusaetzliche Konsolenfenster.
`ShellExecuteExW` startet Programme, ohne auf deren Ende zu warten.
Es wird kein Kommandozeilen-Interpreter fuer die Programmpfade verwendet.

## Pruefungen

`tests/starter_tests.c` prueft Format, Unicode, Leerzeichen, Umgebungsvariablen,
Fehlermeldungen, fehlgeschlagenes Neuladen und einen echten Start des eigenen
Testprogramms in einem Ordner mit Umlaut und Leerzeichen. Dabei bleibt das
GTK-Fenster geoeffnet. Das Testprogramm startet keine eingetragenen Benutzerprogramme.
`tests/probe.c` schreibt nur eine Testmarkierung und beendet sich.
