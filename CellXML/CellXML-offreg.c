/*
Copyright 2015 Thomas Laurenson
thomaslaurenson.com

This file is part of HiveXML.

HiveXML is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 2.1 of the License, or
(at your option) any later version.

HiveXML is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with HiveXML.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include "offreg.h"
#pragma comment (lib, "offreg.lib")

// ----------------------------------------------------------------------
// Definition for QWORD (not yet defined globally in WinDef.h)
// ----------------------------------------------------------------------
#ifndef QWORD
typedef unsigned __int64 QWORD, NEAR *PQWORD, FAR *LPQWORD;
#endif

// ----------------------------------------------------------------------
// Set up program heap
// ----------------------------------------------------------------------
#define USEHEAPALLOC_DANGER
#ifdef USEHEAPALLOC_DANGER
HANDLE hHeap;
#define MYALLOC(x)  HeapAlloc(hHeap,0,x)
#define MYALLOC0(x) HeapAlloc(hHeap,8,x)
#define MYFREE(x)   HeapFree(hHeap,0,x)
#else
#define MYALLOC(x)  GlobalAlloc(GMEM_FIXED,x)
#define MYALLOC0(x) GlobalAlloc(GPTR,x)
#define MYFREE(x)   GlobalFree(x)
#endif

// ----------------------------------------------------------------------
// WinHiveXML functions
// ----------------------------------------------------------------------
VOID printHelpMenu();
int EnumerateKeys(ORHKEY OffKey, LPWSTR szKeyName);
LPTSTR GetValueDataType(DWORD nTypeCode);
LPTSTR ParseValueData(LPBYTE lpData, PDWORD lpcbData, DWORD nTypeCode);
LPTSTR TransformValueData(LPBYTE lpData, PDWORD lpcbData, DWORD nConversionType);
size_t AdjustBuffer(LPVOID *lpBuffer, size_t nCurrentSize, size_t nWantedSize, size_t nAlign);
LPTSTR determineRootKey(LPTSTR lpszHiveFileName);

// ----------------------------------------------------------------------
// WinHiveXML global variables
// ----------------------------------------------------------------------
#define MAX_KEY_NAME 255		// Maximum length for Registry key name
#define MAX_VALUE_NAME 16383	// Maximum length for Registry value name
#define MAX_DATA 1024000		// Maximum length for Registry value data
HANDLE hHeap;					// HiveXML heap
LPTSTR lpStringBuffer;
size_t nStringBufferSize;

//-----------------------------------------------------------------
// CellXML wmain function
//-----------------------------------------------------------------
int wmain(DWORD argc, TCHAR *argv[])
{
	hHeap = GetProcessHeap();
	ORHKEY OffHive;
	LPTSTR HiveFileName;
	LPTSTR HiveRootKey;
	BOOL tryGetRootKey = FALSE;
	BOOL userSuppliedRootKey = FALSE;

	//-----------------------------------------------------------------
	// Parse command line arguments
	//-----------------------------------------------------------------
	if (argc > 1)
	{
		// Print help menu if -h, --help or /?
		if ((_tcscmp(argv[1], _T("-h")) == 0) ||
			(_tcscmp(argv[1], _T("--help")) == 0) ||
			(_tcscmp(argv[1], _T("/?")) == 0))
		{
			// Print help menu
			printHelpMenu();
			return 0;
		}

		// Scan the command line arguments and set booleans
		for (DWORD i = 0; i < argc; i++)
		{
			// Determine rootkey fetching method
			if (_tcscmp(argv[i], _T("-a")) == 0) {
				tryGetRootKey = TRUE;
			}
			if (_tcscmp(argv[i], _T("-r")) == 0) {
				userSuppliedRootKey = TRUE;
			}
		}
	}
	else
	{
		// Print help menu
		printHelpMenu();
		return 0;
	}

	// PROCESSING STARTS HERE

	// Find the Registry hive file (should be the last argument)
	HiveFileName = MYALLOC0(100 * sizeof(TCHAR));
	_tcscat_s(HiveFileName, 100, argv[argc - 1]);

	// Check if the user supplied Registry hive file exists
	if (GetFileAttributesW(HiveFileName) == INVALID_FILE_ATTRIBUTES) {
		printf("\n>>> ERROR: File appears to not exist. Check file input...\n");
		printf("  > System error code: %d\n", GetLastError());
		return -1;
	}

	// Check if we have a valid Registry hive file
	// Use OROpenHive to check, then close hive if no errors
	if (OROpenHive(HiveFileName, &OffHive) != ERROR_SUCCESS) {
		printf("\n>>> ERROR: Cannot open or validate Registry hive...\n");
		printf("  > System error code: %d\n", GetLastError());
		return -1;
	}
	else {
		ORCloseHive(OffHive);
	}

	// Determine how we are going to get the rootkey
	if (tryGetRootKey) {
		// Try an automatically determine the hive root key
		HiveRootKey = determineRootKey(HiveFileName);
	}
	else if (userSuppliedRootKey) {
		// The root key has been specified by "-r"
		HiveRootKey = argv[2];
	}
	else
	{
		// Use the filename provided
		LPTSTR lpszBaseName;
		LPTSTR lpszFileExt;
		lpszBaseName = MYALLOC0(MAX_PATH * sizeof(TCHAR));
		lpszFileExt = MYALLOC0(MAX_PATH * sizeof(TCHAR));
		_tsplitpath_s(HiveFileName,
			NULL, 0,
			NULL, 0,
			lpszBaseName, MAX_PATH,
			lpszFileExt, MAX_PATH);
		HiveRootKey = MYALLOC0(MAX_PATH * sizeof(TCHAR));
		_tcscat_s(HiveRootKey, MAX_PATH, lpszBaseName);
		_tcscat_s(HiveRootKey, MAX_PATH, lpszFileExt);
	}

	// Open the offline Registry hive file
	// No error check needed, it has already been validated
	OROpenHive(HiveFileName, &OffHive);

	// Print CellXML header
	printf("<?xml version = '1.0' encoding = 'UTF-8'?>\n");
	printf("<hive>\n");

	// Start enumerating the first Registry root key
	// This function is recursive and will enumerate all subkeys and values
	EnumerateKeys(OffHive, HiveRootKey);

	// Close hive XML element
	printf("</hive>\n");

	// All done! Exit.
	return 0;
}


//-----------------------------------------------------------------
// Print the help menu
//-----------------------------------------------------------------
VOID printHelpMenu()
{
	PDWORD pdwMajorVersion;
	PDWORD pdwMinorVersion;
	pdwMajorVersion = MYALLOC(sizeof(DWORD));
	pdwMinorVersion = MYALLOC(sizeof(DWORD));
	// Get offreg.dll version (usually 1.0, but might change in future)
	ORGetVersion(pdwMajorVersion, pdwMinorVersion);

	// Print the help menu banner
	printf("\n_________        .__  .__    ____  ___  _____  .____     ");
	printf("\n\\_   ___ \\  ____ |  | |  |   \\   \\/  / /     \\ |    |    ");
	printf("\n/    \\  \\/_/ __ \\|  | |  |    \\     / /  \\ /  \\|    |    ");
	printf("\n\\     \\___\\  ___/|  |_|  |__  /     \\/    |    \\    |___ ");
	printf("\n \\______  /\\___  >____/____/ /___/\\  \\____|__  /_______ \\");
	printf("\n        \\/     \\/                  \\_/       \\/        \\/");
	printf("\n                                     By Thomas Laurenson");
	printf("\n                                     thomaslaurenson.com");
	printf("\n                                     CellXML version 1.1.0");
	printf("\n                                     offreg.dll version %d.%d\n\n\n", *pdwMajorVersion, *pdwMinorVersion);

	// Print help menu text
	printf("Description: CellXML.exe is a program which parses a Windows Registry hive\n");
	printf("             file and converts the data to XML format. The XML structure \n");
	printf("             is modelled after the RegXML project.\n\n");
	printf("      Usage: CellXML.exe [options] hive-file\n\n");
	printf("   Examples: 1) Print Registry hive file to standard output (stdout):\n");
	printf("                 CellXML.exe hive-file\n");
	printf("             2) Manually specify the hive root key:\n");
	printf("                 CellXML.exe -r $$$PROTO.HIV hive-file\n");
	printf("             3) Automatically determine hive root key:\n");
	printf("                 CellXML.exe -a hive-file\n");
	printf("             4) Direct standard output to an XML file:\n");
	printf("                 CellXML.exe hive-file > output.xml\n\n");
}


//-----------------------------------------------------------------
// Enumerate Keys function to iterate over every Registry key
// in a offline Registry hive file
//-----------------------------------------------------------------
int EnumerateKeys(ORHKEY OffKey, LPWSTR szKeyName)
{
	DWORD	nSubkeys;
	DWORD	nValues;
	DWORD	nSize;
	DWORD	dwType;
	DWORD	cbData;
	ORHKEY	OffKeyNext;
	WCHAR	szValue[MAX_VALUE_NAME];
	WCHAR	szSubKey[MAX_KEY_NAME];
	WCHAR	szNextKey[MAX_KEY_NAME];
	WCHAR	szAll[MAX_KEY_NAME + MAX_VALUE_NAME];
	DWORD	i;
	PFILETIME lpftLastWriteTime;

	// Allocated space for the Registry key last write time
	lpftLastWriteTime = MYALLOC(sizeof(FILETIME));

	// Query the key, determine the number of keys, values and the key's last write time
	if (ORQueryInfoKey(OffKey, NULL, NULL, &nSubkeys,
		NULL, NULL, &nValues, NULL,
		NULL, NULL, lpftLastWriteTime) != ERROR_SUCCESS)
	{
		return 0;
	}

	// Convert the key's last write time to a SYSTEMTIME (for printing purposes)
	SYSTEMTIME st;
	LPTSTR lpszModifiedTime;
	FileTimeToSystemTime(lpftLastWriteTime, &st);
	lpszModifiedTime = MYALLOC0(21 * sizeof(TCHAR));
	_sntprintf_s(lpszModifiedTime, 21 + 1 * sizeof(TCHAR), 21, TEXT("%i-%02i-%02iT%02i:%02i:%02iZ"),
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

	// We have all the Registry key details, write out using DFXML/RegXML syntax
	printf("  <cellobject>\n");
	printf("    <cellpath>%ws</cellpath>\n", szKeyName);
	printf("    <name_type>k</name_type>\n");
	printf("    <mtime>%ws</mtime>\n", lpszModifiedTime);
	printf("    <alloc>1</alloc>\n");
	printf("  </cellobject>\n");

	// Free allocated memory for last write time
	//MYFREE(lpszModifiedTime);

	// Loop through each of the Registry key's values
	for (i = 0; i < nValues; i++)
	{
		memset(szValue, 0, sizeof(szValue));
		nSize = MAX_VALUE_NAME;
		dwType = 0;
		cbData = 0;

		// Enumerate the Registry Valye, get the name and data size
		if (OREnumValue(OffKey, i, szValue, &nSize,
			&dwType, NULL, &cbData) != ERROR_MORE_DATA)
		{
			continue;
		}

		// Allocate some memory to store the Registry value name
		LPBYTE pData;
		pData = MYALLOC0(cbData + 2);
		if (!pData) {
			continue;
		}
		memset(pData, 0, cbData + 2);

		// Fetch the Registry value name, data type and data 
		if (OREnumValue(OffKey,
			i,
			szValue,
			&nSize,
			&dwType,
			pData,
			&cbData) != ERROR_SUCCESS)
		{
			MYFREE(pData);
			continue;
		}

		// Determine Registry value name including parent key
		if (nSize == 0) {
			swprintf(szAll, MAX_KEY_NAME + MAX_VALUE_NAME, L"%s\\%s", szKeyName, TEXT("(Default)"));
		}
		else {
			swprintf(szAll, MAX_KEY_NAME + MAX_VALUE_NAME, L"%s\\%s", szKeyName, szValue);
		}

		// Determine Registry value data type
		LPTSTR lpszDataType;
		lpszDataType = GetValueDataType(dwType);

		// Determine Registry value data
		LPTSTR lpszValueData;
		lpszValueData = ParseValueData(pData, &cbData, dwType);

		LPTSTR lpszRawValueData;
		lpszRawValueData = ParseValueData(pData, &cbData, REG_BINARY);

		// We have all the Registry value details, write out using DFXML/RegXML syntax
		printf("  <cellobject>\n");
		printf("    <cellpath>%ws</cellpath>\n", szAll);
		if (nSize == 0) {
			printf("    <basename>%ws</basename>\n", TEXT("(Default)"));
		}
		else {
			printf("    <basename>%ws</basename>\n", szValue);
		}
		printf("    <name_type>v</name_type>\n");
		printf("    <mtime>%ws</mtime>\n", lpszModifiedTime);
		printf("    <alloc>1</alloc>\n");
		printf("    <data_type>%ws</data_type>\n", lpszDataType);
		printf("    <data>%ws</data>\n", lpszValueData);
		printf("    <raw_data>%ws</raw_data>\n", lpszRawValueData);
		printf("  </cellobject>\n");

		// Free the Registry value data
		MYFREE(pData);
	}

	// Now loop over each of the Registry key's subkeys
	// This is recursive, EmumerateKeys will be called recursively
	for (i = 0; i<nSubkeys; i++)
	{
		memset(szSubKey, 0, sizeof(szSubKey));
		nSize = MAX_KEY_NAME;

		// Fetch the subkey name
		if (OREnumKey(OffKey, i, szSubKey, &nSize,
			NULL, NULL, NULL) != ERROR_SUCCESS)
		{
			continue;
		}

		swprintf(szNextKey, MAX_KEY_NAME, L"%s\\%s", szKeyName, szSubKey);

		// Open the subkey
		if (OROpenKey(OffKey, szSubKey, &OffKeyNext)
			== ERROR_SUCCESS)
		{
			EnumerateKeys(OffKeyNext, szNextKey);
			ORCloseKey(OffKeyNext);
		}
	}

	// All done!
	return 0;
}

// ----------------------------------------------------------------------
// Determine the Registry value type (e.g., REG_SZ, REG_BINARY) and return 
// ----------------------------------------------------------------------
LPTSTR GetValueDataType(DWORD nTypeCode)
{
	LPTSTR lpszDataType;
	lpszDataType = MYALLOC0(40 * sizeof(wchar_t));
	if (nTypeCode == REG_NONE) { lpszDataType = TEXT("REG_NONE"); }
	else if (nTypeCode == REG_SZ) { lpszDataType = TEXT("REG_SZ"); }
	else if (nTypeCode == REG_EXPAND_SZ) { lpszDataType = TEXT("REG_EXPAND_SZ"); }
	else if (nTypeCode == REG_BINARY) { lpszDataType = TEXT("REG_BINARY"); }
	else if (nTypeCode == REG_DWORD) { lpszDataType = TEXT("REG_DWORD"); }
	else if (nTypeCode == REG_DWORD_BIG_ENDIAN) { lpszDataType = TEXT("REG_DWORD_BIG_ENDIAN"); }
	else if (nTypeCode == REG_LINK) { lpszDataType = TEXT("REG_LINK"); }
	else if (nTypeCode == REG_MULTI_SZ) { lpszDataType = TEXT("REG_MULTI_SZ"); }
	else if (nTypeCode == REG_RESOURCE_LIST) { lpszDataType = TEXT("REG_RESOURCE_LIST"); }
	else if (nTypeCode == REG_FULL_RESOURCE_DESCRIPTOR) { lpszDataType = TEXT("REG_FULL_RESOURCE_DESCRIPTOR"); }
	else if (nTypeCode == REG_RESOURCE_REQUIREMENTS_LIST) { lpszDataType = TEXT("REG_RESOURCE_REQUIREMENTS_LIST"); }
	else if (nTypeCode == REG_QWORD) { lpszDataType = TEXT("REG_QWORD"); }
	return lpszDataType;
}

// ----------------------------------------------------------------------
// Parse Registry value data
// Return a string (based on data type) that can be printed
// ----------------------------------------------------------------------
LPTSTR ParseValueData(LPBYTE lpData, PDWORD lpcbData, DWORD nTypeCode)
{
	LPTSTR lpszValueData;
	lpszValueData = NULL;

	// Check if the Registry value data is NULL, else process the Registry value data
	if (NULL == lpData)
	{
		lpszValueData = TransformValueData(lpData, lpcbData, REG_BINARY);
	}
	else
	{
		DWORD cbData;
		size_t cchMax;
		size_t cchActual;
		cbData = *lpcbData;
		switch (nTypeCode) {

			// Process the three different string types (normal, expanded and multi)
		case REG_SZ:
		case REG_EXPAND_SZ:
		case REG_MULTI_SZ:
			// We need to do a check for hidden bytes after string[s]
			cchMax = cbData / sizeof(TCHAR);
			if (REG_MULTI_SZ == nTypeCode)
			{
				// Do a search for double NULL chars (REG_MULTI_SZ ends with \0\0)
				for (cchActual = 0; cchActual < cchMax; cchActual++)
				{
					if (0 != ((LPTSTR)lpData)[cchActual]) {
						continue;
					}
					cchActual++;
					// Special case check for incorrectly terminated string
					if (cchActual >= cchMax) {
						break;
					}
					if (0 != ((LPTSTR)lpData)[cchActual]) {
						continue;
					}
					// Found a double NULL terminated string
					cchActual++;
					break;
				}
			}
			else
			{
				// Must be a REG_SZ or REG_EXPAND_SZ
				cchActual = _tcsnlen((LPTSTR)lpData, cchMax);
				if (cchActual < cchMax) {
					cchActual++;  // Account for NULL character
				}
			}

			// Do a size check; actual size VS specified size
			if ((cchActual * sizeof(TCHAR)) == cbData) {
				// If determined size is the same as specified size
				// process using the specified Registry value type
				lpszValueData = TransformValueData(lpData, lpcbData, nTypeCode);
			}
			else
			{
				// Else process the string as REG_BINARY (binary)
				lpszValueData = TransformValueData(lpData, lpcbData, REG_BINARY);
			}
			break;

			// Process DWORD types
		case REG_DWORD_LITTLE_ENDIAN:
		case REG_DWORD_BIG_ENDIAN:
			// Do a size check; DWORD size VS specified size
			if (sizeof(DWORD) == cbData)
			{
				// If the size of the value data is the same as a DWORD size
				// process using the specified Registry value type
				lpszValueData = TransformValueData(lpData, lpcbData, nTypeCode);
			}
			else
			{
				// Else process the string as REG_BINARY (binary)
				lpszValueData = TransformValueData(lpData, lpcbData, REG_BINARY);
			}
			break;

			// Process QWORD types (currently only little endian)
		case REG_QWORD_LITTLE_ENDIAN:
			// If the size if the value data is the same as a QWORD size
			// process using the specified Registry value type
			if (sizeof(QWORD) == cbData)
			{
				// If the size of the value data is the same as a QWORD size
				// process using the specified Registry value type
				lpszValueData = TransformValueData(lpData, lpcbData, nTypeCode);
			}
			else
			{
				// Else process the string as REG_BINARY (binary)
				lpszValueData = TransformValueData(lpData, lpcbData, REG_BINARY);
			}
			break;

			// Process any other Registry value data type as REG_BINARY (binary)
		default:
			lpszValueData = TransformValueData(lpData, lpcbData, REG_BINARY);
		}
	}
	return lpszValueData;
}


// ----------------------------------------------------------------------
// Transform Registry value data based on data_type
// ----------------------------------------------------------------------
LPTSTR TransformValueData(LPBYTE lpData, PDWORD lpcbData, DWORD nConversionType)
{
	LPTSTR lpszValueDataIsNULL = TEXT("NULL");
	LPTSTR lpszValueData;
	LPDWORD lpDword;
	LPQWORD lpQword;
	DWORD nDwordCpu;

	lpszValueData = NULL;
	lpDword = NULL;
	lpQword = NULL;
	lpStringBuffer = NULL;

	if (NULL == lpData)
	{
		lpszValueData = MYALLOC((_tcslen(lpszValueDataIsNULL) + 1) * sizeof(TCHAR));
		//_tcscpy(lpszValueData, lpszValueDataIsNULL);
		_tcscpy_s(lpszValueData, (_tcslen(lpszValueDataIsNULL) + 1) * sizeof(TCHAR), lpszValueDataIsNULL);
	}
	else
	{
		DWORD cbData;
		DWORD ibCurrent;
		size_t cchToGo;
		size_t cchString;
		size_t cchActual;
		LPTSTR lpszSrc;
		LPTSTR lpszDst;

		cbData = *lpcbData;

		switch (nConversionType) {
		case REG_SZ:
			// A normal NULL termination string
			lpszValueData = MYALLOC0(sizeof(TCHAR) + cbData);
			if (NULL != lpData) {
				memcpy(lpszValueData, lpData, cbData);
			}
			break;

		case REG_EXPAND_SZ:
			// A NULL terminated string that contains unexpanded references to environment variables (e.g., "%PATH%")
			// Process and output in the following format: "<string>"\0
			lpszValueData = MYALLOC0(sizeof(TCHAR) + cbData);
			if (NULL != lpData) {
				memcpy(lpszValueData, lpData, cbData);
			}
			break;

		case REG_MULTI_SZ:
			// A sequence of null-terminated strings, terminated by an empty string (\0).
			// Process and output in the following format: "<string>", "<string>", "<string>", ...\0
			nStringBufferSize = AdjustBuffer(&lpStringBuffer, nStringBufferSize, 10 + (2 * cbData), 1024);
			ZeroMemory(lpStringBuffer, nStringBufferSize);
			lpszDst = lpStringBuffer;

			cchActual = 0;
			// Only process if the actual value data is not NULL
			if (NULL != lpData)
			{
				lpszSrc = (LPTSTR)lpData;
				// Convert the byte count to a char count
				cchToGo = cbData / sizeof(TCHAR);
				while ((cchToGo > 0) && (*lpszSrc)) {
					if (0 != cchActual)
					{
						// Add comma (",") to separate strings
						//_tcscpy(lpszDst, TEXT(","));
						_tcscpy_s(lpszDst, 2 * sizeof(TCHAR), TEXT(","));
						// Increase the count by 1 to account for comma
						lpszDst += 1;
						cchActual += 1;
					}
					cchString = _tcsnlen(lpszSrc, cchToGo);
					_tcsncpy(lpszDst, lpszSrc, cchString);
					lpszDst += cchString;
					cchActual += cchString;

					// Decrease count for processed, if count ToGo is 0 then we are done
					cchToGo -= cchString;
					if (cchToGo == 0) {
						break;
					}

					// Account for the NULL character
					lpszSrc += cchString + 1;
					cchToGo -= 1;
				}
			}
			cchActual += 3 + 1;

			// Allocate memory for the constructed string and copy to value data string
			lpszValueData = MYALLOC(cchActual * sizeof(TCHAR));
			_tcscpy(lpszValueData, lpStringBuffer);
			//_tcscpy_s(lpszValueData, cchActual * sizeof(TCHAR), lpStringBuffer);

			break;

		case REG_DWORD_BIG_ENDIAN:
			// convert DWORD big endian
			lpDword = &nDwordCpu;
			for (ibCurrent = 0; ibCurrent < sizeof(DWORD); ibCurrent++) {
				((LPBYTE)&nDwordCpu)[ibCurrent] = lpData[sizeof(DWORD) - 1 - ibCurrent];
			}
			// Output format: "0xXXXXXXXX\0"
			lpszValueData = MYALLOC0((3 + 8 + 1) * sizeof(TCHAR));
			if (NULL != lpData) {
				//_sntprintf(lpszValueData, (3 + 8 + 1), TEXT("0x%08X\0"), *lpDword);
				_sntprintf_s(lpszValueData, (3 + 8 + 1) * sizeof(TCHAR), (3 + 8 + 1), TEXT("0x%08X\0"), *lpDword);
			}
			break;

		case REG_DWORD:
			// A native DWORD that can be displayed as DWORD
			// This includes REG_DWORD_LITTLE_ENDIAN (the same as DWORD)
			if (NULL == lpDword) {
				lpDword = (LPDWORD)lpData;
			}
			// Output format: "0xXXXXXXXX\0"
			lpszValueData = MYALLOC0((2 + 8 + 1) * sizeof(TCHAR));
			if (NULL != lpData) {
				_sntprintf(lpszValueData, (2 + 8 + 1), TEXT("0x%08X\0"), *lpDword);
				//_sntprintf_s(lpszValueData, (2 + 8 + 1) * sizeof(TCHAR), (2 + 8 + 1), TEXT("0x%08X\0"), *lpDword);
			}
			break;

		case REG_QWORD:
			// A native QWORD that can be displayed as QWORD
			// This includes REG_QWORD_LITTLE_ENDIAN (which is the same as QWORD)
			if (NULL == lpQword) {
				lpQword = (LPQWORD)lpData;
			}
			// Output format: "0xXXXXXXXXXXXXXXXX\0"
			lpszValueData = MYALLOC0((3 + 16 + 1) * sizeof(TCHAR));
			if (NULL != lpData) {
				_sntprintf(lpszValueData, (3 + 16 + 1), TEXT("0x%016I64X\0"), *lpQword);
			}
			break;

		default:
			// Default processing and display method: Present value as hex bytes
			// Output format: "[XX][XX]...[XX]\0"
			lpszValueData = MYALLOC0((1 + (cbData * 3) + 1) * sizeof(TCHAR));
			for (ibCurrent = 0; ibCurrent < cbData; ibCurrent++) {
				_sntprintf(lpszValueData + (ibCurrent * 3), 4, TEXT(" %02X\0"), *(lpData + ibCurrent));
			}
			lpszValueData += 1; // Remove the first space character
		}
	}
	return lpszValueData;
}


// ----------------------------------------------------------------------
// Adjust the buffer 
// ----------------------------------------------------------------------
size_t AdjustBuffer(LPVOID *lpBuffer, size_t nCurrentSize, size_t nWantedSize, size_t nAlign)
{
	if (NULL == *lpBuffer) {
		nCurrentSize = 0;
	}

	if (nWantedSize > nCurrentSize) {
		if (NULL != *lpBuffer) {
			MYFREE(*lpBuffer);
			*lpBuffer = NULL;
		}

		if (1 >= nAlign) {
			nCurrentSize = nWantedSize;
		}
		else {
			nCurrentSize = nWantedSize / nAlign;
			nCurrentSize *= nAlign;
			if (nWantedSize > nCurrentSize) {
				nCurrentSize += nAlign;
			}
		}

		*lpBuffer = MYALLOC(nCurrentSize);
	}
	return nCurrentSize;
}

// ----------------------------------------------------------------------
// Attempt to determine the Registry hive root key
// Also perform a variety of structure/magic number checks
// ----------------------------------------------------------------------
LPTSTR determineRootKey(LPTSTR lpszHiveFileName)
{
	DWORD NBW;

	// Open the hive file based on user passed file name
	HANDLE hFileHive = CreateFile(lpszHiveFileName,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	// Get the Hive file magic number: "regf"
	// Exit if this does not look like a hive file
	char szRegMagicNumber[5];
	ReadFile(hFileHive, &szRegMagicNumber, 4 * sizeof(char), &NBW, NULL);
	szRegMagicNumber[4] = (TCHAR)'\0';
	if (strcmp(szRegMagicNumber, "regf") != 0) {
		printf("\n>>> ERROR: Registry hive magic number match error.\n");
		printf("  >        Check input file. Exiting.\n");
		exit(0);
	}

	// Move to the first HBIN (4096 bytes into the file)
	// Get the hbin magic number ("hbin")
	DWORD dwPtr = SetFilePointer(hFileHive, 4096, NULL, FILE_BEGIN);
	char szHbinMagicNumber[5];
	ReadFile(hFileHive, &szHbinMagicNumber, 4 * sizeof(char), &NBW, NULL);
	szHbinMagicNumber[4] = (TCHAR)'\0';
	if (strcmp(szHbinMagicNumber, "hbin") != 0) {
		printf("\n>>> ERROR: Registry hive hbin magic number match error.\n");
		printf("  >        Check input file. Exiting.\n");
		exit(0);
	}

	// Move to the Rootkey length offset (seems to always be 4204 bytes from start)
	DWORD dwPtr2 = SetFilePointer(hFileHive, 4204, NULL, FILE_BEGIN);

	// Determine key name length
	int keyNameLength;
	ReadFile(hFileHive, &keyNameLength, sizeof(DWORD), &NBW, NULL);
	//printf("keyNameLength: %d\n", keyNameLength);

	char chRootKeyName[MAX_KEY_NAME + 1];
	ReadFile(hFileHive, &chRootKeyName, keyNameLength * sizeof(char), &NBW, NULL);
	//printf("chRootKeyName: %s\n", chRootKeyName);

	// Close the file handle
	CloseHandle(hFileHive);

	// Convert to wide string to return
	wchar_t * lpszRootKey = MYALLOC0(keyNameLength * sizeof(wchar_t));
	size_t convertedChars = 0;
	mbstowcs_s(&convertedChars, lpszRootKey, keyNameLength + 1, chRootKeyName, _TRUNCATE);
	//printf("lpszRootKey: %ws\n", lpszRootKey);
	//exit(0);

	return lpszRootKey;
}