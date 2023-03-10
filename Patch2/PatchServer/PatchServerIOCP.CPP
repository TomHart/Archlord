#include "PatchServerIOCP.h"
#include "ApServerMonitorPacket.h"

CSocketIOContext::CSocketIOContext()
{
	init( 0, false, 0, false );
}

CSocketIOContext::CSocketIOContext(SOCKET hSocket, int iSendBufferSize, bool bSendEvent, int iRecvBufferSize, bool bRecvEvent )
{
	m_hSocket = hSocket;

	init( iSendBufferSize, bSendEvent, iRecvBufferSize, bRecvEvent );
}

CSocketIOContext::~CSocketIOContext()
{
	cleanupContext();
}

void CSocketIOContext::init( int iSendBufferSize, bool bSendEvent, int iRecvBufferSize, bool bRecvEvent )
{
	InitializeCriticalSection( &m_cSendCriticalSection );
	InitializeCriticalSection( &m_cRecvCriticalSection );

	m_pcPatchFileInfo = NULL;
	m_pcPatchFileInfoNode = NULL;
}

void CSocketIOContext::SendLock()
{
	EnterCriticalSection( &m_cSendCriticalSection );
}

void CSocketIOContext::SendUnlock()
{
	LeaveCriticalSection( &m_cSendCriticalSection );
}

void CSocketIOContext::RecvLock()
{
	EnterCriticalSection( &m_cRecvCriticalSection );
}

void CSocketIOContext::RecvUnlock()
{
	LeaveCriticalSection( &m_cRecvCriticalSection );
}

void CSocketIOContext::cleanupContext()
{
	SendLock();
	RecvLock();

	if( m_pcPatchFileInfo != NULL )
	{
		delete m_pcPatchFileInfo;
		m_pcPatchFileInfo = NULL;
	}

/*	if( m_pcPatchFileInfo != NULL )
	{
		m_pcPatchFileInfo->Destroy();
		m_pcPatchFileInfo = NULL;
	}*/

	m_pcPatchFileInfo = NULL;
	m_pcPatchFileInfoNode = NULL;

	if( m_hSocket != NULL )
	{
		closesocket( m_hSocket );
		m_hSocket = NULL;
	}

	SendUnlock();
	RecvUnlock();

	DeleteCriticalSection( &m_cSendCriticalSection );
	DeleteCriticalSection( &m_cRecvCriticalSection );
}


bool CIOCP::CreateIOCP( int iThreadNum )
{
	bool		bResult;
	HANDLE		hTempHandle;

	hTempHandle = CreateIoCompletionPort( INVALID_HANDLE_VALUE, NULL, 0, iThreadNum );

	if( hTempHandle != NULL )
	{
		m_hIOCP = hTempHandle;
		bResult = true;
	}
	else
	{
		bResult = false;
	}

	return bResult;
}

bool CIOCP::AssociateIOCP( HANDLE hAssociateHandle, void *pvKey )
{
	bool		bResult;
	HANDLE		hTempHandle;

	//printf( "(DWORD)pvKey : %d\n", (DWORD)pvKey );

	hTempHandle = CreateIoCompletionPort( hAssociateHandle, m_hIOCP, (DWORD)pvKey, 0 );

	if( hTempHandle != NULL )
	{
		m_hIOCP = hTempHandle;
		bResult = true;
	}
	else
	{
		bResult = false;
	}

	return bResult;
}

bool CIOCP::CleanupIOCP()
{
	CloseHandle( m_hIOCP );

	return true;
}

CLockMgr::CLockMgr()
{
	Init();
}

CLockMgr::~CLockMgr()
{
	CleanUp();
}

void CLockMgr::Init()
{
	m_csContextTree.InitializeObject( sizeof(int), 10000 );
	InitializeCriticalSection( &m_cCriticalSection );
}

void CLockMgr::CleanUp()
{
	DeleteCriticalSection( &m_cCriticalSection );
}

void CLockMgr::Lock()
{
	EnterCriticalSection( &m_cCriticalSection );
}

void CLockMgr::Unlock()
{
	LeaveCriticalSection( &m_cCriticalSection );
}

bool CLockMgr::AddObject( void *pvData, int iKey )
{
	bool			bResult;

	bResult = false;

	Lock();

	if(	m_csContextTree.AddObject( pvData, iKey ) )
	{
		bResult = true;
	}

	Unlock();

	return bResult;
}

bool CLockMgr::RemoveObject( int iKey )
{
	bool			bResult;

	bResult = false;

	Lock();

	if(	m_csContextTree.RemoveObject( iKey ) )
	{
		bResult = true;
	}

	Unlock();

	return bResult;
}

void *CLockMgr::GetObject( int iKey )
{
	void			*pvResult;

	pvResult = NULL;

	Lock();

	pvResult = m_csContextTree.GetObject( iKey );

	Unlock();

	return pvResult;
}

CIOCPNetworkModule::CIOCPNetworkModule()
{
	m_bIPBlockingCheck = false;
	m_iStatus = IOCP_OP_ACTIVE;
	m_iCurrentUsers = 0;

	m_hSendQueueEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
	m_hRecvQueueEvent = CreateEvent( NULL, TRUE, FALSE, NULL );

	//Patch?????? ??????.
	m_cAuPackingManager.LoadResFile( "Patch.res" );
	m_iVersion = m_cAuPackingManager.GetLastVersion();	

	m_ulClientFileCRC = _GetClientFileCRC( "root", "AlefClient.exe" );
}

CIOCPNetworkModule::~CIOCPNetworkModule()
{
	m_cSendQueue.removeAll();
	m_cRecvQueue.removeAll();

	CloseHandle( m_hSendQueueEvent );
	CloseHandle( m_hRecvQueueEvent );
}

char *CIOCPNetworkModule::GetDownloadServerIP()
{
	return m_strServerIP;	
}

unsigned short CIOCPNetworkModule::GetDownloadServerPort()
{
	return m_iServerPort;	
}

bool CIOCPNetworkModule::SetDownloadServer( char *pstrServerIP, unsigned short iServerPort )
{
	memset( m_strServerIP, 0, sizeof(m_strServerIP) );
	strcat( m_strServerIP, pstrServerIP );

	m_iServerPort = iServerPort;

	return true;
}

void CIOCPNetworkModule::SetIPBlocking( bool bUseIPBlocking )
{
	m_bIPBlockingCheck = bUseIPBlocking;
}

bool CIOCPNetworkModule::LoadPatchCode( char *pstrFileName )
{
	return m_cAuPatchCheckCode.LoadPatchCode( pstrFileName );
}

