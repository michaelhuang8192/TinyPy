#include <windows.h>
#include <stdio.h>
#include "python.h"
#include <direct.h>


const char PY_MAIN_SCRIPT[] = "main.py";
WCHAR gCurAppDir[4096];

struct {
	int showConsole;
	const wchar_t *app;
	const wchar_t *action;
	const wchar_t *appdir;
	const wchar_t *serviceName;
	const wchar_t *serviceArgs;
} gArgCtx;

SERVICE_STATUS_HANDLE gSrvStatusHandle;
SERVICE_STATUS gSrvStatus = {
	SERVICE_WIN32_OWN_PROCESS,
	SERVICE_START_PENDING,
	SERVICE_ACCEPT_STOP,
	NO_ERROR,
	0,
	0,
	0
};
HANDLE gSvcStopEvent;
int gStopPending;

void openConsole();
void setupEnv();
void setupPyPath();
int main();

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	return main();
}

void parseArgs()
{
#define NextArg() (i < argc - 1 ? argv[i + 1] : NULL)

	int argc;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	for(int i = 0; i < argc; i++) {
		LPWSTR arg = argv[i];
		
		if(!wcscmp(L"--app", arg)) {
			gArgCtx.app = NextArg();

		} else if(!wcscmp(L"--appdir", arg)) {
			gArgCtx.appdir = NextArg();

		} else if(!wcscmp(L"--console", arg)) {
			gArgCtx.showConsole = 1;

		} else if(!wcscmp(L"--action", arg)) {
			gArgCtx.action = NextArg();

		} else if(!wcscmp(L"--service-name", arg)) {
			gArgCtx.serviceName = NextArg();

		} else if(!wcscmp(L"--service-args", arg)) {
			gArgCtx.serviceArgs = NextArg();

		} else if(!wcscmp(L"--help", arg)) {
			openConsole();

			printf(
"TinyPy Help\n"
"--app app_type: shell or service, if not set, it runs main.py which is located in appdir.\n"
"--appdir current_working_directory: if not set, the program path is used.\n"
"--console: show console.\n"
"\n"
"for service only:\n"
"--action action_type: install or remove.\n"
"--service-name name: windows service name.\n"
"--service-args arguments: arguments are passed the service instance.\n"
"\n"
"TinyPy.stopPending() returns true when there's a stop request"
"\n"
				);

			getchar();
			exit(0);
		}

	}

#undef NextArg
}

void openConsole()
{
	AllocConsole();
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);
	freopen("CONIN$", "r", stdin);
}

void openFileOutput()
{
	_mkdir("log");
	freopen("log\\output.txt", "w", stdout);
	freopen("log\\error.txt", "w", stderr);
}

static PyObject* PyStopPending(PyObject *self, PyObject *args)
{
	return PyInt_FromLong(gStopPending);
}

static PyMethodDef PySvcMethods[] =
{
	{"stopPending", PyStopPending, METH_NOARGS},
    {NULL, NULL, 0}
};

int runShell()
{
	Py_DontWriteBytecodeFlag++;
	Py_NoSiteFlag++;
    Py_InteractiveFlag++;

	_PyRandom_Init();
	Py_SetProgramName(__argv[0]);
	Py_Initialize();

	openConsole();
	setvbuf(stdout, (char *)NULL, _IONBF, BUFSIZ);

	PySys_SetArgv(__argc, __argv);
	setupPyPath();
	Py_InitModule("TinyPy", PySvcMethods);
	
	fprintf(stderr, "Python %s on %s\n", Py_GetVersion(), Py_GetPlatform());
	int sts = PyRun_AnyFileEx(stdin, "<stdin>", false) != 0;

	Py_Finalize();
	return sts;
}

