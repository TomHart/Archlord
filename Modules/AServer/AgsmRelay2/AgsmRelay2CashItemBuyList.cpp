/*===============================================================

	AgsmRelay2CashItemBuyList.cpp

===============================================================*/


#include "AgsmRelay2.h"
#include "ApMemoryTracker.h"


//
//	==========		UI Status		==========
//
void AgsmRelay2::InitPacketCashItemBuyList()
	{
	m_csPacketCashItemBuyList.SetFlagLength(sizeof(INT16));
	m_csPacketCashItemBuyList.SetFieldType(AUTYPE_INT16,			1,		// eAgsmRelay2Operation
										   AUTYPE_UINT64,			1,		// buy id
										   AUTYPE_CHAR,				AGSMACCOUNT_MAX_ACCOUNT_NAME + 1,
										   AUTYPE_CHAR,				_MAX_CHARNAME_LENGTH + 1,
										   AUTYPE_INT32,			1,		// char tid
										   AUTYPE_INT32,			1,		// char level
										   AUTYPE_UINT64,			1,		// item seq.
										   AUTYPE_INT32,			1,		// item tid
										   AUTYPE_INT32,			1,		// item qty.
										   AUTYPE_INT64,			1,		// money
										   AUTYPE_UINT64,			1,		// order no
										   AUTYPE_INT8,				1,		// status
										   AUTYPE_CHAR,				15,		// ip address
										   AUTYPE_CHAR,				32 + 1,	// order id 4 chinese.
										   AUTYPE_END,				0
										   );

	}

void AgsmRelay2::InitPacketRequestCash()
	{
		m_csPacketRequestCash.SetFlagLength(sizeof(INT16));
		m_csPacketRequestCash.SetFieldType(AUTYPE_CHAR,	AGPACHARACTER_MAX_ID_STRING + 1,	// Game ID
										   AUTYPE_INT32,			1,		// CID
										   AUTYPE_END,				0
										   );

	}

	/*AgsdQueryWithParam* pQuery = new AgsdQueryWithParam;
	pQuery->m_nIndex = 522;
	pQuery->m_pParam = pAgsdRelay2;
	pQuery->SetCallback(AgsmRelay2::CBFinishOperationRefreshCash,
						AgsmRelay2::CBFailOperation,
						this, pAgsdRelay2);

	return m_pAgsmDatabasePool->Execute(pQuery);*/




	/*CashInfoGlobal pCash;
	m_pAgpm
	pThis->m_pcsAgpmBillInfo->GetCashGlobal(pcsCharacter, pCash.m_WCoin, pCash.m_PCoin);
	printf("Increasing w from %f to %f\n", pCash.m_WCoin, pCash.m_WCoin + 5000.0);
	printf("Increasing p from %f to %f\n", pCash.m_WCoin, pCash.m_PCoin + 5000.0);
	pThis->m_pcsAgpmBillInfo->SetCashGlobal(pcsCharacter, pCash.m_WCoin + 5000.0, pCash.m_PCoin + 5000.0);

	pThis->m_pcsAgpmBillInfo->GetCashGlobal(pcsCharacter, pCash.m_WCoin, pCash.m_PCoin);
	printf("Now %f and %f\n", pCash.m_WCoin, pCash.m_PCoin);

	PACKET_BILLINGINFO_CASHINFO pPacket(pcsCharacter->m_lID, pCash.m_WCoin, pCash.m_PCoin);
	pThis->SendPacketUser(pPacket, pThis->m_pcsAgsmCharacter->GetCharDPNID(pcsCharacter));*/

