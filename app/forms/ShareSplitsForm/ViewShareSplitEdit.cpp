// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ViewShareSplitEdit.h"
#include "PresenterShareSplitEdit.h"
#include "ModelShareSplitEdit.h"
#include "../../IconProvider.h"
#include "../../config/AppSettings.h"
#include "../../core/DocumentRootMigrator.h"
#include "../../widgets/DocumentPreviewPanel.h"
#include "../UiConstants.h"
#include "../OwnMessageBoxForm/OwnMessageBox.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDoubleValidator>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QFontMetrics>
#include <QLocale>
#include <QSizePolicy>

namespace {
/// Spaltenindizes der Übersichtstabelle.
enum Column { kColDate = 0, kColRatio, kColConversion, kColAdjusted, kColComment,
              kColDocument, kColCount };

/// Breite der Dokument-Spalte, projektweit vereinheitlicht (17.07.2026).
constexpr int kDocumentColumnWidth = 36;

// Textfarben fuer das Pruefergebnis (14.08.2026, Nessies Vorgabe): trotz
// vier Ergebnistypen in SplitPriceJumpDetector::Result gibt es fuers Auge
// nur zwei Zustaende, siehe IViewShareSplitEdit::PriceJumpTone. Dieselben
// Hex-Werte wie AppSettings' Erfolg-/Fehler-Logfarben (#388e3c / #d32f2f) —
// laut deren Kommentar bewusst kontrastreich auf HELLEM wie DUNKLEM
// Hintergrund (WCAG-Luminanzformel, ca. 4:1 zu Schwarz UND Weiss) statt an
// ein bestimmtes Theme angepasst. Bewusst eine eigene Konstante statt ueber
// AppSettings::logColorAt() bezogen: die Logfarben sind ueber
// LoggerSettingsForm frei aenderbar, dieses Ergebnisfeld hat mit dem
// Log-Fenster aber nichts zu tun.
const QColor kPriceJumpAdoptedColor(QStringLiteral("#388e3c"));
const QColor kPriceJumpManualColor(QStringLiteral("#d32f2f"));
}

// ── Constructor ───────────────────────────────────────────────────────────────

ViewShareSplitEdit::ViewShareSplitEdit(const QString& shareGuid, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Hinzufügen / editieren der Splits dieser Aktie"));
    setFixedSize(1100, 680);

    setupUi();

    auto* model = new ModelShareSplitEdit();
    m_presenter = new PresenterShareSplitEdit(this, model, shareGuid, this);

    connect(m_btnAdd,    &QPushButton::clicked, m_presenter, &PresenterShareSplitEdit::onSave);
    connect(m_btnRemove, &QPushButton::clicked, m_presenter, &PresenterShareSplitEdit::onRemove);
    connect(m_btnReset,  &QPushButton::clicked, m_presenter, &PresenterShareSplitEdit::onReset);
    connect(m_btnClose,  &QPushButton::clicked, m_presenter, &PresenterShareSplitEdit::onClose);

    // Verhältnis-Eingaben halten die Umrechnungs-Vorschau aktuell.
    connect(m_ratioNew, &QLineEdit::textChanged,
            m_presenter, &PresenterShareSplitEdit::onValuesChanged);
    connect(m_ratioOld, &QLineEdit::textChanged,
            m_presenter, &PresenterShareSplitEdit::onValuesChanged);

    connect(m_table, &QTableWidget::itemSelectionChanged,
            this, &ViewShareSplitEdit::onTableSelectionChanged);

    connect(m_btnBrowseDoc, &QPushButton::clicked,
            this, &ViewShareSplitEdit::onBrowseDocument);

    connect(m_btnCheckPriceJump, &QPushButton::clicked,
            m_presenter, &PresenterShareSplitEdit::onCheckPriceJump);

    connect(m_btnReverseSplitHint, &QPushButton::clicked,
            this, &ViewShareSplitEdit::onShowReverseSplitHint);
}

// ── setupUi ───────────────────────────────────────────────────────────────────

void ViewShareSplitEdit::setupUi()
{
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // Linke Spalte: Formular, Buttons, Übersicht.
    auto* leftColumn = new QVBoxLayout;
    leftColumn->setContentsMargins(0, 0, 0, 0);
    leftColumn->setSpacing(6);
    leftColumn->addWidget(createSplitDataGroup());
    leftColumn->addWidget(createDocumentGroup());
    leftColumn->addWidget(createButtonBar());
    leftColumn->addWidget(createOverviewGroup(), 1);

    mainLayout->addLayout(leftColumn, 3);
    mainLayout->addWidget(createPreviewPanel(), 2);
}

