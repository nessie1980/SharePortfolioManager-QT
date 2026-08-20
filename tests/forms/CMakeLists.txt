find_package(Qt6 REQUIRED COMPONENTS Test Sql Widgets Multimedia Network Charts)

qt_add_resources(FORM_TEST_RESOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/resources/resources.qrc
)

qt_add_executable(tst_mainwindow tst_mainwindow.cpp
    ../../app/forms/MainForm/MainWindow.cpp
    ../parser/FakeNetworkAccessManager.h
    ../parser/FakeNetworkAccessManager.cpp
    ../../app/forms/LoggerSettingsForm/LoggerSettingsForm.cpp
    ../../app/forms/SoundSettingsForm/SoundSettingsForm.cpp
    ../../app/forms/BackupSettingsForm/BackupSettingsForm.h
    ../../app/forms/BackupSettingsForm/BackupSettingsForm.cpp
    ../../app/forms/DocumentsSettingsForm/DocumentsSettingsForm.h
    ../../app/forms/DocumentsSettingsForm/DocumentsSettingsForm.cpp
    ../../app/forms/TraySettingsForm/TraySettingsForm.h
    ../../app/forms/TraySettingsForm/TraySettingsForm.cpp
    ../../app/forms/AboutForm/AboutForm.cpp
    ../../app/forms/OwnMessageBoxForm/OwnMessageBox.h
    ../../app/forms/OwnMessageBoxForm/OwnMessageBox.cpp
    ../../app/forms/BackupProgressForm/BackupWorker.h
    ../../app/forms/BackupProgressForm/BackupWorker.cpp
    ../../app/forms/BackupProgressForm/BackupProgressDialog.h
    ../../app/forms/BackupProgressForm/BackupProgressDialog.cpp
    ../../app/forms/ApiSettingsForm/ApiSettingsForm.cpp
    ../../app/forms/ShareAddForm/ModelShareAdd.cpp
    ../../app/forms/ShareAddForm/PresenterShareAdd.cpp
    ../../app/forms/ShareAddForm/ViewShareAdd.cpp
    ../../app/forms/ShareEditForm/IViewShareEdit.h
    ../../app/forms/ShareEditForm/IModelShareEdit.h
    ../../app/forms/ShareEditForm/ModelShareEdit.h
    ../../app/forms/ShareEditForm/ModelShareEdit.cpp
    ../../app/forms/ShareEditForm/PresenterShareEdit.h
    ../../app/forms/ShareEditForm/PresenterShareEdit.cpp
    ../../app/forms/ShareEditForm/ViewShareEdit.h
    ../../app/forms/ShareEditForm/ViewShareEdit.cpp
    ../../app/forms/ShareSplitsForm/IViewShareSplitEdit.h
    ../../app/forms/ShareSplitsForm/IModelShareSplitEdit.h
    ../../app/forms/ShareSplitsForm/ModelShareSplitEdit.h
    ../../app/forms/ShareSplitsForm/ModelShareSplitEdit.cpp
    ../../app/forms/ShareSplitsForm/PresenterShareSplitEdit.h
    ../../app/forms/ShareSplitsForm/PresenterShareSplitEdit.cpp
    ../../app/forms/ShareSplitsForm/ViewShareSplitEdit.h
    ../../app/forms/ShareSplitsForm/ViewShareSplitEdit.cpp
    ../../app/forms/ShareDetailsForm/IViewShareDetails.h
    ../../app/forms/ShareDetailsForm/IModelShareDetails.h
    ../../app/forms/ShareDetailsForm/ModelShareDetails.h
    ../../app/forms/ShareDetailsForm/ModelShareDetails.cpp
    ../../app/forms/ShareDetailsForm/PresenterShareDetails.h
    ../../app/forms/ShareDetailsForm/PresenterShareDetails.cpp
    ../../app/forms/ShareDetailsForm/ViewShareDetails.h
    ../../app/forms/ShareDetailsForm/ViewShareDetails.cpp
    ../../app/widgets/OverviewTabWidget.h
    ../../app/widgets/OverviewTabWidget.cpp
    ../../app/widgets/DocumentPreviewPanel.h
    ../../app/widgets/DocumentPreviewPanel.cpp
    ../../app/forms/ChartForm/ChartTypes.h
    ../../app/forms/ChartForm/IViewChart.h
    ../../app/forms/ChartForm/IModelChart.h
    ../../app/forms/ChartForm/ModelChart.h
    ../../app/forms/ChartForm/ModelChart.cpp
    ../../app/forms/ChartForm/PresenterChart.h
    ../../app/forms/ChartForm/PresenterChart.cpp
    ../../app/forms/ChartForm/ViewChart.h
    ../../app/forms/ChartForm/ViewChart.cpp
    ../../app/forms/ChartForm/ChartPopup.h
    ../../app/forms/ChartForm/ChartPopup.cpp
    ../../app/forms/PortfolioChartForm/PortfolioChartTypes.h
    ../../app/forms/PortfolioChartForm/IViewPortfolioChart.h
    ../../app/forms/PortfolioChartForm/IModelPortfolioChart.h
    ../../app/forms/PortfolioChartForm/ModelPortfolioChart.h
    ../../app/forms/PortfolioChartForm/ModelPortfolioChart.cpp
    ../../app/forms/PortfolioChartForm/PresenterPortfolioChart.h
    ../../app/forms/PortfolioChartForm/PresenterPortfolioChart.cpp
    ../../app/forms/PortfolioChartForm/ViewPortfolioChart.h
    ../../app/forms/PortfolioChartForm/ViewPortfolioChart.cpp
    ../../app/forms/BuysForm/IViewBuyEdit.h
    ../../app/forms/BuysForm/IModelBuyEdit.h
    ../../app/forms/BuysForm/ModelBuyEdit.h
    ../../app/forms/BuysForm/ModelBuyEdit.cpp
    ../../app/forms/BuysForm/PresenterBuyEdit.h
    ../../app/forms/BuysForm/PresenterBuyEdit.cpp
    ../../app/forms/BuysForm/ViewBuyEdit.h
    ../../app/forms/BuysForm/ViewBuyEdit.cpp
    ../../app/forms/SalesForm/IViewSaleEdit.h
    ../../app/forms/SalesForm/IModelSaleEdit.h
    ../../app/forms/SalesForm/ModelSaleEdit.h
    ../../app/forms/SalesForm/ModelSaleEdit.cpp
    ../../app/forms/SalesForm/PresenterSaleEdit.h
    ../../app/forms/SalesForm/PresenterSaleEdit.cpp
    ../../app/forms/SalesForm/ViewSaleEdit.h
    ../../app/forms/SalesForm/ViewSaleEdit.cpp
    ../../app/forms/DividendForm/IViewDividendEdit.h
    ../../app/forms/DividendForm/IModelDividendEdit.h
    ../../app/forms/DividendForm/ModelDividendEdit.h
    ../../app/forms/DividendForm/ModelDividendEdit.cpp
    ../../app/forms/DividendForm/PresenterDividendEdit.h
    ../../app/forms/DividendForm/PresenterDividendEdit.cpp
    ../../app/forms/DividendForm/ViewDividendEdit.h
    ../../app/forms/DividendForm/ViewDividendEdit.cpp
    ../../app/forms/BrokeragesForm/IModelBrokerageEdit.h
    ../../app/forms/BrokeragesForm/IViewBrokerageEdit.h
    ../../app/forms/BrokeragesForm/ModelBrokerageEdit.h
    ../../app/forms/BrokeragesForm/ModelBrokerageEdit.cpp
    ../../app/forms/BrokeragesForm/PresenterBrokerageEdit.h
    ../../app/forms/BrokeragesForm/PresenterBrokerageEdit.cpp
    ../../app/forms/BrokeragesForm/ViewBrokerageEdit.h
    ../../app/forms/BrokeragesForm/ViewBrokerageEdit.cpp
    ../../app/config/AppSettings.cpp
    ../../app/config/WebSitesConfig.cpp
    ../../app/config/DocumentsConfig.cpp
    ../../app/AppStartup.cpp
    ../../app/IconProvider.cpp
    ../../app/models/ShareObject.cpp
    ../../app/models/BuyObject.cpp
    ../../app/models/SaleObject.cpp
    ../../app/models/DividendObject.cpp
    ../../app/models/BrokerageObject.cpp
    ../../app/models/DailyValuesObject.cpp
    ../../app/repositories/ShareRepository.cpp
    ../../app/repositories/BuyRepository.cpp
    ../../app/repositories/SaleRepository.cpp
    ../../app/repositories/DividendRepository.cpp
    ../../app/repositories/BrokerageRepository.cpp
    ../../app/repositories/DailyValuesRepository.cpp
    ../../app/core/DocumentRootMigrator.h
    ../../app/core/DocumentRootMigrator.cpp
    ../../app/models/ShareSplitObject.h
    ../../app/models/ShareSplitObject.cpp
    ../../app/repositories/ShareSplitRepository.h
    ../../app/repositories/ShareSplitRepository.cpp
    ../../app/utils/ShareCalculator.cpp
    ../../app/utils/ShareSplitAdjuster.h
    ../../app/utils/ShareSplitAdjuster.cpp
    ../../app/utils/ShareSplitHint.h
    ../../app/utils/ShareSplitHint.cpp
    ../../app/utils/SplitPriceJumpDetector.h
    ../../app/utils/SplitPriceJumpDetector.cpp
    ../../app/utils/SplitAdjustmentAudit.h
    ../../app/utils/SplitAdjustmentAudit.cpp
    ../../app/utils/SaleFifoAllocator.h
    ../../app/utils/SaleFifoAllocator.cpp
    ../../app/utils/PortfolioSeriesCalculator.cpp
    ../../app/utils/PdfTextExtractor.h
    ../../app/utils/PdfTextExtractor.cpp
    ../../app/utils/DocumentClassifier.h
    ../../app/utils/DocumentClassifier.cpp
    ${FORM_TEST_RESOURCES}
)

