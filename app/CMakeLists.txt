set(APP_SOURCES
    main.cpp
    Version.h.in
    AppStartup.h
    AppStartup.cpp
    IconProvider.h
    IconProvider.cpp

    # ── Core / Data layer ──────────────────────────────────────────────────
    models/ShareObject.h
    models/ShareObject.cpp
    models/BuyObject.h
    models/BuyObject.cpp
    models/SaleObject.h
    models/SaleObject.cpp
    models/DividendObject.h
    models/DividendObject.cpp
    models/BrokerageObject.h
    models/BrokerageObject.cpp
    models/DailyValuesObject.h
    models/DailyValuesObject.cpp
    models/ShareSplitObject.h
    models/ShareSplitObject.cpp

    # ── Repositories ──────────────────────────────────────────────────────
    repositories/ShareRepository.h
    repositories/ShareRepository.cpp
    repositories/BuyRepository.h
    repositories/BuyRepository.cpp
    repositories/SaleRepository.h
    repositories/SaleRepository.cpp
    repositories/DividendRepository.h
    repositories/DividendRepository.cpp
    repositories/BrokerageRepository.h
    repositories/BrokerageRepository.cpp
    repositories/DailyValuesRepository.h
    repositories/DailyValuesRepository.cpp
    repositories/ShareSplitRepository.h
    repositories/ShareSplitRepository.cpp
    core/DocumentRootMigrator.h
    core/DocumentRootMigrator.cpp
    core/SingleInstanceGuard.h
    core/SingleInstanceGuard.cpp

    # ── Configuration ──────────────────────────────────────────────────────
    config/AppSettings.h
    config/AppSettings.cpp
    config/WebSitesConfig.h
    config/WebSitesConfig.cpp
    config/DocumentsConfig.h
    config/DocumentsConfig.cpp

    # ── Forms – Main ──────────────────────────────────────────────────────
    forms/MainForm/MainWindow.h
    forms/MainForm/MainWindow.cpp
    forms/MainForm/TwoLineDelegate.h
    forms/MainForm/CenterIconDelegate.h

    # ── Utils ──────────────────────────────────────────────────────────────
    utils/ShareCalculator.h
    utils/ShareCalculator.cpp
    # Header-only, bewusst ohne .cpp — Begründung im Dateikopf (06.08.2026).
    utils/ShareUpdateRules.h
    utils/PortfolioSeriesCalculator.h
    utils/PortfolioSeriesCalculator.cpp
    utils/ShareSplitAdjuster.h
    utils/ShareSplitAdjuster.cpp
    utils/PdfTextExtractor.h
    utils/PdfTextExtractor.cpp
    utils/DocumentClassifier.h
    utils/DocumentClassifier.cpp

    # ── Widgets (shared) ──────────────────────────────────────────────────
    widgets/OverviewTabWidget.h
    widgets/OverviewTabWidget.cpp
    widgets/DocumentPreviewPanel.h
    widgets/DocumentPreviewPanel.cpp

    # ── Forms – Buys ──────────────────────────────────────────────────────
    forms/BuysForm/IViewBuyEdit.h
    forms/BuysForm/IModelBuyEdit.h
    forms/BuysForm/ModelBuyEdit.h
    forms/BuysForm/ModelBuyEdit.cpp
    forms/BuysForm/PresenterBuyEdit.h
    forms/BuysForm/PresenterBuyEdit.cpp
    forms/BuysForm/ViewBuyEdit.h
    forms/BuysForm/ViewBuyEdit.cpp

    # ── Forms – Sales ─────────────────────────────────────────────────────
    forms/SalesForm/IViewSaleEdit.h
    forms/SalesForm/IModelSaleEdit.h
    forms/SalesForm/ModelSaleEdit.h
    forms/SalesForm/ModelSaleEdit.cpp
    forms/SalesForm/PresenterSaleEdit.h
    forms/SalesForm/PresenterSaleEdit.cpp
    forms/SalesForm/ViewSaleEdit.h
    forms/SalesForm/ViewSaleEdit.cpp

    # ── Forms – Dividends ─────────────────────────────────────────────────
    forms/DividendForm/IViewDividendEdit.h
    forms/DividendForm/IModelDividendEdit.h
    forms/DividendForm/ModelDividendEdit.h
    forms/DividendForm/ModelDividendEdit.cpp
    forms/DividendForm/PresenterDividendEdit.h
    forms/DividendForm/PresenterDividendEdit.cpp
    forms/DividendForm/ViewDividendEdit.h
    forms/DividendForm/ViewDividendEdit.cpp

    # ── Forms – Brokerage ─────────────────────────────────────────────────
    forms/BrokeragesForm/IViewBrokerageEdit.h
    forms/BrokeragesForm/IModelBrokerageEdit.h
    forms/BrokeragesForm/ModelBrokerageEdit.h
    forms/BrokeragesForm/ModelBrokerageEdit.cpp
    forms/BrokeragesForm/PresenterBrokerageEdit.h
    forms/BrokeragesForm/PresenterBrokerageEdit.cpp
    forms/BrokeragesForm/ViewBrokerageEdit.h
    forms/BrokeragesForm/ViewBrokerageEdit.cpp

    # ── Forms – Share Add/Edit ────────────────────────────────────────────
    forms/ShareAddForm/IViewShareAdd.h
    forms/ShareAddForm/IModelShareAdd.h
    forms/ShareAddForm/ModelShareAdd.h
    forms/ShareAddForm/ModelShareAdd.cpp
    forms/ShareAddForm/PresenterShareAdd.h
    forms/ShareAddForm/PresenterShareAdd.cpp
    forms/ShareAddForm/ViewShareAdd.h
    forms/ShareAddForm/ViewShareAdd.cpp

    forms/ShareEditForm/IViewShareEdit.h
    forms/ShareEditForm/IModelShareEdit.h
    forms/ShareEditForm/ModelShareEdit.h
    forms/ShareEditForm/ModelShareEdit.cpp
    forms/ShareEditForm/PresenterShareEdit.h
    forms/ShareEditForm/PresenterShareEdit.cpp
    forms/ShareEditForm/ViewShareEdit.h
    forms/ShareEditForm/ViewShareEdit.cpp

    # ── Forms – Details & Chart ───────────────────────────────────────────
    forms/ShareDetailsForm/IViewShareDetails.h
    forms/ShareDetailsForm/IModelShareDetails.h
    forms/ShareDetailsForm/ModelShareDetails.h
    forms/ShareDetailsForm/ModelShareDetails.cpp
    forms/ShareDetailsForm/PresenterShareDetails.h
    forms/ShareDetailsForm/PresenterShareDetails.cpp
    forms/ShareDetailsForm/ViewShareDetails.h
    forms/ShareDetailsForm/ViewShareDetails.cpp
    forms/ChartForm/ChartTypes.h
    forms/ChartForm/IViewChart.h
    forms/ChartForm/IModelChart.h
    forms/ChartForm/ModelChart.h
    forms/ChartForm/ModelChart.cpp
    forms/ChartForm/PresenterChart.h
    forms/ChartForm/PresenterChart.cpp
    forms/ChartForm/ViewChart.h
    forms/ChartForm/ViewChart.cpp
    forms/ChartForm/ChartPopup.h
    forms/ChartForm/ChartPopup.cpp
    forms/PortfolioChartForm/PortfolioChartTypes.h
    forms/PortfolioChartForm/IViewPortfolioChart.h
    forms/PortfolioChartForm/IModelPortfolioChart.h
    forms/PortfolioChartForm/ModelPortfolioChart.h
    forms/PortfolioChartForm/ModelPortfolioChart.cpp
    forms/PortfolioChartForm/PresenterPortfolioChart.h
    forms/PortfolioChartForm/PresenterPortfolioChart.cpp
    forms/PortfolioChartForm/ViewPortfolioChart.h
    forms/PortfolioChartForm/ViewPortfolioChart.cpp

    # ── Forms – Settings ──────────────────────────────────────────────────
    forms/LoggerSettingsForm/LoggerSettingsForm.h
    forms/LoggerSettingsForm/LoggerSettingsForm.cpp
    forms/SoundSettingsForm/SoundSettingsForm.h
    forms/SoundSettingsForm/SoundSettingsForm.cpp
    forms/BackupSettingsForm/BackupSettingsForm.h
    forms/BackupSettingsForm/BackupSettingsForm.cpp
    forms/DocumentsSettingsForm/DocumentsSettingsForm.h
    forms/DocumentsSettingsForm/DocumentsSettingsForm.cpp
    forms/TraySettingsForm/TraySettingsForm.h
    forms/TraySettingsForm/TraySettingsForm.cpp
    forms/ApiSettingsForm/ApiSettingsForm.h
    forms/ApiSettingsForm/ApiSettingsForm.cpp

    # ── Forms – Misc ──────────────────────────────────────────────────────
    forms/BackupProgressForm/BackupWorker.h
    forms/BackupProgressForm/BackupWorker.cpp
    forms/BackupProgressForm/BackupProgressDialog.h
    forms/BackupProgressForm/BackupProgressDialog.cpp
    forms/AboutForm/AboutForm.h
    forms/AboutForm/AboutForm.cpp
    forms/OwnMessageBoxForm/OwnMessageBox.h
    forms/OwnMessageBoxForm/OwnMessageBox.cpp
)