// ── createSplitDataGroup ──────────────────────────────────────────────────────

QGroupBox* ViewShareSplitEdit::createSplitDataGroup()
{
    m_splitDataGroup = new QGroupBox(tr("  Split hinzufügen"));
    m_splitDataGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto* grid = new QGridLayout(m_splitDataGroup);
    grid->setColumnStretch(1, 1);
    grid->setVerticalSpacing(4);
    grid->setHorizontalSpacing(8);
    grid->setContentsMargins(8, 8, 8, 10);

    // labelVAlign steuert nur, wo der LABELTEXT innerhalb seiner (die volle
    // Zeilenhoehe ausfuellenden) Zelle sitzt — Standard vertikal zentriert,
    // wie bei allen einzeiligen Feldern. Die "Pruefung:"-Zeile braucht
    // AlignTop, damit Label und Pruefen-Knopf oben an der zweizeiligen
    // Ergebnisbox ausgerichtet sind statt in deren Mitte zu "schweben"
    // (14.08.2026, Nessies Vorgabe).
    const auto addRow = [&](int row, const QString& labelText, QWidget* field,
                            Qt::Alignment labelVAlign = Qt::AlignVCenter) {
        auto* lbl = new QLabel(labelText);
        lbl->setAlignment(Qt::AlignRight | labelVAlign);
        grid->addWidget(lbl,   row, 0);
        grid->addWidget(field, row, 1);
    };

    int row = 0;

    // Ex-Tag. Zukünftige Daten sind erlaubt (Nessies Entscheidung 08.08.2026):
    // ein angekündigter Split darf sofort erfasst werden — ShareSplitAdjuster
    // rechnet ohnehin nur Datensätze VOR dem Splittag um.
    m_date = new QDateEdit(QDate::currentDate());
    m_date->setObjectName(QStringLiteral("splitDate"));
    m_date->setCalendarPopup(true);
    m_date->setDisplayFormat(tr("dd.MM.yyyy"));
    m_date->setMinimumDate(QDate(2000, 1, 1));
    m_date->setMaximumDate(QDate(9999, 12, 31));
    m_date->setFixedHeight(UiConstants::kFieldHeight);
    addRow(row++, tr("Ex-Tag:"), m_date);

    // Verhältnis neu : alt
    auto* ratioWidget = new QWidget;
    auto* ratioLayout = new QHBoxLayout(ratioWidget);
    ratioLayout->setContentsMargins(0, 0, 0, 0);
    ratioLayout->setSpacing(4);

    const auto makeRatioField = [](const QString& objectName) {
        auto* le = new QLineEdit(QStringLiteral("1"));
        le->setObjectName(objectName);
        le->setValidator(new QDoubleValidator(0.0, 1.0e9, 4, le));
        le->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        le->setFixedHeight(UiConstants::kFieldHeight);
        le->setFixedWidth(80);
        return le;
    };
    m_ratioNew = makeRatioField(QStringLiteral("ratioNew"));
    m_ratioOld = makeRatioField(QStringLiteral("ratioOld"));

    ratioLayout->addWidget(m_ratioNew);
    ratioLayout->addWidget(new QLabel(QStringLiteral(":")));
    ratioLayout->addWidget(m_ratioOld);
    ratioLayout->addWidget(new QLabel(tr("(neu : alt)")));

    // Hinweis-Knopf zu Bruchstücken bei Reverse-Splits (14.08.2026, Nessies
    // Vorgabe — ersetzt einen ersten Anlauf mit Tooltip: der Tooltip
    // verschwand beim Wegbewegen der Maus wieder und verwies auf
    // ARCHITECTURE.md, auf das der Benutzer gar keinen Zugriff hat. Öffnet
    // stattdessen einen ausführlichen, in sich geschlossenen Hinweis-Dialog
    // — siehe reverseSplitHintMessage(). Bewusst dauerhaft sichtbar statt
    // nur bei erkanntem Reverse-Split (neu < alt): die Erklärung soll auch
    // VOR dem Eintragen des Verhältnisses auffindbar sein.
    m_btnReverseSplitHint = new QPushButton(tr("Hinweis Reverse-Split"));
    m_btnReverseSplitHint->setObjectName(QStringLiteral("btnReverseSplitHint"));
    m_btnReverseSplitHint->setFixedHeight(UiConstants::kButtonHeight);
    m_btnReverseSplitHint->setToolTip(
        tr("Zeigt, wie Bruchstücke aus einem Reverse-Split (von der Bank "
           "bar ausgezahlte Spitzen) erfasst werden."));
    ratioLayout->addSpacing(12);
    ratioLayout->addWidget(m_btnReverseSplitHint);

    ratioLayout->addStretch(1);
    addRow(row++, tr("Verhältnis:"), ratioWidget);

    // Umrechnung (abgeleitet)
    //
    // Notationshinweis (11.08.2026, Feldfall Alphabet; als Tooltip statt
    // Dauertext seit 13.08.2026, Nessies Vorgabe): Bankmitteilungen nennen
    // ueblicherweise das Zuteilungsverhaeltnis ("1:19" = 19 ZUSAETZLICHE
    // Stuecke je gehaltenem Stueck), die Maske erwartet aber das
    // Umrechnungsverhaeltnis neu:alt (im selben Beispiel 20:1) — ohne diesen
    // Hinweis wird der Faktor systematisch um genau eins zu klein
    // eingetragen. Erste Fassung war ein dauerhaft sichtbares Label unter
    // dieser Zeile; das wirkte im Dialog aber zu aufdringlich, deshalb jetzt
    // ein Tooltip auf dem Umrechnungs-Feld selbst — gleiches Muster wie beim
    // Tooltip auf "Kurshistorie" direkt darunter.
    m_factorPreview = new QLineEdit;
    m_factorPreview->setObjectName(QStringLiteral("factorPreview"));
    m_factorPreview->setReadOnly(true);
    m_factorPreview->setEnabled(false);
    m_factorPreview->setFixedHeight(UiConstants::kFieldHeight);
    m_factorPreview->setToolTip(
        tr("Bankmitteilungen nennen oft das Zuteilungsverhältnis, z. B. "
           "„1:19“ = 19 zusätzliche Aktien je gehaltenem Stück.\n"
           "Einzutragen ist hier das Umrechnungsverhältnis neu:alt (im "
           "Beispiel 20:1)."));
    addRow(row++, tr("Umrechnung:"), m_factorPreview);

    // prices_adjusted
    //
    // Tooltip erweitert (13.08.2026, Nessies Vorgabe) um die praktische
    // Unterscheidung: betrifft nur die Kurshistorie, nicht Käufe/Verkäufe/
    // Dividenden (die laufen immer über volumeFactor(), unabhängig von
    // diesem Haken — siehe ShareSplitAdjuster.h). Die zwei Fälle mit
    // Faustregel "Sprung am Ex-Tag ja/nein" sollen den Nutzer in die Lage
    // versetzen, das selbst an der eigenen Kurshistorie abzulesen, ohne erst
    // in ARCHITECTURE.md nachschlagen zu müssen.
    m_pricesAdjusted = new QCheckBox(
        tr("Kurshistorie vor dem Ex-Tag liegt bereits split-bereinigt vor"));
    m_pricesAdjusted->setObjectName(QStringLiteral("pricesAdjusted"));
    m_pricesAdjusted->setToolTip(
        tr("Betrifft nur die Kurshistorie (Tageswerte), nicht Käufe, Verkäufe "
           "oder Dividenden.\n\n"
           "Nicht angehakt (Standard): Der Kurs zeigt am Ex-Tag einen Sprung "
           "um etwa den Split-Faktor — die App rechnet die Kurshistorie davor "
           "selbst auf heutige Stücke um.\n\n"
           "Angehakt: Der Anbieter lieferte die Kurshistorie bereits "
           "bereinigt, ohne Sprung am Ex-Tag — die App lässt sie unangetastet."));
    // Feste Höhe wie alle anderen Zeilen (14.08.2026, Nessies Vorgabe): bis
    // zur Layout-Korrektur teilte sich diese Zeile den Platz mit dem
    // "Prüfen"-Knopf, dessen kFieldHeight-Höhe die Zeile auf Standardmaß
    // brachte. Jetzt steckt der Knopf in der "Prüfung:"-Zeile, und die
    // Checkbox stünde ohne diese Zeile hier alleine mit ihrer schmaleren
    // Sizehint-Höhe da — sichtbar kleinerer Abstand zur Zeile darüber/
    // darunter als bei allen anderen Zeilen.
    m_pricesAdjusted->setFixedHeight(UiConstants::kFieldHeight);

    addRow(row++, tr("Kurshistorie:"), m_pricesAdjusted);

    // Pruefergebnis und Pruefen-Knopf (13.08.2026, Nessies Vorgabe; Layout
    // korrigiert 14.08.2026). Erste Fassung zeigte das Ergebnis in einem
    // QLabel unter der Checkbox — je nach Textlaenge ein- oder zweizeilig,
    // wodurch beim Klick auf "Pruefen" (und beim Reset) alles darunter
    // (Kommentar, Dokument, Buttons) im Dialog nach unten bzw. wieder nach
    // oben sprang. Jetzt ein read-only Feld mit fester Zweizeilen-Hoehe,
    // damit sich am Layout nichts mehr verschiebt, egal ob das Ergebnis
    // ein- oder zweizeilig ist. Der Pruefen-Knopf sitzt seitdem direkt
    // daneben statt neben der Checkbox.
    m_priceJumpResult = new QPlainTextEdit;
    m_priceJumpResult->setObjectName(QStringLiteral("priceJumpResult"));
    m_priceJumpResult->setReadOnly(true);
    m_priceJumpResult->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    m_priceJumpResult->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_priceJumpResult->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_priceJumpResult->setPlaceholderText(tr("Noch nicht geprüft …"));
    // Zweizeilige Hoehe aus der tatsaechlichen Zeilenhoehe der Schrift
    // ableiten (statt eines festen Pixelwerts) — bleibt so auch bei
    // groesseren System-Schriftgroessen zweizeilig nutzbar.
    const QFontMetrics priceJumpFm(m_priceJumpResult->font());
    m_priceJumpResult->setFixedHeight(priceJumpFm.lineSpacing() * 2 + 14);
    // Ausgangsfarbe sichern, BEVOR setPriceJumpHint() sie je einfärbt — siehe
    // resetPriceJumpResult().
    m_priceJumpDefaultPalette = m_priceJumpResult->palette();

    m_btnCheckPriceJump = new QPushButton(tr("Prüfen"));
    m_btnCheckPriceJump->setObjectName(QStringLiteral("btnCheckPriceJump"));
    m_btnCheckPriceJump->setFixedHeight(UiConstants::kButtonHeight);
    m_btnCheckPriceJump->setToolTip(
        tr("Vergleicht den letzten verfügbaren Kurs vor mit dem ersten "
           "verfügbaren Kurs nach dem Ex-Tag. Ist das Ergebnis eindeutig, "
           "wird der Haken automatisch gesetzt bzw. entfernt."));

    auto* priceJumpRow = new QWidget;
    auto* priceJumpLayout = new QHBoxLayout(priceJumpRow);
    priceJumpLayout->setContentsMargins(0, 0, 0, 0);
    priceJumpLayout->setSpacing(8);
    priceJumpLayout->addWidget(m_priceJumpResult, 1);
    // AlignTop statt AlignVCenter (14.08.2026, Nessies Vorgabe): der Knopf
    // ist niedriger als die zweizeilige Ergebnisbox und soll oben an ihr
    // anliegen statt mittig zu "schweben".
    priceJumpLayout->addWidget(m_btnCheckPriceJump, 0, Qt::AlignTop);
    // Label ebenfalls oben ausgerichtet, aus demselben Grund — siehe addRow().
    addRow(row++, tr("Prüfung:"), priceJumpRow, Qt::AlignTop);

    // Kommentar
    m_comment = new QLineEdit;
    m_comment->setObjectName(QStringLiteral("comment"));
    m_comment->setPlaceholderText(tr("z. B. Quelle oder Anlass des Splits"));
    m_comment->setFixedHeight(UiConstants::kFieldHeight);
    addRow(row++, tr("Kommentar:"), m_comment);

    return m_splitDataGroup;
}

