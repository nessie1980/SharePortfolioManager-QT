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

| Dependency | Version | Install |
|---|---|---|
| Qt | 6.6+ | [qt.io](https://qt.io) or `sudo apt install qt6-base-dev qt6-tools-dev qt6-l10n-tools libqt6sql6-sqlite` |
| CMake | 3.21+ | `sudo apt install cmake` |
| pdftotext | current | `sudo apt install poppler-utils` |
| Graphviz | current | `sudo apt install graphviz` *(optional, for docs)* |
| Doxygen | 1.10+ | [doxygen.nl](https://doxygen.nl) *(optional, for docs)* |

## Build

```bash
git clone https://github.com/nessie1980/SharePortfolioManager-QT.git
cd spm-qt
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . --parallel
```

Or open `CMakeLists.txt` directly in **Qt Creator** and select your Qt kit.

## Run Tests

```bash
cd build
ctest --output-on-failure
```

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