int CIOCPNetworkModule::GetPatchFiles( int iVersion, unsigned int iPatchCheckCode, CDList<CJZPFileInfo *> *pcList )
{
	int				iResult;

	//???? ?????? ?????????? ????????.
	if( m_cAuPatchCheckCode.GetCode( iVersion ) == iPatchCheckCode )
	{
		//iVersion???? ???????? ?????? ??????. ???? ???????????? ?????? ?????? ???????? ???????? 1?? ??????????.
		iVersion++;

		if( iVersion > m_iVersion )
		{
			iResult = g_dErrorNoPatchFile;
		}
		else
		{
			//??????, ?????? ?????? ?????? ??????.
			iResult = 0;

			for( int i=iVersion; i<=m_iVersion; i++ )
			{
				CJZPFileInfo		*pcJZPFileInfo;

				if( (pcJZPFileInfo = m_cAuPackingManager.GetJZPInfo(i) ) != NULL )
				{
					int				iVersion;
					int				iFileNameSize;
					char			*pstrFileName;
					int				iFileSize;
					int				iRawFileSize;
					unsigned long	iCRCCheckSum;

					//????????.
					iVersion = pcJZPFileInfo->m_iVersion;
					//File???????? ????.
					iFileNameSize = pcJZPFileInfo->m_iFileNameSize;
					//????????????
					pstrFileName = new char[iFileNameSize];
					memcpy( pstrFileName, pcJZPFileInfo->m_pstrFileName, iFileNameSize );
					//FileSize????
					iFileSize = pcJZPFileInfo->m_iFileSize;
					//iRawFileSize????
					iRawFileSize = pcJZPFileInfo->m_iRawFileSize;
					//CRC CheckSum????
					iCRCCheckSum = pcJZPFileInfo->m_iCRCCheckSum;

					pcJZPFileInfo = new CJZPFileInfo( iVersion, iFileNameSize, pstrFileName, iFileSize, iRawFileSize, iCRCCheckSum );

					pcList->Add( pcJZPFileInfo );
				}
			}
		}
	}
	else
	{
		iResult = g_dErrorInvalidPatchCode;
	}

	return iResult;
}

int	CIOCPNetworkModule::postAccept()
{
	unsigned long	iReadBytes;
	unsigned int	iError;

	iReadBytes = 0;
	iError = 0;

	//Overlap???????? ??????.
	m_AcceptOverlappedEx.recycleOverlapped();

	//?????? ??????.
	m_AcceptOverlappedEx.m_hSocket = WSASocket(AF_INET, SOCK_STREAM,0, NULL, 0, WSA_FLAG_OVERLAPPED );

	//AcceptEx?? ????????.
	if(!AcceptEx( m_cWinSockLib.m_hSocket,
				m_AcceptOverlappedEx.m_hSocket,
				m_AcceptOverlappedEx.m_pstrAddrInfo,
				0, // read nothing from the socket
				0,
				sizeof(struct sockaddr_in) +16,
				&iReadBytes,
				&m_AcceptOverlappedEx.m_cOverlapped ))
	{
		iError=WSAGetLastError();

		if(iError!=ERROR_IO_PENDING){
			fprintf(stderr,"acceptex failed %d\n",iError);
			return -1;
		}
	}

	return true;
}

int CIOCPNetworkModule::postRecv( CSocketIOContext *pContext, COverlappedRecv *pcOverlappedRecv )
{
	int				iResult;
	unsigned long	lNumOfRecvBytes;
	unsigned long	lFlag = 0;

	//printf( "PostRecv Overlapped : %p\n", &pContext->m_cRecvOverlapped.m_cOverlapped );

	//?????? ???????? Thread???? ???? pContext?? ?????? ??????? Race Condition????.
	//?????? ??????!!
	EnterCriticalSection( &pContext->m_cRecvCriticalSection );

	iResult = WSARecv( pContext->m_hSocket, &pcOverlappedRecv->m_cWSABuf, 1, &lNumOfRecvBytes, &lFlag, &pcOverlappedRecv->m_cOverlapped, NULL );

	LeaveCriticalSection( &pContext->m_cRecvCriticalSection );

	if( iResult == SOCKET_ERROR)
    {
		int			iError;

		iError = WSAGetLastError();

        if( iError != WSA_IO_PENDING)
        {
            return -1;
        }
    }

	return 1;
}

int CIOCPNetworkModule::sendPacket( CSocketIOContext *pContext, char *pstrPacket, int iPacketSize )
{
	int				iResult;
	unsigned long	lNumOfSendBytes;
	unsigned long	lFlag = 0;

	COverlappedSend		*pcOverlappedSend;
	pcOverlappedSend = new COverlappedSend( true, 0 );

	//?????? ???????? Thread???? ???? pContext?? ?????? ??????? Race Condition????.
	//?????? ??????!!
	EnterCriticalSection( &pContext->m_cSendCriticalSection );

	pcOverlappedSend->m_cWSABuf.buf = pstrPacket;
	pcOverlappedSend->m_cWSABuf.len = iPacketSize;

	iResult = WSASend( pContext->m_hSocket, &pcOverlappedSend->m_cWSABuf, 1, &lNumOfSendBytes, lFlag, &pcOverlappedSend->m_cOverlapped, NULL );

	LeaveCriticalSection( &pContext->m_cSendCriticalSection );

	if( iResult == SOCKET_ERROR)
    {
		int iError;

		iError = WSAGetLastError();

        if( iError != WSA_IO_PENDING)
        {
            return -1;
        }
		else
		{
			printf( "iError:%d\n", iError );
		}
    }

	return 1;
}