// ── createDocumentGroup ───────────────────────────────────────────────────────
// Eigene Groupbox statt Zeile in createSplitDataGroup() (13.08.2026, Nessies
// Vorgabe) — Icon und Layout jetzt 1:1 wie in ViewBuyEdit::createDocumentGroup()
// (Ordner-Icon statt "…"-Button, read-only Pfadfeld). Der Root-Zwang wird
// weiterhin in onBrowseDocument() geprüft, die Doppelbelegungs-Prüfung läuft
// jetzt ausschliesslich über die Dateiauswahl (onDocumentSelected() ruft
// PresenterShareSplitEdit::onDocumentPathEdited() bereits selbst auf) — die
// bisherige editingFinished-Verbindung für manuelles Eintippen entfällt, das
// Feld lässt sich nicht mehr direkt beschreiben.
QGroupBox* ViewShareSplitEdit::createDocumentGroup()
{
    auto* gb   = new QGroupBox(tr("  Dokument"));
    gb->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    auto* grid = new QGridLayout(gb);
    grid->setColumnStretch(1, 1);
    grid->setVerticalSpacing(4);
    grid->setHorizontalSpacing(8);
    grid->setContentsMargins(8, 8, 8, 10);

    m_documentPath = new QLineEdit;
    m_documentPath->setObjectName(QStringLiteral("documentPath"));
    m_documentPath->setReadOnly(true);
    m_documentPath->setPlaceholderText(tr("Kein Dokument ausgewählt …"));
    m_documentPath->setFixedHeight(UiConstants::kFieldHeight);

    m_btnBrowseDoc = new QPushButton(IconProvider::icon(IconProvider::MenuFolderOpen16), QString());
    m_btnBrowseDoc->setObjectName(QStringLiteral("btnBrowseDocument"));
    m_btnBrowseDoc->setFixedWidth(36);
    m_btnBrowseDoc->setFixedHeight(UiConstants::kFieldHeight);
    m_btnBrowseDoc->setToolTip(tr("PDF-Dokument auswählen"));

    auto* docRow = new QWidget;
    auto* docLayout = new QHBoxLayout(docRow);
    docLayout->setContentsMargins(0, 0, 0, 0);
    docLayout->setSpacing(4);
    docLayout->addWidget(m_documentPath, 1);
    docLayout->addWidget(m_btnBrowseDoc, 0);

    auto* label = new QLabel(tr("Dokument:"));
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(label,  0, 0);
    grid->addWidget(docRow, 0, 1);

    return gb;
}

