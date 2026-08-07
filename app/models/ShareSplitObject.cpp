// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ShareSplitObject.h"

ShareSplitObject::ShareSplitObject(const QString& guid,
                                   const QString& shareGuid,
                                   const QDate&   date,
                                   double ratioNew,
                                   double ratioOld,
                                   bool   pricesAdjusted,
                                   const QString& comment)
    : m_guid(guid)
    , m_shareGuid(shareGuid)
    , m_date(date)
    , m_ratioNew(ratioNew)
    , m_ratioOld(ratioOld)
    , m_pricesAdjusted(pricesAdjusted)
    , m_comment(comment)
{}
