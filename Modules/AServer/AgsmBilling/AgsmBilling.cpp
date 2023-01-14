/*===================================================================

	AgsmBilling.h

===================================================================*/

#include "AuGameEnv.h"
#include "AuTimeStamp.h"
#include "AgsmBilling.h"
#include "AgsmCashMall.h"
#include "AgpmBillInfo.h"
#include "AgsmBillInfo.h"
#include "AgppBillInfo.h"

#ifdef _AREA_JAPAN_
#include "AgsmBillingJapan.h"
#elif defined(_AREA_CHINA_)
#include "AgsmBillingChina.h"
#elif defined(_AREA_KOREA_)
#include "HannEverBillForSvr.h"
#elif defined(_AREA_GLOBAL_)
#include "AgsmBillingGlobal.h"
#endif

AgsmBilling* AgsmBilling::m_pInstance = NULL;

// 일본에서 사용하는 주문 번호의 기본형을 만든다.
// AL01_07_08_23_01 과 같은 스트링을 만들어냄(_제외)
void GetOrderSeed( INT32 serverID, char* buffer, int length )
{
	SYSTEMTIME st;
	GetLocalTime( &st );

	_snprintf_s( buffer, length, _TRUNCATE,
				 "AL%02d%02d%02d%02d%02d%02d",
				 serverID%100,	// 일본에서 서버 ID를 2자리만 쓴다고 해서 이렇게 해놨음. 혹 3자리 이상이 나오면 문제가 있어서
				 st.wYear%100, st.wMonth, st.wDay, st.wHour, st.wMinute );
}

/****************************************************/
/*		The Implementation of AgsmBilling class		*/
/****************************************************/
//
BOOL AgsdBillingMoney::Decode(char *pszPacket, INT32 lCode)
	{
	if (NULL == pszPacket || '\0' == *pszPacket)
		return FALSE;
	
	switch (lCode)
		{
		case 99 :
			m_lResult = AGSMBILLING_RESULT_SYSTEM_FAILURE;
			break;
		
		case 0 :
		case 1 :
			if (4 == sscanf(pszPacket, "%I64d|%I64d|%I64d|%I64d|", &m_llMoney,
																   &m_llInternalEventMoney,
																   &m_llExternalEventMoney,
																   &m_llCouponMoney)
				)
				m_lResult = AGSMBILLING_RESULT_SUCCESS;
			break;
	
		default :
			m_lResult = AGSMBILLING_RESULT_FAIL;
			break;
		}
	
	return TRUE;
	}


BOOL AgsdBillingItem::Decode(char *pszPacket, INT32 lCode)
	{
	if (NULL == pszPacket || '\0' == *pszPacket)
		return FALSE;
	
	switch (lCode)
		{
		case 0 :	
			m_lResult = AGSMBILLING_RESULT_SUCCESS;
			// 주문번호.
			m_ullOrderNo = _atoi64(pszPacket);
			break;
		
		case 99 :
			m_lResult = AGSMBILLING_RESULT_SYSTEM_FAILURE;
			break;

		case 4 :
			m_lResult = AGSMBILLING_RESULT_NOT_ENOUGH_MONEY;
			break;
						
		default :		// 다른 응답코드도 있지만 모두 FAIL로 처리한다.
			m_lResult = AGSMBILLING_RESULT_FAIL;
			break;
		}			
	
	return TRUE;
	}




/****************************************************/
/*		The Implementation of AgsmBilling class		*/
/****************************************************/
//
AgsmBilling::AgsmBilling()
	{
	m_pInstance = this;

	SetModuleName(_T("AgsmBilling"));
	EnableIdle2(TRUE);

	m_pAgsmCharacter			= NULL;
	m_pAgsmServerManager2		= NULL;
	
	m_pAgpmCashMall				= NULL;
	m_pAgsmCashMall				= NULL;
	m_pAgsmItemManager			= NULL;
	m_pAgpmItem					= NULL;
	m_pAgsmItem					= NULL;
	m_pagsmSkill				= NULL;

	m_pAgsmBillingChina			= NULL;
	m_pAgsmBillingJapan			= NULL;
	m_pAgsmBillingGlobal		= NULL;
	m_pAgsdServerBilling		= NULL;

	m_pagpmBillInfo				= NULL;
	m_pagsmBillInfo				= NULL;
	m_pagpmFactors				= NULL;

	m_ulLastCheckClockClock	= 0;
	m_ulLastCheckClockForHanGame = 0;

	InitFuncPtr();
	}