# ── Version header (generiert aus Version.h.in) ────────────────────────────
# Zentrale Versionsnummer kommt aus project(SharePortfolioManager VERSION ...)
# in der Root-CMakeLists.txt — hier nur zu einem verwendbaren C++-Header
# umgewandelt. main.cpp verwendet SPM_VERSION_STRING statt eines Hardcodes.
# Siehe ARCHITECTURE.md, Abschnitt "Versionierung".
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/Version.h
    @ONLY
)

# ── Translation files (.ts) ────────────────────────────────────────────────
# qt_add_translations() in Qt >= 6.7 must be attached to an existing target.
# We attach it to the executable after qt_add_executable() below.
set(TS_FILES
    ${CMAKE_SOURCE_DIR}/translations/spm_de.ts
    ${CMAKE_SOURCE_DIR}/translations/spm_en.ts
    ../translations/spm_de.ts
)

# ── Resources (.qrc) ──────────────────────────────────────────────────────
qt_add_resources(APP_RESOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/resources.qrc
)

# ── Executable ────────────────────────────────────────────────────────────
qt_add_executable(SharePortfolioManager
    ${APP_SOURCES}
    ${APP_RESOURCES}
)

target_include_directories(SharePortfolioManager
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/core
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(SharePortfolioManager
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Sql
        Qt6::Network
        Qt6::Multimedia
        Qt6::Charts
        Database
        Logger
        Parser
)

# Link Qt PDF if available
if(Qt6PdfWidgets_FOUND)
    target_link_libraries(SharePortfolioManager PRIVATE Qt6::Pdf Qt6::PdfWidgets)
    target_compile_definitions(SharePortfolioManager PRIVATE SPM_HAVE_QTPDF)
endif()

set_target_properties(SharePortfolioManager PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
    WIN32_EXECUTABLE TRUE          # no console window on Windows release
    MACOSX_BUNDLE    TRUE
)

# ── Translations (attached to executable — required by Qt >= 6.7) ─────────
if(Qt6LinguistTools_FOUND)
    qt_add_translations(SharePortfolioManager
        TS_FILES ${TS_FILES}
        QM_FILES_OUTPUT_VARIABLE QM_FILES
        LUPDATE_OPTIONS -no-obsolete
    )
endif()

# ── Copy config and sound files to output directory after build ────────────
add_custom_command(TARGET SharePortfolioManager POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/config/WebSites.xml
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/WebSites.xml
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/config/Documents.xml
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Documents.xml
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sounds
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/sounds/UpdateFinished.wav
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sounds/UpdateFinished.wav
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/sounds/Error.wav
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sounds/Error.wav
    COMMENT "Copying config and sound files to ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)

# ── Install / Deploy ──────────────────────────────────────────────────────
install(TARGETS SharePortfolioManager
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/config/WebSites.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/config/Documents.xml
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(Qt6LinguistTools_FOUND AND QM_FILES)
    install(FILES ${QM_FILES} DESTINATION ${CMAKE_INSTALL_BINDIR}/translations)
endif()