int runScript()
{
	Py_DontWriteBytecodeFlag++;
	Py_NoSiteFlag++;

	_PyRandom_Init();
	Py_SetProgramName(__argv[0]);
	Py_Initialize();

	if(gArgCtx.showConsole) {
		openConsole();
		setvbuf(stdout, (char *)NULL, _IONBF, BUFSIZ);
	} else {
		openFileOutput();
	}

	PySys_SetArgv(__argc, __argv);
	setupPyPath();
	Py_InitModule("TinyPy", PySvcMethods);

	int sts = -1;
	FILE* fp = fopen(PY_MAIN_SCRIPT, "r");
	if(fp != NULL)
		sts = PyRun_AnyFileEx(fp, PY_MAIN_SCRIPT, true) != 0;

	Py_Finalize();
	return sts;
}

void setSrvStatus(DWORD state, DWORD error)
{
	static int chk_pt = 1;
	gSrvStatus.dwCurrentState = state;
	gSrvStatus.dwCheckPoint = (state == SERVICE_RUNNING || state == SERVICE_STOPPED) ? 0 : chk_pt++;
	gSrvStatus.dwWin32ExitCode = error;

	SetServiceStatus(gSrvStatusHandle, &gSrvStatus);
}

VOID WINAPI srvCtrlHandler(DWORD dwCtrl)
{
	switch(dwCtrl)
	{
	case SERVICE_CONTROL_STOP:
		setSrvStatus(SERVICE_STOP_PENDING, NO_ERROR);
		gStopPending = 1;
		SetEvent(gSvcStopEvent);
		break;
	default: break;
	}
}

VOID WINAPI srvMain(DWORD dwArgc, LPTSTR *lpszArgv)
{
	gSrvStatusHandle = RegisterServiceCtrlHandler(L"", srvCtrlHandler);
	setSrvStatus(SERVICE_START_PENDING, NO_ERROR);

	gSvcStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if(gSvcStopEvent == NULL) {
		setSrvStatus(SERVICE_STOPPED, NO_ERROR);
		return;
	}
	setSrvStatus(SERVICE_RUNNING, NO_ERROR);

	PyGILState_STATE gstate = PyGILState_Ensure();

	FILE* fp = fopen(PY_MAIN_SCRIPT, "r");
	if(fp != NULL)
		PyRun_AnyFileEx(fp, PY_MAIN_SCRIPT, true);

	PyGILState_Release(gstate);

	WaitForSingleObject(gSvcStopEvent, INFINITE);
	setSrvStatus(SERVICE_STOPPED, NO_ERROR);
}

int runService()
{
	Py_DontWriteBytecodeFlag++;
	Py_NoSiteFlag++;

	_PyRandom_Init();
	Py_SetProgramName(__argv[0]);
	Py_Initialize();

	openFileOutput();

	PySys_SetArgv(__argc, __argv);
	setupPyPath();
	Py_InitModule("TinyPy", PySvcMethods);

	Py_BEGIN_ALLOW_THREADS

	SERVICE_TABLE_ENTRY srv_tbl[] = {
		{ L"", srvMain },
		{ 0, 0 }
	};
	StartServiceCtrlDispatcher(srv_tbl);

	Py_END_ALLOW_THREADS

	Py_Finalize();
	return 0;
}

int remove_service()
{
	const wchar_t* srvName = gArgCtx.serviceName != NULL ? gArgCtx.serviceName : L"TinyPyService";
	SERVICE_STATUS status = {};

	SC_HANDLE scmgr = OpenSCManager(0, 0, SC_MANAGER_CONNECT);
	if(!scmgr) {
		MessageBoxA(NULL, "Error: OpenSCManager", "Error", 0);
		return -1;
	}

	SC_HANDLE scsrv = OpenService(scmgr, srvName, SERVICE_STOP|SERVICE_QUERY_STATUS|DELETE);
	if(!scsrv) { 
		MessageBoxA(NULL, "Error: OpenService", "Error", 0);
		goto error_0;
	}

	if (ControlService(scsrv, SERVICE_CONTROL_STOP, &status)) {
		Sleep(1000);
		while(QueryServiceStatus(scsrv, &status) && status.dwCurrentState == SERVICE_STOP_PENDING) Sleep(500);
	} 

	if (!DeleteService(scsrv)) {
		MessageBoxA(NULL, "Error: DeleteService", "Error", 0);
		goto error_1;
	}

	CloseServiceHandle(scsrv);
	CloseServiceHandle(scmgr);
	return 1;

error_1:
	CloseServiceHandle(scsrv);
error_0:
	CloseServiceHandle(scmgr);
	return 0;
}