// ── createButtonBar ───────────────────────────────────────────────────────────

QWidget* ViewShareSplitEdit::createButtonBar()
{
    auto* bar    = new QWidget;
    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    const auto makeButton = [](IconProvider::IconName icon, const QString& text) {
        auto* btn = new QPushButton(IconProvider::icon(icon), text);
        btn->setFixedHeight(UiConstants::kButtonHeight);
        btn->setMinimumWidth(110);
        return btn;
    };

    m_btnAdd    = makeButton(IconProvider::ButtonAdd,    tr("Hinzufügen"));
    m_btnRemove = makeButton(IconProvider::ButtonDelete, tr("Entfernen"));
    m_btnReset  = makeButton(IconProvider::ButtonReset,  tr("Reset"));
    m_btnClose  = makeButton(IconProvider::ButtonCancel, tr("Schließen"));

    layout->addStretch(1);
    layout->addWidget(m_btnAdd);
    layout->addWidget(m_btnRemove);
    layout->addWidget(m_btnReset);
    layout->addWidget(m_btnClose);

    return bar;
}

// ── createOverviewGroup ───────────────────────────────────────────────────────

QGroupBox* ViewShareSplitEdit::createOverviewGroup()
{
    auto* gb     = new QGroupBox(tr("  Erfasste Splits"));
    auto* layout = new QVBoxLayout(gb);
    layout->setContentsMargins(8, 8, 8, 8);

    m_table = new QTableWidget(0, kColCount);
    m_table->setObjectName(QStringLiteral("splitsTable"));
    // Die Dokument-Spalte bleibt bewusst ohne Überschrift und bei 36 px —
    // projektweit vereinheitlicht am 17.07.2026 (siehe ARCHITECTURE.md).
    m_table->setHorizontalHeaderLabels({ tr("Datum"), tr("Verhältnis"),
                                         tr("Umrechnung"), tr("Kurse bereinigt"),
                                         tr("Kommentar"), QString() });
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);

    m_table->setColumnWidth(kColDate,       90);
    m_table->setColumnWidth(kColRatio,      90);
    m_table->setColumnWidth(kColConversion, 170);
    m_table->setColumnWidth(kColAdjusted,   110);
    m_table->setColumnWidth(kColDocument,   kDocumentColumnWidth);
    // Nicht die letzte Spalte strecken — das wäre die Dokument-Spalte, die
    // ihre feste Breite behalten soll. Stattdessen wächst der Kommentar.
    m_table->horizontalHeader()->setSectionResizeMode(kColComment, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(kColDocument, QHeaderView::Fixed);

    layout->addWidget(m_table);
    return gb;
}