BOOL AgsmRelay2::OnParamRequestCash(INT16 nParam, PVOID pvPacket, UINT32 ulNID, PACKET_HEADER* pvOuterPacket)
{

	PACKET_AGSP_REFRESH_CASH_RELAY *pPacketRaw = (PACKET_AGSP_REFRESH_CASH_RELAY*) pvOuterPacket;

	if (NULL != pPacketRaw && pPacketRaw->nOperation == AGPMCASH_PACKET_OPERATION_REFRESH_CASH) {
		// Don't need the CID?
		printf("Send coins (%d) back to user '%s' (%d)\n", pPacketRaw->nCoins, pPacketRaw->strCharName, pPacketRaw->nCID);
		AgpdCharacter *pcsCharacter = m_pAgpmCharacter->GetCharacter(pPacketRaw->strCharName);
		m_pAgpmBillInfo->SetCashGlobal(pcsCharacter, pPacketRaw->nCoins, pPacketRaw->nCoins);
		PACKET_BILLINGINFO_CASHINFO pPacket(pcsCharacter->m_lID, pPacketRaw->nCoins, pPacketRaw->nCoins);
		SendPacketUser(pPacket, m_pAgsmCharacter->GetCharDPNID(pcsCharacter));
		return TRUE;
	}

	AgsdRelay2RequestCash *pAgsdRelay2 = new AgsdRelay2RequestCash;

	CHAR *pszAccountID = NULL;
	INT32 pszCID = 0;

	m_csPacketRequestCash.GetField(TRUE, pvPacket, 256, &pszAccountID, &pszCID);

	printf("%s: %s %d\n", __FUNCTION__, pszAccountID, pszCID);

	pAgsdRelay2->m_ulNID = ulNID;
	_tcscpy(pAgsdRelay2->m_szAccountID, NULL != pszAccountID ? pszAccountID : _T(""));
	pAgsdRelay2->m_lCID = pszCID;

	return EnumCallback(AGSMRELAY_PARAM_REQUEST_CASH, (PVOID)pAgsdRelay2, (PVOID)nParam);
	}

BOOL AgsmRelay2::CBOperationRefreshCash(PVOID pData, PVOID pClass, PVOID pCustData)
	{
	AgsmRelay2 *pThis = (AgsmRelay2 *) pClass;
	AgsdRelay2RequestCash *pAgsdRelay2 = (AgsdRelay2RequestCash *) pData;
	// INT16 nParam = (INT16) pCustData;

	// pAgsdRelay2->m_nParam = nParam;

	AgsdQueryWithParam* pQuery = new AgsdQueryWithParam;
	pAgsdRelay2->m_eOperation = AGSMDATABASE_OPERATION_SELECT;
	pQuery->m_nIndex = 1027;
	pQuery->m_pParam = pAgsdRelay2;
	pQuery->SetCallback(AgsmRelay2::CBFinishOperationRefreshCash,
						AgsmRelay2::CBFailOperation,
						pThis, pAgsdRelay2);

	return pThis->m_pAgsmDatabasePool->Execute(pQuery);
	}


BOOL AgsmRelay2::CBFinishOperationRefreshCash(PVOID pData, PVOID pClass, PVOID pCustData)
	{
	if (!pClass || !pCustData)
		return FALSE;

	AgsmRelay2 *pThis = (AgsmRelay2 *) pClass;
	AgsdDBParam *pAgsdRelay2 = NULL;
	pAgsdRelay2 = (AgsdDBParam *) pCustData;
	AuRowset *pRowset = (AuRowset *) pData;

	if (NULL != pRowset && AGSMDATABASE_OPERATION_SELECT == pAgsdRelay2->m_eOperation)
		pThis->OnSelectResultRefreshCash(pRowset, pAgsdRelay2);

	pAgsdRelay2->Release();

	return TRUE;
	}

BOOL AgsmRelay2::OnSelectResultRefreshCash(AuRowset *pRowset, AgsdDBParam *pAgsdRelay2)
	{
	AgsdRelay2RequestCash *pAgsdRelay2Mail = static_cast<AgsdRelay2RequestCash *>(pAgsdRelay2);

	AgsdServer *pGameServer = m_pAgsmServerManager->GetGameServerBySocketIndex(pAgsdRelay2Mail->m_ulNID);
	if (!pGameServer)
		return FALSE;

	INT32 lTotalSize = pRowset->GetRowBufferSize() * pRowset->GetRowCount();
	INT32 lTotalStep = 1 + (INT32) (lTotalSize / 20000);
	INT32 lRowsPerStep = 20000 / pRowset->GetRowBufferSize();
	for (INT32 lStep2 = 0; lStep2 < lTotalStep; lStep2++)
		{

		UINT32 ulCol = 0;
		
		CHAR *psz = NULL;

		//if (NULL != (psz = pRowset->Get(ul, ulCol++)))	// item tid
		//	lItemTID = _ttoi(psz);

		CHAR *charName = NULL;
		INT32 coins = 0;
		INT32 CID = 0;

		
		if (NULL != (psz = (CHAR *) pRowset->Get(lStep2, ulCol++))) {
			charName = psz;
		}

		if (NULL != (psz = (CHAR *) pRowset->Get(lStep2, ulCol++))) {
			coins = _ttoi(psz);
		}

		//if (NULL != (psz = (CHAR *) pRowset->Get(lStep2, ulCol++))) {
		//	CID = _ttoi(psz);
		//}

		printf("Got '%s' and '%d'. CID %d\n", charName, coins, pAgsdRelay2Mail->m_lCID);

		PACKET_AGSP_REFRESH_CASH_RESULT_RELAY pPacket(charName, coins, pAgsdRelay2Mail->m_lCID);

		AgsEngine::GetInstance()->SendPacket(pPacket, pAgsdRelay2Mail->m_ulNID);
		break;
	}

	return TRUE;
	}


