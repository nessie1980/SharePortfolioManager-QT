// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QString>

/**
 * @brief Bringt eine gespeicherte Depotnummer auf ihre reine Nummer.
 *
 * Bugfix 18.09.2026 (Nessies Bugreport: "Bestand am Ex-Tag 0,0000 Stk.").
 * Die alte C#-Anwendung legte die Depotnummer als `<Nummer> - <Bankname>`
 * ab, z. B. "8006189848 - ING diba". `PortfolioImporter` übernahm diesen
 * Wert unverändert. Die heutigen Formulare speichern dagegen nur die Nummer
 * aus `Documents.xml` (`BankIdentifierValue`) — sie ist der eindeutige
 * Schlüssel eines Depots, der Bankname reiner Anzeigetext (Klarstellung
 * 25.08.2026). Jeder Vergleich über die Depotnummer — Bestandsprüfung der
 * Dividenden am Ex-Tag, FIFO-Zuteilung beim Verkauf — fand die
 * importierten Käufe deshalb nicht.
 *
 * Die Regel steht an genau EINER Stelle und wird von zwei Aufrufern
 * benutzt: `Database::migrateDepotNumbers()` (bestehende Portfolios) und
 * `PortfolioImporter` (neue Importe). Header-only, weil zustandslos — und
 * weil der Importer die Database-Bibliothek ohnehin einbindet, liegt die
 * Datei bei ihr.
 *
 * ### Die Regel
 *
 * Aus `<Ziffern> - <beliebiger Text>` wird `<Ziffern>`. Alles andere bleibt
 * UNVERÄNDERT, insbesondere:
 * - ein Wert, der schon nur aus der Nummer besteht,
 * - ein Wert, dessen Teil vor " - " nicht ausschliesslich aus Ziffern
 *   besteht (z. B. "ING - diba") — hier wäre jede Umformung geraten,
 * - ein Bindestrich OHNE Leerzeichen ("8006189848-ING"): das alte Format
 *   trug immer " - ", eine andere Schreibweise stammt nicht von dort.
 *
 * Führende Nullen bleiben erhalten (Consors: "0878031421"); die Nummer wird
 * nicht als Zahl interpretiert, sondern als Zeichenkette abgeschnitten.
 */
namespace DepotNumberNormalizer
{

/// Trennzeichen zwischen Nummer und Bankname im alten C#-Format.
inline QString separator() { return QStringLiteral(" - "); }

/**
 * @brief Normalisierte Depotnummer nach obiger Regel.
 *
 * @param value  Gespeicherter oder importierter Wert.
 * @return Die reine Nummer, wenn @p value dem alten Format entspricht;
 *         sonst @p value unverändert.
 */
inline QString normalize(const QString& value)
{
    const int pos = value.indexOf(separator());
    if (pos < 0)
        return value;

    const QString number = value.left(pos).trimmed();
    if (number.isEmpty())
        return value;

    for (const QChar c : number) {
        // Bewusst nur ASCII-Ziffern — QChar::isDigit() liesse auch andere
        // Schriftsysteme durch, die in einer Depotnummer nichts verloren haben.
        if (c < QLatin1Char('0') || c > QLatin1Char('9'))
            return value;
    }
    return number;
}

/**
 * @brief true, wenn normalize() den Wert verändern würde.
 * @param value  Gespeicherter oder importierter Wert.
 */
inline bool needsNormalization(const QString& value)
{
    return normalize(value) != value;
}

} // namespace DepotNumberNormalizer
