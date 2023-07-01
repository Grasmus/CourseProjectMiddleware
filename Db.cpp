#include "Db.h"

namespace db{
	Db::Db()
	{
	}

	Db::~Db()
	{
	}

	bool Db::fileExists(const string& filename)
	{
		ifstream file(filename);
		return file.good();
	}

	long long Db::generateUniqueId()
	{
		std::random_device rd;
		std::mt19937_64 generator(rd());
		std::uniform_int_distribution<long long> distribution;
		return distribution(generator) % (10000000 + 1);
	}

	void Db::createDataBase(vector<userAddressInfo>* addresses, unique_ptr<loggernamespace::Logger> logger)
	{
		fstream file("database.dat", ios::in | ios::out | ios::app);

		if (!file)
		{
			logger->addLog("Failed to open the database file.");
			exit(EXIT_FAILURE);
		}
		for (const auto& address : *addresses) {
			file << address.id << " " << address.pcAddress << " " << address.isDecisionMakerCenter << '\n';
		}
		file.close();
	}

	vector<userAddressInfo> Db::readAddresses(bool * isDecisionMakerCenter, string hostname, unique_ptr<loggernamespace::Logger> logger)
	{
		vector<userAddressInfo> users;
		userAddressInfo tempUser;
		fstream file("database.dat", ios::in | ios::out | ios::app);
		if (!file) {
			logger->addLog("Failed to open the database file.");
			exit(EXIT_FAILURE);
		}
		while (file) {
			file >> tempUser.id >> tempUser.pcAddress >> tempUser.isDecisionMakerCenter;
			if (!file)
				break;
			if (tempUser.pcAddress != hostname)
			{
				users.push_back(tempUser);
			}
			else
			{
				*isDecisionMakerCenter = tempUser.isDecisionMakerCenter;
			}
		}

		file.close();

		return users;
	}

	//void Db::deleteAddressById(string filename, int idToDelete, loggernamespace::Logger* logger)
	//{
	//	ifstream inputFile(filename);
	//	if (!inputFile) {
	//		logger->addLog("Failed to open the database file.");
	//		return;
	//	}
	//	vector<userAddressInfo> addresses;
	//	userAddressInfo tempAddress;
	//	while (inputFile >> tempAddress.id >> tempAddress.pcAddress >> tempAddress.isDecisionMakerCenter) {
	//		if (tempAddress.id != idToDelete) {
	//			addresses.push_back(tempAddress);
	//		}
	//	}
	//	inputFile.close();
	//	updateDataBase(filename, &addresses);
	//}

	void Db::updateDataBase(string filename, vector<userAddressInfo>* addresses, unique_ptr<loggernamespace::Logger> logger)
	{
		ofstream outputFile(filename, std::ios::trunc);
		if (!outputFile) {
			logger->addLog("Failed to open the database file for writing.");
			return;
		}

		for (const auto& address : *addresses) {
			outputFile << address.id << ' ' << address.pcAddress << ' ' << address.isDecisionMakerCenter << '\n';
		}
		outputFile.close();
	}

	/*void Db::addAddressToDataBase(string filename, string newAddress, loggernamespace::Logger* logger)
	{
		vector<userAddressInfo> addresses = readAddresses();
		for (const auto& address : addresses) {
			if (address.pcAddress == newAddress) {
				logger->addLog("This address is already exists in database.");
				return;
			}
		}
		ofstream file("database.dat", ios::app);
		if (!file.is_open()) {
			logger->addLog("Failed to open the database file in addAddressToDataBase.");
			return;
		}
		file << generateUniqueId() << " " << newAddress << '\n';
		file.close();
		logger->addLog("Address was successfully added to database.");
	}*/

}