/*********************************************************************************
*FileName:        d3dApp.h
*Author:
*Version:         1.0
*Date:            2020/1/4
*Description:     d3d Ӧ�û���

D3DӦ�ó�ʼ�����̣�
1��ʹ�� D3D12CreateDevice �������� ID3D12Device
2������ ID3D12Fence object ��ȷ�� descriptor ��С
3����� 4X MSAA quality level ֧��
4������ command queue, command list allocator, and main command list
5�����岢���� swap chain
6������Ӧ����Ҫ�� descriptor heaps
7������ back buffer ��С����Ϊ back buffer ���� render target view
8������ depth/stencil buffer �� associated depth/stencil view
9������ viewport �� �ü���

**********************************************************************************/

#pragma once

// Debugģʽ�£������ڴ��⹤��
#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#include<iostream>
#include "D3DUtil.h"
#include "GameTimer.h"
#include<dsound.h>			//����dsoundͷ�ļ�
#include"WaveFile.h"		//������wave�ļ�������ͷ�ļ�

#pragma comment(lib, "Dsound.lib")
#pragma comment(lib, "WinMM.Lib")

#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")


#define MAINPAGE 0
#define	CHOOSEMODE 1
#define HELP	2
#define LOADING 3
#define GUNMANAGE 4

class D3DApp
{
protected:

	D3DApp(UINT width, UINT height, std::wstring name, HINSTANCE hInstance);

	// ��ֹʹ�����·�ʽ����
	D3DApp(const D3DApp& rhs) = delete;
	D3DApp& operator=(const D3DApp& rhs) = delete;

	virtual ~D3DApp();

public:

	static D3DApp* GetApp();							// ��ȡ��ǰʵ������ D3DApp ��ָ��
	HINSTANCE AppInst()const;							// ��ȡ��ǰ����� HINSTANCE ����
	HWND MainWnd()const;								// ��ȡ���ھ��
	float AspectRatio()const;							// ��ȡ���ڿ�߱�

														// ����/��ȡ ��������ӳ��״̬
	bool Get4xMsaaState()const;
	void Set4xMsaaState(bool value);

	int Run();											// �������߼�ѭ��

	virtual bool Initialize();							// ��ʼ������
	virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);				// �û�������Ϣ��Ӧ

protected:

	virtual void CreateRtvAndDsvDescriptorHeaps();		// ���� descriptor heaps
	virtual void OnResize();							// ���ڴ�С�ı�ʱ��Ӧ��Ϣ
	virtual void Update(const GameTimer& gt) = 0;		// ���ڸ���
	virtual void Draw(const GameTimer& gt) = 0;			// ���ڻ���

														// ���������Ӧ
	virtual void OnMouseDown(WPARAM btnState, int x, int y) { }
	virtual void OnMouseUp(WPARAM btnState, int x, int y) { }
	virtual void OnMouseMove(WPARAM btnState, int x, int y) { }

protected:

	bool InitMainWindow();								// ��ʼ����Ϸ����
	bool InitDirect3D();								// ��ʼ��D3D
	void CreateCommandObjects();						// ���� command queue �� command list
	void CreateSwapChain();								// ����ʹ��� swap chain

	void FlushCommandQueue();							// ���� CPU/GPU ͬ��
	void SetCustomWindowText(LPCWSTR text);					//���ô��ڱ���

	virtual bool InitSound(std::string fileName);

	ID3D12Resource* CurrentBackBuffer()const;						// ��ȡ back buffer
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView()const;		// ��ȡ��ǰ back buffer view
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView()const;			// ��ȡ��ǰ DSV

	virtual void CalculateFrameStats();							// ����֡���ʺ�ÿ֡����ʱ�䣬�������ʾ�ڴ��ڱ�����
	virtual void ChangeUIPages() {}
	void LogAdapters();									// ��ӡ���� �Կ�/���ģ�� ������
	void LogAdapterOutputs(IDXGIAdapter* adapter);		// ��ӡ���������
	void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);		// ��ӡ��ʾģʽ

																				// ��Ա����																					
protected:

	static D3DApp* mApp;								// ��ǰʵ������ D3DApp ���ã���������
	HINSTANCE mhAppInst = nullptr;						// Application ���
	HWND mhMainWnd = nullptr;							// �����ھ��

	bool mAppPaused = false;							// ��ǰӦ���Ƿ���ͣ
	bool mMinimized = false;							// ��ǰӦ���Ƿ���С��
	bool mMaximized = false;							// ��ǰӦ���Ƿ����
	bool mResizing = false;								// ��ǰӦ���Ƿ��ڱ����ô�С���϶��߽磩
	bool mFullscreenState = false;						// �Ƿ����ȫ��

														// ����ӳ����ز���
	bool m4xMsaaState = false;							// 4X MSAA enabled
	UINT m4xMsaaQuality = 0;							// quality level of 4X MSAA

	GameTimer mTimer;									// ��������ʱ��

														// DXGI - DirectX Graphics Infrastructure: ���Կ�ʱ��һЩͨ�õķ���
														//�������� swap chain �� ����������ʾ������
	Microsoft::WRL::ComPtr<IDXGIFactory4> mdxgiFactory;

	Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;	// ����˫ BUFFER ����
	Microsoft::WRL::ComPtr<ID3D12Device> md3dDevice;	// D3DӲ���豸

														// Fence������ǿ�� CPU/GPU ͬ��
														// CPUһֱ�ȴ���GPUִ�е� fence, �ſ�ʼ����ִ�У����Է�ֹ���ݳ�ͻ
	Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
	UINT64 mCurrentFence = 0;

	// command queue and command list
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;

	static const int SwapChainBufferCount = 2;			// ʹ��˫ buffer ����
	int mCurrBackBuffer = 0;							// ��ǰ back buffer index

	Microsoft::WRL::ComPtr<ID3D12Resource> mSwapChainBuffer[SwapChainBufferCount];		// back buffers
	Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer;							// DSV buffer

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;		// RTV descriptor heap
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap;		// DSV descriptor heap

																// ���ڴ�С�Ͳ��д�С
	D3D12_VIEWPORT mScreenViewport;
	D3D12_RECT mScissorRect;

	// Descriptor ��С��Descriptor��С�ڲ�ͬGPU֮�䲻ͬ��������Ҫ�ڳ�ʼ��ʱȷ�ϣ�
	UINT mRtvDescriptorSize = 0;						// RTV��render target resources
	UINT mDsvDescriptorSize = 0;						// DSV: depth/stencil resources
	UINT mCbvSrvUavDescriptorSize = 0;					// DBV/SRV/UAV��constant buffers, shader resources, unloadered access view resources
	UINT m_CbvSrvDescriptorSize = 0;
	std::wstring mMainWndCaption = L"DirectX12Demo";			// �����ڱ�������

	DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	std::vector<IDXGIAdapter*> adapterList; //����������
											// ��Ϸ���ڳ�ʼ���
	int mClientWidth = 800;
	int mClientHeight = 600;
	//�Ƿ��������������
	bool m_useWarpDevice = false;

	bool gameStart = false;
	int mPagesNum = 0;

	//��Ƶ�ӿڱ���
	LPDIRECTSOUND8 mpDsd;				//����ӿڱ���   
	CWaveFile* mWaveFile;
	LPDIRECTSOUNDBUFFER lpBuffer;
	LPDIRECTSOUNDBUFFER8 mpDSBuffer8;
};