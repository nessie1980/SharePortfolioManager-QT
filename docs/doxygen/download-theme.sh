#!/bin/bash
# download-theme.sh
# Downloads the Doxygen Awesome CSS theme files.
# Run once before generating docs for the first time.
#
# Requirements: curl or wget
# Usage: cd docs/doxygen && ./download-theme.sh
#
# Pinned to v2.3.3 — compatible with Doxygen 1.9.8.
# To upgrade: change TAG below and regenerate header.html via
#   doxygen -w html theme/header.html theme/footer.html theme/doxygen.css

THEME_DIR="$(dirname "$0")/theme"
TAG="v2.3.3"
BASE_URL="https://raw.githubusercontent.com/jothepro/doxygen-awesome-css/${TAG}"

FILES=(
    "doxygen-awesome.css"
    "doxygen-awesome-sidebar-only.css"
    "doxygen-awesome-interactive-toc.js"
    "doxygen-awesome-paragraph-link.js"
    "doxygen-awesome-darkmode-toggle.js"
)

echo "Downloading Doxygen Awesome theme files (${TAG}) to $THEME_DIR ..."

for FILE in "${FILES[@]}"; do
    TARGET="$THEME_DIR/$FILE"
    echo "  [download] $FILE"
    if command -v curl &> /dev/null; then
        curl -sSL "$BASE_URL/$FILE" -o "$TARGET"
    elif command -v wget &> /dev/null; then
        wget -q "$BASE_URL/$FILE" -O "$TARGET"
    else
        echo "ERROR: neither curl nor wget found. Please install one."
        exit 1
    fi
done

echo ""
echo "Done. Now run:"
echo "  cd docs/doxygen && doxygen Doxyfile"
echo ""
echo "Output will be in: build/docs/html/index.html"
