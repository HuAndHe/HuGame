/*********************************************************************************
*FileName:        d3dApp.cpp
*Author:
*Version:         1.0
*Date:            2018/8/1
*Description:     d3d 应用基类
**********************************************************************************/

#include "d3dApp.h"

using Microsoft::WRL::ComPtr;
using namespace std;

// Windows 消息处理函数
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Forward hwnd on because we can get messages (e.g., WM_CREATE)
	// before CreateWindow returns, and thus before mhMainWnd is valid.
	return D3DApp::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}

D3DApp* D3DApp::mApp = nullptr;
D3DApp::D3DApp(UINT width, UINT height, std::wstring name, HINSTANCE hInstance) :
	mClientWidth(width),
	mClientHeight(height),
	mMainWndCaption(name),
	m_useWarpDevice(false),
	mhAppInst(hInstance)
{
	assert(mApp == nullptr);
	mApp = this;
}

D3DApp::~D3DApp()
{
	if (md3dDevice != nullptr)
		FlushCommandQueue();
}

D3DApp* D3DApp::GetApp()
{
	return mApp;
}

HINSTANCE D3DApp::AppInst() const
{
	return mhAppInst;
}

HWND D3DApp::MainWnd()const
{
	return mhMainWnd;
}

float D3DApp::AspectRatio()const
{
	return static_cast<float>(mClientWidth) / mClientHeight;
}

bool D3DApp::Get4xMsaaState()const
{
	return m4xMsaaState;
}

void D3DApp::Set4xMsaaState(bool value)
{
	if (m4xMsaaState != value)
	{
		m4xMsaaState = value;

		// Recreate the swapchain and buffers with new multisample settings.
		CreateSwapChain();
		OnResize();
	}
}

int D3DApp::Run()
{
	MSG msg = { 0 };
	mTimer.Reset();

	while (msg.message != WM_QUIT)
	{
		// 如果有窗口消息，处理它们
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else  //没有系统消息 处理游戏循环
		{
			mTimer.Tick();

			if (!mAppPaused)
			{
				CalculateFrameStats(); //时间信息计算
				Update(mTimer);
				Draw(mTimer);
			}
			else
			{
				Sleep(100);
			}
		}
	}

	return (int)msg.wParam;
}

bool D3DApp::Initialize()
{
	if (SUCCEEDED(InitMainWindow()))
	{
		ShowWindow(mhMainWnd, SW_SHOW);				// 显示窗口
		UpdateWindow(mhMainWnd);					// 更新窗口
		Sleep(2 + (rand() % 3000));
		if (SUCCEEDED(InitDirect3D()))
		{
			OnResize();
			return true;
		}
	}
	// 初始化游戏窗口
	//if (!InitMainWindow())
	//	return false;
	//
	//if (!InitDirect3D())
	//	return false;

	//   // Do the initial resize code.
	//   OnResize();

	//return true;
	return false;
}

void D3DApp::CreateRtvAndDsvDescriptorHeaps()
{
	//创建资源描述
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));


	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));
}