// ── createPreviewPanel ────────────────────────────────────────────────────────

QWidget* ViewShareSplitEdit::createPreviewPanel()
{
    // DocumentPreviewPanel bringt bereits ein eigenes GroupBox
    // ("Dokumenten-Vorschau") mit, kein zusätzlicher Wrapper nötig — analog
    // zu ViewBuyEdit/ViewSaleEdit/ViewDividendEdit/ViewBrokerageEdit.
    m_previewPanel = new DocumentPreviewPanel(this);
    return m_previewPanel;
}

// ── IViewShareSplitEdit: read ─────────────────────────────────────────────────

QDate   ViewShareSplitEdit::splitDate()      const { return m_date->date(); }
double  ViewShareSplitEdit::ratioNew()       const { return parseDouble(m_ratioNew->text()); }
double  ViewShareSplitEdit::ratioOld()       const { return parseDouble(m_ratioOld->text()); }
bool    ViewShareSplitEdit::pricesAdjusted() const { return m_pricesAdjusted->isChecked(); }
QString ViewShareSplitEdit::comment()        const { return m_comment->text(); }
QString ViewShareSplitEdit::documentPath()   const { return m_documentPath->text(); }

// ── IViewShareSplitEdit: write ────────────────────────────────────────────────

void ViewShareSplitEdit::loadSplit(const ShareSplitObject& split)
{
    if (split.date().isValid())
        m_date->setDate(split.date());

    m_ratioNew->setText(formatRatioPart(split.ratioNew()));
    m_ratioOld->setText(formatRatioPart(split.ratioOld()));
    m_pricesAdjusted->setChecked(split.pricesAdjusted());
    resetPriceJumpResult();
    m_comment->setText(split.comment());
    m_documentPath->setText(split.document());
}