int CIOCPNetworkModule::sendPatchFileInfo( CSocketIOContext *pContext )
{
	pContext->SendLock();

	//?????? ???? ?????? ?????? ??????.
	if( pContext->m_pcPatchFileInfoNode != NULL )
	{
		char			*pstrPacket;

		int				iPacketSize;
		int				iCurrentPos;
		int				iFileNameSize;
		int				iFileSize;
		int				iResult;

		unsigned long	lNumOfSendBytes;
		unsigned long	lFlag = 0;

		COverlappedSendPatchFile		*pcOverlappedSendPatchFile;
		pcOverlappedSendPatchFile = new COverlappedSendPatchFile( true, 0 );

		//?????????? ??????????.
		CPacketTypeSC_0x02	cPacketTypeSC_0x02;

		iPacketSize = 0;
		iCurrentPos = 0;

		//File?????????? ????????.
		iFileNameSize = pContext->m_pcPatchFileInfoNode->m_tData->m_iFileNameSize;
		//FileSize?? ????????.
		iFileSize = pContext->m_pcPatchFileInfoNode->m_tData->m_iFileSize;

		//???? ????.
		iPacketSize += sizeof( cPacketTypeSC_0x02.m_iSize );
		iPacketSize += sizeof( cPacketTypeSC_0x02.m_iCommand );
		//???????? ????.
		iPacketSize += sizeof(iFileNameSize);
		//????????
		iPacketSize += iFileNameSize;
		//?????????? ?????? ????.
		iPacketSize += sizeof( iFileSize );
		//CRCCheckSum?? ????.
		iPacketSize += sizeof( pContext->m_pcPatchFileInfoNode->m_tData->m_iCRCCheckSum );

		//???? ???????? ??????.
		cPacketTypeSC_0x02.m_iSize = iPacketSize;

		//?????? ?????? ?????? ??????.
		pstrPacket = new char[iPacketSize];

		//????????????.
		memcpy( &pstrPacket[iCurrentPos], &cPacketTypeSC_0x02.m_iSize, sizeof(cPacketTypeSC_0x02.m_iSize) );
		iCurrentPos+=sizeof(cPacketTypeSC_0x02.m_iSize);
		memcpy( &pstrPacket[iCurrentPos], &cPacketTypeSC_0x02.m_iCommand, sizeof(cPacketTypeSC_0x02.m_iCommand) );
		iCurrentPos+=sizeof(cPacketTypeSC_0x02.m_iCommand);
		//????????????????.
		memcpy( &pstrPacket[iCurrentPos], &iFileNameSize, sizeof(iFileNameSize) );
		iCurrentPos+=sizeof(iFileNameSize);
		//????????????.
		memcpy( &pstrPacket[iCurrentPos], pContext->m_pcPatchFileInfoNode->m_tData->m_pstrFileName, iFileNameSize );
		iCurrentPos+=iFileNameSize;
		//????????????.
		memcpy( &pstrPacket[iCurrentPos], &iFileSize, sizeof(iFileSize) );
		iCurrentPos+=sizeof(iFileSize);
		//CRC CheckSum????
		memcpy( &pstrPacket[iCurrentPos], &pContext->m_pcPatchFileInfoNode->m_tData->m_iCRCCheckSum, sizeof(pContext->m_pcPatchFileInfoNode->m_tData->m_iCRCCheckSum) );
		iCurrentPos+=sizeof(pContext->m_pcPatchFileInfoNode->m_tData->m_iCRCCheckSum);

		//?????? ???????? Thread???? ???? pContext?? ?????? ??????? Race Condition????.
		//?????? ??????!!
		EnterCriticalSection( &pContext->m_cSendCriticalSection );

		pcOverlappedSendPatchFile->m_cWSABuf.buf = pstrPacket;
		pcOverlappedSendPatchFile->m_cWSABuf.len = iPacketSize;

		iResult = WSASend( pContext->m_hSocket, &pcOverlappedSendPatchFile->m_cWSABuf, 1, &lNumOfSendBytes, lFlag, &pcOverlappedSendPatchFile->m_cOverlapped, NULL );

		LeaveCriticalSection( &pContext->m_cSendCriticalSection );

		if( iResult == SOCKET_ERROR)
		{
			int iError;

			iError = WSAGetLastError();

			if( iError != WSA_IO_PENDING)
			{
				iResult = -1;
			}
			else
			{
				iResult = 1;
			}
		}
		else
		{
			iResult = 1;
		}

		//???????? ??????.
		pContext->m_pcPatchFileInfoNode = pContext->m_pcPatchFileInfoNode->m_pcNextNode;
	}

	pContext->SendUnlock();

	return 0;
}

int CIOCPNetworkModule::cleanup()
{
	BOOL			bResult;
	unsigned int	iReturn;
		
	bResult = PostQueuedCompletionStatus(m_hIOCP, 0, IOCP_OP_DESTROY, NULL);

	if( bResult == FALSE )
	{
		iReturn = GetLastError();
	}
	else
	{
		iReturn = 0;
	}

	return iReturn;
}

int CIOCPNetworkModule::parsePacket( CSocketIOContext *pcContext, COverlappedRecv *pcOverlappedRecv, int iTransferedBytes )
{
	int				iPacketSize;
	int				iRemainPacketLength;
	int				iPacketHeaderSize;
	int				iOffset;

	iOffset = 0;
	iPacketHeaderSize = 5;
	iRemainPacketLength = pcOverlappedRecv->m_iRemainPacketLength;

	while( 1 )
	{
		if( (iRemainPacketLength + iTransferedBytes) >= iPacketHeaderSize )
		{
			//?????? ????.
		}
		else
		{
			if( iTransferedBytes == 0 )
			{
				//?????? ?????? ???????? ??????.
				pcOverlappedRecv->m_iRemainPacketLength = 0;
			}
			else
			{
				//?????? ????????. ?????? ?????? ???????? ????????????~ ????~
			}

			break;
		}

		//???????? ?????? ?????????????????? ?????? ?? ?? ?????? ??????. ????????~~
		//?????? ?????? ??????????~
		if( iRemainPacketLength == 0 )
		{
			memcpy( &iPacketSize, &pcOverlappedRecv->m_cWSABuf.buf[iOffset], sizeof( iPacketSize) );
		}
		else
		{
			memcpy( &iPacketSize, pcOverlappedRecv->m_pstrBuffer, sizeof( iPacketSize) );
		}

		//iPacketSize?? ???????? ?????? ????????.

		//???? ?????? ???? ?????? ?????? ?? ???????
		if( (iRemainPacketLength + iTransferedBytes) >= iPacketSize )
		{
			char			*pstrPacket;
			int				iCopySize;

			iCopySize = iPacketSize-iRemainPacketLength;

			//?????? Queue?? ??????.
			pstrPacket = new char[iPacketSize];

			//Packet?? ???? ?????? ??????????.
			if( iRemainPacketLength == 0 )
			{
				//printf( "iPacketSize:%d", iPacketSize );

				//???? ???????? ???? ?????? ??????.
				memcpy( pstrPacket, &pcOverlappedRecv->m_cWSABuf.buf[iOffset], iPacketSize );
			}
			else
			{
				//printf( "iPacketSize:%d", iPacketSize );

				//?????? ???????????? ?????? m_strBuffer???? ???????? ???????? m_cWSABuf.buf???? ????????.
				memcpy( pstrPacket, pcOverlappedRecv->m_pstrBuffer, iRemainPacketLength );
				memcpy( &pstrPacket[iRemainPacketLength], &pcOverlappedRecv->m_cWSABuf.buf[iOffset], iCopySize );

				delete [] pcOverlappedRecv->m_pstrBuffer;
				pcOverlappedRecv->m_pstrBuffer = NULL;
				iRemainPacketLength = 0;				
			}

			//Packet?? ???????? RecvQueue?? ??????.
			CQueueUnit	*pcQueueUnit;
			pcQueueUnit = new CQueueUnit;
			pcQueueUnit->m_pcContext = pcContext;
			pcQueueUnit->m_pstrPacket = pstrPacket;
			pcQueueUnit->m_hSocket = pcContext->m_hSocket;
			pcQueueUnit->m_iPacketSize = iPacketSize;

			m_cRecvQueue.push( pcQueueUnit );

			if( SetEvent( m_hRecvQueueEvent ) == FALSE )
			{
				//puts( "[Debug] Set Event Failed!!\n" );
			}
			else
			{
				//puts( "[Debug] Signal for m_hRecvQueueEvent.\n" );
			}

			iRemainPacketLength = 0;
			iOffset += iCopySize;
			iTransferedBytes -= iCopySize;
		}
		//?????? ???? ?????? ?????????? ???? ?????? ???????
		else
		{
			//???? ???????? ????????????. ???? ?????? ?????? ????????~ ????????~
			if( pcOverlappedRecv->m_pstrBuffer == NULL )
				pcOverlappedRecv->m_pstrBuffer = new char[iPacketSize];

			memcpy( &pcOverlappedRecv->m_pstrBuffer[iRemainPacketLength], &pcOverlappedRecv->m_cWSABuf.buf[iOffset], iTransferedBytes );
			pcOverlappedRecv->m_iRemainPacketLength = iRemainPacketLength + iTransferedBytes;

			break;
		}
	}

	return 1;
}