void D3DApp::OnResize()
{
	//资源检测
	assert(md3dDevice);
	assert(mSwapChain);
	assert(mDirectCmdListAlloc);

	// Flush before changing any resources.
	// 更改资源前确保gpu执行完了所有指令,调用reset函数前必须执行
	FlushCommandQueue();

	//清空Alloc的指令，后面需要重新设置
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	// Release the previous resources we will be recreating.
	for (int i = 0; i < SwapChainBufferCount; ++i)
		mSwapChainBuffer[i].Reset();
	mDepthStencilBuffer.Reset();

	// Resize the swap chain.
	ThrowIfFailed(mSwapChain->ResizeBuffers(
		SwapChainBufferCount,
		mClientWidth, mClientHeight,
		mBackBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

	mCurrBackBuffer = 0;

	// 创建 render target view
	//-----------------------------------------//
	//************** DX12初始化第七步 *****************//
	//说明：创建RTV和DSV
	//-----------------------------------------//
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < SwapChainBufferCount; i++)
	{
		ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffer[i])));
		md3dDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
		rtvHeapHandle.Offset(1, mRtvDescriptorSize);
	}

	// Create the depth/stencil buffer and view.
	D3D12_RESOURCE_DESC depthStencilDesc;
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = mClientWidth;
	depthStencilDesc.Height = mClientHeight;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;

	// Correction 11/12/2016: SSAO chapter requires an SRV to the depth buffer to read from 
	// the depth buffer.  Therefore, because we need to create two views to the same resource:
	//   1. SRV format: DXGI_FORMAT_R24_UNORM_X8_TYPELESS
	//   2. DSV Format: DXGI_FORMAT_D24_UNORM_S8_UINT
	// we need to create the depth buffer resource with a typeless format.  
	depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;

	depthStencilDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	depthStencilDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear;
	optClear.Format = mDepthStencilFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;
	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),	//资源类型
		D3D12_HEAP_FLAG_NONE,	//资源标志
		&depthStencilDesc,		//资源信息结构体
		D3D12_RESOURCE_STATE_COMMON,	//资源状态
		&optClear,		//清空设置 D3D12_CLEAR_VALUE
		IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())));	//需要创建的资源

															// Create descriptor to mip level 0 of entire resource using the format of the resource.
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = mDepthStencilFormat;
	dsvDesc.Texture2D.MipSlice = 0;
	md3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), &dsvDesc, DepthStencilView());

	// Transition(过渡) the resource from its initial state to be used as a depth buffer.
	//将资源状态从common转换到depthwrite
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	// Execute the resize commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until resize is complete.
	FlushCommandQueue();
	//-----------------------------------------//
	//************** DX12初始化第八步 *****************//
	//说明：设置视口
	//-----------------------------------------//
	// Update the viewport transform to cover the client area.
	mScreenViewport.TopLeftX = 0;
	mScreenViewport.TopLeftY = 0;
	mScreenViewport.Width = static_cast<float>(mClientWidth);
	mScreenViewport.Height = static_cast<float>(mClientHeight);
	mScreenViewport.MinDepth = 0.0f;
	mScreenViewport.MaxDepth = 1.0f;

	mScissorRect = { 0, 0, mClientWidth, mClientHeight };
}