//PACKET_AGSMCHARACTER_RELAY_CHANGENAME_RESULT pPacket;
//pPacket.CID = pAgsdRelay2->CID;
//pPacket.NID = pAgsdRelay2->NID;
//pPacket.EID = pAgsdRelay2->EID;
//pPacket.STEP = pAgsdRelay2->STEP;
//pPacket.nResult = pAgsdRelay2->m_nCode;
//
//AgsEngine::GetInstance()->SendPacket(pPacket, pAgsdRelay2->m_ulNID);


BOOL AgsmRelay2::OnParamCashItemBuyList(INT16 nParam, PVOID pvPacket, UINT32 ulNID)
	{
	AgsdRelay2CashItemBuyList *pAgsdRelay2 = new AgsdRelay2CashItemBuyList;

	CHAR	*pszAccountID = NULL;
	CHAR	*pszCharID = NULL;
	CHAR	*pszIP = NULL;
	CHAR	*pszOrderID = NULL;

	m_csPacketCashItemBuyList.GetField(FALSE, pvPacket, 0,
								&pAgsdRelay2->m_eOperation,
								&pAgsdRelay2->m_stList.m_ullBuyID,
								&pszAccountID,
								&pszCharID,
								&pAgsdRelay2->m_lCharTID,
								&pAgsdRelay2->m_lCharLevel,
								&pAgsdRelay2->m_stList.m_ullItemSeq,
								&pAgsdRelay2->m_stList.m_lItemTID,
								&pAgsdRelay2->m_stList.m_lItemQty,
								&pAgsdRelay2->m_stList.m_llMoney,
								&pAgsdRelay2->m_stList.m_ullOrderNo,
								&pAgsdRelay2->m_stList.m_cStatus,
								&pszIP,
								&pszOrderID
								);

	pAgsdRelay2->m_ulNID = ulNID;
	
	_tcsncpy(pAgsdRelay2->m_szAccountID, pszAccountID ? pszAccountID : _T(""), AGSMACCOUNT_MAX_ACCOUNT_NAME);
	_tcsncpy(pAgsdRelay2->m_szCharID, pszCharID ? pszCharID : _T(""), _MAX_CHARNAME_LENGTH);
	_tcsncpy(pAgsdRelay2->m_szIP, pszIP ? pszIP : _T(""), 14);
	_tcsncpy(pAgsdRelay2->m_stList.m_szOrderID, pszOrderID ? pszOrderID : _T(""), 32);

	// dummy conversion
	_i64tot(pAgsdRelay2->m_stList.m_ullBuyID, pAgsdRelay2->m_szBuySeq, 10);
	_i64tot(pAgsdRelay2->m_stList.m_ullItemSeq, pAgsdRelay2->m_szItemSeq, 10);
	_i64tot(pAgsdRelay2->m_stList.m_llMoney, pAgsdRelay2->m_szMoney, 10);
	_i64tot(pAgsdRelay2->m_stList.m_ullOrderNo, pAgsdRelay2->m_szOrderNo, 10);
	_i64tot(pAgsdRelay2->m_stList.m_ullBuyID, pAgsdRelay2->m_szBuySeq, 10);

	switch (pAgsdRelay2->m_stList.m_cStatus)
		{
		case AGSDITEM_CASHITEMBUYLIST_STATUS_REQUEST :
			_tcscpy(pAgsdRelay2->m_szStatus, _T("BUYREQ"));
			break;
		case AGSDITEM_CASHITEMBUYLIST_STATUS_COMPLETE :
			_tcscpy(pAgsdRelay2->m_szStatus, _T("BUYCOM"));
			break;		
		case AGSDITEM_CASHITEMBUYLIST_STATUS_GMGIVE :
			_tcscpy(pAgsdRelay2->m_szStatus, _T("GMGIVE"));
			break;
		/*
		case AGSDITEM_CASHITEMBUYLIST_STATUS_EXHAUST :		SPDELETECASHITEM에서 처리한다.
			_tcscpy(pAgsdRelay2->m_szStatus, _T("EXHAUST"));
			break;
		case AGSDITEM_CASHITEMBUYLIST_STATUS_DRAWBACK :
			_tcscpy(pAgsdRelay2->m_szStatus, _T("DRAWBACK"));
			break;				
		*/
		}

	if (m_pAgsmDatabasePool && 0 != pAgsdRelay2->m_stList.m_ullBuyID)
		pAgsdRelay2->m_nForcedPoolIndex = (INT16)(pAgsdRelay2->m_stList.m_ullBuyID % m_pAgsmDatabasePool->m_nTotalForced) + 
										   m_pAgsmDatabasePool->m_nForcedStart;
		
	return EnumCallback(nParam, (PVOID)pAgsdRelay2, (PVOID)nParam);
	}


