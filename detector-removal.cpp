// APT27-lucky-variant.cpp : This file contains the 'main' function. Program execution begins and ends there.
// Developed by Ali Moharrami


#include <iostream>
#include <fstream>
#include <vector>
#include <cwctype>
#include <filesystem>
#include <stdio.h>
namespace fs = std::filesystem;

#include "detector_removal_func.h"



#define MAX_KEY_LENGTH 255
#define MAX_VALUE_NAME 16383

#define FILE				0x01
#define REGISTRY			0x02
#define PROCESS				0x04
#define OTHER				0xff

#define NOT_EXIST			0x00 
#define EXIST				0x01
#define NEED_RESTART		0x02
#define WARNING				0x08

HANDLE hConsole;


class registry_key {

public:
	registry_key(){
	}

	registry_key(const char reg_key[]) {
		SplitRoot(std::string(reg_key));
		value_count = 0;
	}




private:
	HKEY key;
	std::string root_key;
	std::string subkey;
	int value_count;
	std::vector<std::string> value;
	std::vector<std::string> data;

	bool SplitRoot(std::string path) {
		
		int pos;
		if (!(pos = path.find("\\")))
			return 0;

		std::string root = path.substr(0, pos);
		key = NULL;
		if (root == "hklm" || root == "HKLM") {
			root_key = "HKLM";
			key = HKEY_LOCAL_MACHINE;
		}if (root == "hkcu" || root == "HKCU") {
			root_key = "HKCU";
			key = HKEY_CURRENT_USER;
		}if (root == "hkcr" || root == "HKCR") {
			root_key = "HKCR";
			key = HKEY_CLASSES_ROOT;
		}if (root == "hkcc" || root == "HKCC") {
			root_key = "HKCC";
			key = HKEY_CURRENT_CONFIG;
		}

		if (key != NULL)
			subkey = path.substr(pos + 1);
		else
			return 0;

		return 1;

	}


public:
	void set_key(std::string in) { subkey = in; }
	
	std::string get_key() { return subkey; }

	HKEY get_root() { return key; }

	std::string get_registry() { return std::string(root_key + "\\" + subkey); }
	
	int add_value(std::string val,std::string val_data) {
		value_count++;
		value.push_back(val);
		data.push_back(val_data);

		return value_count;
	}

};

class IOC {

public:
	IOC(USHORT type, USHORT st) {
		state = st;
		detection_handler = generic_detector;
		removal_handler = generic_removal;
		ioc_type = type;
		
	}
	IOC(USHORT type, USHORT st, registry_key rIOC) : IOC(type, st) {
		reg_key = rIOC;
		ioc_type = REGISTRY;
	}
	IOC(USHORT type,USHORT st, std::string vIOC) : IOC(type, st) {
		ioc_val = vIOC;
		ioc_type = FILE;
	}
	IOC(USHORT type, USHORT st, registry_key rIOC, int(*dh)(IOC&), int(*rh)(IOC&) ) : IOC(type,st,rIOC) {
		removal_handler = rh;
		detection_handler = dh;
	}
	IOC(USHORT type, USHORT st, std::string vIOC, int(*dh)(IOC&), int(*rh)(IOC&)) : IOC(type,st,vIOC) {
		removal_handler = rh;
		detection_handler = dh;
	}
	IOC(USHORT type, USHORT st, int(*dh)(IOC&), int(*rh)(IOC&)) : IOC(type, st) {
		removal_handler = rh;
		detection_handler = dh;
	}


	USHORT state;
	registry_key reg_key;
	std::string ioc_val;
	int  (*detection_handler)(IOC&);
	int (*removal_handler)(IOC&);
	USHORT ioc_type;
  
	
};


bool icompare_pred(wchar_t a, wchar_t b)
{
	return std::towlower(a) == towlower(b);
}

bool icompare(std::wstring const& a, std::wstring const& b)
{
	if (a.length() == b.length()) {
		return std::equal(b.begin(), b.end(),
			a.begin(), icompare_pred);
	}
	else {
		return false;
	}
}




int generic_detector(IOC& ioc)
{
	if (ioc.ioc_type == REGISTRY)
	{

		HKEY subKey = nullptr;

		if (RegOpenKeyExA(ioc.reg_key.get_root(), ioc.reg_key.get_key().c_str(), 0, KEY_READ, &subKey) == ERROR_SUCCESS)
		{
			if (ioc.state & WARNING)
			{
				std::cout << "Suspicious registry artifact has been seen: '" << ioc.reg_key.get_registry().c_str() << "'.\n";
			}
			else
			{
				std::cout << "Registry IOC has been seen: '" << ioc.reg_key.get_registry().c_str() << "'.\n";
			}
			return 1;
		}

	}
	else if (ioc.ioc_type == FILE)
	{

		if (std::filesystem::exists(ioc.ioc_val.c_str()))
		{
			if (ioc.state & WARNING)
			{
				std::cout << "Suspicious File/Directory artifact has been seen: '" << ioc.ioc_val.c_str() << "'.\n";
			}
			else
			{
				std::cout << "File/Directory IOC has been seen: '" << ioc.ioc_val.c_str() << "'.\n";
			}
			return 1;
		}

	}

	return 0;
}


