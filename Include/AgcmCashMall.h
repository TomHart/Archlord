#pragma once

#include "AgpmCashMall.h"

#include "AgcmCharacter.h"
#include "AgcmItem.h"

class AgcmCashMall : public AgcModule {
private:
	AgpmFactors		*m_pcsAgpmFactors;
	AgpmItem		*m_pcsAgpmItem;
	AgpmCashMall	*m_pcsAgpmCashMall;
	AgcmCharacter	*m_pcsAgcmCharacter;
	AgcmItem		*m_pcsAgcmItem;

public:
	AgcmCashMall();

	virtual BOOL	OnAddModule();
	virtual BOOL	OnDestroy();

	AgpdItem*	CreateDummyItem(INT32 lTID, INT32 lCount);

private:
	BOOL		DestroyDummyItem(AgpdItem *pcsItem);
	BOOL		DestroyAllDummyItem();

public:
	BOOL	SendPacketRequestMallList(INT32 lTab);
	BOOL	SendPacketBuyProduct(INT32 lProductID, INT32 lType);
	BOOL	SendPacketRefreshCash();
	BOOL	SendPacketCheckListVersion();

	static BOOL	CBUpdateMallList(PVOID pData, PVOID pClass, PVOID pCustData);
};