#include "AgcmRegistryManager.h"
#include <stdio.h>

//-----------------------------------------------------------------------
//

AgcmRegistryManager::AgcmRegistryManager()
	: handle_(0)
{
	// key setting
	HKEY key = HKEY_LOCAL_MACHINE;

	// sub key setting
#ifdef _AREA_GLOBAL_
	const char * subKey			= "SOFTWARE\\Webzen\\ArchLord";
	const char * subKey_alpha	= "SOFTWARE\\Webzen\\ArchLord_GB_Alpha";
#else
	const char * subKey = "SOFTWARE\\ArchLord";
#endif

#ifndef USE_MFC
	// open
	FILE * fp = fopen("AlphaTest.arc", "rb");
	if(!fp)
	{
		if( ERROR_SUCCESS != RegOpenKeyEx( key, subKey, 0, KEY_READ, &handle_ ) )
		{
			std::string message = "FAILED - AgcmRegistryManager::RegOpenKey, ";
			message += (char const*)subKey;
			OutputDebugString( message.c_str() );

			handle_ = 0;
		}
	}
	else
	{
		if( ERROR_SUCCESS != RegOpenKeyEx( key, subKey_alpha, 0, KEY_READ, &handle_ ) )
		{
			std::string message = "FAILED - AgcmRegistryManager::RegOpenKey, ";
			message += (char const*)subKey;
			OutputDebugString( message.c_str() );

			handle_ = 0;
		}
		fclose(fp);
	}
#endif
}

//-----------------------------------------------------------------------
//

AgcmRegistryManager::~AgcmRegistryManager()
{
	if( handle_ )
		RegCloseKey(handle_);
}

//-----------------------------------------------------------------------
//

namespace AgcmRegistryManagerUtill
{
	void set_value( std::string & v, char * tmp, DWORD len )
	{
		v = tmp;
	}

	void set_value( std::wstring & v, char * tmp, DWORD len )
	{
		wchar_t buf[1024];
		size_t convertedSize = 0;

		mbstowcs_s( &convertedSize, buf, sizeof(buf), tmp, len );

		v = buf;
	}
}

//-----------------------------------------------------------------------