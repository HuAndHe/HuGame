/*********************************************************************************
*FileName:        AppMain.cpp
*Author:
*Version:         1.0
*Date:            2019/12/11
*Description:     程序执行入口
**********************************************************************************/

#include "DemoMain.h"

int wmain()
{
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	try
	{
		HINSTANCE hInstance = GetModuleHandle(0);
		DemoMain theApp(800, 600, L"穿越防线", hInstance);
		if (!theApp.Initialize())
			return 0;

		return theApp.Run();
	}
	catch (DxException & e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}

}