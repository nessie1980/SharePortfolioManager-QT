#!/bin/bash
# generate-docs.sh
# Generiert die Doxygen-Dokumentation.
#
# Aufruf: cd docs/doxygen && ./generate-docs.sh
# Das Script muss aus dem Verzeichnis docs/doxygen/ aufgerufen werden,
# weil das Doxyfile alle Pfade relativ zu diesem Verzeichnis angibt.
#
# Versionsprüfung: Wird eine neue Doxygen-Version erkannt, wird header.html
# automatisch per "doxygen -w" neu generiert und die doxygen-awesome
# Script-Tags werden automatisch wieder eingefügt.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
DOXYFILE="$SCRIPT_DIR/Doxyfile"
OUTPUT_DIR="$PROJECT_ROOT/build/docs"
THEME_DIR="$SCRIPT_DIR/theme"
VERSION_FILE="$SCRIPT_DIR/.doxygen-version"
HEADER="$THEME_DIR/header.html"
FOOTER="$THEME_DIR/footer.html"
DOXYGEN_CSS="$THEME_DIR/doxygen.css"

echo "=== Doxygen Dokumentation ==="
echo "Projekt-Root: $PROJECT_ROOT"
echo "Doxyfile:     $DOXYFILE"
echo ""

if [ ! -f "$DOXYFILE" ]; then
    echo "FEHLER: Doxyfile nicht gefunden: $DOXYFILE"
    echo "Bitte das Script aus dem Verzeichnis docs/doxygen/ aufrufen."
    exit 1
fi

# ── Doxygen-Version ermitteln ─────────────────────────────────────────────────
DOXYGEN_CURRENT="$(doxygen --version 2>/dev/null | head -1)"
if [ -z "$DOXYGEN_CURRENT" ]; then
    echo "FEHLER: doxygen nicht gefunden. Bitte installieren."
    exit 1
fi

# ── Theme-Dateien prüfen ──────────────────────────────────────────────────────
THEME_OK=true
for F in doxygen-awesome.css doxygen-awesome-sidebar-only.css \
         doxygen-awesome-interactive-toc.js \
         doxygen-awesome-paragraph-link.js \
         doxygen-awesome-darkmode-toggle.js; do
    if [ ! -f "$THEME_DIR/$F" ]; then
        THEME_OK=false
        break
    fi
done

if [ "$THEME_OK" = false ]; then
    echo "Theme-Dateien fehlen — lade herunter..."
    "$SCRIPT_DIR/download-theme.sh" || exit 1
fi

# ── Versionsprüfung: header.html neu generieren wenn nötig ───────────────────
DOXYGEN_SAVED=""
if [ -f "$VERSION_FILE" ]; then
    DOXYGEN_SAVED="$(cat "$VERSION_FILE")"
fi

if [ "$DOXYGEN_CURRENT" != "$DOXYGEN_SAVED" ] || [ ! -f "$HEADER" ]; then
    echo "Doxygen-Version geändert: ${DOXYGEN_SAVED:-<neu>} → $DOXYGEN_CURRENT"
    echo "Generiere header.html neu..."

    # Doxygen generiert ein versions-korrektes header.html
    doxygen -w html "$HEADER" "$FOOTER" "$DOXYGEN_CSS"

    # doxygen-awesome Script-Tags nach <body> einfügen
    # Wir suchen nach der ersten <body>-Zeile und fügen direkt danach ein
    INJECT='<script type="text\/javascript" src="$relpath^doxygen-awesome-interactive-toc.js"><\/script>\n\
<script type="text\/javascript" src="$relpath^doxygen-awesome-paragraph-link.js"><\/script>\n\
<script type="text\/javascript" src="$relpath^doxygen-awesome-darkmode-toggle.js"><\/script>\n\
<script type="text\/javascript">\n\
    DoxygenAwesomeInteractiveToc.init();\n\
    DoxygenAwesomeParagraphLink.init();\n\
    DoxygenAwesomeDarkModeToggle.init();\n\
<\/script>'

    sed -i "s|<body>|<body>\n${INJECT}|" "$HEADER"

    # Aktuelle Version speichern
    echo "$DOXYGEN_CURRENT" > "$VERSION_FILE"
    echo "header.html aktualisiert für Doxygen $DOXYGEN_CURRENT"
    echo ""
else
    echo "Doxygen-Version unverändert ($DOXYGEN_CURRENT) — header.html wird nicht neu generiert."
    echo ""
fi

# ── Output-Verzeichnis sicherstellen ─────────────────────────────────────────
# Doxygen legt nur die letzte Pfadebene an; fehlt z. B. build/ (nach einem
# Clean oder ohne vorherigen Build), schlaegt OUTPUT_DIRECTORY fehl.
mkdir -p "$OUTPUT_DIR"

# ── Output-Verzeichnis leeren (sauberer Build) ────────────────────────────────
if [ -d "$OUTPUT_DIR/html" ]; then
    echo "Lösche alten Build: $OUTPUT_DIR/html"
    rm -rf "$OUTPUT_DIR/html"
fi

# ── Dokumentation generieren ──────────────────────────────────────────────────
cd "$SCRIPT_DIR"
doxygen Doxyfile
EXIT_CODE=$?

if [ $EXIT_CODE -eq 0 ]; then
    echo ""
    echo "=== Fertig ==="
    echo "Dokumentation: $OUTPUT_DIR/html/index.html"
else
    echo ""
    echo "=== Fehler bei der Generierung (Exit-Code: $EXIT_CODE) ==="
fi

exit $EXIT_CODE