AgsmBilling::~AgsmBilling()
{
#ifdef _AREA_JAPAN_
	if(m_pAgsdServerBilling)
		delete m_pAgsdServerBilling;
	if(m_pAgsmBillingJapan)
		delete m_pAgsmBillingJapan;
#elif defined (_AREA_CHINA_)
	if(m_pAgsmBillingChina)
		delete m_pAgsmBillingChina;
#elif defined(_AREA_GLOBAL_)
	if(m_pAgsmBillingGlobal)
		delete m_pAgsmBillingGlobal;
#endif
}

// Callback setting method(for result processing)
//==========================================================
//
BOOL AgsmBilling::SetCallbackGetCashMoney(ApModuleDefaultCallBack pfCallback, PVOID pClass)
{
	return SetCallback(AGSMBILLING_CB_RESULT_GETMONEY, pfCallback, pClass);
}


BOOL AgsmBilling::SetCallbackBuyCashItem(ApModuleDefaultCallBack pfCallback, PVOID pClass)
{
	return SetCallback(AGSMBILLING_CB_RESULT_BUYITEM, pfCallback, pClass);
}

//	Admin
//======================================================
//
AgsdBilling* AgsmBilling::Get(INT32 lID)
{
	return m_Admin.Get(lID);
}

AgsdBilling* AgsmBilling::Get(CHAR* szAccountID)
{
	return m_Admin.Get(szAccountID);
}

BOOL AgsmBilling::Add(AgsdBilling *pAgsdBilling)
{
	return m_Admin.Add(pAgsdBilling);
}

BOOL AgsmBilling::Add(AgsdBilling* pAgsdBilling, CHAR* AccountID)
{
	return m_Admin.Add(pAgsdBilling, AccountID);
}

BOOL AgsmBilling::Remove(INT32 lID)
{

	return m_Admin.Remove(lID);
}

BOOL AgsmBilling::Remove(CHAR* AccountID)
{

	return m_Admin.Remove(AccountID);
}

void AgsmBilling::InitFuncPtr( void )
{
#ifdef _AREA_CHINA_
	OnAddModulePtr	= &AgsmBilling::OnAddModuleCn;
	OnInitPtr		= &AgsmBilling::OnInitCn;
	OnDisconnectPtr = &AgsmBilling::OnDisconnectCn;

	SendGetCashMoneyPtr		= &AgsmBilling::SendGetCashMoneyCn;
	SendBuyCashItemPtr		= &AgsmBilling::SendBuyCashItemCn;
#elif defined(_AREA_JAPAN_)
	OnAddModulePtr	= &AgsmBilling::OnAddModuleJp;
	OnInitPtr		= &AgsmBilling::OnInitJp;
	OnDisconnectPtr = &AgsmBilling::OnDisconnectJp;

	SendGetCashMoneyPtr		= &AgsmBilling::SendGetCashMoneyJp;
	SendBuyCashItemPtr		= &AgsmBilling::SendBuyCashItemJp;
#elif defined(_AREA_KOREA_)
	OnAddModulePtr	= &AgsmBilling::OnAddModuleKr;
	OnInitPtr		= &AgsmBilling::OnInitKr;
	OnDisconnectPtr = &AgsmBilling::OnDisconnectKr;

	ConnectBillingServerPtr = &AgsmBilling::ConnectBillingServerKr;
	SendGetCashMoneyPtr		= &AgsmBilling::SendGetCashMoneyKr;
	SendBuyCashItemPtr		= &AgsmBilling::SendBuyCashItemKr;
#elif defined(_AREA_GLOBAL_)
	OnAddModulePtr	= &AgsmBilling::OnAddModuleGlobal;
	OnInitPtr		= &AgsmBilling::OnInitGlobal;
	OnDisconnectPtr = &AgsmBilling::OnDisconnectGlobal;

	SendGetCashMoneyPtr		= &AgsmBilling::SendGetCashMoneyGlobal;
	SendBuyCashItemPtr		= &AgsmBilling::SendBuyCashItemGlobal;
#endif
}
//	ApModule inherited
//====================================================
BOOL AgsmBilling::OnIdle2(UINT32 ulClockCount)
{

	return TRUE;
}

