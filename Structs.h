#pragma once
#include <string>
#include <iostream>
#include <vector>

using namespace std;

struct Activity
{
	bool isActive{};
	bool isDecisionMakerActive{}; //makes sense only if isDecisionMakerCenter = true
};

struct userAddressInfo
{
	long long id{ 0 };
	string pcAddress;
	bool isDecisionMakerCenter{};
	Activity activity{}; //makes sense only if isDecisionMakerCenter = true
};

struct Data
{
	char nodeName[100]{};
	int a;
	int b;
};