int generic_removal(IOC& ioc)
{
	if (ioc.ioc_type == REGISTRY)
	{
		if (RegDeleteTreeA(ioc.reg_key.get_root(), ioc.reg_key.get_key().c_str()) == ERROR_SUCCESS)
		{
			std::cout << "Removed : '" << ioc.reg_key.get_registry().c_str() << "' \n";
		}
		else
			return 1;

	}
	else  if (ioc.ioc_type == FILE)
	{
		if (std::filesystem::remove_all(ioc.ioc_val.c_str()))
		{
			std::cout << "Removed : '" << ioc.ioc_val.c_str() << "' \n";
		}
		else
			return 1;
	}
	
	return 0;

}

int EnumKey(HKEY root, std::wstring subkey)
{
	int found_count = 0;
	TCHAR    achKey[MAX_KEY_LENGTH];   // buffer for subkey name
	DWORD    cbName;                   // size of name string 
	TCHAR    achClass[MAX_PATH] = TEXT("");  // buffer for class name 
	DWORD    cchClassName = MAX_PATH;  // size of class string 
	DWORD    cSubKeys = 0;               // number of subkeys 
	DWORD    cbMaxSubKey;              // longest subkey size 
	DWORD    cchMaxClass;              // longest class string 
	DWORD    cValues;              // number of values for key 
	DWORD    cchMaxValue;          // longest value name 
	DWORD    cbMaxValueData;       // longest value data 
	DWORD    cbSecurityDescriptor; // size of security descriptor 
	FILETIME ftLastWriteTime;      // last write time 

	DWORD i, retCode;

	TCHAR  achValue[MAX_VALUE_NAME];
	DWORD cchValue = MAX_VALUE_NAME;

	HKEY hkey = nullptr;
	if (RegOpenKeyEx(root, subkey.c_str(), 0, KEY_READ, &hkey) != ERROR_SUCCESS)
		return 0;

	// Get the class name and the value count. 
	retCode = RegQueryInfoKey(
		hkey,      // key handle 
		achClass,                // buffer for class name 
		&cchClassName,           // size of class string 
		NULL,                    // reserved 
		&cSubKeys,               // number of subkeys 
		&cbMaxSubKey,            // longest subkey size 
		&cchMaxClass,            // longest class string 
		&cValues,                // number of values for this key 
		&cchMaxValue,            // longest value name 
		&cbMaxValueData,         // longest value data 
		&cbSecurityDescriptor,   // security descriptor 
		&ftLastWriteTime);       // last write time 


	// Enumerate the subkeys, until RegEnumKeyEx fails.

	if (cSubKeys)
	{

		for (i = 0; i < cSubKeys; i++)
		{
			cbName = MAX_KEY_LENGTH;
			retCode = RegEnumKeyEx(hkey, i,
				achKey,
				&cbName,
				NULL,
				NULL,
				NULL,
				&ftLastWriteTime);
			if (retCode == ERROR_SUCCESS)
			{
				found_count += EnumKey(root, subkey + std::wstring((TCHAR*)achKey) + L"\\");
			}
		}
	}


	// Enumerate the key values. 



	ULONG TotalSize = 0;

	if (cValues && cbMaxValueData)
	{
		//printf("\nNumber of values: %d\n", cValues);
		BYTE* buffer = new BYTE[cbMaxValueData];
		ZeroMemory(buffer, cbMaxValueData);

		for (i = 0, retCode = ERROR_SUCCESS; i < cValues; i++)
		{
			cchValue = MAX_VALUE_NAME;
			achValue[0] = '\0';
			retCode = RegEnumValue(hkey, i,
				achValue,
				&cchValue,
				NULL,
				NULL,
				NULL,
				NULL);

			if (retCode == ERROR_SUCCESS)
			{

				DWORD lpData = cbMaxValueData;
				buffer[0] = '\0';
				DWORD ValueType;
				LONG dwRes = RegQueryValueEx(hkey, achValue, 0, &ValueType, buffer, &lpData);
				if (ValueType == REG_BINARY)
				{
					TotalSize += lpData;
				}
				//_tprintf(TEXT("(%d) %s : %s\n"), i + 1, achValue, buffer);
			}
		}

		delete[] buffer;
	}

	if (TotalSize >= 5000)
	{
		std::wcout << L"Suspicious registry BLOB has been seen: '" << subkey << L"'.\n";
		found_count++;

	}

	return found_count;
}