bool CIOCPNetworkModule::registerThread( unsigned int ( CIOCPNetworkModule::*pf )( void * ) )
{
	m_cRegisteredThreadLlist.push( pf );
	return true;
}

void CIOCPNetworkModule::beginRegisteredThread()
{
	int			iRegistedThreads;

	iRegistedThreads = m_cRegisteredThreadLlist.getCount();

	for( int i=0; i<iRegistedThreads; i++ )
	{
		unsigned int	iThreadID;

		_beginthreadex(NULL,
			0,
			(PBEGINTHREADEX_THREADFUNC)CIOCPNetworkModule::executeRegistedThread,
			(LPVOID)this,
			0,
			(PBEGINTHREADEX_THREADID)&iThreadID );
	}

	return;
}

DWORD CIOCPNetworkModule::executeRegistedThread( LPVOID param )
{
	CIOCPNetworkModule	*pcIOCP;
	pcIOCP = (CIOCPNetworkModule *)param;

	return (pcIOCP->*(pcIOCP->m_cRegisteredThreadLlist.pop()))(NULL);
}

unsigned int CIOCPNetworkModule::IOCPGQCSThread( void *pArg )
{
	printf( "Begin IOCPGQCSThread.\n" );

	CSocketIOContext	*pcContext = NULL;
	CAcceptOverlappedEx *pcAcceptOverlappedEx = NULL;
	COverlappedEx		*pcOverlappedEx = NULL;
	COverlappedRecv		*pcOverlappedRecv = NULL;
	COverlappedSend		*pcOverlappedSend = NULL;
	OVERLAPPED			*pcOverlapped =NULL;

	BOOL			bResult;
	unsigned long	iTransferedBytes;

	while( 1 )
	{
		bResult = GetQueuedCompletionStatus( m_hIOCP, &iTransferedBytes, (DWORD *)&pcContext, &pcOverlapped, INFINITE );

		if( bResult == FALSE )
		{
			if( pcOverlapped == NULL )
			{
				//printf( "Fatal Error. Illegal call to GQCS.\n" );
			}
			else//if( lpOverlapped != NULL )
			{
				//2003.12.2 Accept?????? ????????. ???? ????????.
//				if( (unsigned int)pcContext == IOCP_ACCEPTEX )
//				{
//					;
//				}
//				else
//				{
				if( (unsigned int)pcContext == IOCP_ACCEPTEX )
				{
					printf("IOCP_ACCEPTEX ");
				}
					//Close Connection.
					printf( "Abnormal Closing!!\n" );
					m_iCurrentUsers--;
					printf( "Remove user : %d\n", m_iCurrentUsers );

					m_cLockMgr.Lock();
					m_cLockMgr.RemoveObject( (int) pcContext->m_hSocket );
					delete pcContext;
					m_cLockMgr.Unlock();

					pcOverlappedEx = (COverlappedEx *)pcOverlapped;

					if( pcOverlappedEx->m_iOperation == IOCP_OVERLAP_RECV )
					{
						pcOverlappedRecv = (COverlappedRecv *)pcOverlappedEx;
						delete pcOverlappedRecv;
					}
					else if( pcOverlappedEx->m_iOperation == IOCP_OVERLAP_SEND )
					{
						pcOverlappedSend = (COverlappedSend *)pcOverlappedEx;
						delete pcOverlappedSend;
					}
//				}
			}
		}
		else //if( bResult == TRUE )
		{
			if( iTransferedBytes == 0 )
			{
				if( (unsigned int)pcContext == IOCP_OP_DESTROY )
				{
					//IOCP ?????? Destroy?? ??????.
					m_iStatus = IOCP_OP_DESTROY;

					//Send,Recv Thread?? ???????????? Signal??????.
					SetEvent( m_hSendQueueEvent );
					SetEvent( m_hRecvQueueEvent );
					break;
				}
				else if( (unsigned int)pcContext == IOCP_ACCEPTEX )
				{
					SOCKET			hConnectedSocket;
					sockaddr_in		*lpRemoteAddr;
					int				nRemoteSockAddrLen;

					nRemoteSockAddrLen = 0;

					pcAcceptOverlappedEx = (CAcceptOverlappedEx *)pcOverlapped;

					GetAcceptExSockaddrs((LPWSABUF)pcAcceptOverlappedEx->m_pstrAddrInfo, 
										0, 
										0, 
										sizeof(SOCKADDR_IN) + 16, 
										NULL, 
										NULL, 
										(LPSOCKADDR*)&lpRemoteAddr, 
										&nRemoteSockAddrLen);

					printf( "Accept Client Connection.\n" );

					if( lpRemoteAddr != NULL )
					{
						BOOL			bBlockIP;
						unsigned long	ulIP;

						bBlockIP = false;
						ulIP = 0;

						printf( "Client IP : %s\n", inet_ntoa( lpRemoteAddr->sin_addr ) );

						ulIP = lpRemoteAddr->sin_addr.S_un.S_addr;

/*						if( m_bIPBlockingCheck )
						{
							bBlockIP = m_cIPBlockingManager.SearchIP( ulIP );
						}

						//???????? IP???? ????.
						if( bBlockIP )*/
						{
							hConnectedSocket = pcAcceptOverlappedEx->m_hSocket;

							CSocketIOContext	*pSocketIOContext;
							pSocketIOContext = new CSocketIOContext( hConnectedSocket, 0, false, 4096, true );

							//Associate with IOCP Port
							AssociateIOCP( (HANDLE)hConnectedSocket, (void *)pSocketIOContext );
							//?????? ???? ???????????? ???????? ?????? ????????.

							m_cLockMgr.AddObject( pSocketIOContext, (int) hConnectedSocket );

							//???? OverlappedRecv?? ??????????.
							COverlappedRecv		*pcOverlappedRecv;
							pcOverlappedRecv = new COverlappedRecv( true, 4096, 0 );

							postRecv( pSocketIOContext, pcOverlappedRecv );

							m_iCurrentUsers++;
							printf( "Add user : %d\n", m_iCurrentUsers );
						}
/*						else
						{
							printf( "????IP ????\n" );
							closesocket( pcAcceptOverlappedEx->m_hSocket );
						}*/
					}

					//?? Accept?? ????.
					postAccept();
				}
				else
				{
					//Close Connection.
					printf( "Close a connection......\n" );
					m_iCurrentUsers--;
					printf( "Remove user : %d\n", m_iCurrentUsers );

					m_cLockMgr.Lock();
					m_cLockMgr.RemoveObject( (int) pcContext->m_hSocket );
					delete pcContext;
					m_cLockMgr.Unlock();

					pcOverlappedEx = (COverlappedEx *)pcOverlapped;

					if( pcOverlappedEx->m_iOperation == IOCP_OVERLAP_RECV )
					{
						pcOverlappedRecv = (COverlappedRecv *)pcOverlappedEx;
						delete pcOverlappedRecv;
					}
					else if( pcOverlappedEx->m_iOperation == IOCP_OVERLAP_SEND )
					{
						pcOverlappedSend = (COverlappedSend *)pcOverlappedEx;
						delete pcOverlappedSend;
					}
				}
			}
			//???? ?? ?????? ??????. ??????????!!
			else
			{
				pcOverlappedEx = (COverlappedEx *)pcOverlapped;

				if( pcOverlappedEx->m_iOperation == IOCP_OVERLAP_RECV )
				{
					//printf( "(%d)Byte Recv succeeded!!\n", iTransferedBytes );
					pcOverlappedRecv = (COverlappedRecv *)pcOverlappedEx;

					//???? ?????? Parsing????.
					parsePacket( pcContext, pcOverlappedRecv, iTransferedBytes );
					
					//???????? ???????? Overlapped???????? ?? ??????(??????)??????. ???? ????~
					pcOverlappedRecv->recycleOverlapped();
					postRecv( pcContext, pcOverlappedRecv );
				}
				else if( pcOverlappedEx->m_iOperation == IOCP_OVERLAP_SEND )
				{
					//printf( "(%d)Byte Send succeeded!!\n", iTransferedBytes );
					pcOverlappedSend = (COverlappedSend *)pcOverlappedEx;

					//?????? ???????? ???? ?????? ?????? ?????? 0???? ??????????.
					delete pcOverlappedSend;
				}
				else if( pcOverlappedEx->m_iOperation == IOCP_OVERLAP_PATCH_FILE )
				{
					//printf( "[PatchFile] (%d)bytes Send\n", iTransferedBytes );

					//???? ?????? ???? ?????? ??????.
					COverlappedSendPatchFile  *pcOverlappedSendPatchFile;
					pcOverlappedSendPatchFile = (COverlappedSendPatchFile *)pcOverlappedEx;

					delete pcOverlappedSendPatchFile;
				}
			}
		}
	}

	printf( "End IOCPGQCSThread.\n" );

	return 0;
}

