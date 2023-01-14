#pragma once

#include <string>
#include <map>

class AuRegionNameConvert
{
public:
	~AuRegionNameConvert();

	bool Load(char* filename, bool encrypt);
	std::string GetStr(char* key);

	friend AuRegionNameConvert& RegionLocalName();

private:
	AuRegionNameConvert();

	char* GetDecryptStr( char* str, unsigned size );
	void parse_data( char * ptr );
	void parse_data( wchar_t * ptr );

	typedef std::map<std::string, std::string> StrMap;
	StrMap m_map;
};

AuRegionNameConvert& RegionLocalName();