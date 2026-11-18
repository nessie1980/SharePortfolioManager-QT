// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QColor>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QSignalBlocker>
#include <QString>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

/**
 * @brief Gruppe zum Auf- und Zuklappen: Kopfzeile mit Pfeil, Titel links und
 * Kurzinfo rechts, darunter der Inhalt (ergänzt 18.11.2026, v1.31.0).
 *
 * Eingeführt für den Tab "Depot- vs. Marktwert" (Mischform, Nessies
 * Entscheidung 17.11.2026): unter der Gegenüberstellung stehen Abgleich,
 * Auffälligkeiten und die Transaktionen als Gruppen untereinander. Die
 * Kurzinfo zeigt auch im zugeklappten Zustand das Wichtigste, z. B. Anzahl
 * und Summe oder "2 Käufe abweichend".
 *
 * Header-only ohne Q_OBJECT wie GridStyle.h: der Klick auf die Kopfzeile ist
 * über ein Lambda verbunden, eigene Signale gibt es nicht. Wer den Zustand
 * vorgibt, ruft setExpanded() auf.
 *
 * @code
 * auto* section = new CollapsibleSection(tr("Käufe (3)"));
 * section->setSummary(tr("290 Stk. · 8.717,30 €"));
 * section->bodyLayout()->addWidget(table);
 * section->setExpanded(false);
 * @endcode
 */
class CollapsibleSection : public QWidget
{
public:
    explicit CollapsibleSection(const QString& title = QString(), QWidget* parent = nullptr)
        : QWidget(parent)
    {
        auto* outer = new QVBoxLayout(this);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->setSpacing(0);

        // ── Kopfzeile ─────────────────────────────────────────────────────
        m_header = new QFrame();
        m_header->setFrameShape(QFrame::StyledPanel);
        m_header->setAutoFillBackground(true);
        m_header->setBackgroundRole(QPalette::AlternateBase);
        auto* headerLayout = new QHBoxLayout(m_header);
        headerLayout->setContentsMargins(4, 2, 8, 2);
        headerLayout->setSpacing(8);

        m_toggle = new QToolButton();
        m_toggle->setObjectName(QStringLiteral("collapsibleSectionToggle"));
        m_toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        m_toggle->setAutoRaise(true);
        m_toggle->setCheckable(true);
        QFont font = m_toggle->font();
        font.setBold(true);
        m_toggle->setFont(font);
        headerLayout->addWidget(m_toggle);
        headerLayout->addStretch(1);

        m_summary = new QLabel();
        m_summary->setObjectName(QStringLiteral("collapsibleSectionSummary"));
        m_summary->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        headerLayout->addWidget(m_summary);
        outer->addWidget(m_header);

        // ── Inhalt ────────────────────────────────────────────────────────
        m_body = new QFrame();
        m_body->setFrameShape(QFrame::StyledPanel);
        m_bodyLayout = new QVBoxLayout(m_body);
        m_bodyLayout->setContentsMargins(6, 4, 6, 6);
        m_bodyLayout->setSpacing(4);
        outer->addWidget(m_body);

        QObject::connect(m_toggle, &QToolButton::toggled, this,
                         [this](bool expanded) { applyExpanded(expanded); });

        setTitle(title);
        setExpanded(false);
    }

    void setTitle(const QString& title) { m_toggle->setText(title); }
    QString title() const { return m_toggle->text(); }

    /// Kurzinfo rechts in der Kopfzeile; ungültige Farbe = normale Textfarbe.
    void setSummary(const QString& text, const QColor& color = QColor())
    {
        m_summary->setText(text);
        QPalette palette = m_summary->palette();
        palette.setColor(QPalette::WindowText,
                         color.isValid() ? color : this->palette().color(QPalette::WindowText));
        m_summary->setPalette(palette);
    }
    QString summary() const { return m_summary->text(); }

    /// Klappt auf oder zu, ohne dass ein Klick nötig ist.
    void setExpanded(bool expanded)
    {
        const QSignalBlocker blocker(m_toggle);
        m_toggle->setChecked(expanded);
        applyExpanded(expanded);
    }
    bool isExpanded() const { return m_toggle->isChecked(); }

    /// Layout des Inhalts; hier hängt der Aufrufer seine Widgets ein.
    QVBoxLayout* bodyLayout() const { return m_bodyLayout; }

private:
    void applyExpanded(bool expanded)
    {
        m_toggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        m_body->setVisible(expanded);
    }

    QFrame*      m_header     = nullptr;
    QToolButton* m_toggle     = nullptr;
    QLabel*      m_summary    = nullptr;
    QFrame*      m_body       = nullptr;
    QVBoxLayout* m_bodyLayout = nullptr;
};