//	Socket callback point
//======================================================
//
BOOL AgsmBilling::DispatchBilling(PVOID pvPacket, PVOID pvParam, PVOID pvSocket)
{
	if (NULL == pvPacket || NULL == pvParam || NULL == pvSocket)
		return FALSE;

	AgsmBilling		*pThis = (AgsmBilling *) pvParam;
	AsServerSocket	*pSocket = (AsServerSocket *) pvSocket;

	return pThis->OnReceive(pvPacket, pSocket->GetIndex());
}

BOOL AgsmBilling::DisconnectBilling(PVOID pvPacket, PVOID pvParam, PVOID pvSocket)
{
	if (NULL == pvParam || NULL == pvSocket)
		return FALSE;

	AgsmBilling		*pThis = (AgsmBilling *) pvParam;
	AsServerSocket	*pSocket = (AsServerSocket *) pvSocket;

	return pThis->OnDisconnect();
}

//	Packet processing
//===========================================================
//
BOOL AgsmBilling::OnReceive(PVOID pvPacket, UINT32 ulNID)
	{
	if (NULL == pvPacket || 0 == ulNID)
		return FALSE;

	char *pszPacket = (char *) pvPacket;
	
	char sz[6];
	// code
	ZeroMemory(sz, sizeof(sz));
	char *psz = sz;
	for (INT32 i = 0; i < 2; ++i)
		*psz++ = *pszPacket++;
	
	INT32 lCode = atoi(sz);

	// length
	ZeroMemory(sz, sizeof(sz));
	psz = sz;
	for (INT32 i = 0; i < 5; ++i)
		*psz++ = *pszPacket++;
		
	INT32 lLength = atoi(sz);

	// skip '|'
	pszPacket++;

	char szKey[21];
	ZeroMemory(szKey, sizeof(szKey));
	char *pszKey = szKey;
	while ('|' != *pszPacket)
		*pszKey++ = *pszPacket++;

	INT32 lKey = atoi(szKey);

	// find AgsdBilling
	AgsdBilling *pAgsdBilling = Get(lKey);
	if (NULL == pAgsdBilling)
		{
		return FALSE;
		}

	// skip '|'
	pszPacket++;
	
	char szFormat[256];
	ZeroMemory(szFormat, sizeof(szFormat));
	strncpy(szFormat, pszPacket, lLength);

	if (FALSE == pAgsdBilling->Decode(szFormat, lCode))
		return FALSE;
		
	if (AGSMBILLING_TYPE_MONEY == pAgsdBilling->Type())
		EnumCallback(AGSMBILLING_CB_RESULT_GETMONEY, pAgsdBilling, pAgsdBilling->m_pAgpdCharacter);
	else if (AGSMBILLING_TYPE_ITEM == pAgsdBilling->Type())
		EnumCallback(AGSMBILLING_CB_RESULT_BUYITEM, pAgsdBilling, pAgsdBilling->m_pAgpdCharacter);
	
	m_GenerateID.AddRemoveID(pAgsdBilling->m_lID);
	Remove(pAgsdBilling->m_lID);
	pAgsdBilling->Release();
	
	return TRUE;
	}

//	Connection
//==========================================================
//

eAGSMBILLING_CONNECT_RESULT AgsmBilling::ConnectBillingServer()
{
#ifdef _AREA_KOREA_
	return ConnectBillingServerKr();
#else
	return AGSMBILLING_CONNECT_RESULT_SUCCESS;
#endif
}

