#include "IMemPool.h"

#if 0

#include <string>
#include <map>

#include <iostream>

struct MyStruct
{
	char str[1024];
};

struct MyStruct2
{
	int a = 0;
	float b = 0.0f;
};


static int Test()
{

	INVENT::IMemPool* pool = INVENT::IMemPool::CreatePool();

	using MapAlloc = INVENT::IMemPoolAllocator<std::pair<const std::string, MyStruct>>;
	MapAlloc alloc(pool);
	using MapAlloc2 = INVENT::IMemPoolAllocatorOnlyFixedBlock<std::pair<const std::string, MyStruct2>>;
	MapAlloc2 alloc2(pool);

	std::map<std::string, MyStruct, std::less<std::string>, MapAlloc> map1(alloc);
	std::map<std::string, MyStruct2, std::less<std::string>, MapAlloc2> map2(alloc2);

	MyStruct struct11{};
	struct11.str[0] = 'A';
	MyStruct struct12{};
	struct12.str[0] = 'B';

	MyStruct2 struct21{};
	struct21.a = 1;
	struct21.b = 1.0f;
	MyStruct2 struct22{};
	struct22.a = 2;
	struct22.b = 2.0f;

	map1["struct11"] = struct11;
	map1["struct12"] = struct12;

	map2["struct21"] = struct21;
	map2["struct22"] = struct22;

	std::cout << "map1[\"struct11\"].str[0] : " << (*map1.find("struct11")).second.str[0] << "\n";
	std::cout << "map1[\"struct12\"].str[0] : " << (*map1.find("struct12")).second.str[0] << "\n";
	std::cout << "map2[\"struct21\"].a,b : " << (*map2.find("struct21")).second.a << ", " << map2["struct21"].b << "\n";
	std::cout << "map2[\"struct22\"].a,b : " << (*map2.find("struct22")).second.a << ", " << map2["struct22"].b << "\n";


	return 0;
}
#endif // 0


