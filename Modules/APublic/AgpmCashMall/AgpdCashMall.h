#pragma once

#include "ApDefine.h"

#define	AGPMCASHMALL_MIN_INTERVAL_REFRESH_CASH		100

typedef struct _AgpdCashMall
{
	UINT32		m_ulLastRefreshCashTimeMsec;
} AgpdCashMall;