BOOL AgsmBilling::CheckConnectBillingServer(INT32 lCID, PVOID pClass, UINT32 uDelay, PVOID pvData)
{
	if (NULL == pClass)
		return FALSE;

	AgsmBilling *pThis = (AgsmBilling *) pClass;

	if (NULL == pThis->m_pAgsdServerBilling)
		return FALSE;

	if (pThis->m_pAgsdServerBilling->m_bIsConnected == FALSE)
	{
		eAGSMBILLING_CONNECT_RESULT eResult = pThis->ConnectBillingServer();
		if (AGSMBILLING_CONNECT_RESULT_SUCCESS == eResult ||
			AGSMBILLING_CONNECT_RESULT_NEED_WAIT == eResult)
			return TRUE;
		else
			return FALSE;
	}
	return TRUE;
}

BOOL AgsmBilling::CheckBillingServer()
{
	return TRUE;
}
//
BOOL AgsmBilling::OnDisconnectKr()
{
	if (NULL != m_pAgsdServerBilling)
	{
		m_pAgsdServerBilling->m_bIsConnected = FALSE;
		if (!AgsModule::AddTimer(3000, m_pAgsdServerBilling->m_lServerID, this, CheckConnectBillingServer, m_pAgsdServerBilling))
			return FALSE;
	}

	return TRUE;
}

BOOL AgsmBilling::OnDisconnectJp()
{
	return TRUE;	
}

BOOL AgsmBilling::OnDisconnectCn()
{
	return TRUE;	
}


//	Billing methods
//=========================================================================
//
//	!!! UNICODE기반으로 소스가 바뀔 경우 빌링서버의 프로토콜 포맷이 바뀌지
//	않으면 multi-byte 코드로 수정해서 보내야 한다.
//


BOOL AgsmBilling::SendBuyCashItem(AgpdCharacter *pAgpdCharacter, INT32 lProductID, CHAR *pszDesc, INT64 llMoney, stCashItemBuyList &sList)
{ 
	return (this->*SendBuyCashItemPtr)(pAgpdCharacter, lProductID, pszDesc, llMoney, sList);
}

#if  defined(_AREA_GLOBAL_) 
BOOL AgsmBilling::OnAddModuleGlobal()
{
	return TRUE;
}

BOOL AgsmBilling::OnInitGlobal()
{
	m_pAgsmServerManager2	= (AgsmServerManager2*)GetModule("AgsmServerManager2");
	m_pagpmBillInfo			= (AgpmBillInfo*)GetModule("AgpmBillInfo");
	m_pagsmBillInfo			= (AgsmBillInfo*)GetModule("AgsmBillInfo");
	m_pAgsmCharacter		= (AgsmCharacter*)GetModule("AgsmCharacter");
	m_pagpmFactors			= (AgpmFactors*)GetModule("AgpmFactors");
	m_pAgpmCashMall			= (AgpmCashMall*)GetModule("AgpmCashMall");
	m_pAgpmItem				= (AgpmItem*)GetModule("AgpmItem");
	m_pAgsmItem				= (AgsmItem*)GetModule("AgsmItem");
	m_pagsmSkill			= (AgsmSkill*)GetModule("AgsmSkill");

	if ( !m_pAgsmServerManager2 || !m_pagpmBillInfo || !m_pAgsmCharacter || !m_pagpmFactors || !m_pAgpmCashMall || !m_pAgpmItem )
		return FALSE;

	m_Admin.SetCount(10000);

	if (!m_Admin.InitializeObject(sizeof(AgsdBilling *), m_Admin.GetCount()))
		return FALSE;

	m_pAgsdServerBilling = new AgsdServer;
	if(!m_pAgsdServerBilling)
		return FALSE;

	m_pAgsdServerBilling->m_bIsConnected = FALSE;

	return TRUE;
}

BOOL AgsmBilling::OnDisconnectGlobal()
{
	return TRUE;	
}