LRESULT D3DApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;
	TCHAR greeting[] = L"Hello, World!";
	switch (msg)
	{
		// WM_ACTIVATE：窗口 activated or deactivated 状态：deactivated 时暂停游戏； activated 时继续游戏
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			mAppPaused = true;
			mTimer.Stop();
		}
		else
		{
			mAppPaused = false;
			mTimer.Start();
		}
		return 0;
		// 窗口绘制
	case WM_PAINT:
		//要处理 WM_PAINT 消息，首先应调用 BeginPaint
		//然后处理所有的逻辑以及在窗口中布局文本、按钮和其他控件等
		//然后调用 EndPaint。 
		hdc = BeginPaint(hwnd, &ps);

		// -----------------在这段代码对应用程序进行布局------------------------ 
		// 对于此应用程序，开始调用和结束调用之间的逻辑是在窗口中显示字符串 “Hello，World!”。
		// 请注意 TextOut 函数用于显示字符串。
		TextOut(hdc, 50, 5, greeting, lstrlen(greeting));
		// -----------------------布局模块结束----------------------------------
		EndPaint(hwnd, &ps);
		return 0;
		// WM_SIZE：窗口大小发生变化消息
	case WM_SIZE:
		// Save the new client area dimensions.
		mClientWidth = LOWORD(lParam);
		mClientHeight = HIWORD(lParam);
		if (md3dDevice)
		{
			if (wParam == SIZE_MINIMIZED)
			{
				mAppPaused = true;
				mMinimized = true;
				mMaximized = false;
			}
			else if (wParam == SIZE_MAXIMIZED)
			{
				mAppPaused = false;
				mMinimized = false;
				mMaximized = true;
				OnResize();
			}
			else if (wParam == SIZE_RESTORED)
			{

				// Restoring from minimized state?
				if (mMinimized)
				{
					mAppPaused = false;
					mMinimized = false;
					OnResize();
				}

				// Restoring from maximized state?
				else if (mMaximized)
				{
					mAppPaused = false;
					mMaximized = false;
					OnResize();
				}
				else if (mResizing)
				{
					// If user is dragging the resize bars, we do not resize 
					// the buffers here because as the user continuously 
					// drags the resize bars, a stream of WM_SIZE messages are
					// sent to the window, and it would be pointless (and slow)
					// to resize for each WM_SIZE message received from dragging
					// the resize bars.  So instead, we reset after the user is 
					// done resizing the window and releases the resize bars, which 
					// sends a WM_EXITSIZEMOVE message.
				}
				else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
				{
					OnResize();
				}
			}
		}
		return 0;

		// WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
	case WM_ENTERSIZEMOVE:
		mAppPaused = true;
		mResizing = true;
		mTimer.Stop();
		return 0;

		// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
		// Here we reset everything based on the new window dimensions.
	case WM_EXITSIZEMOVE:
		mAppPaused = false;
		mResizing = false;
		mTimer.Start();
		OnResize();
		return 0;

		// WM_DESTROY is sent when the window is being destroyed.
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

		// The WM_MENUCHAR message is sent when a menu is active and the user presses 
		// a key that does not correspond to any mnemonic or accelerator key. 
	case WM_MENUCHAR:
		// Don't beep when we alt-enter.
		return MAKELRESULT(0, MNC_CLOSE);

		// Catch this message so to prevent the window from becoming too small.
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		return 0;

	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_KEYUP:
		if (int(wParam) == VK_ESCAPE)
		{
			switch (mPagesNum)
			{
			case MAINPAGE:
				if (MessageBox(mhMainWnd, L"确定退出游戏？", L"确认", MB_OKCANCEL) == IDOK)
				{
					PostQuitMessage(0);
				}
				break;
			case CHOOSEMODE:
				mPagesNum = MAINPAGE;
				ChangeUIPages();
				break;
			case HELP:
				mPagesNum = MAINPAGE;
				ChangeUIPages();
				break;
			default:
				break;

			}

		}
		else if ((int)wParam == VK_F2)
			Set4xMsaaState(!m4xMsaaState);
		break;
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
}

