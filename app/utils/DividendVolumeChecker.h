// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../models/BuyObject.h"
#include "../models/SaleObject.h"
#include "../models/ShareSplitObject.h"

#include <QDate>
#include <QList>
#include <QString>

/**
 * @brief Ergebnis der Stückzahl-Plausibilitätsprüfung einer Dividende.
 *
 * `expectedVolume` und `enteredVolume` liegen beide in der BELEG-SKALA DES
 * EX-TAGS vor — siehe `DividendVolumeChecker` für die Begründung.
 */
struct DividendVolumeCheckResult
{
    /**
     * @brief false, wenn die Prüfung mangels Daten gar nicht möglich ist.
     *
     * Drei Fälle: kein (gültiger) Ex-Tag, keine Depotnummer, oder für die
     * Aktie ist überhaupt kein Kauf erfasst. In allen dreien bleiben
     * `expectedVolume`/`matches` bedeutungslos und der Aufruf darf NICHT als
     * Abweichung gewertet werden.
     */
    bool   checkable      = false;

    /// true, wenn `enteredVolume` innerhalb der Toleranz zu `expectedVolume` passt.
    bool   matches        = true;

    /// Errechneter Bestand im gewählten Depot am Ex-Tag, Beleg-Skala des Ex-Tags.
    double expectedVolume = 0.0;

    /// Die im Formular eingetragene Stückzahl, unverändert durchgereicht.
    double enteredVolume  = 0.0;

    /// Anzahl der in die Rechnung eingegangenen Käufe bzw. Verkäufe.
    int    consideredBuys  = 0;
    int    consideredSales = 0;

    /// enteredVolume − expectedVolume (positiv = es wurde zu viel eingetragen).
    double deviation() const { return enteredVolume - expectedVolume; }
};

/**
 * @brief Prüft, ob die bei einer Dividende eingetragene Stückzahl zum
 * tatsächlich gehaltenen Bestand des jeweiligen Depots am Ex-Tag passt.
 *
 * Zustandslos und vollständig datenbankfrei — Käufe, Verkäufe und Splits
 * kommen als Parameter herein, gleicher Stil wie `ShareSplitAdjuster`,
 * `SaleFifoAllocator` und `SplitPriceJumpDetector`. Phase 3 der
 * Ex-Tag-Behandlung, siehe ARCHITECTURE.md, "Plausibilitätsprüfung der
 * Dividenden-Stückzahl".
 *
 * ### Warum der Ex-Tag und nicht der Zahltag
 *
 * Die Bank schüttet auf den Bestand aus, der am Ex-Tag bestand — nicht auf
 * den am Zahltag. Zwischen beiden liegen typischerweise Tage bis Wochen, in
 * denen zugekauft oder verkauft worden sein kann. Das Formularfeld heisst
 * zwar "Anteile am Auszahlungstag", die Zahl auf der Abrechnung ist aber die
 * Ex-Tag-Stückzahl; deshalb ist der Ex-Tag die richtige Bezugsgrösse.
 *
 * ### Stichtagsregel: ECHT VOR dem Ex-Tag
 *
 * Der Ex-Tag ist der erste Handelstag, an dem die Aktie OHNE Dividenden-
 * anspruch gehandelt wird. Wer am Ex-Tag kauft, bekommt die Dividende also
 * nicht mehr; wer am Ex-Tag verkauft, bekommt sie noch. Gezählt werden
 * deshalb Käufe und Verkäufe mit einem Datum ECHT VOR dem Ex-Tag
 * (`< exDate`), nicht `<= exDate`.
 *
 * @note Das unterscheidet sich bewusst von `ShareSplitAdjuster::volumeFactor()`,
 * wo der Stichtag selbst noch zur alten Skala zählt. Beides ist jeweils
 * fachlich richtig: dort geht es um die Stückelung eines Belegs, hier um die
 * Dividendenberechtigung.
 *
 * ### Skalen
 *
 * Käufe und Verkäufe liegen jeweils in der Beleg-Skala IHRES EIGENEN Datums
 * vor. Liegt zwischen einem Kauf und dem Ex-Tag ein Split, sind das zwei
 * verschiedene Skalen — ein direktes Aufsummieren wäre dann genau um den
 * Split-Faktor falsch (derselbe Fallstrick wie in `SaleFifoAllocator`). Die
 * Summe wird deshalb über `ShareSplitAdjuster::adjustedVolume()` auf die
 * heutige Skala gebildet und am Ende über `ShareSplitAdjuster::belegVolume()`
 * auf die Beleg-Skala DES EX-TAGS zurückgerechnet — denn genau in dieser
 * Skala steht die Stückzahl auf der Dividendenabrechnung, und genau die trägt
 * der Benutzer ins Formular ein.
 *
 * Ohne Splits liefert `ShareSplitAdjuster` überall den Faktor 1,0; die
 * Rechnung ist dann eine schlichte Summe.
 */