unsigned int CIOCPNetworkModule::IOCPGQCSThreadSM( void *pArg )
{
	printf( "Begin IOCPGQCSThread.\n" );

	CSocketIOContext	*pcContext = NULL;
	CAcceptOverlappedEx *pcAcceptOverlappedEx = NULL;
	COverlappedEx		*pcOverlappedEx = NULL;
	COverlappedRecv		*pcOverlappedRecv = NULL;
	COverlappedSend		*pcOverlappedSend = NULL;
	OVERLAPPED			*pcOverlapped =NULL;

	BOOL			bResult;
	unsigned long	iTransferedBytes;

	InitLocalIPAddr();

	while( 1 )
	{
		bResult = GetQueuedCompletionStatus( m_hIOCP, &iTransferedBytes, (DWORD *)&pcContext, &pcOverlapped, INFINITE );

		if( bResult == FALSE )
		{
			if( pcOverlapped == NULL )
			{
				//printf( "Fatal Error. Illegal call to GQCS.\n" );
			}
			else//if( lpOverlapped != NULL )
			{
				//2003.12.2 Accept?????? ????????. ???? ????????.
//				if( (unsigned int)pcContext == IOCP_ACCEPTEX )
//				{
//					;
//				}
//				else
//				{
				if( (unsigned int)pcContext == IOCP_ACCEPTEX )
				{
					printf("IOCP_ACCEPTEX (SM)");
				}
					//Close Connection.
					printf( "Abnormal Closing (SM) !!\n" );
					m_iCurrentUsers--;
					printf( "Remove SM\n" );

					m_cLockMgr.Lock();
					m_cLockMgr.RemoveObject( (int) pcContext->m_hSocket );
					delete pcContext;
					m_cLockMgr.Unlock();

					pcOverlappedEx = (COverlappedEx *)pcOverlapped;

					if( pcOverlappedEx->m_iOperation == IOCP_OVERLAP_RECV )
					{
						pcOverlappedRecv = (COverlappedRecv *)pcOverlappedEx;
						delete pcOverlappedRecv;
					}
					else if( pcOverlappedEx->m_iOperation == IOCP_OVERLAP_SEND )
					{
						pcOverlappedSend = (COverlappedSend *)pcOverlappedEx;
						delete pcOverlappedSend;
					}
//				}
			}
		}
		else //if( bResult == TRUE )
		{
			if( iTransferedBytes == 0 )
			{
				if( (unsigned int)pcContext == IOCP_OP_DESTROY )
				{
					//IOCP ?????? Destroy?? ??????.
					m_iStatus = IOCP_OP_DESTROY;

					//Send,Recv Thread?? ???????????? Signal??????.
					SetEvent( m_hSendQueueEvent );
					SetEvent( m_hRecvQueueEvent );
					break;
				}
				else if( (unsigned int)pcContext == IOCP_ACCEPTEX )
				{
					SOCKET			hConnectedSocket;
					sockaddr_in		*lpRemoteAddr;
					int				nRemoteSockAddrLen;

					nRemoteSockAddrLen = 0;

					pcAcceptOverlappedEx = (CAcceptOverlappedEx *)pcOverlapped;

					GetAcceptExSockaddrs((LPWSABUF)pcAcceptOverlappedEx->m_pstrAddrInfo, 
										0, 
										0, 
										sizeof(SOCKADDR_IN) + 16, 
										NULL, 
										NULL, 
										(LPSOCKADDR*)&lpRemoteAddr, 
										&nRemoteSockAddrLen);

					printf( "Accept SM Connection.\n" );

					if( lpRemoteAddr != NULL )
					{
						BOOL			bBlockIP;
						unsigned long	ulIP;

						bBlockIP = false;
						ulIP = 0;

						printf( "Client SM : %s\n", inet_ntoa( lpRemoteAddr->sin_addr ) );

						ulIP = lpRemoteAddr->sin_addr.S_un.S_addr;

						hConnectedSocket = pcAcceptOverlappedEx->m_hSocket;

						CSocketIOContext	*pSocketIOContext;
						pSocketIOContext = new CSocketIOContext( hConnectedSocket, 0, false, 4096, true );

						//Associate with IOCP Port
						AssociateIOCP( (HANDLE)hConnectedSocket, (void *)pSocketIOContext );
						//?????? ???? ???????????? ???????? ?????? ????????.

						m_cLockMgr.AddObject( pSocketIOContext, (int) hConnectedSocket );

						//???? OverlappedRecv?? ??????????.
						COverlappedRecv		*pcOverlappedRecv;
						pcOverlappedRecv = new COverlappedRecv( true, 4096, 0 );

						postRecv( pSocketIOContext, pcOverlappedRecv );

						m_iCurrentUsers++;
						printf( "Add SM \n" );

						{
							ApServerMonitorPacket	csPacket;

							int		lServerType	= ASSM_SERVER_TYPE_PATCH_SERVER;
							int		lServerStatus = 1;
							short	nPacketLength;
							void *	pvPacket;
							//DWORD	dwSendBytes;

							pvPacket = csPacket.m_csApsmServerInfoPacket.MakeStaticPacket(TRUE, &nPacketLength, ASSM_PACKET_SERVER_INFO,
							/*  1 */		&lServerType,
							/*  2 */		NULL,
							/*  3 */		NULL,				// Group ?? ???????? ????.
							/*  4 */		m_szLocalAddr[m_nNumAddr-1],
							/*  5 */		m_szLocalAddr[m_nNumAddr-1],
							/*  6 */		&lServerStatus,
							/*  7 */		&m_iCurrentUsers,
							/*  8 */		NULL,
							/*  9 */		NULL,
							/* 10 */		NULL,
							/* 11 */		NULL,
							/* 12 */		NULL,
							/* 13 */		NULL,
							/* 14 */		&m_iVersion,
							/* 15 */		NULL,
							/* 16 */		NULL,
							/* 17 */		NULL,
							/* 18 */		NULL,
							/* 19 */		NULL,
							/* 20 */		NULL,
							/* 21 */		NULL,
							/* 22 */		NULL
									);

							sendPacket( pSocketIOContext, (char *) pvPacket, nPacketLength );
						}
					}

					//?? Accept?? ????.
					postAccept();
				}
				else
				{
					//Close Connection.
					printf( "Close a SM connection......\n" );
					m_iCurrentUsers--;
					printf( "Remove SM\n" );

					m_cLockMgr.Lock();
					m_cLockMgr.RemoveObject( (int) pcContext->m_hSocket );
					delete pcContext;
					m_cLockMgr.Unlock();

					pcOverlappedEx = (COverlappedEx *)pcOverlapped;

					if( pcOverlappedEx->m_iOperation == IOCP_OVERLAP_RECV )
					{
						pcOverlappedRecv = (COverlappedRecv *)pcOverlappedEx;
						delete pcOverlappedRecv;
					}
					else if( pcOverlappedEx->m_iOperation == IOCP_OVERLAP_SEND )
					{
						pcOverlappedSend = (COverlappedSend *)pcOverlappedEx;
						delete pcOverlappedSend;
					}
				}
			}
			//???? ?? ?????? ??????. ??????????!!
			else
			{
				pcOverlappedEx = (COverlappedEx *)pcOverlapped;

				if( pcOverlappedEx->m_iOperation == IOCP_OVERLAP_RECV )
				{
					//printf( "(%d)Byte Recv succeeded!!\n", iTransferedBytes );
					pcOverlappedRecv = (COverlappedRecv *)pcOverlappedEx;

					//???? ?????? Parsing????.
					parsePacket( pcContext, pcOverlappedRecv, iTransferedBytes );
					
					//???????? ???????? Overlapped???????? ?? ??????(??????)??????. ???? ????~
					pcOverlappedRecv->recycleOverlapped();
					postRecv( pcContext, pcOverlappedRecv );
				}
				else if( pcOverlappedEx->m_iOperation == IOCP_OVERLAP_SEND )
				{
					//printf( "(%d)Byte Send succeeded!!\n", iTransferedBytes );
					pcOverlappedSend = (COverlappedSend *)pcOverlappedEx;

					//?????? ???????? ???? ?????? ?????? ?????? 0???? ??????????.
					delete pcOverlappedSend;
				}
				else if( pcOverlappedEx->m_iOperation == IOCP_OVERLAP_PATCH_FILE )
				{
					//printf( "[PatchFile] (%d)bytes Send\n", iTransferedBytes );

					//???? ?????? ???? ?????? ??????.
					COverlappedSendPatchFile  *pcOverlappedSendPatchFile;
					pcOverlappedSendPatchFile = (COverlappedSendPatchFile *)pcOverlappedEx;

					delete pcOverlappedSendPatchFile;
				}
			}
		}
	}

	printf( "End IOCPGQCSThread.\n" );

	return 0;
}