bool D3DApp::InitMainWindow()
{
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = mhAppInst;
	wc.hIcon = (HICON)LoadImage(0, (L"Resource/game.ico"), IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
	/*wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);*/
	wc.hCursor = (HCURSOR)LoadImage(0, (L"Resource/normal.cur"), IMAGE_CURSOR, 0, 0, LR_LOADFROMFILE);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = L"MainWnd";

	if (!RegisterClass(&wc))
	{
		MessageBox(0, L"RegisterClass Failed.", 0, 0);
		return false;
	}

	// 初始化游戏窗口大小
	RECT R = { 0, 0, mClientWidth, mClientHeight };
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;

	// 创建窗口
	mhMainWnd = CreateWindow(L"MainWnd", mMainWndCaption.c_str(),
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, mhAppInst, 0);
	if (!mhMainWnd)
	{
		MessageBox(0, L"CreateWindow Failed.", 0, 0);
		return false;
	}
	return true;
}

bool D3DApp::InitDirect3D()
{
#if defined(DEBUG) || defined(_DEBUG) 
	// Enable the D3D12 debug layer.
	{
		ComPtr<ID3D12Debug> debugController;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
		debugController->EnableDebugLayer();
	}
#endif
	//-----------------------------------------//
	//************** DX12初始化第一步 *****************//
	//说明：获取dxFactory并创建Device
	//-----------------------------------------//
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mdxgiFactory)));

	// 尝试创建D3D硬件设备
	HRESULT hardwareResult = D3D12CreateDevice(
		nullptr,								// default adapter
		D3D_FEATURE_LEVEL_11_0,					// 最小 feature level
		IID_PPV_ARGS(&md3dDevice));

	// 如果硬件设备创建失败，使用 WARP(Windows Advanced Rasterization Platform) 软件设备
	if (FAILED(hardwareResult))
	{
		ComPtr<IDXGIAdapter> pWarpAdapter;
		ThrowIfFailed(mdxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter)));

		ThrowIfFailed(D3D12CreateDevice(
			pWarpAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&md3dDevice)));
	}

	// 创建 fence
	//-----------------------------------------//
	//************** DX12初始化第二步 *****************//
	//说明：创建 Fence 和 Desciptor size
	//-----------------------------------------//
	ThrowIfFailed(md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&mFence)));

	mRtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mDsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_CbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// 多重纹理映射 Quality level 检查
	// Check 4X MSAA quality support for our back buffer format.
	// All Direct3D 11 capable devices support 4X MSAA for all render 
	// target formats, so we only need to check quality support.

	//-----------------------------------------//
	//************** DX12初始化第三步 *****************//
	//说明：4X检测 获取 adapter信息
	//-----------------------------------------//

	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
	msQualityLevels.Format = mBackBufferFormat;
	msQualityLevels.SampleCount = 4;
	msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	msQualityLevels.NumQualityLevels = 0;
	ThrowIfFailed(md3dDevice->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&msQualityLevels,
		sizeof(msQualityLevels)));

	m4xMsaaQuality = msQualityLevels.NumQualityLevels;
	assert(m4xMsaaQuality > 0 && "Unexpected MSAA quality level.");

#ifdef _DEBUG
	//获取适配器信息
	LogAdapters();
#endif
	//-----------------------------------------//
	//************** DX12初始化第四步 *****************//
	//说明：指令组建创建 command queue  allocator  command list
	//-----------------------------------------//
	CreateCommandObjects();
	//-----------------------------------------//
	//************** DX12初始化第五步 *****************//
	//说明：创建交换链
	//-----------------------------------------//
	CreateSwapChain();
	//-----------------------------------------//
	//************** DX12初始化第六步 *****************//
	//说明：创建RTV和DSV的 DescriptorHeaps
	//-----------------------------------------//
	CreateRtvAndDsvDescriptorHeaps();
	//第七八步放置在OnRisize函数中，因为该两步骤需要在缓冲区发生改变时重新设置

	//七：创建rtv 和  dsv
	//八：设置视口

	return true;
}

void D3DApp::CreateCommandObjects()
{
	// 创建 command queues
	//创建GPU指令队列
	//-------填写指令队列结构------------//
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));

	// 创建 command allocator
	//创建指令分配算符器，gpu的指令队列在这里引用数据
	ThrowIfFailed(md3dDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(mDirectCmdListAlloc.GetAddressOf())));

	// 创建 command list
	//创建cpu指令链表
	ThrowIfFailed(md3dDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		mDirectCmdListAlloc.Get(), // Associated command allocator
		nullptr,                   // Initial PipelineStateObject
		IID_PPV_ARGS(mCommandList.GetAddressOf())));

	// Start off in a closed state.  This is because the first time we refer 
	// to the command list we will Reset it, and it needs to be closed before
	// calling Reset.
	mCommandList->Close();
}