int Enumerate_REG_BLOB(IOC& reg) {

	std::string tmp = reg.reg_key.get_key();
	return EnumKey(reg.reg_key.get_root(), std::wstring( tmp.begin(),tmp.end() ) );

}


bool GetProcessFromPID(DWORD pid, PWCHAR out_process_name, DWORD out_process_size)
{
	HANDLE HSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (HSnap) {
		PROCESSENTRY32 pe32;
		pe32.dwSize = sizeof(PROCESSENTRY32);
		if (Process32First(HSnap, &pe32)) {
			do {
				if (pe32.th32ProcessID == pid)
				{
					wcscpy_s(out_process_name, out_process_size, pe32.szExeFile);
					CloseHandle(HSnap);
					return true;
				}
			} while (Process32Next(HSnap, &pe32));
		}
		CloseHandle(HSnap);
	}
	return false;
}



int ParentProcessValidity(PWCHAR process_name, std::vector<PWCHAR> parent_names)
{
	HANDLE HSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32 process_entry = { 0 };
	process_entry.dwSize = sizeof(PROCESSENTRY32);
	int ioc_count = 0;


	if (!Process32First(HSnap, &process_entry))
	{
		CloseHandle(HSnap);
		return false;
	}
	do {
		if (wcscmp(process_entry.szExeFile, process_name) == 0) {
			WCHAR parent_p_name[512] = { 0 };
			bool parent_process_exists = GetProcessFromPID(process_entry.th32ParentProcessID, parent_p_name, 512);
			if ( !parent_process_exists || ( std::find(parent_names.begin(),parent_names.end(), std::wstring(parent_p_name)) == parent_names.end() )  )
			{
				process_entry.th32ProcessID;
				std::wcout << L"Suspicious process '"<< std::wstring(process_name) << L"' ( pid: " << process_entry.th32ProcessID << L" ) with parent process id:" <<
								process_entry.th32ParentProcessID <<" has been seen\n" ;

				ioc_count++;
			}
		}
	} while (Process32Next(HSnap, &process_entry));
	CloseHandle(HSnap);


	return ioc_count;
}


int CheckParentSvchost(IOC& ioc)
{
	return ParentProcessValidity((PWCHAR)L"svchost.exe", { (PWCHAR)L"services.exe",(PWCHAR)L"MsMpEng.exe" });
}


int CheckDLLSideLoadFiles(IOC& ioc)
{
	int ioc_count = 0;

	for (auto& p : fs::recursive_directory_iterator(ioc.ioc_val, fs::directory_options::skip_permission_denied) )
	{
		if (p.is_directory() && std::wstring(p.path()).find(L"WinSxS") == std::wstring::npos )
		{
			bool EXE_exist = false, DLL_exist = false, Payload_exist = false;
			std::wstring SideLoad;
			for (auto& p2 : fs::directory_iterator(p.path(),fs::directory_options::skip_permission_denied) ) 
			{
				if (p2.is_directory() || (EXE_exist && DLL_exist && Payload_exist))
				{
					Payload_exist = false;
					break;
				}

				if ( icompare(p2.path().extension(), L".exe") )
				{
					if (EXE_exist)
						break;
					EXE_exist = true;
				}
				else if (icompare(p2.path().extension(), L".dll") )
				{
					if (DLL_exist)
						break;
					if (Payload_exist )
					{
						if (icompare(p2.path().stem(), SideLoad))
							DLL_exist = true;
						else
							break;
					}
					else
					{
						SideLoad = p2.path().stem();
						DLL_exist = true;
					}
				}
				else if ( !icompare(p2.path().extension(), L".dll") && !icompare(p2.path().extension(), L".exe") )
				{
					if (Payload_exist)
						break;
					if(DLL_exist)
					{
						if (icompare(p2.path().stem(),SideLoad) )
							Payload_exist = true;
						else
							break;
					}
					else
					{
						SideLoad = p2.path().stem();
						Payload_exist = true;
					}

				}
		
			}

			if (EXE_exist && DLL_exist && Payload_exist)
			{
				ioc_count++;
				std::cout << "Path: '" << p.path() << "' contains suspicious files" << std::endl;
			}
		}

	}

	return ioc_count;
}


class Infection {


public:
	Infection(std::vector<IOC> tmp) {
		IOCs = tmp;
		found = 0;
		warns = 0;
		not_cleaned = 0;
		restart = false;
	};

	int get_warn_count() {
		return warns;
	}

	int CheckForInfection()
	{
		for (int i = 0; i < IOCs.size(); i++)
		{
			if (int ret = IOCs[i].detection_handler(IOCs[i]))
			{
				IOCs[i].state |= EXIST;
				if (IOCs[i].state & WARNING)
					warns += ret;
				else
					found += ret;
			}
			
		}

		return found;
		
	}

