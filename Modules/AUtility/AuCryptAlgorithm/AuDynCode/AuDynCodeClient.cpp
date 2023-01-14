// AuDynCode.cpp
// (C) NHN Games - ArchLord Development Team
// steeple, 2006.06.08.

#ifdef _AREA_CHINA_

#ifdef _DEBUG
	#ifdef _DLL
		#pragma comment(lib, "SDCltDynCodeMDd.lib")
	#else
		#pragma comment(lib, "SDCltDynCodeMTd.lib")
	#endif
#else
	#ifdef _DLL
		#pragma comment(lib, "SDCltDynCodeMD.lib")
	#else
		#pragma comment(lib, "SDCltDynCodeMT.lib")
	#endif
#endif

#include "AuDynCode.h"
#include <windows.h>
#include "SDCltDynCode.h"
#include "SDSvrDynCode.h"

const CHAR* CLIENTBIN_PATH = "\\DynCodeBin\\Client32";

using namespace SGDP;

void DYNCODE_CTX::init()
{
	eAlgorithm = AUCRYPT_ALGORITHM_DYNCODE;
	nCodeIndex = 0;
	if(m_pClientDynCode)
		m_pClientDynCode->Release();
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
	return TRUE;
}

void AuDynCode::Cleanup()
{
}

void AuDynCode::Initialize(void* pctx, BYTE* key, DWORD lKeySize, BOOL bClone)
{
	if(!pctx)
		return;

	DYNCODE_CTX& ctx = *static_cast<DYNCODE_CTX*>(pctx);

	const BYTE* pCode = NULL;
	{
		// Client �� ���� �Ѿ�� key, lKeySize �� �������� �� pCode �̴�.
		if(ctx.m_pClientDynCode == NULL)
			ctx.m_pClientDynCode = SDCreateCltDynCode();

		if(ctx.m_pClientDynCode == NULL)
			return;

		ctx.m_pClientDynCode->SetDynCode(key, lKeySize);
	}
}

void AuDynCode::Initialize(void* pctx, void* pctxSource)
{
	if(!pctx || !pctxSource)
		return;

	DYNCODE_CTX& ctx = *static_cast<DYNCODE_CTX*>(pctx);
	DYNCODE_CTX& ctxSource = *static_cast<DYNCODE_CTX*>(pctxSource);

	if(bServer)
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
		if(!ctx.m_pClientDynCode)
			return 0;

		if(ctx.m_pClientDynCode->Encode(pInput, lSize))
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
		if(!ctx.m_pClientDynCode)
			return;

		if(ctx.m_pClientDynCode->Decode(pInput, lSize))
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

	return 0;
}

#endif//_AREA_CHINA_