BOOL AgsmRelay2::CBCashItemBuyListUpdate(PVOID pData, PVOID pClass, PVOID pCustData)
	{
	if (!pData || !pClass)
		return FALSE;

	AgsmRelay2			*pThis = (AgsmRelay2 *)	pClass;
	stCashItemBuyList	*pstList = (stCashItemBuyList *) pData;

	AgsdRelay2CashItemBuyList csRelay2List;
	csRelay2List.m_stList = *pstList;

	return pThis->SendCashItemBuyListUpdate(&csRelay2List);
	}


BOOL AgsmRelay2::CBCashItemBuyListUpdate2(PVOID pData, PVOID pClass, PVOID pCustData)
	{
	if (!pData || !pClass)
		return FALSE;

	AgsmRelay2			*pThis = (AgsmRelay2 *)	pClass;
	stCashItemBuyList	*pstList = (stCashItemBuyList *) pData;

	AgsdRelay2CashItemBuyList csRelay2List;
	csRelay2List.m_stList = *pstList;

	return pThis->SendCashItemBuyListUpdate2(&csRelay2List);
	}


BOOL AgsmRelay2::CBCashItemBuyListInsert(PVOID pData, PVOID pClass, PVOID pCustData)
	{
	if (!pData || !pClass)
		return FALSE;

	AgsmRelay2			*pThis = (AgsmRelay2 *)	pClass;
	stCashItemBuyList	*pstList = (stCashItemBuyList *) pData;

	AgsdRelay2CashItemBuyList csRelay2List;
	csRelay2List.m_stList = *pstList;
	_tcsncpy(csRelay2List.m_szCharID, pstList->m_szCharName, _MAX_CHARNAME_LENGTH);
	csRelay2List.m_lCharTID = pstList->m_lCharTID;
	csRelay2List.m_lCharLevel = pstList->m_lCharLevel;
	
	_tcsncpy(csRelay2List.m_szAccountID, pstList->m_szAccName, AGSMACCOUNT_MAX_ACCOUNT_NAME);
	_tcsncpy(csRelay2List.m_szIP, pstList->m_szIP, 14);

	return pThis->SendCashItemBuyListInsert(&csRelay2List);
	}