target_include_directories(tst_mainwindow
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
            # Feature (01.08.2026): Version.h wird von CMake nur in app/CMakeLists.txt
            # via configure_file(Version.h.in ... ${CMAKE_CURRENT_BINARY_DIR}/Version.h)
            # generiert — CMAKE_CURRENT_BINARY_DIR ist zu diesem Zeitpunkt
            # ${CMAKE_BINARY_DIR}/app (spiegelt den Quellbaum). tst_mainwindow.cpp
            # braucht SPM_VERSION_STRING für den Fenstertitel-Versionstest, siehe
            # tst_mainwindow.cpp main() und ARCHITECTURE.md, "Versionierung".
            ${CMAKE_BINARY_DIR}/app
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/ShareEditForm
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/BuysForm
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/SalesForm
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/DividendForm
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/BrokeragesForm
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/ShareSplitsForm
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/OwnMessageBoxForm
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/BackupProgressForm
            ${CMAKE_CURRENT_SOURCE_DIR}/../../libs/logger/src
            ${CMAKE_CURRENT_SOURCE_DIR}/../../libs/parser/src
)

set_target_properties(tst_mainwindow PROPERTIES
    AUTOMOC ON
    AUTORCC ON
)

target_link_libraries(tst_mainwindow
    PRIVATE Qt6::Test Qt6::Sql Qt6::Widgets Qt6::Multimedia Qt6::Network Qt6::Charts Database Logger Parser
)