void ViewShareSplitEdit::clearForm()
{
    m_date->setDate(QDate::currentDate());
    m_ratioNew->setText(QStringLiteral("1"));
    m_ratioOld->setText(QStringLiteral("1"));
    m_pricesAdjusted->setChecked(false);
    resetPriceJumpResult();
    m_comment->clear();
    m_documentPath->clear();

    m_splitDataGroup->setTitle(tr("  Split hinzufügen"));
}

void ViewShareSplitEdit::setFactorPreview(const QString& text)
{
    m_factorPreview->setText(text);
}

void ViewShareSplitEdit::setDocumentPath(const QString& path)
{
    m_documentPath->setText(path);
}

void ViewShareSplitEdit::setPricesAdjusted(bool value)
{
    m_pricesAdjusted->setChecked(value);
}

void ViewShareSplitEdit::setPriceJumpHint(const QString& text, PriceJumpTone tone)
{
    m_priceJumpResult->setPlainText(text);

    // Palette statt Stylesheet — gleiche Konvention wie ViewShareEdit::
    // m_updateHint und ViewShareDetails::m_updateWarningLine, damit die
    // Farbe unabhängig vom Systemtheme wirksam bleibt. QPalette::Text statt
    // ::WindowText, da QPlainTextEdit (anders als QLabel) diese Rolle für
    // den angezeigten Text verwendet. Ausgangspunkt ist bewusst die
    // gesicherte m_priceJumpDefaultPalette statt der aktuellen Palette —
    // sonst würde ein zweiter Prüflauf die Farbe des ersten als Basis nehmen.
    QPalette pal = m_priceJumpDefaultPalette;
    pal.setColor(QPalette::Text, tone == PriceJumpTone::Adopted
                                     ? kPriceJumpAdoptedColor
                                     : kPriceJumpManualColor);
    m_priceJumpResult->setPalette(pal);
}

void ViewShareSplitEdit::resetPriceJumpResult()
{
    m_priceJumpResult->clear();
    m_priceJumpResult->setPalette(m_priceJumpDefaultPalette);
}

void ViewShareSplitEdit::openPdfPreview(const QString& path)
{
    m_previewPanel->showDocument(path);
}

void ViewShareSplitEdit::clearPdfPreview()
{
    m_previewPanel->clearDocument();
}

