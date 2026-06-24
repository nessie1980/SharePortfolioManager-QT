// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include <QtTest>
#include "Logger.h"

using namespace Logging;

class TestLogger : public QObject
{
    Q_OBJECT

private slots:

    void test_init_success()
    {
        Logger logger;
        auto state = logger.loggerInitialize(
            3,   // logLevelStates:    State1(1) | State2(2)
            3,   // logLevelComponents: Component1(1) | Component2(2)
            {"Start", "Info", "Warning"},
            {"App", "Parser"},
            {Qt::black, Qt::blue, QColor("OrangeRed")},
            false, 50, {}, "Unit test start"
        );
        QCOMPARE(state, Logger::InitState::Initialized);
        QCOMPARE(logger.loggerState(), Logger::LoggerState::Initialized);
    }

    void test_init_logging_disabled_when_levels_zero()
    {
        Logger logger;
        auto state = logger.loggerInitialize(0, 0, {}, {}, {}, false, 50);
        QCOMPARE(state, Logger::InitState::Initialized);
        QCOMPARE(logger.loggerState(), Logger::LoggerState::LoggingDisabled);
    }

    void test_addEntry_success()
    {
        Logger logger;
        logger.loggerInitialize(
            3, 3,
            {"Start", "Info"},
            {"App", "Parser"},
            {Qt::black, Qt::blue},
            false, 50, {}, "Start"
        );

        // State2 = Info (index 1), Component1 = App (index 0)
        auto result = logger.addEntry("Test message",
                                      Logger::StateLevel::State2,
                                      Logger::ComponentLevel::Component1);
        QCOMPARE(result, Logger::LoggerState::NewEntryAddSuccessful);
        // entry list: 1 startup + 1 new
        QCOMPARE(logger.entryList().count(), 2);
        const auto lastEntry = logger.entryList().constLast();
        QCOMPARE(lastEntry.message(), QString("Test message"));
        QCOMPARE(lastEntry.state(), QString("Info"));
        QCOMPARE(lastEntry.componentName(), QString("App"));
    }

    void test_addEntry_filtered_by_state_level()
    {
        Logger logger;
        // Only State1 (1) enabled, State2 (2) not
        logger.loggerInitialize(
            1, 1,
            {"Start", "Info"},
            {"App"},
            {Qt::black, Qt::blue},
            false, 50, {}, "Start"
        );

        auto result = logger.addEntry("Should be filtered",
                                      Logger::StateLevel::State2,   // not in mask
                                      Logger::ComponentLevel::Component1);
        // Filtered — stays Initialized (not an error)
        QCOMPARE(result, Logger::LoggerState::Initialized);
        QCOMPARE(logger.entryList().count(), 1); // only startup entry
    }

    void test_ring_buffer()
    {
        Logger logger;
        logger.loggerInitialize(
            1, 1,
            {"Start"},
            {"App"},
            {Qt::black},
            false, 3, {}, "Start" // size = 3
        );

        // Add 5 entries — buffer should only keep 3
        for (int i = 0; i < 5; ++i)
            logger.addEntry(QString("Entry %1").arg(i),
                            Logger::StateLevel::State1,
                            Logger::ComponentLevel::Component1);

        QCOMPARE(logger.entryList().count(), 3);
    }

    void test_entry_added_signal()
    {
        Logger logger;
        logger.loggerInitialize(
            1, 1,
            {"Start"},
            {"App"},
            {Qt::black},
            false, 50, {}, "Start"
        );

        int signalCount = 0;
        connect(&logger, &Logger::entryAdded, [&](const LogEntry&) {
            ++signalCount;
        });

        logger.addEntry("Signal test",
                        Logger::StateLevel::State1,
                        Logger::ComponentLevel::Component1);

        QCOMPARE(signalCount, 1);
    }

    void test_get_color_of_state_level()
    {
        Logger logger;
        logger.loggerInitialize(
            3, 1,
            {"Start", "Info"},
            {"App"},
            {Qt::black, Qt::blue},
            false, 50, {}, "Start"
        );

        QCOMPARE(logger.getColorOfStateLevel(Logger::StateLevel::State1), Qt::black);
        QCOMPARE(logger.getColorOfStateLevel(Logger::StateLevel::State2), Qt::blue);
    }

    void test_file_logging()
    {
        const QString logPath = QDir::tempPath() + "/spm_logger_test";
        const QString logFile = logPath + "/test.log";

        QDir().mkpath(logPath);
        QFile::remove(logFile);

        Logger logger;
        logger.loggerInitialize(
            1, 1,
            {"Start"},
            {"App"},
            {Qt::black},
            true, 50, logFile, "File test start", false
        );

        QCOMPARE(logger.initState(), Logger::InitState::Initialized);

        logger.addEntry("Written to file",
                        Logger::StateLevel::State1,
                        Logger::ComponentLevel::Component1);

        // Verify file exists and has content
        QFile f(logFile);
        QVERIFY(f.exists());
        QVERIFY(f.size() > 0);

        // Cleanup
        QFile::remove(logFile);
        QDir().rmpath(logPath);
    }
};

QTEST_MAIN(TestLogger)
#include "tst_logger.moc"