	int Disinfect()
	{
		for (int i = 0; i < IOCs.size(); i++)
		{
			if ((IOCs[i].state & EXIST) && !(IOCs[i].state & WARNING))
			{
				if (int ret = IOCs[i].removal_handler(IOCs[i]))
					not_cleaned += ret;
				else if (IOCs[i].state & NEED_RESTART)
					restart = true;
			}
			
		}

		if (restart) {
			SetConsoleTextAttribute(hConsole, 0x0006|FOREGROUND_INTENSITY);
			std::cout << "\nATTENTION: Disinfection process needs restarting the system. Please RESTART the system and re-run this removal ...\n";
			SetConsoleTextAttribute(hConsole, 0x000f);
		}
		else if (!not_cleaned) {
			SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN);
			std::cout << "System is disinfected\n";
			SetConsoleTextAttribute(hConsole, 0x000f);
		}
		else
		{
			SetConsoleTextAttribute(hConsole, 0x0006);
			std::cout << "Some of the indicators can not be cleaned from the system. Please contact Graph Inc. support\n";
			SetConsoleTextAttribute(hConsole, 0x000f);
		}
		return 0;
	}


private:
	std::vector <IOC> IOCs;
	unsigned int found;
	unsigned int warns;
	unsigned int not_cleaned;
	bool restart;
};







int main()
{
	hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(hConsole, 0x000f);

	std::cout << "###################################################################\n";
	std::cout << "###\t\t" << "APT 27 New Variant Detector & Removal" << "\t\t###\n";
	std::cout << "###\t\t" << "Provided by Graph Inc.(graph-inc.ir)" << "\t\t###\n";
	std::cout << "###\tFor ";
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED);
	std::cout << "URGENT";
	SetConsoleTextAttribute(hConsole, 0x000f);
	std::cout << " Incident Response service requests, call \t###\n";
	std::cout << "###\t\t+ 9821 47 71 6666 (Iran) or send email" << "\t\t###\n";
	std::cout << "###\t\tto mdr@graph-inc.ir(other countries)" << "\t\t###\n";
	
	std::cout << "###################################################################\n";

	std::cout << "Press enter to start detector ...\n";

	std::cin.ignore();


	Infection APT27_lucky(std::vector<IOC>{	IOC(REGISTRY, NOT_EXIST, registry_key("HKLM\\SYSTEM\\CurrentControlSet\\Services\\Wdf02000\\")),
											IOC(REGISTRY, NEED_RESTART, registry_key("HKLM\\SOFTWARE\\Classes\\wds2s2update\\") ),
											IOC(REGISTRY, NOT_EXIST, registry_key("HKLM\\SOFTWARE\\Classes\\vmware_up\\") ),
											IOC(OTHER, WARNING , registry_key("HKLM\\SOFTWARE\\Classes\\"), Enumerate_REG_BLOB,generic_removal),
											IOC(FILE, NOT_EXIST, std::string("C:\\Windows\\Security\\capexex\\")),
											IOC(FILE, NOT_EXIST, std::string("C:\\Windows\\Security\\capex\\")),
											IOC(FILE, NOT_EXIST, std::string("C:\\Windows\\setup\\setup.exe")),
											IOC(FILE, NOT_EXIST, std::string("C:\\Windows\\SchCache\\setup.exe")),
											IOC(FILE, NOT_EXIST, std::string("C:\\Windows\\Speech\\setup.exe")),
											IOC(FILE, WARNING, std::string("C:\\ProgramData\\x.log")),
											IOC(FILE, WARNING, std::string("C:\\ProgramData\\sw.log")),
											IOC(PROCESS, WARNING,CheckParentSvchost, generic_removal),
											IOC(OTHER,WARNING, std::string("C:\\windows\\"), CheckDLLSideLoadFiles, generic_removal)
										}
						);


	std::cout << "Please wait. The detector is searching for malicious signs...\n\n";
	if ( APT27_lucky.CheckForInfection() )
	{
		SetConsoleTextAttribute(hConsole, FOREGROUND_RED);
		std::cout << "System is Infected\n";
		SetConsoleTextAttribute(hConsole, 0x000f);
		std::cout << "Press enter to continute to disinfect ...\n";

		std::cin.ignore();

		APT27_lucky.Disinfect();
	}
	
	if (APT27_lucky.get_warn_count()) {
		SetConsoleTextAttribute(hConsole, 0x0006);
		std::cout << "\nPossible suspicious indecators have been detected. Please contact Graph for further confirmation.\n";
		SetConsoleTextAttribute(hConsole, 0x000f);
	}
	else
	{
		SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN);
		std::cout << "\nSystem is not Infected\n";
		SetConsoleTextAttribute(hConsole, 0x000f);
	}

	std::cout << "\nPress enter to exit ...";
	std::cin.ignore();
	
	return 0;
}

