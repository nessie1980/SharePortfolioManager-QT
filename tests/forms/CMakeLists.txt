find_package(Qt6 REQUIRED COMPONENTS Test Sql Widgets Multimedia Network)

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
    ../../app/utils/ShareCalculator.cpp
    ${FORM_TEST_RESOURCES}
)

target_include_directories(tst_mainwindow
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/ShareEditForm
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/BuysForm
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/SalesForm
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/DividendForm
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/BrokeragesForm
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
    PRIVATE Qt6::Test Qt6::Sql Qt6::Widgets Qt6::Multimedia Qt6::Network Database Logger Parser
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
    ../../app/forms/BuysForm/IViewBuyEdit.h
    ../../app/forms/BuysForm/IModelBuyEdit.h
    ../../app/forms/BuysForm/ModelBuyEdit.h
    ../../app/forms/BuysForm/ModelBuyEdit.cpp
    ../../app/forms/BuysForm/PresenterBuyEdit.h
    ../../app/forms/BuysForm/PresenterBuyEdit.cpp
    ../../app/forms/BuysForm/ViewBuyEdit.h
    ../../app/forms/BuysForm/ViewBuyEdit.cpp
    ../../app/forms/OwnMessageBoxForm/OwnMessageBox.h
    ../../app/forms/OwnMessageBoxForm/OwnMessageBox.cpp
    ../../app/config/AppSettings.cpp
    ../../app/config/DocumentsConfig.cpp
    ../../app/IconProvider.cpp
    ../../app/models/ShareObject.cpp
    ../../app/models/BuyObject.cpp
    ../../app/models/BrokerageObject.cpp
    ../../app/repositories/ShareRepository.cpp
    ../../app/repositories/BuyRepository.cpp
    ../../app/repositories/BrokerageRepository.cpp
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
    ../../app/forms/OwnMessageBoxForm/OwnMessageBox.h
    ../../app/forms/OwnMessageBoxForm/OwnMessageBox.cpp
    ../../app/config/AppSettings.cpp
    ../../app/config/WebSitesConfig.cpp
    ../../app/config/DocumentsConfig.cpp
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
    ${FORM_TEST_RESOURCES}
)

target_include_directories(tst_shareeditform
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/ShareEditForm
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/BuysForm
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/SalesForm
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/DividendForm
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/forms/BrokeragesForm
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
