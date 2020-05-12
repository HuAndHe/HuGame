/*********************************************************************************
*FileName:        d3dApp.cpp
*Author:
*Version:         1.0
*Date:            2018/8/1
*Description:     d3d Ӧ�û���
**********************************************************************************/

#include "d3dApp.h"

using Microsoft::WRL::ComPtr;
using namespace std;

// Windows ��Ϣ������
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
		// ����д�����Ϣ����������
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else  //û��ϵͳ��Ϣ ������Ϸѭ��
		{
			mTimer.Tick();

			if (!mAppPaused)
			{
				CalculateFrameStats(); //ʱ����Ϣ����
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
		ShowWindow(mhMainWnd, SW_SHOW);				// ��ʾ����
		UpdateWindow(mhMainWnd);					// ���´���
		Sleep(2 + (rand() % 3000));
		if (SUCCEEDED(InitDirect3D()))
		{
			OnResize();
			return true;
		}
	}
	// ��ʼ����Ϸ����
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
	//������Դ����
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
	//��Դ���
	assert(md3dDevice);
	assert(mSwapChain);
	assert(mDirectCmdListAlloc);

	// Flush before changing any resources.
	// ������Դǰȷ��gpuִ����������ָ��,����reset����ǰ����ִ��
	FlushCommandQueue();

	//���Alloc��ָ�������Ҫ��������
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

	// ���� render target view
	//-----------------------------------------//
	//************** DX12��ʼ�����߲� *****************//
	//˵��������RTV��DSV
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
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),	//��Դ����
		D3D12_HEAP_FLAG_NONE,	//��Դ��־
		&depthStencilDesc,		//��Դ��Ϣ�ṹ��
		D3D12_RESOURCE_STATE_COMMON,	//��Դ״̬
		&optClear,		//������� D3D12_CLEAR_VALUE
		IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())));	//��Ҫ��������Դ

															// Create descriptor to mip level 0 of entire resource using the format of the resource.
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = mDepthStencilFormat;
	dsvDesc.Texture2D.MipSlice = 0;
	md3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), &dsvDesc, DepthStencilView());

	// Transition(����) the resource from its initial state to be used as a depth buffer.
	//����Դ״̬��commonת����depthwrite
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	// Execute the resize commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until resize is complete.
	FlushCommandQueue();
	//-----------------------------------------//
	//************** DX12��ʼ���ڰ˲� *****************//
	//˵���������ӿ�
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
		// WM_ACTIVATE������ activated or deactivated ״̬��deactivated ʱ��ͣ��Ϸ�� activated ʱ������Ϸ
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
		// ���ڻ���
	case WM_PAINT:
		//Ҫ���� WM_PAINT ��Ϣ������Ӧ���� BeginPaint
		//Ȼ�������е��߼��Լ��ڴ����в����ı�����ť�������ؼ���
		//Ȼ����� EndPaint�� 
		hdc = BeginPaint(hwnd, &ps);

		// -----------------����δ����Ӧ�ó�����в���------------------------ 
		// ���ڴ�Ӧ�ó��򣬿�ʼ���úͽ�������֮����߼����ڴ�������ʾ�ַ��� ��Hello��World!����
		// ��ע�� TextOut ����������ʾ�ַ�����
		TextOut(hdc, 50, 5, greeting, lstrlen(greeting));
		// -----------------------����ģ�����----------------------------------
		EndPaint(hwnd, &ps);
		return 0;
		// WM_SIZE�����ڴ�С�����仯��Ϣ
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
				if (MessageBox(mhMainWnd, L"ȷ���˳���Ϸ��", L"ȷ��", MB_OKCANCEL) == IDOK)
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

	// ��ʼ����Ϸ���ڴ�С
	RECT R = { 0, 0, mClientWidth, mClientHeight };
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;

	// ��������
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
	//************** DX12��ʼ����һ�� *****************//
	//˵������ȡdxFactory������Device
	//-----------------------------------------//
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mdxgiFactory)));

	// ���Դ���D3DӲ���豸
	HRESULT hardwareResult = D3D12CreateDevice(
		nullptr,								// default adapter
		D3D_FEATURE_LEVEL_11_0,					// ��С feature level
		IID_PPV_ARGS(&md3dDevice));

	// ���Ӳ���豸����ʧ�ܣ�ʹ�� WARP(Windows Advanced Rasterization Platform) ����豸
	if (FAILED(hardwareResult))
	{
		ComPtr<IDXGIAdapter> pWarpAdapter;
		ThrowIfFailed(mdxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter)));

		ThrowIfFailed(D3D12CreateDevice(
			pWarpAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&md3dDevice)));
	}

	// ���� fence
	//-----------------------------------------//
	//************** DX12��ʼ���ڶ��� *****************//
	//˵�������� Fence �� Desciptor size
	//-----------------------------------------//
	ThrowIfFailed(md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&mFence)));

	mRtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mDsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_CbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// ��������ӳ�� Quality level ���
	// Check 4X MSAA quality support for our back buffer format.
	// All Direct3D 11 capable devices support 4X MSAA for all render 
	// target formats, so we only need to check quality support.

	//-----------------------------------------//
	//************** DX12��ʼ�������� *****************//
	//˵����4X��� ��ȡ adapter��Ϣ
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
	//��ȡ��������Ϣ
	LogAdapters();