unsigned int CIOCPNetworkModule::IOCPSendThread( void *pArg )
{
	printf( "Begin IOCPSendThread.\n" );

	CQueueUnit		*pcQueueUnit;

	while( 1 )
	{
		if( WaitForSingleObject( m_hSendQueueEvent, INFINITE ) == WAIT_FAILED )
		{
			//printf( "Error:%d\n", GetLastError() );
			getchar();
		}
		else
		{
			if( m_iStatus == IOCP_OP_DESTROY )
				break;
		}

		pcQueueUnit = m_cSendQueue.pop();

		if( pcQueueUnit != NULL )
		{
			sendPacket( pcQueueUnit->m_pcContext, pcQueueUnit->m_pstrPacket, pcQueueUnit->m_iPacketSize );

			delete pcQueueUnit;
		}
		else
		{
			ResetEvent( m_hSendQueueEvent );
		}
	}

	printf( "End IOCPSendThread.\n" );

	return 0;
}

unsigned int CIOCPNetworkModule::IOCPRecvThread( void *pArg )
{
	printf( "Begin IOCPRecvThread.\n" );

	CQueueUnit		*pcQueueUnit;

	while( 1 )
	{
		if( WaitForSingleObject( m_hRecvQueueEvent, INFINITE ) == WAIT_FAILED )
		{
			printf( "Error:%d\n", GetLastError() );
			getchar();
		}
		else
		{
			if( m_iStatus == IOCP_OP_DESTROY )
				break;
		}

		pcQueueUnit = m_cRecvQueue.pop();

		if( pcQueueUnit != NULL )
		{
			m_cLockMgr.Lock();

			if( m_cLockMgr.GetObject( (int) pcQueueUnit->m_hSocket ) )
			{
				pcQueueUnit->m_pcContext->RecvLock();

				// Echo Packet
				if( (char) 0xff == pcQueueUnit->m_pstrPacket[4] )
				{
					CPacketTypeCS_0xff	cPacketTypeCS_0xff;
					CPacketTypeSC_0xff	cPacketTypeSC_0xff;

					int				iPacketSize;
					int				iIndex;
					char			*pstrData;

					iIndex = 0;

					//cPacketTypeCS_0x00?? ????????.
					memcpy( &cPacketTypeCS_0xff.m_iSize, &pcQueueUnit->m_pstrPacket[iIndex], sizeof(cPacketTypeCS_0xff.m_iSize) );
					iIndex += sizeof(cPacketTypeCS_0xff.m_iSize);

					memcpy( &cPacketTypeCS_0xff.m_iCommand, &pcQueueUnit->m_pstrPacket[iIndex], sizeof(cPacketTypeCS_0xff.m_iCommand) );
					iIndex += sizeof(cPacketTypeCS_0xff.m_iCommand);

					memcpy( &cPacketTypeCS_0xff.m_iSeq, &pcQueueUnit->m_pstrPacket[iIndex], sizeof(cPacketTypeCS_0xff.m_iSeq) );
					iIndex += sizeof(cPacketTypeCS_0xff.m_iSeq);

					iIndex = 0;
					cPacketTypeSC_0xff.m_iSeq = cPacketTypeCS_0xff.m_iSeq;

					iPacketSize = cPacketTypeSC_0xff.m_iSize;

					pstrData = new char[iPacketSize];

					memcpy( &pstrData[iIndex], &cPacketTypeSC_0xff.m_iSize, sizeof(cPacketTypeSC_0xff.m_iSize) );
					iIndex += sizeof(cPacketTypeSC_0xff.m_iSize);

					memcpy( &pstrData[iIndex], &cPacketTypeSC_0xff.m_iCommand, sizeof(cPacketTypeSC_0xff.m_iCommand) );
					iIndex += sizeof(cPacketTypeSC_0xff.m_iCommand);

					memcpy( &pstrData[iIndex], &cPacketTypeSC_0xff.m_iSeq, sizeof(cPacketTypeSC_0xff.m_iSeq) );
					iIndex += sizeof(cPacketTypeSC_0xff.m_iSeq);

					sendPacket(pcQueueUnit->m_pcContext, (char *)pstrData, iPacketSize );
				}
				// Patch ???? Packet
				else if( 0x00 == pcQueueUnit->m_pstrPacket[4] )
				{
					CDList< CJZPFileInfo * > *pcPatchFileInfo;
					CPacketTypeCS_0x00	cPacketTypeCS_0x00;

					int				iVersion;
					unsigned int	iPatchCheckCode;
					int				iResult;
					int				iIndex;

					iIndex = 0;

					//cPacketTypeCS_0x00?? ????????.
					memcpy( &cPacketTypeCS_0x00.m_iSize, &pcQueueUnit->m_pstrPacket[iIndex], sizeof(cPacketTypeCS_0x00.m_iSize) );
					iIndex += sizeof(cPacketTypeCS_0x00.m_iSize);

					memcpy( &cPacketTypeCS_0x00.m_iCommand, &pcQueueUnit->m_pstrPacket[iIndex], sizeof(cPacketTypeCS_0x00.m_iCommand) );
					iIndex += sizeof(cPacketTypeCS_0x00.m_iCommand);

					memcpy( &cPacketTypeCS_0x00.m_iVersion, &pcQueueUnit->m_pstrPacket[iIndex], sizeof(cPacketTypeCS_0x00.m_iVersion) );
					iIndex += sizeof(cPacketTypeCS_0x00.m_iVersion);

					memcpy( &cPacketTypeCS_0x00.m_iPatchCheckCode, &pcQueueUnit->m_pstrPacket[iIndex], sizeof(cPacketTypeCS_0x00.m_iPatchCheckCode) );
					iIndex += sizeof(cPacketTypeCS_0x00.m_iPatchCheckCode);

					//???????????????? ?????? ????????.
					iVersion = cPacketTypeCS_0x00.m_iVersion;
					iPatchCheckCode = cPacketTypeCS_0x00.m_iPatchCheckCode;

					pcPatchFileInfo = new CDList< CJZPFileInfo * >;

					//?????????????? ???????? ?????? ???? ??????.
					iResult = GetPatchFiles( iVersion, iPatchCheckCode, pcPatchFileInfo );

					if( iResult == 0 )
					{
						CListNode< CJZPFileInfo * >	*pcNode;
						__int64			iTotalSize;

						iTotalSize = 0;

						for( pcNode = pcPatchFileInfo->GetStartNode(); pcNode; pcNode=pcNode->m_pcNextNode )
						{
							iTotalSize += pcNode->m_tData->m_iFileSize;
							iTotalSize += pcNode->m_tData->m_iRawFileSize;
						}

						//???? ?????? ???????? ?????? ???? ????????.
						CPacketTypeSC_0x01		cPacketTypeSC_0x01;

						int				iPacketSize;
						int				iIndex;
						char			*pstrData;

						iIndex = 0;
						cPacketTypeSC_0x01.m_iRecvFiles = pcPatchFileInfo->getCount(); //?????????? ?????? ?????????
						cPacketTypeSC_0x01.m_iVersion = m_iVersion;                    //?????????? ???????
						cPacketTypeSC_0x01.m_iPatchCheckCode =  m_cAuPatchCheckCode.GetCode(m_iVersion);
						cPacketTypeSC_0x01.m_iTotalSize = iTotalSize;
						memset( cPacketTypeSC_0x01.m_strServerIP, 0, sizeof(cPacketTypeSC_0x01.m_strServerIP) );
						strcat( cPacketTypeSC_0x01.m_strServerIP, GetDownloadServerIP() );
						cPacketTypeSC_0x01.m_iServerPort = GetDownloadServerPort();

						iPacketSize = cPacketTypeSC_0x01.m_iSize;

						pstrData = new char[iPacketSize];

						memcpy( &pstrData[iIndex], &cPacketTypeSC_0x01.m_iSize, sizeof(cPacketTypeSC_0x01.m_iSize) );
						iIndex += sizeof(cPacketTypeSC_0x01.m_iSize);

						memcpy( &pstrData[iIndex], &cPacketTypeSC_0x01.m_iCommand, sizeof(cPacketTypeSC_0x01.m_iCommand) );
						iIndex += sizeof(cPacketTypeSC_0x01.m_iCommand);

						memcpy( &pstrData[iIndex], &cPacketTypeSC_0x01.m_iRecvFiles, sizeof(cPacketTypeSC_0x01.m_iRecvFiles) );
						iIndex += sizeof(cPacketTypeSC_0x01.m_iRecvFiles);

						memcpy( &pstrData[iIndex], &cPacketTypeSC_0x01.m_iVersion, sizeof(cPacketTypeSC_0x01.m_iVersion) );
						iIndex += sizeof(cPacketTypeSC_0x01.m_iVersion);

						memcpy( &pstrData[iIndex], &cPacketTypeSC_0x01.m_iPatchCheckCode, sizeof(cPacketTypeSC_0x01.m_iPatchCheckCode) );
						iIndex += sizeof(cPacketTypeSC_0x01.m_iPatchCheckCode);

						memcpy( &pstrData[iIndex], &cPacketTypeSC_0x01.m_iTotalSize, sizeof(cPacketTypeSC_0x01.m_iTotalSize) );
						iIndex += sizeof(cPacketTypeSC_0x01.m_iTotalSize);

						memcpy( &pstrData[iIndex], &cPacketTypeSC_0x01.m_strServerIP, sizeof(cPacketTypeSC_0x01.m_strServerIP) );
						iIndex += sizeof(cPacketTypeSC_0x01.m_strServerIP);

						memcpy( &pstrData[iIndex], &cPacketTypeSC_0x01.m_iServerPort, sizeof(cPacketTypeSC_0x01.m_iServerPort) );
						iIndex += sizeof(cPacketTypeSC_0x01.m_iServerPort);

						sendPacket(pcQueueUnit->m_pcContext, (char *)pstrData, iPacketSize );

						//???? ???? ?????? ??????.
						//1. ???? ???? ???????? ??????.
						pcQueueUnit->m_pcContext->m_pcPatchFileInfo = pcPatchFileInfo;
						pcQueueUnit->m_pcContext->m_pcPatchFileInfoNode = pcQueueUnit->m_pcContext->m_pcPatchFileInfo->GetStartNode();
					}
					else
					{
						int				iPacketSize;
						int				iIndex;
						char			*pstrData;

						iIndex = 0;

						CPacketTypeSC_0xEE  cPacketType0xEE;

						iPacketSize = cPacketType0xEE.m_iSize;

						pstrData = new char[iPacketSize];

						memcpy( &pstrData[iIndex], &cPacketType0xEE.m_iSize, sizeof(cPacketType0xEE.m_iSize) );
						iIndex += sizeof(cPacketType0xEE.m_iSize);

						memcpy( &pstrData[iIndex], &cPacketType0xEE.m_iCommand, sizeof(cPacketType0xEE.m_iCommand) );
						iIndex += sizeof(cPacketType0xEE.m_iCommand);

						memcpy( &pstrData[iIndex], &iResult, sizeof(iResult) );
						iIndex += sizeof(iResult);

						sendPacket(pcQueueUnit->m_pcContext, (char *)pstrData, iPacketSize );
					}
				}
				//???????? ?????? ?????? ?????? ?????????
				else if( 0x04 == pcQueueUnit->m_pstrPacket[4] )
				{
					if( pcQueueUnit->m_pcContext->m_pcPatchFileInfoNode != NULL )
					{
						sendPatchFileInfo( pcQueueUnit->m_pcContext );
					}
					else //if( pcQueueUnit->m_pcContext->m_pcPatchFileInfoNode == NULL )
					{
						//?????? ?? ?????????? ?????? ??????.
						closesocket( pcQueueUnit->m_pcContext->m_hSocket );
					}
				}
				//???????? ?????? ?????? ?????? ?????????
				else if( 0x05 == pcQueueUnit->m_pstrPacket[4] )
				{
					SendClientFileCRC( pcQueueUnit->m_pcContext );
				}

				pcQueueUnit->m_pcContext->RecvUnlock();

				delete [] pcQueueUnit->m_pstrPacket;
				delete pcQueueUnit;
			}

			m_cLockMgr.Unlock();
		}
		else
		{
			ResetEvent( m_hRecvQueueEvent );
		}
	}

	printf( "End IOCPRecvThread.\n" );

	return 0;
}

