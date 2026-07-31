// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ChartPopup.h"

#include <QVBoxLayout>
#include <QScreen>
#include <QGuiApplication>
#include <QCursor>

namespace {
// Feste Startgröße — großzügig genug für Überschrift + Graph, ohne die
// Selektion-/Zeitraum-Steuerelemente (die im Compact-Modus ohnehin nicht
// sichtbar sind) braucht das Popup deutlich weniger Platz als die volle
// ViewChart im "Aktien-Chart"-Tab der ShareDetailsForm. Höhe gegenüber der
// ursprünglichen Fassung (340px, ohne Überschrift) um 40px erhöht, damit die
// neue Überschriftenzeile (ergänzt 31.07.2026) nicht auf Kosten der
// Chart-Fläche geht.
constexpr int kPopupWidth  = 560;
constexpr int kPopupHeight = 380;
} // namespace

// ── Constructor ───────────────────────────────────────────────────────────────

ChartPopup::ChartPopup(const QString& shareGuid, const QString& shareName, QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint)
    , m_shareName(shareName)
{
    setObjectName(QStringLiteral("ChartPopup"));
    // Selbstzerstörung beim Schließen (siehe leaveEvent()) — MainWindow
    // erzeugt das Popup ohne Owner und kümmert sich um dessen Lebenszyklus
    // nicht weiter.
    setAttribute(Qt::WA_DeleteOnClose);

    m_headerLabel = new QLabel();
    m_headerLabel->setObjectName(QStringLiteral("chartPopupHeader"));
    m_headerLabel->setAlignment(Qt::AlignCenter);
    m_headerLabel->setWordWrap(true);
    m_headerLabel->setTextFormat(Qt::RichText);
    m_headerLabel->setContentsMargins(4, 4, 4, 4);

    m_chart = new ViewChart(shareGuid, /*compact=*/true, this);
    connect(m_chart, &ViewChart::titleInfoChanged, this, &ChartPopup::updateHeaderText);
    // ViewChart's Konstruktor hat setRangeInfo()/titleInfoChanged() bereits
    // synchron einmal gefeuert (PresenterChart::loadAndDisplay(), s.o.) —
    // BEVOR die obige connect()-Zeile lief. Der initiale Wert würde ohne
    // diesen Nachhol-Aufruf verloren gehen (siehe ViewChart::rangeInfo()-
    // Doku und ChartPopup.h-Klassendoku).
    updateHeaderText(m_chart->rangeInfo());

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_headerLabel, 0);
    layout->addWidget(m_chart, 1);

    resize(kPopupWidth, kPopupHeight);
}

// ── showAt ────────────────────────────────────────────────────────────────────

void ChartPopup::showAt(const QPoint& globalPos)
{
    const QScreen* screen = QGuiApplication::screenAt(globalPos);
    const QRect avail = screen ? screen->availableGeometry()
                                : QGuiApplication::primaryScreen()->availableGeometry();

    QPoint topLeft = globalPos;
    topLeft.setX(qMin(topLeft.x(), avail.right()  - width()));
    topLeft.setY(qMin(topLeft.y(), avail.bottom() - height()));
    topLeft.setX(qMax(topLeft.x(), avail.left()));
    topLeft.setY(qMax(topLeft.y(), avail.top()));

    move(topLeft);
    show();
}

// ── leaveEvent ────────────────────────────────────────────────────────────────

void ChartPopup::leaveEvent(QEvent* event)
{
    // Portiert vom C#-Referenzverhalten (OnChartDailyValues_MouseLeave/
    // OnLblNoDataMessage_MouseLeave in FrmChart) — verlässt die Maus den
    // Fensterbereich, schließt sich das Popup automatisch.
    QWidget::leaveEvent(event);

    // Nessies Rückmeldung (31.07.2026): "Dialog geht zu, auch wenn die Maus
    // noch auf dem Dialog ist." Ursache: QChartView (QGraphicsView) hat
    // einen eigenen Viewport, an dessen inneren Widget-Grenzen (z. B. beim
    // Wechsel zwischen Chart-Zeichenfläche und der "Legende"-Box daneben)
    // Qt gelegentlich ein Leave auf ChartPopup selbst auslöst, obwohl die
    // Maus tatsächlich noch innerhalb des Popups steht — ein bekannter Qt-
    // Effekt bei QGraphicsView-Kindwidgets. Deshalb hier zusätzlich die
    // tatsächliche Cursor-Position gegen die eigene Bildschirmgeometrie
    // prüfen: nur schließen, wenn die Maus WIRKLICH außerhalb liegt.
    const QRect globalRect(mapToGlobal(QPoint(0, 0)), size());
    if (globalRect.contains(QCursor::pos()))
        return; // Falsches Leave (Maus ist noch innerhalb) — ignorieren.

    // close() löst dank Qt::WA_DeleteOnClose gleich die Selbstzerstörung mit aus.
    close();
}

// ── updateHeaderText ──────────────────────────────────────────────────────────

void ChartPopup::updateHeaderText(const QString& rangeInfoText)
{
    QString html = QStringLiteral("<b>%1</b>").arg(m_shareName.toHtmlEscaped());
    if (!rangeInfoText.isEmpty())
        html += QStringLiteral("<br>%1").arg(rangeInfoText.toHtmlEscaped());
    m_headerLabel->setText(html);
}