BOOL AgsmBilling::SendBuyCashItemGlobal(AgpdCharacter *pAgpdCharacter, INT32 lProductID, CHAR *pszDesc, INT64 llMoney, stCashItemBuyList &sList)
{
	AgsdCharacter *pAgsdCharacter = m_pAgsmCharacter->GetADCharacter(pAgpdCharacter);
	if (NULL == pAgsdCharacter)
		return FALSE;

	AgsdServer *pAgsdServer = m_pAgsmServerManager2->GetThisServer();
	if (NULL == pAgsdServer)
		return FALSE;

	AgpmCashItemInfo* pCashItemInfo = m_pAgpmCashMall->GetCashItem( lProductID );
	if ( NULL == pCashItemInfo )
		return FALSE;

	AgpdItemTemplate* pItemTemplate = m_pAgpmItem->GetItemTemplate( pCashItemInfo->m_alItemTID[0] );
	if ( NULL == pItemTemplate )
		return FALSE;

	INT32 Class = m_pagpmFactors->GetClass(&pAgpdCharacter->m_csFactor);
	INT32 Level = m_pagpmFactors->GetLevel(&pAgpdCharacter->m_csFactor);
	//	int MethodType = (lType == AGPMCASHMALL_TYPE_WCOIN_PPCARD) ? 1 : 0;

	CHAR szItemName[50] = {0, };
	strncpy_s(szItemName, sizeof(szItemName), pItemTemplate->m_szName, _TRUNCATE);

	UINT64 ullListSeq	= sList.m_ullBuyID;
	INT32 lType			= sList.m_lType; 

	AgsdBillingItem *pAgsdBilling = new AgsdBillingItem;
	pAgsdBilling->m_lID = m_GenerateID.GetID();
	pAgsdBilling->m_pAgpdCharacter = pAgpdCharacter;
	pAgsdBilling->m_lProductID = lProductID;
	pAgsdBilling->m_llBuyMoney = llMoney;
	pAgsdBilling->m_ullListSeq = ullListSeq;
	pAgsdBilling->m_lType      = lType;

	if (FALSE == Add(pAgsdBilling, pAgsdCharacter->m_szAccountID))
	{
		m_GenerateID.AddRemoveID(pAgsdBilling->m_lID);
		Remove(pAgsdCharacter->m_szAccountID);
		pAgsdBilling->Release();

		return FALSE;
	}

	return TRUE;
}

BOOL AgsmBilling::OnInquireCash(long ReturnCode, CHAR* AccountID, double WCoin, double PCoin )
{
	AgsdBilling *pAgsdBilling = Get(AccountID);
	if (!pAgsdBilling)
		return FALSE;

	if(pAgsdBilling->Type() != AGSMBILLING_TYPE_MONEY)
		return FALSE;

	if(ReturnCode != 0)
		return FALSE;

	AgpdCharacter* pcsCharacter = pAgsdBilling->m_pAgpdCharacter;
	if(!pcsCharacter)
		return FALSE;

	AgsdCharacter* pagsdCharacter = m_pAgsmCharacter->GetADCharacter(pcsCharacter);
	if(!pagsdCharacter)
		return FALSE;

	m_pagpmBillInfo->SetCashGlobal(pcsCharacter, WCoin, PCoin);

	m_GenerateID.AddRemoveID(pAgsdBilling->m_lID);
	Remove(pagsdCharacter->m_szAccountID);
	pAgsdBilling->Release();

	CashInfoGlobal pCash;
	m_pagpmBillInfo->GetCashGlobal(pcsCharacter, pCash.m_WCoin, pCash.m_PCoin);

	PACKET_BILLINGINFO_CASHINFO pPacket(pcsCharacter->m_lID, pCash.m_WCoin, pCash.m_PCoin);
	SendPacketUser(pPacket, m_pAgsmCharacter->GetCharDPNID(pcsCharacter));

	return TRUE;
}