void D3DApp::CreateSwapChain()
{
	// Release the previous swapchain we will be recreating.
	//创建交换链
	mSwapChain.Reset();
	////填写交换链描述结构,该结构和dx11是相同的
	DXGI_SWAP_CHAIN_DESC sd;
	sd.BufferDesc.Width = mClientWidth; //后台缓冲区宽度
	sd.BufferDesc.Height = mClientHeight;	//后台缓冲区高度
	sd.BufferDesc.RefreshRate.Numerator = 60;	//显示模式的更新频率  分子
	sd.BufferDesc.RefreshRate.Denominator = 1;	//分母
	sd.BufferDesc.Format = mBackBufferFormat;	//格式
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;	//扫描线模式 未指定
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;	//缩放 未指定
	sd.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	sd.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = SwapChainBufferCount;
	sd.OutputWindow = mhMainWnd;
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// Note: Swap chain uses queue to perform flush.
	ThrowIfFailed(mdxgiFactory->CreateSwapChain(
		mCommandQueue.Get(),
		&sd,
		mSwapChain.GetAddressOf()));
}

void D3DApp::FlushCommandQueue()
{

	/*
	本函数主要是用来同步gpu和cpu，m_CurrentFence是记录cpu的值，而fence中的值则由gpu管理，Signal函数是由cpu发出的指令（gpu队列最后一条指令）
	告诉gpu将fence中的值加1，如果gpu执行了这条指令则fence中的值和m_CurrentFence相同，说明指令全部执行完毕。否则未执行完
	所有指令，fence中的值比m_CurrentFence小1。
	*/
	// Advance the fence value to mark commands up to this fence point.
	mCurrentFence++;

	// Add an instruction to the command queue to set a new fence point.  Because we 
	// are on the GPU timeline, the new fence point won't be set until the GPU finishes
	// processing all the commands prior to this Signal().
	ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));
	UINT FenceValue = (UINT)mFence->GetCompletedValue();
	bool isEndCommand = FenceValue < mCurrentFence;
	// Wait until the GPU has completed commands up to this fence point.
	if (isEndCommand)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

		// Fire event when GPU hits current fence.  
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, eventHandle));

		// Wait until the GPU hits current fence event is fired.
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

