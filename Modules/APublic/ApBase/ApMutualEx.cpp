/******************************************************************************
Module:	Optex.cpp
Notices: Copyright (c) 2000 Jeffrey Richter
******************************************************************************/

#include "ApDefine.h"
#include "ApMutualEx.h"
#include "ApLockManager.h"
#include <assert.h>

#pragma message ( "Service Area Start" ) 
#ifdef _AREA_CHINA_
	#pragma message ( "China" ) 
	const ApServiceArea	g_eServiceArea	= AP_SERVICE_AREA_CHINA;
#endif

#ifdef _AREA_WESTERN_
	#pragma message ( "Western" ) 
	const ApServiceArea	g_eServiceArea	= AP_SERVICE_AREA_WESTERN;
#endif
		
#ifdef _AREA_JAPAN_
	#pragma message ( "Japan" ) 
	const ApServiceArea	g_eServiceArea	= AP_SERVICE_AREA_JAPAN;
#endif

#ifdef _AREA_KOREA_
	#pragma message ( "Korea" ) 
	const ApServiceArea	g_eServiceArea	= AP_SERVICE_AREA_KOREA;
#endif

#ifdef _AREA_GLOBAL_
	#pragma message ( "Global" ) 
	const ApServiceArea	g_eServiceArea	= AP_SERVICE_AREA_GLOBAL;
#endif

#ifdef _AREA_TAIWAN_
	#pragma message ( "Taiwan" ) 
	const ApServiceArea	g_eServiceArea	= AP_SERVICE_AREA_TAIWAN;
#endif

#pragma message ( "Service Area End" ) 

INT32 g_lLocalServer = 0;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

ApMutualEx::ApMutualEx()
{
	m_bInit	= TRUE;
	m_bRemoveLock	= FALSE;
	m_pvParent	= NULL;

	m_bNotUseLockManager	= FALSE;

	if(FALSE == InitializeCriticalSectionAndSpinCount(&m_csCriticalSection, 4000))
	{
		m_bInit = FALSE;
	}
}

///////////////////////////////////////////////////////////////////////////////
VOID ApMutualEx::Init(PVOID pvParent)
{
	if (m_bInit)
		return;

	m_bInit	= TRUE;
	m_pvParent	= pvParent;
	m_bRemoveLock = FALSE;

	m_bNotUseLockManager	= FALSE;

	if(FALSE == InitializeCriticalSectionAndSpinCount(&m_csCriticalSection, 4000))
	{
		m_bInit = FALSE;
	}
}

VOID ApMutualEx::Destroy()
{
	if (!m_bInit)
		return;

	// CriticalSection?? ???? EnterCriticalSection()?? ???? ???????? ?????? ?? ???? ?????? ???????? ?????? ???? ??????.
	// ???????????? ?????? ???? ???????? CriticalSection ???? ???????? ?????? ?????? ????.
	// ?????? ?????? ??????.

	// 1. Lock()???? m_bRemoveLock?? ????????. (m_bRemoveLock?? ???????? ?????? WLock()???? ?????? FALSE?? ????????.)
	// 2. UnLock() ????. (?????? ?? ?????? ?????? ???? ???????? ???????? ???? ?????? m_bRemoveLock?? ???? ???? FALSE?? ?????? ???? ?? ???? ?????? ???? ????????.)
	// 3. Lock() ????. (???? ???????? ?????? ???? FALSE?? ???????????? ???????? ????????.)
	// 4. ???? ????????. ??????.

	if (!m_bRemoveLock)
	{
		if (m_bNotUseLockManager)
			EnterCriticalSection(&m_csCriticalSection);
		else
			ApLockManager::Instance()->InsertLock( this );

		m_bRemoveLock = TRUE;
	}

	if (m_bNotUseLockManager)
		LeaveCriticalSection(&m_csCriticalSection);
	else
		ApLockManager::Instance()->RemoveUnlock( this );

	if (m_bNotUseLockManager)
		EnterCriticalSection(&m_csCriticalSection);
	else
		ApLockManager::Instance()->InsertLock( this );

	if (m_bNotUseLockManager)
		LeaveCriticalSection(&m_csCriticalSection);
	else
		ApLockManager::Instance()->RemoveUnlock( this );

	if (!m_bNotUseLockManager)
		ApLockManager::Instance()->SafeRemoveUnlock( this );

	DeleteCriticalSection(&m_csCriticalSection);

	m_bInit = FALSE;	
}


ApMutualEx::~ApMutualEx() 
{
	if(TRUE == m_bInit)
		Destroy();
}

BOOL ApMutualEx::RLock()
{
	return WLock();
}

BOOL ApMutualEx::WLock() {
	if (!m_bInit || m_bRemoveLock)
		return FALSE;

	if (m_bNotUseLockManager)
		EnterCriticalSection(&m_csCriticalSection);
	else
	{
		if(!ApLockManager::Instance()->InsertLock( this ))
			return FALSE;
	}

	if (m_bRemoveLock)
	{
		Release(TRUE);
		return FALSE;
	}

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////


BOOL ApMutualEx::RemoveLock()
{
	if (!WLock())
		return FALSE;
	
	m_bRemoveLock = TRUE;

	return TRUE;
}

VOID ApMutualEx::ResetRemoveLock()
{
	m_bRemoveLock = FALSE;
}

///////////////////////////////////////////////////////////////////////////////


BOOL ApMutualEx::Release(BOOL bForce) {
	if (!m_bInit || (m_bRemoveLock && !bForce)) return FALSE;

	if (m_bNotUseLockManager)
		LeaveCriticalSection(&m_csCriticalSection);
	else
		ApLockManager::Instance()->RemoveUnlock( this );

//	LeaveCriticalSection(&m_csCriticalSection);

// ?????? (04-05-23 ???? 6:09:51) : 98?????? ?? ?????? ???? ?? ??????..
// CRITICAL_SECTION ?? ??98?????? ulong ???? , ?????? ???? ?????? ??????????????~

//	if (m_csCriticalSection.RecursionCount < 0 && m_csCriticalSection.LockCount < 0)
//	{
//		char buffer[256] = {0,};
//		sprintf(buffer, "\nRecursionCount : %d, LockCount : %d", m_csCriticalSection.RecursionCount, m_csCriticalSection.LockCount);
//		OutputDebugString(buffer);
//		DebugBreak();
//	}

	return TRUE;
}

BOOL ApMutualEx::SafeRelease()
{
	if (!m_bInit)	return FALSE;

	if (!m_bNotUseLockManager)
		ApLockManager::Instance()->SafeRemoveUnlock( this );

	return TRUE;
}

BOOL ApMutualEx::SetNotUseLockManager()
{
	m_bNotUseLockManager	= TRUE;

	return TRUE;
}

//////////////////////////////// End of File //////////////////////////////////