void ViewShareSplitEdit::populateOverview(const QList<ShareSplitObject>& splits)
{
    // Während des Neuaufbaus darf itemSelectionChanged() nicht auf den
    // Presenter durchschlagen — sonst würde clearContents() die gerade
    // geladene Maske sofort wieder zurücksetzen.
    m_suppressSelectionSignal = true;

    m_table->clearContents();
    m_table->setRowCount(splits.size());

    const QLocale loc;
    int row = 0;
    for (const ShareSplitObject& s : splits) {
        const QString ratio = QStringLiteral("%1:%2")
                                  .arg(formatRatioPart(s.ratioNew()),
                                       formatRatioPart(s.ratioOld()));
        // Singular/Plural als getrennte Sätze statt eingesetztem Verb — sonst
        // wäre der Satzbau für Übersetzer nicht auflösbar.
        const QString from = formatRatioPart(s.ratioOld());
        const QString to   = formatRatioPart(s.ratioNew());
        const QString conversion = (qAbs(s.ratioNew() - 1.0) < 1e-9)
                                       ? tr("aus %1 Stk. wird %2 Stk.").arg(from, to)
                                       : tr("aus %1 Stk. werden %2 Stk.").arg(from, to);

        const QStringList texts = {
            loc.toString(s.date(), QLocale::ShortFormat),
            ratio,
            conversion,
            s.pricesAdjusted() ? tr("ja") : tr("nein"),
            s.comment(),
            QString()            // Dokument-Spalte trägt nur ein Icon
        };

        for (int col = 0; col < texts.size(); ++col) {
            auto* item = new QTableWidgetItem(texts.at(col));
            // GUID an jeder Zelle, damit die Auswahl unabhängig von der
            // angeklickten Spalte aufgelöst werden kann.
            item->setData(Qt::UserRole, s.guid());
            if (col == kColDate || col == kColRatio || col == kColAdjusted)
                item->setTextAlignment(Qt::AlignCenter);

            if (col == kColDocument && !s.document().isEmpty()) {
                item->setIcon(IconProvider::icon(IconProvider::DocPdfImage16));
                item->setToolTip(s.document());
                item->setTextAlignment(Qt::AlignCenter);
            }

            m_table->setItem(row, col, item);
        }
        ++row;
    }

    m_table->clearSelection();
    m_suppressSelectionSignal = false;
}

void ViewShareSplitEdit::setButtonStates(bool canRemove, bool isEdit)
{
    m_btnRemove->setEnabled(canRemove);

    if (isEdit) {
        m_btnAdd->setText(tr("Speichern"));
        m_btnAdd->setIcon(IconProvider::icon(IconProvider::ButtonSave));
    } else {
        m_btnAdd->setText(tr("Hinzufügen"));
        m_btnAdd->setIcon(IconProvider::icon(IconProvider::ButtonAdd));
    }

    m_splitDataGroup->setTitle(isEdit ? tr("  Split editieren")
                                      : tr("  Split hinzufügen"));
}

void ViewShareSplitEdit::showError(const QString& message)
{
    OwnMessageBox::critical(this, tr("Fehler"), message);
}

bool ViewShareSplitEdit::confirm(const QString& title, const QString& message)
{
    return OwnMessageBox::question(this, title, message);
}

void ViewShareSplitEdit::acceptAndClose()
{
    accept();
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void ViewShareSplitEdit::onTableSelectionChanged()
{
    if (m_suppressSelectionSignal || !m_presenter)
        return;

    const QList<QTableWidgetItem*> selected = m_table->selectedItems();
    if (selected.isEmpty()) {
        m_presenter->onRowSelected(QString());
        return;
    }
    m_presenter->onRowSelected(selected.constFirst()->data(Qt::UserRole).toString());
}

// ── onBrowseDocument ──────────────────────────────────────────────────────────

void ViewShareSplitEdit::onBrowseDocument()
{
    const QString root = AppSettings::instance().documentsRootPath();
    const QString startDir = !root.isEmpty() ? root : QDir::homePath();

    // Nur PDF zulässig — dieselbe Einschränkung wie in den anderen fünf
    // Dialogen (siehe ARCHITECTURE.md, "Offene Punkte / TODO": Word-/
    // Excel-Unterstützung bewusst nicht wieder eingebaut).
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("PDF-Dokument auswählen"),
        startDir,
        tr("PDF-Dokumente (*.pdf)"));

    if (path.isEmpty())
        return;

    if (!DocumentRootMigrator::isPathWithinRoot(path, root)) {
        OwnMessageBox::critical(this, tr("Fehler"),
            tr("Die gewählte Datei muss innerhalb des Dokument-Root-Verzeichnisses "
               "liegen:\n%1").arg(root));
        return;
    }

    m_presenter->onDocumentSelected(path);
}