ID3D12Resource* D3DApp::CurrentBackBuffer()const
{
	return mSwapChainBuffer[mCurrBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::CurrentBackBufferView()const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
		mCurrBackBuffer,
		mRtvDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::DepthStencilView()const
{
	return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}


//打印适配器
void D3DApp::LogAdapters()
{
	UINT i = 0;
	IDXGIAdapter* adapter = nullptr;

	while (mdxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_ADAPTER_DESC desc;
		adapter->GetDesc(&desc);

		std::wstring text = L"***Adapter: ";
		text += desc.Description;

		text += L"\n";

		OutputDebugString(text.c_str());
		adapterList.push_back(adapter);

		++i;
	}


	/*for (size_t i = 0; i < adapterList.size(); ++i)
	{
		LogAdapterOutputs(adapterList[i]);
		ReleaseCom(adapterList[i]);
	}*/
}

void D3DApp::LogAdapterOutputs(IDXGIAdapter* adapter)
{
	UINT i = 0;
	IDXGIOutput* output = nullptr;
	while (adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_OUTPUT_DESC desc;
		output->GetDesc(&desc);

		std::wstring text = L"***Output: ";
		text += desc.DeviceName;
		text += L"\n";
		OutputDebugString(text.c_str());

		LogOutputDisplayModes(output, mBackBufferFormat);

		ReleaseCom(output);

		++i;
	}
}
void D3DApp::CalculateFrameStats()
{
	static int frameCnt = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	// Compute averages over one second period.
	if ((mTimer.TotalTime() - timeElapsed) >= 1.0f)
	{
		float fps = (float)frameCnt; // fps = frameCnt / 1
		float mspf = 1000.0f / fps;
		DXGI_ADAPTER_DESC desc;
		if (adapterList[0])
		{
			adapterList[0]->GetDesc(&desc);
		}
		wstring fpsStr = to_wstring(fps);
		wstring mspfStr = to_wstring(mspf);
		//打印当前显示设备相关属性
		wstring windowText = mMainWndCaption +
			L"    fps: " + fpsStr +
			L"   mspf: " + mspfStr +
			L"   cWidth:" + to_wstring(mClientWidth) +
			L"   cHeight:" + to_wstring(mClientHeight) +
			L"   Adaptor: " + desc.Description;

		SetWindowText(mhMainWnd, windowText.c_str());

		// Reset for next average.
		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}
void D3DApp::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format)
{
	UINT count = 0;
	UINT flags = 0;

	// Call with nullptr to get list count.
	output->GetDisplayModeList(format, flags, &count, nullptr);

	std::vector<DXGI_MODE_DESC> modeList(count);
	output->GetDisplayModeList(format, flags, &count, &modeList[0]);

	for (auto& x : modeList)
	{
		UINT n = x.RefreshRate.Numerator;
		UINT d = x.RefreshRate.Denominator;
		std::wstring text =
			L"宽 = " + std::to_wstring(x.Width) + L" " +
			L"高 = " + std::to_wstring(x.Height) + L" " +
			L"屏幕刷新率 = " + std::to_wstring(n) + L"/" + std::to_wstring(d) +
			L"\n";

		::OutputDebugString(text.c_str());
	}
}
void D3DApp::SetCustomWindowText(LPCWSTR text)
{
	std::wstring windowText = mMainWndCaption + L": " + text;
	SetWindowText(mhMainWnd, windowText.c_str());
}

bool D3DApp::InitSound(std::string fileName)
{
	//创建声音播放接口
	HRESULT hr;
	if (FAILED(hr = DirectSoundCreate8(NULL, &mpDsd, NULL)))
		return false;
	//设置声音优先级
	if (FAILED(hr = mpDsd->SetCooperativeLevel(mhMainWnd, DSSCL_PRIORITY)))
		return false;
	//创建声音播放缓冲区
	mWaveFile = new CWaveFile;
	wstring widstr;
	widstr = std::wstring(fileName.begin(), fileName.end());
	mWaveFile->Open((LPWSTR)widstr.c_str(), NULL, WAVEFILE_READ);
	DSBUFFERDESC dsc;
	ZeroMemory(&dsc, sizeof(DSBUFFERDESC));
	dsc.dwSize = sizeof(DSBUFFERDESC);
	dsc.dwFlags = 0;
	dsc.dwBufferBytes = mWaveFile->GetSize();
	dsc.lpwfxFormat = mWaveFile->GetFormat();
	//通过结构体信息创建缓冲区
	if (FAILED(hr = mpDsd->CreateSoundBuffer(&dsc, &lpBuffer, NULL)))
		return false;
	//通过QueryOnterface()函数查询是否支持创建相应的缓冲区，并获得缓冲区的地址mpDSBuffer8

	if (FAILED(hr = lpBuffer->QueryInterface(IID_IDirectSoundBuffer8, (LPVOID*)&mpDSBuffer8)))
		return false;
	lpBuffer->Release();

	//锁定缓冲区，读取wav文件中的数据，并发送到缓冲区
	//先锁定缓冲区，然后将wav文件的信息读入到缓冲区中，然后解锁，以便后续进行播放
	LPVOID lpLockBuffer;
	DWORD lpLen, dwRead;
	mpDSBuffer8->Lock(0, 0, &lpLockBuffer, &lpLen, NULL, NULL, DSBLOCK_ENTIREBUFFER);
	mWaveFile->Read((BYTE*)lpLockBuffer, lpLen, &dwRead);
	mpDSBuffer8->Unlock(lpLockBuffer, lpLen, NULL, 0);
	mpDSBuffer8->SetVolume(10000);
	////播放缓冲区中的文件
	//mpDSBuffer8->SetCurrentPosition(0);
	//mpDSBuffer8->Play(0, 0, DSBPLAY_LOOPING);
	return true;
}