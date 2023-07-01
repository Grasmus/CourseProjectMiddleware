#pragma once
#include "Structs.h"
#include "Logger.h"
#include <fstream>
#include <random>

using namespace std;

namespace db {
	class Db
	{
	public:
		Db();
		~Db();

	public:
		bool fileExists(const string& filename);
		long long generateUniqueId();
		void createDataBase(vector<userAddressInfo>* addresses, unique_ptr<loggernamespace::Logger> logger);
		vector<userAddressInfo> readAddresses(bool* isDecisionMakerCenter, string hostname, unique_ptr<loggernamespace::Logger> logger);
		//void deleteAddressById(string filename, int idToDelete, loggernamespace::Logger* logger);
		void updateDataBase(string filename, vector<userAddressInfo>* addresses, unique_ptr<loggernamespace::Logger> logger);
		//void addAddressToDataBase(string filename, string newAddress, loggernamespace::Logger* logger);
	private:
		unique_ptr<loggernamespace::Logger> logger_;
	};

}