/*
BOOL AgsmBilling::OnInquireCash(long ReturnCode, CHAR* AccountID, double CoinSum )
{
	AgsdBilling *pAgsdBilling = Get(AccountID);
	if (!pAgsdBilling)
		return FALSE;

	if(pAgsdBilling->Type() != AGSMBILLING_TYPE_MONEY)
		return FALSE;

	if(ReturnCode != 0)
		return FALSE;

	AgpdCharacter* pcsCharacter = pAgsdBilling->m_pAgpdCharacter;
	if(!pcsCharacter)
		return FALSE;

	AgsdCharacter* pagsdCharacter = m_pAgsmCharacter->GetADCharacter(pcsCharacter);
	if(!pagsdCharacter)
		return FALSE;

	m_pagpmBillInfo->SetCashTaiwan(pcsCharacter, CoinSum);

	m_GenerateID.AddRemoveID(pAgsdBilling->m_lID);
	Remove(pagsdCharacter->m_szAccountID);
	pAgsdBilling->Release();

	CashInfoGlobal pCash;
	m_pagpmBillInfo->GetCashGlobal(pcsCharacter, pCash.m_WCoin, pCash.m_PCoin);

	PACKET_BILLINGINFO_CASHINFO pPacket(pcsCharacter->m_lID, pCash.m_WCoin, pCash.m_PCoin);
	SendPacketUser(pPacket, m_pAgsmCharacter->GetCharDPNID(pcsCharacter));

	return TRUE;
}
*/

BOOL AgsmBilling::OnBuyProduct(long ResultCode, CHAR* AccounID, DWORD DeductCashSeq)
{
	AgsdBilling *pAgsdBilling = Get(AccounID);
	if (NULL == pAgsdBilling)
		return FALSE;

	AgsdBillingItem* pcsBillingItem = (AgsdBillingItem*)pAgsdBilling;
	pcsBillingItem->m_ullOrderNo = DeductCashSeq;

	AgpdCharacter* pcsCharacter = pAgsdBilling->m_pAgpdCharacter;

	eAGSMBILLING_RESULT lResult;

	switch(ResultCode)
	{
	case 0:
		{
			if(DeductCashSeq != -1)
				lResult = AGSMBILLING_RESULT_SUCCESS;
			else
				lResult = AGSMBILLING_RESULT_SYSTEM_FAILURE;
		} break;
	case 1:
		{
			lResult = AGSMBILLING_RESULT_NOT_ENOUGH_MONEY;				
		} break;
	default:
		{
			lResult = AGSMBILLING_RESULT_SYSTEM_FAILURE;
		} break;
	}

	pAgsdBilling->m_lResult = (INT32)lResult;

	EnumCallback(AGSMBILLING_CB_RESULT_BUYITEM, pAgsdBilling, pAgsdBilling->m_pAgpdCharacter);

	m_GenerateID.AddRemoveID(pAgsdBilling->m_lID);
	Remove(AccounID);
	pAgsdBilling->Release();

	if(lResult == AGSMBILLING_RESULT_SUCCESS) // success
		SendGetCashMoney(pcsCharacter);

	return TRUE;
}

BOOL AgsmBilling::OnInquirePersonDeduct(DWORD AccountGUID, DWORD ResultCode)
{
	return TRUE;
}

BOOL AgsmBilling::OnUserStatus(DWORD AccountGUID, DWORD BillingGUID, DWORD RealEndDate, DOUBLE RestTime, INT32 DeductType)
{
	return TRUE;
}

void AgsmBilling::CheckIn(AgpdCharacter *pAgpdCharacter)
{
}

void AgsmBilling::CheckOut(AgpdCharacter *pAgpdCharacter)
{
}

BOOL AgsmBilling::SendGetCashMoneyGlobal(AgpdCharacter *pAgpdCharacter)
{
	if (NULL == pAgpdCharacter)
		return FALSE;

	AgsdCharacter *pAgsdCharacter = m_pAgsmCharacter->GetADCharacter(pAgpdCharacter);
	if (NULL == pAgsdCharacter)
		return FALSE;

	AgsdBillingMoneyGlobal *pAgsdBilling = new AgsdBillingMoneyGlobal;
	if(!pAgsdBilling)
		return FALSE;

	// add to map
	pAgsdBilling->m_lID = m_GenerateID.GetID();
	pAgsdBilling->m_pAgpdCharacter = pAgpdCharacter;

	if (FALSE == Add(pAgsdBilling, pAgsdCharacter->m_szAccountID))
	{
		m_GenerateID.AddRemoveID(pAgsdBilling->m_lID);
		Remove(pAgsdCharacter->m_szAccountID);
		pAgsdBilling->Release();

		return FALSE;
	}

	m_pagpmBillInfo->GiveGlobalCash(pAgpdCharacter, 999999, 999999);

#endif

	return true;
}