BOOL AgsmRelay2::SendCashItemBuyListInsert(AgsdRelay2CashItemBuyList *pAgsdRelay2, UINT32 ulNID)
	{
	pAgsdRelay2->m_eOperation = AGSMDATABASE_OPERATION_INSERT;
	INT16 nPacketLength	= 0;
	PVOID pvPacketEmb= m_csPacketCashItemBuyList.MakePacket(FALSE, &nPacketLength, 0,
										&pAgsdRelay2->m_eOperation,
										&pAgsdRelay2->m_stList.m_ullBuyID,	// buy id
										pAgsdRelay2->m_szAccountID,			// account id
										pAgsdRelay2->m_szCharID,			// char id
										&pAgsdRelay2->m_lCharTID,			// char tid
										&pAgsdRelay2->m_lCharLevel,			// char level
										&pAgsdRelay2->m_stList.m_ullItemSeq,// item seq
										&pAgsdRelay2->m_stList.m_lItemTID,	// item tid
										&pAgsdRelay2->m_stList.m_lItemQty,	// item qty.
										&pAgsdRelay2->m_stList.m_llMoney,	// money
										&pAgsdRelay2->m_stList.m_ullOrderNo,// order no
										&pAgsdRelay2->m_stList.m_cStatus,	// status
										pAgsdRelay2->m_szIP,
										pAgsdRelay2->m_stList.m_szOrderID
										);
	if (!pvPacketEmb)
		return FALSE;

	BOOL bResult = FALSE;

	if (0 == ulNID)
		bResult = MakeAndSendRelayPacket(pvPacketEmb, AGSMRELAY_PARAM_CASHITEMBUYLIST);
	else
		bResult = MakeAndSendRelayPacket(pvPacketEmb, AGSMRELAY_PARAM_CASHITEMBUYLIST, ulNID);

	m_csPacketCashItemBuyList.FreePacket(pvPacketEmb);
	
	return bResult;	
	}


BOOL AgsmRelay2::SendCashItemBuyListUpdate(AgsdRelay2CashItemBuyList *pAgsdRelay2, UINT32 ulNID)
	{
	pAgsdRelay2->m_eOperation = AGSMDATABASE_OPERATION_UPDATE;
	INT16 nPacketLength	= 0;
	PVOID pvPacketEmb= m_csPacketCashItemBuyList.MakePacket(FALSE, &nPacketLength, 0,
										&pAgsdRelay2->m_eOperation,
										&pAgsdRelay2->m_stList.m_ullBuyID,	// buy id
										NULL,								// account id
										NULL,								// char id
										NULL,								// char tid
										NULL,								// char level
										&pAgsdRelay2->m_stList.m_ullItemSeq,// item seq
										NULL,								// item tid
										NULL,								// item qty.
										NULL,								// money
										&pAgsdRelay2->m_stList.m_ullOrderNo,// order no
										&pAgsdRelay2->m_stList.m_cStatus,	// status
										NULL,
										pAgsdRelay2->m_stList.m_szOrderID
										);
	if (!pvPacketEmb)
		return FALSE;

	BOOL bResult = FALSE;

	if (0 == ulNID)
		bResult = MakeAndSendRelayPacket(pvPacketEmb, AGSMRELAY_PARAM_CASHITEMBUYLIST);
	else
		bResult = MakeAndSendRelayPacket(pvPacketEmb, AGSMRELAY_PARAM_CASHITEMBUYLIST, ulNID);

	m_csPacketCashItemBuyList.FreePacket(pvPacketEmb);
	
	return bResult;		
	}


BOOL AgsmRelay2::SendCashItemBuyListUpdate2(AgsdRelay2CashItemBuyList *pAgsdRelay2, UINT32 ulNID)
	{
	pAgsdRelay2->m_eOperation = AGSMDATABASE_OPERATION_UPDATE;
	INT16 nPacketLength	= 0;
	PVOID pvPacketEmb= m_csPacketCashItemBuyList.MakePacket(FALSE, &nPacketLength, 0,
										&pAgsdRelay2->m_eOperation,
										NULL,								// buy id
										NULL,								// account id
										NULL,								// char id
										NULL,								// char tid
										NULL,								// char level
										&pAgsdRelay2->m_stList.m_ullItemSeq,// item seq
										NULL,								// item tid
										NULL,								// item qty.
										NULL,								// money
										NULL,								// order no
										&pAgsdRelay2->m_stList.m_cStatus,	// status
										NULL,
										NULL
										);
	if (!pvPacketEmb)
		return FALSE;

	BOOL bResult = FALSE;

	if (0 == ulNID)
		bResult = MakeAndSendRelayPacket(pvPacketEmb, AGSMRELAY_PARAM_CASHITEMBUYLIST2);
	else
		bResult = MakeAndSendRelayPacket(pvPacketEmb, AGSMRELAY_PARAM_CASHITEMBUYLIST2, ulNID);

	m_csPacketCashItemBuyList.FreePacket(pvPacketEmb);
	
	return bResult;
	}




