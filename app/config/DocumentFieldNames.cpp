// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "DocumentFieldNames.h"

namespace DocumentFieldNames {

// ── ShareAddForm ─────────────────────────────────────────────────────────────

const QStringList& shareAddKnown()
{
    // Vollstaendig durchlaufen, auch wenn ParserLib wegen eines fehlenden
    // Pflichtfelds vorzeitig aufgehoert hat (ParsingFailed).
    //
    // Das ist buyKnown() plus Wkn/Isin/Name — bewusst ausgeschrieben und
    // nicht abgeleitet, siehe Klassennotiz im Header.
    static const QStringList names = {
        "Wkn","Isin","Name","Date","Time","DepotNumber","OrderNumber",
        "Volume","Price","Provision","BrokerFee","TraderPlaceFee","Reduction"
    };
    return names;
}

const QStringList& shareAddRequired()
{
    static const QStringList names = {
        "Wkn","Isin","Name","Date","DepotNumber","OrderNumber","Volume","Price"
    };
    return names;
}

// ── BuysForm ─────────────────────────────────────────────────────────────────

const QStringList& buyKnown()
{
    static const QStringList names = {
        "Date","Time","DepotNumber","OrderNumber",
        "Volume","Price","Provision","BrokerFee","TraderPlaceFee","Reduction"
    };
    return names;
}

const QStringList& buyRequired()
{
    static const QStringList names = {
        "Date","DepotNumber","OrderNumber","Volume","Price"
    };
    return names;
}

// ── SalesForm ────────────────────────────────────────────────────────────────

const QStringList& saleKnown()
{
    // Wie buyKnown(), zusaetzlich die drei Steuerfelder. Der Verkaufsbeleg
    // nennt sie, der Kaufbeleg nicht.
    //
    // "CapitalGainTax" OHNE s — bis zum 02.09.2026 stand hier "CapitalGainsTax"
    // MIT s, und Documents.xml kennt diese Schreibweise an keiner Stelle. Die
    // Kapitalertragssteuer eines Verkaufsbelegs wurde dadurch nie ins
    // Formular uebernommen, obwohl der Parser sie findet. Siehe
    // ARCHITECTURE.md, "Die Kapitalertragssteuer kam bei Verkaeufen nie an".
    static const QStringList names = {
        "Date","Time","DepotNumber","OrderNumber",
        "Volume","Price",
        "TaxAtSource","CapitalGainTax","SolidarityTax",
        "Provision","BrokerFee","TraderPlaceFee","Reduction"
    };
    return names;
}

const QStringList& saleRequired()
{
    static const QStringList names = {
        "Date","DepotNumber","OrderNumber","Volume","Price"
    };
    return names;
}

// ── DividendForm ─────────────────────────────────────────────────────────────

const QStringList& dividendKnown()
{
    // "CapitalGainTax" OHNE s, wie in Documents.xml. Diese Liste war immer
    // richtig; die Verkaufsliste oben trug bis zum 02.09.2026 faelschlich ein
    // s und traf damit ins Leere.
    static const QStringList names = {
        "Date","Time","ExDate","DepotNumber","Volume","DividendRate",
        "TaxAtSource","CapitalGainTax","SolidarityTax",
        "ExchangeRate","Currency"
    };
    return names;
}

const QStringList& dividendRequired()
{
    // ExDate und DepotNumber zaehlen als Pflicht, weil sie seit Phase 2 auch
    // im Formular Pflicht sind: findet der Parser sie nicht, IST die Analyse
    // unvollstaendig und der Benutzer muss nachtragen. Die Statuszeile sagt
    // das dann ehrlich ("4/5 Pflicht"), statt eine vollstaendige Uebernahme
    // vorzuspiegeln (Phase 5, 21.08.2026).
    static const QStringList names = {
        "Date","ExDate","DepotNumber","Volume","DividendRate"
    };
    return names;
}

} // namespace DocumentFieldNames
