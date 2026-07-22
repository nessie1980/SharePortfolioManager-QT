# Share Portfolio Manager

Cross-platform desktop application for managing stock portfolios.
Built with Qt/C++ for Windows and Linux.

## Features

- Track stock purchases, sales, dividends and brokerage fees
- Automatic market data retrieval via REST APIs (OnVista, Yahoo Finance)
- PDF invoice parsing (broker trade confirmations)
- SQLite storage — no server required
- Multi-language support (German, English) — switchable without rebuild
- Level-based logging with configurable filters

## Requirements

| Dependency | Version | Notes |
|---|---|---|
| Qt | 6.6+ | Core, Gui, Widgets, Sql, Network, Multimedia, Charts, LinguistTools (Pdf/PdfWidgets optional) |
| CMake | 3.21+ | |
| C++ Compiler | C++20 | GCC/Clang on Linux, MSVC (VS 2019/2022) on Windows |
| pdftotext / pdftoppm | current | Part of `poppler-utils`; used for PDF invoice parsing and as PDF-preview fallback if `Qt6::PdfWidgets` is unavailable |
| Graphviz | current | *(optional, for Doxygen diagrams)* |
| Doxygen | 1.10+ | *(optional, for API docs)* |

---

## Build — Linux

Tested on Ubuntu/Debian-based distributions.

**1. Install dependencies:**

```bash
sudo apt install qt6-base-dev qt6-tools-dev qt6-l10n-tools libqt6sql6-sqlite \
    libqt6charts6-dev libqt6pdf6-dev libqt6pdfwidgets6-dev \
    cmake build-essential poppler-utils
```

> `libqt6pdf6-dev` / `libqt6pdfwidgets6-dev` are optional. Without them, CMake
> falls back to the `pdftoppm`-based PDF preview automatically (see CMake
> status output).

**2. Clone and build:**

```bash
git clone https://github.com/nessie1980/SharePortfolioManager-QT.git
cd spm-qt
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

**3. Run the application:**

```bash
./bin/SharePortfolioManager
```

Or open `CMakeLists.txt` directly in **Qt Creator** and select your Qt kit.

---

## Build — Windows

Tested with Qt 6.7 / MSVC 2019+.

**1. Install prerequisites:**

- [Qt Online Installer](https://qt.io) → install a Qt 6.6+ kit for MSVC
  (e.g. `msvc2019_64`), including the modules **Charts** and, optionally,
  **Qt PDF** / **Qt PDF Widgets**
- Visual Studio 2019 or 2022 with the **"Desktop development with C++"**
  workload
- [CMake](https://cmake.org/download/) 3.21+
- [poppler-utils for Windows](https://github.com/oschwartz10612/poppler-windows/releases)
  (provides `pdftotext.exe` / `pdftoppm.exe`) — add its `bin` folder to `PATH`

**2. Clone and build** (in the *"x64 Native Tools Command Prompt for VS"*
or *"Developer PowerShell for VS"*):

```powershell
git clone https://github.com/nessie1980/SharePortfolioManager-QT.git
cd spm-qt
mkdir build
cd build
cmake .. -DCMAKE_PREFIX_PATH="C:/Qt/6.7.0/msvc2019_64" -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release --parallel
```

**3. Run the application:**

```powershell
.\bin\Release\SharePortfolioManager.exe
```

> On first run outside of Qt Creator, the executable needs the Qt DLLs on
> `PATH`, or run `windeployqt` once to copy them next to the `.exe`:
> ```powershell
> C:\Qt\6.7.0\msvc2019_64\bin\windeployqt.exe bin\Release\SharePortfolioManager.exe
> ```

**Alternative:** Open `CMakeLists.txt` directly in **Qt Creator** and select
your Qt/MSVC kit — this is the simplest path on Windows and handles the Qt
`PATH`/deployment setup automatically for local runs.

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| `Could NOT find Qt6 (missing: Qt6_DIR)` | CMake can't locate the Qt installation | Pass `-DCMAKE_PREFIX_PATH=/path/to/Qt6` (Linux) or the Qt kit dir (Windows) |
| `Qt6PdfWidgets not found` (CMake status, not an error) | Optional PDF-viewer module not installed | Safe to ignore — PDF preview falls back to `pdftoppm`; install `Qt6::PdfWidgets` if you want the native in-app viewer |
| App starts but market data / PDF parsing fails silently | `pdftotext`/`pdftoppm` not on `PATH` | Install `poppler-utils` (Linux) or the Windows poppler build and add its `bin` folder to `PATH` |
| Windows `.exe` fails to start with missing DLL errors | Qt DLLs not next to the executable | Run `windeployqt.exe` on the built binary, or add the Qt `bin` folder to `PATH` |

---

## Run Tests

```bash
cd build
ctest --output-on-failure
```

(Windows: `ctest -C Release --output-on-failure` from the `build` directory.)

## Generate Documentation

```bash
# 1. Download Doxygen Awesome theme (once)
cd docs/doxygen
chmod +x download-theme.sh && ./download-theme.sh

# 2. Generate API docs
bash generate-docs.sh

# 3. Open in browser
xdg-open ../../build/docs/html/index.html
```

## Project Structure

```
spm-qt/
├── libs/
│   ├── logger/       # Reusable logging library
│   └── parser/       # Reusable REST/PDF parser library
├── app/              # Main application (Qt Widgets)
├── docs/
│   ├── architecture/ # Architecture documentation (ARCHITECTURE.md)
│   ├── testing/      # Test documentation (TESTING.md)
│   └── doxygen/      # Doxygen config + Awesome theme
└── tests/            # Unit tests (Qt Test)
```

See [`docs/architecture/ARCHITECTURE.md`](docs/architecture/ARCHITECTURE.md) for full architecture documentation
and [`docs/testing/TESTING.md`](docs/testing/TESTING.md) for the test documentation.

## License

MIT License — Copyright (c) 2017 nessie1980