#endif
	//-----------------------------------------//
	//************** DX12��ʼ�����Ĳ� *****************//
	//˵����ָ���齨���� command queue  allocator  command list
	//-----------------------------------------//
	CreateCommandObjects();
	//-----------------------------------------//
	//************** DX12��ʼ�����岽 *****************//
	//˵��������������
	//-----------------------------------------//
	CreateSwapChain();
	//-----------------------------------------//
	//************** DX12��ʼ�������� *****************//
	//˵��������RTV��DSV�� DescriptorHeaps
	//-----------------------------------------//
	CreateRtvAndDsvDescriptorHeaps();
	//���߰˲�������OnRisize�����У���Ϊ����������Ҫ�ڻ����������ı�ʱ��������

	//�ߣ�����rtv ��  dsv
	//�ˣ������ӿ�

	return true;
}

void D3DApp::CreateCommandObjects()
{
	// ���� command queues
	//����GPUָ�����
	//-------��дָ����нṹ------------//
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));

	// ���� command allocator
	//����ָ������������gpu��ָ�������������������
	ThrowIfFailed(md3dDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(mDirectCmdListAlloc.GetAddressOf())));

	// ���� command list
	//����cpuָ������
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
	//����������
	mSwapChain.Reset();
	////��д�����������ṹ,�ýṹ��dx11����ͬ��
	DXGI_SWAP_CHAIN_DESC sd;
	sd.BufferDesc.Width = mClientWidth; //��̨���������
	sd.BufferDesc.Height = mClientHeight;	//��̨�������߶�
	sd.BufferDesc.RefreshRate.Numerator = 60;	//��ʾģʽ�ĸ���Ƶ��  ����
	sd.BufferDesc.RefreshRate.Denominator = 1;	//��ĸ
	sd.BufferDesc.Format = mBackBufferFormat;	//��ʽ
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;	//ɨ����ģʽ δָ��
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;	//���� δָ��
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
	��������Ҫ������ͬ��gpu��cpu��m_CurrentFence�Ǽ�¼cpu��ֵ����fence�е�ֵ����gpu����Signal��������cpu������ָ�gpu�������һ��ָ�
	����gpu��fence�е�ֵ��1�����gpuִ��������ָ����fence�е�ֵ��m_CurrentFence��ͬ��˵��ָ��ȫ��ִ����ϡ�����δִ����
	����ָ�fence�е�ֵ��m_CurrentFenceС1��
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


//��ӡ������
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
		//��ӡ��ǰ��ʾ�豸�������
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
			L"�� = " + std::to_wstring(x.Width) + L" " +
			L"�� = " + std::to_wstring(x.Height) + L" " +
			L"��Ļˢ���� = " + std::to_wstring(n) + L"/" + std::to_wstring(d) +
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
	//�����������Žӿ�
	HRESULT hr;
	if (FAILED(hr = DirectSoundCreate8(NULL, &mpDsd, NULL)))
		return false;
	//�����������ȼ�
	if (FAILED(hr = mpDsd->SetCooperativeLevel(mhMainWnd, DSSCL_PRIORITY)))
		return false;
	//�����������Ż�����
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
	//ͨ���ṹ����Ϣ����������
	if (FAILED(hr = mpDsd->CreateSoundBuffer(&dsc, &lpBuffer, NULL)))
		return false;
	//ͨ��QueryOnterface()������ѯ�Ƿ�֧�ִ�����Ӧ�Ļ�����������û������ĵ�ַmpDSBuffer8

	if (FAILED(hr = lpBuffer->QueryInterface(IID_IDirectSoundBuffer8, (LPVOID*)&mpDSBuffer8)))
		return false;
	lpBuffer->Release();

	//��������������ȡwav�ļ��е����ݣ������͵�������
	//��������������Ȼ��wav�ļ�����Ϣ���뵽�������У�Ȼ��������Ա�������в���
	LPVOID lpLockBuffer;
	DWORD lpLen, dwRead;
	mpDSBuffer8->Lock(0, 0, &lpLockBuffer, &lpLen, NULL, NULL, DSBLOCK_ENTIREBUFFER);
	mWaveFile->Read((BYTE*)lpLockBuffer, lpLen, &dwRead);
	mpDSBuffer8->Unlock(lpLockBuffer, lpLen, NULL, 0);
	mpDSBuffer8->SetVolume(10000);
	////���Ż������е��ļ�
	//mpDSBuffer8->SetCurrentPosition(0);
	//mpDSBuffer8->Play(0, 0, DSBPLAY_LOOPING);
	return true;
}