// AuDynCode.cpp
// (C) NHN Games - ArchLord Development Team
// steeple, 2006.06.08.

#ifdef _AREA_CHINA_

#ifdef _DEBUG
	#ifdef _DLL
		#pragma comment(lib, "SDSvrDynCodeMDD.lib")
	#else
		#pragma comment(lib, "SDSvrDynCodeMTD.lib")
	#endif
#else
	#ifdef _DLL
		#pragma comment(lib, "SDSvrDynCodeMD.lib")
	#else
		#pragma comment(lib, "SDSvrDynCodeMT.lib")
	#endif
#endif

#include "AuDynCode.h"
#include <windows.h>
#include "SDCltDynCode.h"
#include "SDSvrDynCode.h"

const CHAR* CLIENTBIN_PATH = "\\DynCodeBin\\Client32";

#ifdef _M_X64
const CHAR* SERVERBIN_PATH = "\\DynCodeBin\\Server64";
#else
const CHAR* SERVERBIN_PATH = "\\DynCodeBin\\Server32";
#endif

using namespace SGDP;

void DYNCODE_CTX::init()
{
	eAlgorithm = AUCRYPT_ALGORITHM_DYNCODE;
	nCodeIndex = 0;
	m_pClientDynCode = NULL;
}

AuDynCode::AuDynCode()
{
	m_pServerDynCode = NULL;
	bServer = FALSE;
}

AuDynCode::~AuDynCode()
{
	Cleanup();
}

// ���� DynCode �� �ִ� �� ������.
// Static �̾ ������ ��. �ѹ��ϰ� ���� �״���� read �����̴�.
char* AuDynCode::GetRootPath()
{
    static char szPath[MAX_PATH];
    static bool bFirstTime = true;

    if(bFirstTime)
    {
        bFirstTime = false;
        GetModuleFileName(NULL, szPath, sizeof(szPath));
        char *p = strrchr(szPath, '\\');
        *p = '\0';
    }

    return szPath;
}

BOOL AuDynCode::InitServer()
{
	bServer = TRUE;		// ���߿� Ŭ������ ���ؼ�

	if(!m_pServerDynCode)
		m_pServerDynCode = SDCreateSvrDynCode();

    if(NULL == m_pServerDynCode)
		return FALSE;

    CHAR szServerDir[MAX_PATH];
    sprintf(szServerDir, "%s%s", GetRootPath(), SERVERBIN_PATH);

    CHAR szClientDir[MAX_PATH];
    sprintf(szClientDir, "%s%s", GetRootPath(), CLIENTBIN_PATH);

	int nBinCount;
    nBinCount = m_pServerDynCode->LoadBinary(szServerDir, szClientDir);
	if(nBinCount == 0)
	{
		ASSERT(nBinCount > 0);
		return FALSE;
	}

	return TRUE;
}

void AuDynCode::Cleanup()
{
	if(m_pServerDynCode)
	{
		m_pServerDynCode->Release();
		m_pServerDynCode = NULL;
	}
}

void AuDynCode::Initialize(void* pctx, BYTE* key, DWORD lKeySize, BOOL bClone)
{
	if(!pctx)
		return;

	DYNCODE_CTX& ctx = *static_cast<DYNCODE_CTX*>(pctx);

	const BYTE* pCode = NULL;
	{
		if(!m_pServerDynCode)
			return;

		INT32 lCodeLength = 0;

		// Server �� ���� Index �� ���� �Ҵ��Ѵ�.
		ctx.nCodeIndex = m_pServerDynCode->GetRandIdx();
		lCodeLength = m_pServerDynCode->GetCltDynCode(ctx.nCodeIndex, &pCode);
		if(lCodeLength <= 0)
		{
			ASSERT(!"ctx.nCodeIndex �� �߸��Ǿ���.");
			ctx.init();
		}
	}
}

void AuDynCode::Initialize(void* pctx, void* pctxSource)
{
	if(!pctx || !pctxSource)
		return;

	DYNCODE_CTX& ctx = *static_cast<DYNCODE_CTX*>(pctx);
	DYNCODE_CTX& ctxSource = *static_cast<DYNCODE_CTX*>(pctxSource);

	{
		// �� memcpy �� �ɵ�?
		memcpy(&ctx, &ctxSource, sizeof(ctx));
	}
}

// Encrypt �� pOutput �� pInput �� ����, lSize �� �״���̴�.
DWORD AuDynCode::Encrypt(void* pctx, BYTE* pInput, BYTE* pOutput, DWORD lSize)
{
	if(!pctx || !pInput || lSize < 1)
		return 0;

	DYNCODE_CTX& ctx = *static_cast<DYNCODE_CTX*>(pctx);
	{
		if(!m_pServerDynCode)
			return 0;

		if(m_pServerDynCode->Encode(pInput, lSize, ctx.nCodeIndex))
		{
			memcpy(pOutput, pInput, lSize);
			return lSize;
		}
		else
			return 0;
	}
}

// Decrypt �� pOutput �� pInput �� ����, lSize �� �״���̴�.
void AuDynCode::Decrypt(void* pctx, BYTE* pInput, BYTE* pOutput, DWORD lSize)
{
	if(!pctx || !pInput || lSize < 1)
		return;

	DYNCODE_CTX& ctx = *static_cast<DYNCODE_CTX*>(pctx);
	{
		if(!m_pServerDynCode)
			return;

		if(m_pServerDynCode->Decode(pInput, lSize, ctx.nCodeIndex))
			memcpy(pOutput, pInput, lSize);
	}
}

// Output �� ����.
DWORD AuDynCode::GetOutputSize(DWORD lInputSize)
{
	return lInputSize;
}

// AuCryptAlgorithmBase ���� ��ӹ��� �� �ƴ� DynCode �� Ưȭ�� �Լ�
INT32 AuDynCode::GetClientDynCode(void* pctx, const BYTE** ppCode)
{
	if(!pctx)
		return 0;

	DYNCODE_CTX& ctx = *static_cast<DYNCODE_CTX*>(pctx);

	return m_pServerDynCode->GetCltDynCode(ctx.nCodeIndex, ppCode);
}

#endif//_AREA_CHINA_