/********************************************************************/
/*		The Implementation of AgsdRelay2CashItemBuyList class		*/
/********************************************************************/
//
AgsdRelay2CashItemBuyList::AgsdRelay2CashItemBuyList()
	{
	ZeroMemory(&m_stList, sizeof(m_stList));
	ZeroMemory(m_szAccountID, sizeof(m_szAccountID));
	ZeroMemory(m_szCharID, sizeof(m_szCharID));
	m_lCharTID = 0;
	m_lCharLevel = 0;
	ZeroMemory(m_szIP, sizeof(m_szIP));
	
	// dummy
	ZeroMemory(m_szBuySeq, sizeof(m_szBuySeq));
	ZeroMemory(m_szItemSeq, sizeof(m_szItemSeq));
	ZeroMemory(m_szMoney, sizeof(m_szMoney));
	ZeroMemory(m_szOrderNo, sizeof(m_szOrderNo));
	ZeroMemory(m_szStatus, sizeof(m_szStatus));
	}


BOOL AgsdRelay2CashItemBuyList::SetParamUpdate(AuStatement* pStatement)
	{
	INT16 i=0;

	if (AGSMRELAY_PARAM_CASHITEMBUYLIST == m_nParam)
		{
		pStatement->SetParam(i++, m_szItemSeq, sizeof(m_szItemSeq));
		if (AP_SERVICE_AREA_CHINA == g_eServiceArea)
			pStatement->SetParam(i++, m_stList.m_szOrderID, sizeof(m_stList.m_szOrderID));
		else
			pStatement->SetParam(i++, m_szOrderNo, sizeof(m_szOrderNo));
		pStatement->SetParam(i++, m_szStatus, sizeof(m_szStatus));
		pStatement->SetParam(i++, m_szBuySeq, sizeof(m_szBuySeq));
		}
	else if (AGSMRELAY_PARAM_CASHITEMBUYLIST2 == m_nParam)
		{
		pStatement->SetParam(i++, m_szStatus, sizeof(m_szStatus));
		pStatement->SetParam(i++, m_szItemSeq, sizeof(m_szItemSeq));
		}
	
	return TRUE;
	}


BOOL AgsdRelay2CashItemBuyList::SetParamInsert(AuStatement *pStatement)
	{
	INT16 i=0;
	
	pStatement->SetParam(i++, m_szBuySeq, sizeof(m_szBuySeq));
	pStatement->SetParam(i++, m_szAccountID, sizeof(m_szAccountID));
	pStatement->SetParam(i++, m_szCharID, sizeof(m_szCharID));
	pStatement->SetParam(i++, &m_lCharTID);
	pStatement->SetParam(i++, &m_lCharLevel);
	pStatement->SetParam(i++, &m_stList.m_lItemTID);
	pStatement->SetParam(i++, &m_stList.m_lItemQty);
	pStatement->SetParam(i++, m_szMoney, sizeof(m_szMoney));
	pStatement->SetParam(i++, m_szIP, sizeof(m_szIP));
	pStatement->SetParam(i++, m_szStatus, sizeof(m_szStatus));
	
	return TRUE;	
	}


void AgsdRelay2CashItemBuyList::Dump(CHAR *pszOp)
	{
	}


/********************************************************************/
/*		The Implementation of AgsdRelay2RequestCash class		*/
/********************************************************************/
//
AgsdRelay2RequestCash::AgsdRelay2RequestCash()
	{
	ZeroMemory(m_szAccountID, sizeof(m_szAccountID));
	}


BOOL AgsdRelay2RequestCash::SetParamSelect(AuStatement* pStatement)
	{
	INT16 i=0;

	// pStatement->SetParam(i++, &m_lCID);
	pStatement->SetParam(i++, m_szAccountID, sizeof(m_szAccountID));
	
	return TRUE;
	}

BOOL AgsdRelay2RequestCash::SetParamUpdate(AuStatement* pStatement)
	{
	INT16 i=0;

	return TRUE;
	}


BOOL AgsdRelay2RequestCash::SetParamInsert(AuStatement *pStatement)
	{
	INT16 i=0;
	
	pStatement->SetParam(i++, m_szAccountID, sizeof(m_szAccountID));
	
	return TRUE;	
	}

BOOL AgsdRelay2RequestCash::SetParamExecute(AuStatement *pStatement)
	{
	INT16 i=0;
	
	pStatement->SetParam(i++, m_szAccountID, sizeof(m_szAccountID));
	
	return TRUE;
	}


void AgsdRelay2RequestCash::Dump(CHAR *pszOp)
	{
	}