int install_service()
{
	char buf[512];

	wchar_t ffnz[4096];
	if(!GetModuleFileName(NULL, ffnz, sizeof(ffnz)/sizeof(ffnz[0]))) return -1;
	
	wchar_t path[4096];
	if(swprintf(path, sizeof(path)/sizeof(path[0]), L"%s --app service %s", ffnz,
		gArgCtx.serviceArgs != NULL ? gArgCtx.serviceArgs : L"") < 0)
		return -1;

	SC_HANDLE scmgr = OpenSCManager(0, 0, SC_MANAGER_CONNECT|SC_MANAGER_CREATE_SERVICE);
	if(!scmgr) {
		MessageBoxA(NULL, "Error: OpenSCManager", "Error", 0);
		return -1;
	}

	SC_HANDLE scsrv = CreateService(
		scmgr,
		gArgCtx.serviceName != NULL ? gArgCtx.serviceName : L"TinyPyService",
		0,
        SERVICE_QUERY_STATUS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        path,
        0,
        0,
        0,
        0,
        0
		);
	if(!scsrv) {
		sprintf(buf, "Error: CreateService[%08X]\n", GetLastError());
		MessageBoxA(NULL, buf, "Error", 0);
		CloseServiceHandle(scmgr);
		return -1;
	}

	CloseServiceHandle(scsrv);
	CloseServiceHandle(scmgr);
	return 0;
}

int main()
{
	parseArgs();
	setupEnv();

	int ret = -1;
	if(gArgCtx.app == NULL) {
		//normal
		ret = runScript();

	} else if(!wcscmp(gArgCtx.app, L"shell")) {
		//runshell
		ret = runShell();

	} else if(!wcscmp(gArgCtx.app, L"service")) {
		if(gArgCtx.action == NULL) {
			//runservice
			ret = runService();

		} else if(!wcscmp(gArgCtx.action, L"install")) {
			//install service
			ret = install_service();

		} else if(!wcscmp(gArgCtx.action, L"remove")) {
			//remove service
			ret = remove_service();

		} else {
			//runservice
			ret = runService();
		}

	} else {
		//normal
		ret = runScript();
	}

	return ret;
}

void setupEnv()
{
	if(gArgCtx.appdir) {
		_wfullpath(gCurAppDir, gArgCtx.appdir, sizeof(gCurAppDir)/sizeof(gCurAppDir[0]));
	} else {
		GetModuleFileName(NULL, gCurAppDir, sizeof(gCurAppDir)/sizeof(gCurAppDir[0]));
		wcsrchr(gCurAppDir, L'\\')[0] = 0;
	}

	SetCurrentDirectory(gCurAppDir);
}

void setupPyPath()
{
	wchar_t buf[4096];
	PyObject *o;

	PyObject *path = PySys_GetObject("path");
	PyList_SetSlice(path, 0, PyList_GET_SIZE(path), NULL);
	
	o = PyUnicode_FromWideChar(gCurAppDir, wcslen(gCurAppDir));
	PyList_Append(path, o);
	Py_DECREF(o);

	int len = swprintf(buf, sizeof(buf)/sizeof(buf[0]), L"%s\\lib", gCurAppDir);
	o = PyUnicode_FromWideChar(buf, len);
	PyList_Append(path, o);
	Py_DECREF(o);
}