class DividendVolumeChecker
{
public:
    DividendVolumeChecker() = delete;

    /**
     * @brief Toleranz beim Vergleich (absolut, in Stück).
     *
     * Die Eingabefelder der Anwendung führen vier Nachkommastellen; eine aus
     * einem Reverse-Split stammende Drittel-Stückzahl kann daher gerundet
     * erfasst sein (33,3333 statt 33,33333…). 1e-4 fängt genau diese
     * Rundung ab und ist gleichzeitig weit unterhalb jeder Abweichung, die
     * fachlich etwas bedeutet.
     */
    static constexpr double kVolumeTolerance = 1e-4;

    /**
     * @brief Bestand des Depots @p depotNumber am Ex-Tag, Beleg-Skala des Ex-Tags.
     *
     * @param buys        Alle Käufe der Aktie (alle Depots).
     * @param sales       Alle Verkäufe der Aktie (alle Depots).
     * @param splits      Alle Splits der Aktie.
     * @param depotNumber Depot, auf das gefiltert wird (getrimmt verglichen).
     * @param exDate      Ex-Tag der Dividende.
     * @param outBuys     Optional: Anzahl berücksichtigter Käufe.
     * @param outSales    Optional: Anzahl berücksichtigter Verkäufe.
     * @return Bestand in Stück; kann rechnerisch negativ werden, wenn die
     *         erfasste Historie unvollständig ist (wird NICHT auf 0 gekappt,
     *         damit der Aufrufer die Unstimmigkeit sieht statt sie zu
     *         verschlucken).
     */
    static double holdingsAtExDate(const QList<BuyObject>&        buys,
                                   const QList<SaleObject>&       sales,
                                   const QList<ShareSplitObject>& splits,
                                   const QString&                 depotNumber,
                                   const QDate&                   exDate,
                                   int* outBuys  = nullptr,
                                   int* outSales = nullptr);

    /**
     * @brief Vergleicht @p enteredVolume mit dem Bestand am Ex-Tag.
     *
     * @param enteredVolume Im Formular eingetragene Stückzahl.
     * @param exDate        Ex-Tag der Dividende.
     * @param depotNumber   Gewähltes Depot.
     * @param buys          Alle Käufe der Aktie (alle Depots).
     * @param sales         Alle Verkäufe der Aktie (alle Depots).
     * @param splits        Alle Splits der Aktie.
     * @return Ergebnis; `checkable == false`, wenn die Prüfung mangels Daten
     *         nicht möglich ist (siehe `DividendVolumeCheckResult::checkable`).
     */
    static DividendVolumeCheckResult check(double                         enteredVolume,
                                           const QDate&                   exDate,
                                           const QString&                 depotNumber,
                                           const QList<BuyObject>&        buys,
                                           const QList<SaleObject>&       sales,
                                           const QList<ShareSplitObject>& splits);
};
