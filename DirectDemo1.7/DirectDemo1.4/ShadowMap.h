#pragma once
#include"D3DUtil.h"
/*************************************************************************
该工具类的构造函数会根据指定的尺寸和视口来创建纹理
阴影的分辨率会直接影响阴影效果的质量，但是在提升分辨率的同时，
渲染也将产生更多的开销占用更多的内存

阴影贴图算法需要执行两个渲染过程(render pass).在第一次渲染过程中,以光源视角将场景深度数据渲染至
阴影图中；
第二次渲染以玩家的摄像机将场景渲染至后台缓冲区内，为了实现阴影算法，这时以阴影图作为着色器的输入之一。
*************************************************************************/
class ShaowMap
{
public:
	ShaowMap(ID3D12Device* device, UINT width, UINT height);
	ShaowMap(const ShaowMap& rhs) = delete;
	ShaowMap& operator=(const ShaowMap& rhs) = delete;
	~ShaowMap() = default;

	UINT Width()const;
	UINT Height()const;
	ID3D12Resource* Resource();
	CD3DX12_GPU_DESCRIPTOR_HANDLE Srv()const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE Dsv()const;

	D3D12_VIEWPORT Viewport()const;
	D3D12_RECT ScissorRect()const;

	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv);

	void OnResize(UINT newWidth, UINT newHeight);

private:
	void BuildDescriptors();
	void BuildResource();

private:

	ID3D12Device* md3dDevice = nullptr;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;

	UINT mWidth = 0;
	UINT mHeight = 0;
	DXGI_FORMAT mFormat = DXGI_FORMAT_R24G8_TYPELESS;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuDsv;

	Microsoft::WRL::ComPtr<ID3D12Resource> mShadowMap = nullptr;
};