if(Qt6PdfWidgets_FOUND)
    target_link_libraries(tst_mainwindow PRIVATE Qt6::Pdf Qt6::PdfWidgets)
    target_compile_definitions(tst_mainwindow PRIVATE SPM_HAVE_QTPDF)
endif()

add_test(NAME tst_mainwindow COMMAND tst_mainwindow)

# Copy config files so tests can find them next to the test executable
add_custom_command(TARGET tst_mainwindow POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../../app/config/WebSites.xml
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/WebSites.xml
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../../app/config/Documents.xml
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Documents.xml
    COMMENT "Copying config files to ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)

# ─────────────────────────────────────────────────────────────────────────────
# tst_buysform — BuysForm MVP triad (ModelBuyEdit, PresenterBuyEdit, ViewBuyEdit)
# Split out of tst_mainwindow once the BuysForm test count grew large enough
# to warrant its own executable.
# ─────────────────────────────────────────────────────────────────────────────

qt_add_executable(tst_buysform tst_buysform.cpp
    ../../app/models/ShareSplitObject.h
    ../../app/models/ShareSplitObject.cpp
    ../../app/repositories/ShareSplitRepository.h
    ../../app/repositories/ShareSplitRepository.cpp
    ../../app/forms/BuysForm/IViewBuyEdit.h
    ../../app/forms/BuysForm/IModelBuyEdit.h
    ../../app/forms/BuysForm/ModelBuyEdit.h
    ../../app/forms/BuysForm/ModelBuyEdit.cpp
    ../../app/forms/BuysForm/PresenterBuyEdit.h
    ../../app/forms/BuysForm/PresenterBuyEdit.cpp
    ../../app/forms/BuysForm/ViewBuyEdit.h
    ../../app/forms/BuysForm/ViewBuyEdit.cpp
    ../../app/widgets/OverviewTabWidget.h
    ../../app/widgets/OverviewTabWidget.cpp
    ../../app/widgets/DocumentPreviewPanel.h
    ../../app/widgets/DocumentPreviewPanel.cpp
    ../../app/forms/OwnMessageBoxForm/OwnMessageBox.h
    ../../app/forms/OwnMessageBoxForm/OwnMessageBox.cpp
    ../../app/config/AppSettings.cpp
    ../../app/config/DocumentsConfig.cpp
    ../../app/core/DocumentRootMigrator.h
    ../../app/core/DocumentRootMigrator.cpp
    ../../app/IconProvider.cpp
    ../../app/models/ShareObject.cpp
    ../../app/models/BuyObject.cpp
    ../../app/models/SaleObject.cpp
    ../../app/models/DividendObject.cpp
    ../../app/models/BrokerageObject.cpp
    ../../app/repositories/ShareRepository.cpp
    ../../app/repositories/BuyRepository.cpp
    ../../app/repositories/SaleRepository.cpp
    ../../app/repositories/DividendRepository.cpp
    ../../app/repositories/BrokerageRepository.cpp
    ../../app/utils/ShareSplitAdjuster.h
    ../../app/utils/ShareSplitAdjuster.cpp
    ../../app/utils/ShareSplitHint.h
    ../../app/utils/ShareSplitHint.cpp
    ../../app/utils/PdfTextExtractor.h
    ../../app/utils/PdfTextExtractor.cpp
    ../../app/utils/DocumentClassifier.h
    ../../app/utils/DocumentClassifier.cpp
    ${FORM_TEST_RESOURCES}
)

target_include_directories(tst_buysform
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/BuysForm
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/OwnMessageBoxForm
            ${CMAKE_CURRENT_SOURCE_DIR}/../../libs/logger/src
            ${CMAKE_CURRENT_SOURCE_DIR}/../../libs/parser/src
)

set_target_properties(tst_buysform PROPERTIES
    AUTOMOC ON
    AUTORCC ON
)

target_link_libraries(tst_buysform
    PRIVATE Qt6::Test Qt6::Sql Qt6::Widgets Database Logger Parser
)

if(Qt6PdfWidgets_FOUND)
    target_link_libraries(tst_buysform PRIVATE Qt6::Pdf Qt6::PdfWidgets)
    target_compile_definitions(tst_buysform PRIVATE SPM_HAVE_QTPDF)
endif()

add_test(NAME tst_buysform COMMAND tst_buysform)

add_custom_command(TARGET tst_buysform POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../../app/config/WebSites.xml
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/WebSites.xml
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../../app/config/Documents.xml
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Documents.xml
    COMMENT "Copying config files to ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)

# ─────────────────────────────────────────────────────────────────────────────
# tst_shareeditform — ViewShareEdit (and its full dependency tree)
# Split out of tst_mainwindow so every Form has its own executable.
# Note: ViewShareEdit.cpp pulls in all four sub-Form trios (BuysForm,
# SalesForm, DividendForm, BrokeragesForm) as compile-time dependencies.
# ─────────────────────────────────────────────────────────────────────────────

qt_add_executable(tst_shareeditform tst_shareeditform.cpp
    ../../app/forms/ShareEditForm/IViewShareEdit.h
    ../../app/forms/ShareEditForm/IModelShareEdit.h
    ../../app/forms/ShareEditForm/ModelShareEdit.h
    ../../app/forms/ShareEditForm/ModelShareEdit.cpp
    ../../app/forms/ShareEditForm/PresenterShareEdit.h
    ../../app/forms/ShareEditForm/PresenterShareEdit.cpp
    ../../app/forms/ShareEditForm/ViewShareEdit.h
    ../../app/forms/ShareEditForm/ViewShareEdit.cpp
    ../../app/forms/ShareSplitsForm/IViewShareSplitEdit.h
    ../../app/forms/ShareSplitsForm/IModelShareSplitEdit.h
    ../../app/forms/ShareSplitsForm/ModelShareSplitEdit.h
    ../../app/forms/ShareSplitsForm/ModelShareSplitEdit.cpp
    ../../app/forms/ShareSplitsForm/PresenterShareSplitEdit.h
    ../../app/forms/ShareSplitsForm/PresenterShareSplitEdit.cpp
    ../../app/forms/ShareSplitsForm/ViewShareSplitEdit.h
    ../../app/forms/ShareSplitsForm/ViewShareSplitEdit.cpp
    ../../app/forms/BuysForm/IViewBuyEdit.h
    ../../app/forms/BuysForm/IModelBuyEdit.h
    ../../app/forms/BuysForm/ModelBuyEdit.h
    ../../app/forms/BuysForm/ModelBuyEdit.cpp
    ../../app/forms/BuysForm/PresenterBuyEdit.h
    ../../app/forms/BuysForm/PresenterBuyEdit.cpp
    ../../app/forms/BuysForm/ViewBuyEdit.h
    ../../app/forms/BuysForm/ViewBuyEdit.cpp
    ../../app/forms/SalesForm/IViewSaleEdit.h
    ../../app/forms/SalesForm/IModelSaleEdit.h
    ../../app/forms/SalesForm/ModelSaleEdit.h
    ../../app/forms/SalesForm/ModelSaleEdit.cpp
    ../../app/forms/SalesForm/PresenterSaleEdit.h
    ../../app/forms/SalesForm/PresenterSaleEdit.cpp
    ../../app/forms/SalesForm/ViewSaleEdit.h
    ../../app/forms/SalesForm/ViewSaleEdit.cpp
    ../../app/forms/DividendForm/IViewDividendEdit.h
    ../../app/forms/DividendForm/IModelDividendEdit.h
    ../../app/forms/DividendForm/ModelDividendEdit.h
    ../../app/forms/DividendForm/ModelDividendEdit.cpp
    ../../app/forms/DividendForm/PresenterDividendEdit.h
    ../../app/forms/DividendForm/PresenterDividendEdit.cpp
    ../../app/forms/DividendForm/ViewDividendEdit.h
    ../../app/forms/DividendForm/ViewDividendEdit.cpp
    ../../app/forms/BrokeragesForm/IViewBrokerageEdit.h
    ../../app/forms/BrokeragesForm/IModelBrokerageEdit.h
    ../../app/forms/BrokeragesForm/ModelBrokerageEdit.h
    ../../app/forms/BrokeragesForm/ModelBrokerageEdit.cpp
    ../../app/forms/BrokeragesForm/PresenterBrokerageEdit.h
    ../../app/forms/BrokeragesForm/PresenterBrokerageEdit.cpp
    ../../app/forms/BrokeragesForm/ViewBrokerageEdit.h
    ../../app/forms/BrokeragesForm/ViewBrokerageEdit.cpp
    ../../app/widgets/OverviewTabWidget.h
    ../../app/widgets/OverviewTabWidget.cpp
    ../../app/widgets/DocumentPreviewPanel.h
    ../../app/widgets/DocumentPreviewPanel.cpp
    ../../app/forms/OwnMessageBoxForm/OwnMessageBox.h
    ../../app/forms/OwnMessageBoxForm/OwnMessageBox.cpp
    ../../app/config/AppSettings.cpp
    ../../app/config/WebSitesConfig.cpp
    ../../app/config/DocumentsConfig.cpp
    ../../app/core/DocumentRootMigrator.h
    ../../app/core/DocumentRootMigrator.cpp
    ../../app/IconProvider.cpp
    ../../app/models/ShareObject.cpp
    ../../app/models/BuyObject.cpp
    ../../app/models/SaleObject.cpp
    ../../app/models/DividendObject.cpp
    ../../app/models/BrokerageObject.cpp
    ../../app/models/DailyValuesObject.cpp
    ../../app/repositories/ShareRepository.cpp
    ../../app/repositories/BuyRepository.cpp
    ../../app/repositories/SaleRepository.cpp
    ../../app/repositories/DividendRepository.cpp
    ../../app/repositories/BrokerageRepository.cpp
    ../../app/repositories/DailyValuesRepository.cpp
    ../../app/models/ShareSplitObject.h
    ../../app/models/ShareSplitObject.cpp
    ../../app/repositories/ShareSplitRepository.h
    ../../app/repositories/ShareSplitRepository.cpp
    ../../app/utils/ShareSplitAdjuster.h
    ../../app/utils/ShareSplitAdjuster.cpp
    ../../app/utils/ShareSplitHint.h
    ../../app/utils/ShareSplitHint.cpp
    ../../app/utils/SplitPriceJumpDetector.h
    ../../app/utils/SplitPriceJumpDetector.cpp
    ../../app/utils/SaleFifoAllocator.h
    ../../app/utils/SaleFifoAllocator.cpp
    ../../app/utils/PdfTextExtractor.h
    ../../app/utils/PdfTextExtractor.cpp
    ../../app/utils/DocumentClassifier.h
    ../../app/utils/DocumentClassifier.cpp
    ${FORM_TEST_RESOURCES}
)

target_include_directories(tst_shareeditform
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/ShareEditForm
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/BuysForm
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/SalesForm
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/DividendForm
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/BrokeragesForm
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/ShareSplitsForm
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/OwnMessageBoxForm
            ${CMAKE_CURRENT_SOURCE_DIR}/../../libs/logger/src
            ${CMAKE_CURRENT_SOURCE_DIR}/../../libs/parser/src
)

set_target_properties(tst_shareeditform PROPERTIES
    AUTOMOC ON
    AUTORCC ON
)

target_link_libraries(tst_shareeditform
    PRIVATE Qt6::Test Qt6::Sql Qt6::Widgets Qt6::Multimedia Database Logger Parser
)

if(Qt6PdfWidgets_FOUND)
    target_link_libraries(tst_shareeditform PRIVATE Qt6::Pdf Qt6::PdfWidgets)
    target_compile_definitions(tst_shareeditform PRIVATE SPM_HAVE_QTPDF)
endif()

add_test(NAME tst_shareeditform COMMAND tst_shareeditform)

add_custom_command(TARGET tst_shareeditform POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../../app/config/WebSites.xml
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/WebSites.xml
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../../app/config/Documents.xml
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Documents.xml
    COMMENT "Copying config files to ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)

# ─────────────────────────────────────────────────────────────────────────────
# tst_backupsettingsform — BackupSettingsForm (einzelner QDialog, kein
# IView/IModel/Presenter-Triple, analog LoggerSettingsForm/SoundSettingsForm).
# Eigene Executable statt Erweiterung von tst_mainwindow.cpp (08.07.2026) —
# braucht weder Datenbank noch MainWindow, daher deutlich schlanker als
# tst_buysform: nur AppSettings + IconProvider als Compile-Abhängigkeiten.
# ─────────────────────────────────────────────────────────────────────────────

qt_add_executable(tst_backupsettingsform tst_backupsettingsform.cpp
    ../../app/forms/BackupSettingsForm/BackupSettingsForm.h
    ../../app/forms/BackupSettingsForm/BackupSettingsForm.cpp
    ../../app/config/AppSettings.cpp
    ../../app/IconProvider.cpp
    ${FORM_TEST_RESOURCES}
)

target_include_directories(tst_backupsettingsform
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/BackupSettingsForm
)

set_target_properties(tst_backupsettingsform PROPERTIES
    AUTOMOC ON
    AUTORCC ON
)

target_link_libraries(tst_backupsettingsform
    PRIVATE Qt6::Test Qt6::Widgets
)

add_test(NAME tst_backupsettingsform COMMAND tst_backupsettingsform)

# ─────────────────────────────────────────────────────────────────────────────
# tst_traysettingsform — TraySettingsForm (einzelner QDialog, kein
# IView/IModel/Presenter-Triple, analog LoggerSettingsForm/BackupSettingsForm/
# SoundSettingsForm). Eigene Executable statt Erweiterung von
# tst_mainwindow.cpp (03.08.2026, gleiche Begründung wie
# tst_backupsettingsform) — braucht weder Datenbank noch MainWindow, daher nur
# AppSettings + IconProvider als Compile-Abhängigkeiten.
# ─────────────────────────────────────────────────────────────────────────────

qt_add_executable(tst_traysettingsform tst_traysettingsform.cpp
    ../../app/forms/TraySettingsForm/TraySettingsForm.h
    ../../app/forms/TraySettingsForm/TraySettingsForm.cpp
    ../../app/config/AppSettings.cpp
    ../../app/IconProvider.cpp
    ${FORM_TEST_RESOURCES}
)

target_include_directories(tst_traysettingsform
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/TraySettingsForm
)

set_target_properties(tst_traysettingsform PROPERTIES
    AUTOMOC ON
    AUTORCC ON
)

target_link_libraries(tst_traysettingsform
    PRIVATE Qt6::Test Qt6::Widgets
)

add_test(NAME tst_traysettingsform COMMAND tst_traysettingsform)

# ─────────────────────────────────────────────────────────────────────────────
# tst_sharedetailsform — PresenterShareDetails (Fake-View/Fake-Model, siehe
# ARCHITECTURE.md "ShareDetailsForm-Details"). Deckt die "Komplette
# Depotbewertung"-Box (neu gebaut 09.07.2026) sowie, seit 13.07.2026, die
# Gewinne/Verluste-, Dividenden- und Kosten-Tabs ab (nur Depotwert-Modus).
# Kein QDialog, keine Datenbank, kein ShareCalculator.cpp nötig — braucht nur
# PresenterShareDetails.cpp sowie ShareObject.cpp/SaleObject.cpp/
# DividendObject.cpp/BrokerageObject.cpp (für deren Volltyp-Konstruktoren, da
# die Fakes in tst_sharedetailsform.cpp echte Instanzen dieser vier Typen
# konstruieren) als Compile-Abhängigkeit. Seit 11.08.2026 (Aktiensplit-
# Behandlung, Phase 3c) kommt ShareSplitObject.cpp dazu: die Tests prüfen,
# dass PresenterShareDetails die Splits an populateGewinneVerluste()/
# populateDividenden() durchreicht, und konstruieren dafür echte
# ShareSplitObject-Instanzen. ShareSplitAdjuster.cpp/ShareSplitHint.cpp
# werden weiterhin NICHT gebraucht — die Umrechnung und die Marker-Texte
# liegen in der View, die hier Fake ist. ModelShareDetails.cpp wird bewusst
# NICHT mitkompiliert — die Tests laufen ausschließlich gegen die
# Fake-Implementierungen von IViewShareDetails/IModelShareDetails, ohne
# Repository- oder Qt6::Sql-Abhängigkeit. OverviewTabWidget wird hier
# ebenfalls NICHT gebraucht — die drei neuen Tabs werden nur über die reinen
# Objekt-Listen getestet, die View (mit OverviewTabWidget) selbst ist Fake.
# Schlankstes Test-Target im Projekt.
# ─────────────────────────────────────────────────────────────────────────────

qt_add_executable(tst_sharedetailsform tst_sharedetailsform.cpp
    ../../app/forms/ShareDetailsForm/IViewShareDetails.h
    ../../app/forms/ShareDetailsForm/IModelShareDetails.h
    ../../app/forms/ShareDetailsForm/PresenterShareDetails.h
    ../../app/forms/ShareDetailsForm/PresenterShareDetails.cpp
    ../../app/models/ShareObject.cpp
    ../../app/models/SaleObject.cpp
    ../../app/models/DividendObject.cpp
    ../../app/models/BrokerageObject.cpp
    ../../app/models/ShareSplitObject.cpp
)

target_include_directories(tst_sharedetailsform
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/ShareDetailsForm
)

set_target_properties(tst_sharedetailsform PROPERTIES
    AUTOMOC ON
)

# Qt6::Gui wird ausschließlich für QColor (PresenterShareDetails.h) benötigt —
# kommt bereits transitiv über die oben angeforderte Komponente Widgets mit,
# eine eigene find_package()-Komponente ist dafür nicht nötig.
target_link_libraries(tst_sharedetailsform
    PRIVATE Qt6::Test Qt6::Core Qt6::Gui
)

add_test(NAME tst_sharedetailsform COMMAND tst_sharedetailsform)

# ─────────────────────────────────────────────────────────────────────────────
# tst_chartform — PresenterChart (Fake-View/Fake-Model, siehe ARCHITECTURE.md
# "ChartForm-Details"). Kein QWidget, keine QtCharts-Instanziierung, keine DB —
# läuft ausschließlich gegen die Fake-Implementierungen von IViewChart/
# IModelChart, analog tst_sharedetailsform. Braucht nur DailyValuesObject.cpp
# (für dessen einfachen POCO-Konstruktor) als Compile-Abhängigkeit — Buy-/
# SaleObject werden nicht gebraucht, da FakeModelChart::latestBuy()/
# latestSale() direkt ChartReferenceInfo zurückgeben, ohne echte Buy-/
# SaleObject-Instanzen.
#
# Bewusst OHNE Datenbanktests für ModelChart selbst (Aktiensplit-Behandlung,
# Phase 2b, 07.08.2026, siehe ARCHITECTURE.md "Offene Punkte"): eine
# DB-gestützte Prüfung von ModelChart::loadDailyValues()/heldVolumeSeries()/
# etc. würde Qt6::Sql, Database und sämtliche Repositories neu in dieses
# bewusst schlanke, DB-freie Ziel hineinziehen. Die Split-Umrechnung selbst
# nutzt ausschliesslich bereits unabhängig getestete ShareSplitAdjuster-
# Funktionen (siehe tst_sharesplitadjuster.cpp) nach demselben, in
# ModelPortfolioChart bereits DB-getesteten Muster (siehe
# TestModelPortfolioChart in tst_portfoliochartform.cpp). Eine eigene
# DB-gestützte ModelChart-Testabdeckung wäre ein sinnvoller eigener
# Testziel-Zuschnitt (analog tst_sharecalculator), aber ein separater
# Schritt, keine Voraussetzung für diese Phase.
# ─────────────────────────────────────────────────────────────────────────────

qt_add_executable(tst_chartform tst_chartform.cpp
    ../../app/forms/ChartForm/ChartTypes.h
    ../../app/forms/ChartForm/IViewChart.h
    ../../app/forms/ChartForm/IModelChart.h
    ../../app/forms/ChartForm/PresenterChart.h
    ../../app/forms/ChartForm/PresenterChart.cpp
    ../../app/models/DailyValuesObject.cpp
)

target_include_directories(tst_chartform
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/ChartForm
)

set_target_properties(tst_chartform PROPERTIES
    AUTOMOC ON
)

# Qt6::Gui wird ausschließlich für QColor (ChartTypes.h) benötigt — kommt
# bereits transitiv über die oben angeforderte Komponente Widgets mit, eine
# eigene find_package()-Komponente ist dafür nicht nötig (siehe tst_sharedetailsform).
target_link_libraries(tst_chartform
    PRIVATE Qt6::Test Qt6::Core Qt6::Gui
)

add_test(NAME tst_chartform COMMAND tst_chartform)

# ─────────────────────────────────────────────────────────────────────────────
# tst_overviewtabwidget — OverviewTabWidget direkt (echtes QWidget, siehe
# ARCHITECTURE.md "OverviewTabWidget-Details"). Bewusste Ausnahme vom
# Fake-View/Fake-Model-Muster der übrigen Form-Tests: OverviewTabWidget hat
# keinen eigenen Presenter, ist ein wiederverwendbares, in sich geschlossenes
# Anzeige-Widget (Spaltendefinitionen + Populate-Callbacks als reine
# std::function-Argumente) — analog zu tst_backupsettingsform, das ebenfalls
# einen echten QDialog direkt testet statt über eine Fake-Schicht. Keine
# Datenbank nötig.
# ─────────────────────────────────────────────────────────────────────────────

qt_add_executable(tst_overviewtabwidget tst_overviewtabwidget.cpp
    ../../app/widgets/OverviewTabWidget.h
    ../../app/widgets/OverviewTabWidget.cpp
)

target_include_directories(tst_overviewtabwidget
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
)

set_target_properties(tst_overviewtabwidget PROPERTIES
    AUTOMOC ON
)

target_link_libraries(tst_overviewtabwidget
    PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets
)

add_test(NAME tst_overviewtabwidget COMMAND tst_overviewtabwidget)

# ─────────────────────────────────────────────────────────────────────────────
# tst_documentssettingsform — DocumentsSettingsForm + DocumentRootMigrator
# (Root-Verzeichnis für Dokumente, 19.07.2026). Eigene Executable statt
# Erweiterung von tst_mainwindow.cpp, analog tst_backupsettingsform — anders
# als dort braucht DocumentRootMigrator aber eine echte Datenbank
# (Repositories), daher zusätzlich Database/Repositories/Models als
# Abhängigkeiten und Qt6::Sql beim Linken.
# ─────────────────────────────────────────────────────────────────────────────

qt_add_executable(tst_documentssettingsform tst_documentssettingsform.cpp
    ../../app/models/ShareSplitObject.h
    ../../app/models/ShareSplitObject.cpp
    ../../app/repositories/ShareSplitRepository.h
    ../../app/repositories/ShareSplitRepository.cpp
    ../../app/forms/DocumentsSettingsForm/DocumentsSettingsForm.h
    ../../app/forms/DocumentsSettingsForm/DocumentsSettingsForm.cpp
    ../../app/core/DocumentRootMigrator.h
    ../../app/core/DocumentRootMigrator.cpp
    ../../app/config/AppSettings.cpp
    ../../app/IconProvider.cpp
    ../../app/forms/OwnMessageBoxForm/OwnMessageBox.h
    ../../app/forms/OwnMessageBoxForm/OwnMessageBox.cpp
    ../../app/models/ShareObject.cpp
    ../../app/models/BuyObject.cpp
    ../../app/models/SaleObject.cpp
    ../../app/models/BrokerageObject.cpp
    ../../app/models/DividendObject.cpp
    ../../app/repositories/ShareRepository.cpp
    ../../app/repositories/BuyRepository.cpp
    ../../app/repositories/SaleRepository.cpp
    ../../app/repositories/BrokerageRepository.cpp
    ../../app/repositories/DividendRepository.cpp
)

target_include_directories(tst_documentssettingsform
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/DocumentsSettingsForm
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/core
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/OwnMessageBoxForm
)

set_target_properties(tst_documentssettingsform PROPERTIES
    AUTOMOC ON
    AUTORCC ON
)

target_link_libraries(tst_documentssettingsform
    PRIVATE Qt6::Test Qt6::Sql Qt6::Widgets Database Logger
)

add_test(NAME tst_documentssettingsform COMMAND tst_documentssettingsform)

# ─────────────────────────────────────────────────────────────────────────────
# tst_portfoliochartform — PresenterPortfolioChart (Fake-View/Fake-Model,
# analog tst_chartform). Depotwert-Chart, Feature 05.08.2026.
#
# Anders als tst_chartform braucht dieses Ziel Qt6::Sql und die Database-
# Bibliothek: der Presenter ruft PortfolioSeriesCalculator auf, der für die
# Rundung ShareCalculator::roundAway() verwendet, und ShareCalculator.cpp
# bindet die Repositories ein — seit Phase 2 der Aktiensplit-Behandlung
# (07.08.2026, siehe ARCHITECTURE.md "Offene Punkte") zusätzlich
# ShareSplitRepository/ShareSplitObject/ShareSplitAdjuster. TestPortfolio-
# ChartForm selbst rührt keine Datenbank an (reine Fake-basierte Presenter-
# Tests) — seit Phase 2b (07.08.2026) läuft in derselben Datei zusätzlich
# TestModelPortfolioChart gegen eine echte In-Memory-SQLite-Datenbank, um
# ModelPortfolioChart::loadPortfolioInput() direkt zu prüfen; daher auch
# ModelPortfolioChart.cpp selbst sowie ShareObject/ShareRepository und
# DailyValuesObject/DailyValuesRepository als Quellen (vorher nicht nötig, da
# TestPortfolioChartForm ausschliesslich über FakeModelPortfolioChart lief).
# ─────────────────────────────────────────────────────────────────────────────
qt_add_executable(tst_portfoliochartform tst_portfoliochartform.cpp
    ../../app/forms/PortfolioChartForm/PortfolioChartTypes.h
    ../../app/forms/PortfolioChartForm/IViewPortfolioChart.h
    ../../app/forms/PortfolioChartForm/IModelPortfolioChart.h
    ../../app/forms/PortfolioChartForm/PresenterPortfolioChart.h
    ../../app/forms/PortfolioChartForm/PresenterPortfolioChart.cpp
    ../../app/forms/PortfolioChartForm/ModelPortfolioChart.h
    ../../app/forms/PortfolioChartForm/ModelPortfolioChart.cpp
    ../../app/models/ShareObject.cpp
    ../../app/models/BuyObject.cpp
    ../../app/models/SaleObject.cpp
    ../../app/models/BrokerageObject.cpp
    ../../app/models/DividendObject.cpp
    ../../app/models/DailyValuesObject.cpp
    ../../app/repositories/ShareRepository.cpp
    ../../app/repositories/BuyRepository.cpp
    ../../app/repositories/SaleRepository.cpp
    ../../app/repositories/BrokerageRepository.cpp
    ../../app/repositories/DividendRepository.cpp
    ../../app/repositories/DailyValuesRepository.cpp
    ../../app/models/ShareSplitObject.h
    ../../app/models/ShareSplitObject.cpp
    ../../app/repositories/ShareSplitRepository.h
    ../../app/repositories/ShareSplitRepository.cpp
    ../../app/utils/ShareCalculator.cpp
    ../../app/utils/ShareSplitAdjuster.h
    ../../app/utils/ShareSplitAdjuster.cpp
    ../../app/utils/PortfolioSeriesCalculator.cpp
)

target_include_directories(tst_portfoliochartform
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/core
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/PortfolioChartForm
)

set_target_properties(tst_portfoliochartform PROPERTIES
    AUTOMOC ON
)

target_link_libraries(tst_portfoliochartform
    PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Sql Database
)

add_test(NAME tst_portfoliochartform COMMAND tst_portfoliochartform)

# ─────────────────────────────────────────────────────────────────────────────
# tst_sharesplitsform — ViewShareSplitEdit / PresenterShareSplitEdit
# Phase 3 der Aktiensplit-Behandlung (08.08.2026).
# Deutlich schlanker als tst_shareeditform: die Split-Maske hat weder
# Dokumenten-Vorschau noch Parse-Pipeline und braucht deshalb weder
# DocumentPreviewPanel noch PdfTextExtractor.
# ─────────────────────────────────────────────────────────────────────────────

qt_add_executable(tst_sharesplitsform tst_sharesplitsform.cpp
    ../../app/forms/ShareSplitsForm/IViewShareSplitEdit.h
    ../../app/forms/ShareSplitsForm/IModelShareSplitEdit.h
    ../../app/forms/ShareSplitsForm/ModelShareSplitEdit.h
    ../../app/forms/ShareSplitsForm/ModelShareSplitEdit.cpp
    ../../app/forms/ShareSplitsForm/PresenterShareSplitEdit.h
    ../../app/forms/ShareSplitsForm/PresenterShareSplitEdit.cpp
    ../../app/forms/ShareSplitsForm/ViewShareSplitEdit.h
    ../../app/forms/ShareSplitsForm/ViewShareSplitEdit.cpp
    ../../app/forms/OwnMessageBoxForm/OwnMessageBox.h
    ../../app/forms/OwnMessageBoxForm/OwnMessageBox.cpp
    ../../app/widgets/DocumentPreviewPanel.h
    ../../app/widgets/DocumentPreviewPanel.cpp
    ../../app/core/DocumentRootMigrator.h
    ../../app/core/DocumentRootMigrator.cpp
    ../../app/config/AppSettings.cpp
    ../../app/IconProvider.cpp
    ../../app/models/ShareObject.cpp
    ../../app/models/BuyObject.cpp
    ../../app/models/SaleObject.cpp
    ../../app/models/DividendObject.cpp
    ../../app/models/BrokerageObject.cpp
    ../../app/models/ShareSplitObject.h
    ../../app/models/ShareSplitObject.cpp
    ../../app/models/DailyValuesObject.h
    ../../app/models/DailyValuesObject.cpp
    ../../app/repositories/ShareRepository.cpp
    ../../app/repositories/BuyRepository.cpp
    ../../app/repositories/SaleRepository.cpp
    ../../app/repositories/DividendRepository.cpp
    ../../app/repositories/BrokerageRepository.cpp
    ../../app/repositories/ShareSplitRepository.h
    ../../app/repositories/ShareSplitRepository.cpp
    ../../app/repositories/DailyValuesRepository.h
    ../../app/repositories/DailyValuesRepository.cpp
    ../../app/utils/ShareSplitAdjuster.h
    ../../app/utils/ShareSplitAdjuster.cpp
    ../../app/utils/SplitPriceJumpDetector.h
    ../../app/utils/SplitPriceJumpDetector.cpp
    ${FORM_TEST_RESOURCES}
)

target_include_directories(tst_sharesplitsform
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/ShareSplitsForm
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/OwnMessageBoxForm
)

set_target_properties(tst_sharesplitsform PROPERTIES
    AUTOMOC ON
    AUTORCC ON
)

target_link_libraries(tst_sharesplitsform
    PRIVATE Qt6::Test Qt6::Sql Qt6::Widgets Database Logger
)

if(Qt6PdfWidgets_FOUND)
    target_link_libraries(tst_sharesplitsform PRIVATE Qt6::Pdf Qt6::PdfWidgets)
    target_compile_definitions(tst_sharesplitsform PRIVATE SPM_HAVE_QTPDF)
endif()

add_test(NAME tst_sharesplitsform COMMAND tst_sharesplitsform)
