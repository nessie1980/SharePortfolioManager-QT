// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IViewShareSplitEdit.h"
#include "IModelShareSplitEdit.h"

#include <QObject>
#include <QList>
#include <QString>

/**
 * @brief Presenter für den Dialog "Aktiensplits" (MVP-Pattern).
 *
 * Wie DividendForm und BrokeragesForm ohne Letzter-Eintrag-Beschränkung: jeder
 * Split ist jederzeit editier- und löschbar. Ein Beleg kann seit 08.08.2026
 * zugeordnet werden, wird aber nicht ausgewertet — es gibt (noch) keine
 * Parse-Pipeline für Split-Mitteilungen, siehe ARCHITECTURE.md, "Offene
 * Punkte", "Parsing von Split-Mitteilungen der Banken prüfen".
 *
 * Fachliche Prüfungen in validateInput():
 * - Datum gesetzt (Sentinel 01.01.2000 wie in allen anderen Formen)
 * - beide Verhältnis-Seiten > 0
 * - Faktor ungleich 1,0 — ein 1:1-"Split" ist keiner (Nessies Entscheidung
 *   08.08.2026) und würde nur Rechenzeit kosten
 * - kein zweiter Split derselben Aktie am selben Tag (`UNIQUE(share_guid, date)`)
 *
 * Zukünftige Ex-Tage sind ausdrücklich erlaubt (Nessies Entscheidung
 * 08.08.2026): ein angekündigter Split darf sofort erfasst werden. Technisch
 * ist das folgenlos, da `ShareSplitAdjuster::volumeFactor()` nur Datensätze
 * VOR dem Splittag umrechnet.
 */
class PresenterShareSplitEdit : public QObject
{
    Q_OBJECT

public:
    explicit PresenterShareSplitEdit(IViewShareSplitEdit*  view,
                                     IModelShareSplitEdit* model,
                                     const QString&        shareGuid,
                                     QObject*              parent = nullptr);

public slots:
    /** Speichert den erfassten bzw. bearbeiteten Split. */
    void onSave();

    /** Entfernt den geladenen Split nach Rückfrage. */
    void onRemove();

    /** Leert die Maske und hebt die Auswahl auf. */
    void onReset();

    /** Schliesst den Dialog. */
    void onClose();

    /**
     * @brief Lädt den in der Übersicht angeklickten Split in die Maske.
     * @param splitGuid  GUID des Splits; leer setzt die Maske zurück.
     */
    void onRowSelected(const QString& splitGuid);

    /** Aktualisiert die Umrechnungs-Vorschau nach jeder Eingabe im Verhältnis. */
    void onValuesChanged();

    /**
     * @brief Übernimmt ein im Dateidialog gewähltes Dokument.
     * @param path  Bereits gegen den Dokument-Root geprüfter Pfad.
     */
    void onDocumentSelected(const QString& path);

    /** Warnt, wenn der eingetragene Pfad schon einem anderen Split gehört. */
    void onDocumentPathEdited();

signals:
    /**
     * @brief Wird nach jeder erfolgreichen Änderung an `share_splits` gesendet.
     *
     * ViewShareEdit hängt daran refreshSummary(), damit die Split-Zeile in
     * "Allgemein" sofort nachzieht.
     */
    void dataChanged();

private:
    void    reloadOverview();
    void    refreshFactorPreview();
    QString validateInput() const;

    /**
     * @brief Heutiger Bestand für die übergebene Split-Liste.
     *
     * Summiert alle offenen Kauf-Posten, jeweils über
     * `ShareSplitAdjuster::volumeFactor()` auf heutige Stücke umgerechnet.
     * Wird zweimal aufgerufen — mit und ohne den zu löschenden Split —, um
     * die Bestandsänderung in der Löschabfrage beziffern zu können.
     */
    double volumeForSplits(const QList<ShareSplitObject>& splits) const;

    /** Kurzbeschreibung eines Splits für Meldungstexte, z. B. "20:1 vom 18.07.2022". */
    static QString describeSplit(const ShareSplitObject& split);

    /** Formatiert eine Verhältnis-Seite ohne unnötige Nachkommastellen. */
    static QString formatRatioPart(double value);

    IViewShareSplitEdit*    m_view  = nullptr;
    IModelShareSplitEdit*   m_model = nullptr;
    QString                 m_shareGuid;

    QList<ShareSplitObject> m_splits;      ///< Zwischenspeicher der Übersicht
    QString                 m_currentGuid; ///< GUID des geladenen Splits, leer = neu
};