VOID CIOCPNetworkModule::InitLocalIPAddr()
{
    CHAR strLocalHostName[MAX_PATH];
    gethostname( strLocalHostName, MAX_PATH );
    HOSTENT* pHostEnt = gethostbyname( strLocalHostName );

	printf("Local Hostname : %s\n", strLocalHostName);

	m_nNumAddr = 0;

    if( pHostEnt )
    {
		for (int i = 0; i < MAX_IP; i++)
		{
			_tcscpy( m_szLocalAddr[i], TEXT("") );

			in_addr* pInAddr = (in_addr*) pHostEnt->h_addr_list[i];
			if (!pInAddr)
				break;

			char* strLocalIP = inet_ntoa( *pInAddr );
			if( strLocalIP )
			{
				printf("Local IP : %s\n", strLocalIP);
				strncpy(m_szLocalAddr[i], strLocalIP, strlen(strLocalIP) + 1);
			}

			m_nNumAddr++;
		}
    }
}

BOOL CIOCPNetworkModule::SendClientFileCRC( CSocketIOContext *pContext )
{
	if( !pContext ) return FALSE;

	CPacketTypeSC_0x05 cPacket;

	unsigned int nSize = cPacket.m_iSize;
	unsigned int nIndex = 0;

	char* pData = new char[ nSize ];
	memset( pData, 0, sizeof( char ) * nSize );

	memcpy( &pData[ nIndex ], &cPacket.m_iSize, sizeof( cPacket.m_iSize ) );
	nIndex += sizeof( cPacket.m_iSize );

	memcpy( &pData[ nIndex ], &cPacket.m_iCommand, sizeof( cPacket.m_iCommand ) );
	nIndex += sizeof( cPacket.m_iCommand );

	memcpy( &pData[ nIndex ], &m_ulClientFileCRC, sizeof( m_ulClientFileCRC ) );
	nIndex += sizeof( m_ulClientFileCRC );

	if( !sendPacket( pContext, pData, nSize ) ) return FALSE;
	return TRUE;
}

unsigned long CIOCPNetworkModule::_GetClientFileCRC( char* pFolderName, char* pFileName )
{
	unsigned long ulCRC = 0;
	if( !pFolderName || strlen( pFolderName ) <= 0 ) return ulCRC;
	if( !pFileName || strlen( pFileName ) <= 0 ) return ulCRC;

	char strFileName[ 256 ] = { 0, };
	strcpy_s( strFileName, sizeof( char ) * 256, pFileName );

	CPackingFile* pFile = m_cAuPackingManager.GetFile( pFolderName, strFileName, false );
	if( !pFile ) return ulCRC;

	ulCRC = pFile->m_lFileCheckSum;
	return ulCRC;
}
