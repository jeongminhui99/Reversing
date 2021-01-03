#include <stdio.h>
#include <Windows.h>

typedef UINT(WINAPI* WINEXEC)(LPCSTR, UINT);  //함수 포인터


//Thread Parameter
typedef struct _THREAD_PARAM
{
	FARPROC pFunc[2]; //LoadLibraryA(), GetProcAddress()
	char szBuf[4][128]; //"user32.dll", "MessageBoxA",
						// "www.reversecore.com", "ReverseCore"
} THREAD_PARAM, *PTHREAD_PARAM;

// LoadLibraryA()
typedef HMODULE(WINAPI* PFLOADLIBRARYA)
(
	LPCSTR lpLibFileName
);

//GetProcAddress()
typedef FARPROC(WINAPI* PFGETPROCADDRESS)
(
	HMODULE hModule,
	LPCSTR lpProcName
);

//MessageBoxA()
typedef int (WINAPI* PFMESSAGEBOXA)
(
	HWND hWnd,
	LPCSTR lpText,
	LPCSTR lpCaption,
	UINT uType
);


//Thread Procedure
//ThreadProc()함수는 THREAD_PARAM 구조체를 이용해서 2개의 API 주소와 4개의 문자열 데이터를 받아들인다.
// * 2개의 API 주소 : LoadLibraryA(), GetProcAddress()
// dl 2개의 API주소만 있으면 모든 라이브러리의 함수를 호출할 수 있다.
DWORD WINAPI ThreadProc(LPVOID lParam)
{
	PTHREAD_PARAM pParam = (PTHREAD_PARAM)lParam;
	HMODULE hMod = NULL;
	FARPROC pFunc = NULL;

	//LoadLibrary("user32.dll")
	//pParam->pFunc[0] -> kernel32!LoadLibraryA()
	//pParam->szBuf[0] -> "user32.dll"
	hMod = ((PFLOADLIBRARYA)pParam->pFunc[0])(pParam->szBuf[0]);

	// GetProcAddress("MessageBoxA")
	// pParam->pFunc[1] -> kernel32!GetProcAddress()
	// pParam->szBuf[1] -> "MessageBoxA"
	pFunc = (FARPROC)((PFGETPROCADDRESS)pParam->pFunc[1])(hMod, pParam->szBuf[1]);

	//MessageBoxA(NULL, "www.reversecore.com", "ReverseCore", MB_OK)
	//pParam->szBuf[2]->"www.reversecore.com"
	//pParam->szBuf[3]->"ReverseCore"
	((PFMESSAGEBOXA)pFunc)(NULL, pParam->szBuf[2], pParam->szBuf[3], MB_OK);

	return 0;
}

//Code 인젝션 기법의 핵심은 독립 실행 코드를 인젝션하는 것이다. 그러기 위해서는 코드와 데이터를 같이 
//인젝션해야 하는데 이때 인젝션하는 코드에서 인젝션한 데이터를 정확히 참조할 수 있어야 한다.
// 위의 코드들을 보면 전부 스레드 파라미터로 넘어온 THREAD_PARAM구조체에서 가져다 사용하고 있다.

void AfterFunc() {};

BOOL InjectCode(DWORD dwPID)
{
	//THREAD_PARAM 구조체 변수 세팅
	//이 값들은 상대방 프로세스에 인젝션되어 ThreadProc()스레드 함수에 파라미터로 전달될 것이다.
	HMODULE hMod = NULL;
	THREAD_PARAM param = { 0, };
	HANDLE hProcess = NULL;
	HANDLE hThread = NULL;
	LPVOID pRemoteBuf[2] = { 0, };
	DWORD dwSize = 0;
	
	hMod = GetModuleHandleA("kernel32.dll");

	// set THREAD_PARAM
	param.pFunc[0] = GetProcAddress(hMod, "LoadLibraryA");
	param.pFunc[1] = GetProcAddress(hMod, "GetProcAddress");
	printf("%x", param.pFunc);
	strcpy(param.szBuf[0], "user32.dll");
	strcpy(param.szBuf[1], "MessageBoxA");
	strcpy(param.szBuf[2], "www.reversecore.com");
	strcpy(param.szBuf[3], "ReverseCore");

	//아래 코드의 핵심은 상대방 프로세스에 data와 code를 각각 메모리 할당하고 인젝션해준다는 것이다.

	// Open Process
	hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwPID);

	// Allocation for THREAD_PARAM
	dwSize = sizeof(THREAD_PARAM);
	pRemoteBuf[0] = VirtualAllocEx(hProcess, NULL, dwSize, MEM_COMMIT, PAGE_READWRITE);


	WriteProcessMemory(hProcess, // hProcess
		pRemoteBuf[0], //lpBaseAddress
		(LPVOID)&param, //lpBuffer
		dwSize, //nSize
		NULL); //[out]
				// lpNumberOfBytesWritten

	//Allocation for ThreadProc()
	dwSize = (DWORD)InjectCode - (DWORD)ThreadProc;
	pRemoteBuf[1] = VirtualAllocEx(hProcess, //hProcess
		NULL, // lpAddress
		dwSize, // dwSize
		MEM_COMMIT, //flAllocationType
		PAGE_EXECUTE_READWRITE); // flProtect

	WriteProcessMemory(hProcess, //hProcess
		pRemoteBuf[1], //lpBaseAddress
		(LPVOID)ThreadProc, //lpBuffer
		dwSize, //nSize
		NULL); //[out]
	           //lpNumberOfBytesWritten

	//CreateRemoteThread() API를 이용해서 원격 스레드를 실행시킨다.
	hThread = CreateRemoteThread(hProcess, //hProcess
		NULL,  //lpThreadAttributes
		0,     //dwStackSize
		(LPTHREAD_START_ROUTINE)pRemoteBuf[1], 
		pRemoteBuf[0],  // lpParameter
		0, //dwCreationFlags
		NULL); //lpThreadId

	WaitForSingleObject(hThread, INFINITE);

	CloseHandle(hThread);
	CloseHandle(hProcess);

	return TRUE;
}

int main(int argc, char* argv[])
{
	DWORD dwPID = 0;
	int getPID = 0;

	printf("PID :");
	scanf("%d", &getPID);
	dwPID = (DWORD)getPID;
	InjectCode(dwPID); // InjectCode함수 호출

	return 0;
}