// ── onShowReverseSplitHint ───────────────────────────────────────────────────
// Reine View-Angelegenheit ohne Presenter-/Model-Beteiligung (wie
// onBrowseDocument()) — es wird nichts gespeichert oder validiert, nur ein
// Hinweistext angezeigt.

void ViewShareSplitEdit::onShowReverseSplitHint()
{
    OwnMessageBox::information(this, tr("Reverse-Split mit Bruchstücken"),
                               reverseSplitHintMessage());
}

// ── Helpers ───────────────────────────────────────────────────────────────────

double ViewShareSplitEdit::parseDouble(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return 0.0;

    // Erst mit deutschem Locale (Komma), dann als Rückfall mit Punkt —
    // dieselbe Konvention wie in ViewBrokerageEdit::parseDouble().
    bool ok = false;
    double value = QLocale().toDouble(trimmed, &ok);
    if (!ok)
        value = QString(trimmed).replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&ok);
    return ok ? value : 0.0;
}

QString ViewShareSplitEdit::formatRatioPart(double value)
{
    const QLocale loc;
    const double rounded = static_cast<double>(qRound(value));
    if (qAbs(value - rounded) < 1e-9)
        return loc.toString(qRound(value));
    return loc.toString(value, 'f', 2);
}

// ── reverseSplitHintMessage ───────────────────────────────────────────────────
//
// Text bewusst in sich geschlossen (14.08.2026, Nessies Vorgabe): keine
// Verweise auf ARCHITECTURE.md oder interne Klassennamen — der Benutzer hat
// darauf keinen Zugriff. Nutzt, wenn im Formular ein echtes
// Reverse-Split-Verhältnis eingetragen ist (neu < alt, beide > 0), dessen
// Zahlen für eine konkrete Beispielrechnung; sonst ein festes Beispiel
// (1:10). Das illustrative "1 altes Stück übrig" im dynamischen Fall ist
// bewusst gewählt, weil es unabhängig vom tatsächlichen Bestand immer genau
// einen Rest ergibt — Ziel ist die Rechenmethode, nicht der eigene Bestand.
QString ViewShareSplitEdit::reverseSplitHintMessage() const
{
    const QString intro = tr(
        "Bei einem Reverse-Split zahlt die Bank oft Bruchstücke bar aus — "
        "Stücke, die sich nicht glatt zu neuen Aktien zusammenlegen lassen. "
        "Dafür brauchen Sie keinen Sonderfall: Erfassen Sie die Bruchstücke "
        "einfach als ganz normalen Verkauf.");

    const QString steps = tr(
        "1. Verkauf anlegen mit Datum = Ex-Tag dieses Splits.\n"
        "2. Menge im NEUEN (Nach-Split-) Maßstab eintragen, nicht im "
        "alten.\n"
        "3. Kurs = von der Bank ausgezahlter Betrag ÷ diese Menge.\n\n"
        "Den Split selbst tragen Sie wie gewohnt ein — ob zuerst der "
        "Verkauf oder zuerst der Split, spielt keine Rolle.");

    const double newRatio = ratioNew();
    const double oldRatio = ratioOld();
    const bool hasReverseSplitRatio = oldRatio > 0.0 && newRatio > 0.0
                                       && newRatio < oldRatio;

    QString example;
    if (hasReverseSplitRatio) {
        const double factor = newRatio / oldRatio;
        example = tr(
            "Beispiel mit dem hier eingetragenen Verhältnis %1:%2: Legen "
            "sich %2 alte Stücke glatt zu %1 neuen zusammen, bleibt bei "
            "einem Bestand von %3 alten Stücken genau 1 altes Stück als "
            "Bruchstück übrig. Verkauf: Menge = 1 × %1/%2 = %4, Kurs = "
            "Auszahlungsbetrag ÷ %4.")
            .arg(formatRatioPart(newRatio), formatRatioPart(oldRatio),
                 formatRatioPart(oldRatio + 1.0), formatRatioPart(factor));
    } else {
        example = tr(
            "Beispiel bei einem Verhältnis von 1:10 und 105 alten Stücken: "
            "100 alte Stücke legen sich glatt zu 10 neuen zusammen, 5 alte "
            "Stücke sind das Bruchstück. Verkauf: Menge = 5 × 0,1 = 0,5, "
            "Kurs = Auszahlungsbetrag ÷ 0,5.");
    }

    return intro + QStringLiteral("\n\n") + steps + QStringLiteral("\n\n") + example;
}
