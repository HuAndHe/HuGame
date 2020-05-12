#include "DemoMain.h"
const int gNumFrameResources = 3;

DemoMain::DemoMain(UINT width, UINT height, std::wstring name, HINSTANCE hInstance)
	: D3DApp(width, height, name, hInstance)
{
	mSceneBounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
	mSceneBounds.Radius = sqrtf(20.0f * 20.0f + 30.f * 30.0f);
}


DemoMain::~DemoMain()
{
}

bool DemoMain::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	
	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	// Get the increment size of a descriptor in this heap type.  This is hardware specific, 
	// so we have to query this information.
	mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	mCamera.SetPosition(0.0f, 2.0f, -15.0f);

	//构建阴影图
	//越大的阴影图尺寸，效果越精细，越小的阴影图尺寸，效果越粗糙
	mShadowMap = std::make_unique<ShaowMap>(md3dDevice.Get(), 1024, 1024);


	//游戏、玩家、敌人信息初始化
	BuildPlayerInfo();
	BuildGameInfo();
	LoadTextures();
	BuildRootSignature();
	BuildDescriptorHeaps();
	BuildShadersAndInputLayout();
	BuildShapeGeometry();
	BuildSkullGeometry();
	LoadStaticAIMesh();
	LoadSkeletonAndAnimation();
	BuildMaterials();
	BuildRenderItems();
	BuildAniObjItems();
	BuildAIItems(AINum);
	BuildUIMainPages();
	BuildFrameResources();
	BuildPSOs();

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	

	return true;
}

void DemoMain::OnResize()
{
	D3DApp::OnResize();

	mCamera.SetLens(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
}

void DemoMain::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);
	if (gameStart)
	{
		UpdatePlayerInfo(mTimer);					 //更新玩家的信息
		updateEnemyPath(mTimer);					 //更新enemy至玩家所在位置的path
	}
	for (auto& b : mAllRitems)
	{
		if (b->isBullet)
		{
			UpdateBullet(b.get(), gt);
		}
	}
	
	// Cycle through the circular frame resource array.
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}


	/* 更新光源，让光源以场景为半径，原点为中心旋转*/
	mLightRotationAngle += 0.1f * gt.DeltaTime();
	XMMATRIX R = XMMatrixRotationY(mLightRotationAngle);
	for (int i = 0; i < 3; i++)
	{
		XMVECTOR lightDir = XMLoadFloat3(&mBaseLightDirections[i]);
		lightDir = XMVector3TransformNormal(lightDir, R);
		XMStoreFloat3(&mRotatedLightDirections[i], lightDir);
	}

	//调用碰撞检测函数
	DetectCollision();

	//更新各类资源
	AnimateMaterials(gt);
	UpdateObjectCBs(gt);
	UpdateMaterialBuffer(gt);
	UpdateShadowPassCB(gt);
	UpdateShadowTransform(gt);
	UpdateMainPassCB(gt);
	//UpdateAnimation(gt);
	if (mAllEnemy.empty())
	{
		BuildAIItems(AINum);
	}
}

void DemoMain::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(cmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));

	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
	////将动画信息缓冲区绑定到Shader
	auto FinalTransformCB = mCurrFrameResource->SkinnedCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(2, FinalTransformCB->GetGPUVirtualAddress());
	//将材质缓冲区绑定到Shader
	auto matBuffer = mCurrFrameResource->MaterialBuffer->Resource();
	mCommandList->SetGraphicsRootShaderResourceView(3, matBuffer->GetGPUVirtualAddress());
	// Bind all the textures used in this scene.  Observe
	// that we only have to specify the first descriptor in the table.  
	// The root signature knows how many descriptors are expected in the table.
	mCommandList->SetGraphicsRootDescriptorTable(5, mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());


	DrawSceneToShadow();


	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Clear the back buffer and depth buffer.
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

	CD3DX12_GPU_DESCRIPTOR_HANDLE skyTexDescriptor(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	skyTexDescriptor.Offset(mSkyTexHeapIndex, mCbvSrvDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(4, skyTexDescriptor);
	if (gameStart)
	{
		mCommandList->SetPipelineState(mPSOs["opaque"].Get());
		DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);
		DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::AI]);
	}
	else
	{
		mCommandList->SetPipelineState(mPSOs["UI"].Get());
		DrawUIItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::UI]);
	}
	

	mCommandList->SetPipelineState(mPSOs["sky"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Sky]);



	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Swap the back and front buffers
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Advance the fence value to mark commands up to this fence point.
	mCurrFrameResource->Fence = ++mCurrentFence;

	// Add an instruction to the command queue to set a new fence point. 
	// Because we are on the GPU timeline, the new fence point won't be 
	// set until the GPU finishes processing all the commands prior to this Signal().
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void DemoMain::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;
	
	if (gameStart)
	{
		if ((btnState & MK_LBUTTON) != 0)
		{
			XMFLOAT3 cameraPos = mCamera.GetPosition3f();
			XMFLOAT3 lookDir = mCamera.GetLook3f();

			XMFLOAT4X4 P = mCamera.GetProj4x4f();
			// Compute picking ray in view space.
			float vx = (+2.0f * x / mClientWidth - 1.0f) / P(0, 0);
			float vy = (-2.0f * y / mClientHeight + 1.0f) / P(1, 1);
			// Ray definition in view space.
			XMVECTOR rayOrigin = XMVectorSet(0, 0, 0, 1.0f);
			XMVECTOR rayDir = XMVectorSet(vx, vy, 1, 0.0f);
			XMMATRIX V = mCamera.GetView();
			XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(V), V);
			//得到鼠标拾取点在世界坐标系中的位置
			rayDir = XMVector3TransformCoord(rayDir, invView);
			rayOrigin = XMVector3TransformCoord(rayOrigin, invView);

			/*	XMMATRIX V = mCamera.GetView();
				XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(V), V);*/

			AddBullet(x, y, { cameraPos.x,cameraPos.y,cameraPos.z }, { rayDir.m128_f32[0],rayDir.m128_f32[1] ,rayDir.m128_f32[2] });

		}
	}
	else
	{
		switch (mPagesNum)
		{
		case MAINPAGE:		//主界面响应按键操作
			if ((btnState & MK_LBUTTON) != 0)
			{
				if (x > mClientWidth * 0.7187f && x < mClientWidth * 0.8437f)
				{
					//开始游戏
					if (y > mClientHeight * 0.178 && y < mClientHeight * 0.226)
					{
						mPagesNum = CHOOSEMODE;
						ChangeUIPages();
					}
					//枪械管理
					else if (y > mClientHeight * 0.283 && y < mClientHeight * 0.323)
					{

						MessageBox(mhMainWnd, L"枪械管理页面暂未开放", L"枪械管理", MB_OK);
					}
					//帮助
					else if (y > mClientHeight * 0.389 && y < mClientHeight * 0.439)
					{
						mPagesNum = HELP;
						ChangeUIPages();

					}
					//退出游戏
					else if (y > mClientHeight * 0.507 && y < mClientHeight * 0.557)
					{
						std::cout << mLastMousePos.x << "," << mLastMousePos.y << "点击了退出游戏按钮" << std::endl;
						if (MessageBox(mhMainWnd, L"确定退出游戏？", L"确认", MB_OKCANCEL) == IDOK)
						{
							PostQuitMessage(0);
						}

					}
				}
			}
			break;
		case CHOOSEMODE:
			std::cout << "鼠标位置：" << x << "," << y << std::endl;
			if ((btnState & MK_LBUTTON) != 0)
			{
				if (x > mClientWidth * 0.189f && x < mClientWidth * 0.319f)
				{
					if (y > mClientHeight * 0.610 && y < mClientHeight * 0.660)  //战役模式
					{
						mPagesNum = -1;

						gameStart = true;

						//游戏开始才初始化寻径算法
						astar.InitAstar(maze);

						if (!InitSound("audio/background.wav"))
						{
							MessageBox(mhMainWnd, TEXT("声音初始化失败Q！"), TEXT("ERROR"), MB_OK);
						}

						//播放缓冲区中的文件  这里是加载完游戏中场景资源之后才开始播放背景音乐，也就是说游戏界面出现时才播放背景音乐
						/*mpDSBuffer8->SetCurrentPosition(0);
						mpDSBuffer8->Play(0, 0, DSBPLAY_LOOPING);*/
					}
					else if (y > mClientHeight * 0.810 && y < mClientHeight * 0.860)	//每日对战
					{
						MessageBox(mhMainWnd, L"每日挑战暂未开放，请耐心等待", L"每日挑战", MB_OK);
					}
				}
				else if (x > mClientWidth * 0.673f && x < mClientWidth * 0.803f)
				{
					if (y > mClientHeight * 0.610 && y < mClientHeight * 0.660)  //竞技模式
					{
						MessageBox(mhMainWnd, L"竞技模式暂未开放，请耐心等待", L"竞技模式", MB_OK);
					}
					else if (y > mClientHeight * 0.810 && y < mClientHeight * 0.860)	//远征模式
					{
						MessageBox(mhMainWnd, L"远征模式暂未开放，请耐心等待", L"远征模式", MB_OK);
					}
				}
			}
		case HELP:
			break;
		case LOADING:
			break;
		default:
			break;
		}
	}	
	SetCapture(mhMainWnd);
}

void DemoMain::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void DemoMain::OnMouseMove(WPARAM btnState, int x, int y)
{
	if (gameStart)
	{
		//鼠标右键只是修改水平视角不能修改垂直视角
		if ((btnState & MK_RBUTTON) != 0)
		{
			// Make each pixel correspond to a quarter of a degree.
			float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
			
			//float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

			//mCamera.Pitch(dy);
			mCamera.RotateY(dx);
		}

		mLastMousePos.x = x;
		mLastMousePos.y = y;
	}
	
}

void DemoMain::OnKeyboardInput(const GameTimer& gt)
{
	DirectX::XMFLOAT3 lastCameraPos = mCamera.GetPosition3f();
	const float dt = gt.DeltaTime();
	// 1、 & 与操作， & 0x8000就是判断这个返回值的high - order bit（高位字节）
	//2、如果high - order bit是1, 则是按下状态，否则UP状态
	if (GetAsyncKeyState('W') & 0x8000)
		mCamera.Walk(10.0f * dt);

	if (GetAsyncKeyState('S') & 0x8000)
		mCamera.Walk(-10.0f * dt);

	if (GetAsyncKeyState('A') & 0x8000)
		mCamera.Strafe(-10.0f * dt);

	if (GetAsyncKeyState('D') & 0x8000)
		mCamera.Strafe(10.0f * dt);


	bool flag = false;    //标志符  用于是否更新相机视口
	for (int i = 0; i < mAllRitems.size(); i++)
	{
		auto& m = mAllRitems[i];
		if (m->needDetectCollision && mCamera.cmbounds.Contains(m->orBounds))
		{
			flag = false;
			mCamera.SetPosition(lastCameraPos);
			m->collisionDetected.resize(0);
			mCamera.SetViewDirty(flag);
			break;
		}
		else
		{
			flag = true;
		}

	}
	if (flag)
	{
		mCamera.UpdateViewMatrix();	//更新相机视口
		flag = false;
	}
}

void DemoMain::AnimateMaterials(const GameTimer& gt)
{

}

void DemoMain::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for (auto& e : mAllRitems)
	{
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
			objConstants.MaterialIndex = e->Mat->MatCBIndex;
			//不同类型的模型采用不同的贴图数组
			if (e->isMesh)
			{
				objConstants.ObjPad0 = 1;
			}
			if (e->isAnimation)
			{
				objConstants.ObjPad1 = 0;
			}



			/*对子弹进行边界判断:
				判断条件有当前物体是子弹并且满足下列条件之一：
				1.y方向：y<=0.25   y>=8
				2.x方向：x<=-20	 x>=20
				3.z方向：z<=-30	 z>=30
				4.e.collisionDetected.size()>=0  表示已经发生碰撞
			*/
			int e_index = e->OpaqueObjIndex;   //子弹的OpaqueLayerIndex
			//超出边界，删除Bullet
			if (e->isBullet && ((e->World._42 <= 0.25f) || (e->isBullet && e->World._42 >= 8.f) || (e->isBullet && e->World._41 <= -20.0f) || (e->isBullet && e->World._41 >= 20.0f) || (e->isBullet && e->World._43 >= 30.0f) || (e->isBullet && e->World._43 <= -30.0f)))
			{
				//从OpaqueLayer删除Bullet
				mRitemLayer[(int)RenderLayer::Opaque].erase(mRitemLayer[(int)RenderLayer::Opaque].begin() + e_index);		//从当前PSO管道删除
				mAllRitems.erase(mAllRitems.begin() + e->ObjCBIndex);	//全局删除
				--OpaqueObjIndex;
				--ObjCBIndex;
				//这种情况是最后添加子弹  如果不是这种情况则需要更改代码
				for (auto& b : mAllRitems)
				{
					if (b->isBullet)
					{
						b->OpaqueObjIndex--;
						b->ObjCBIndex--;
					}
				}
				break;
			}
			else if (e->isBullet && e->collisionDetected.size() > 0) //如果当前Object是子弹并且已经发生碰撞
			{
				std::cout << e->ObjCBIndex << "撞了" << e->collisionDetected[0]->collisionObjectIndex << std::endl;
				int b_index = e->ObjCBIndex;
				for (auto& bitem : mAllRitems)
				{
					if (bitem->ObjCBIndex == e->collisionDetected[0]->collisionObjectIndex)//子弹碰撞的物体
					{
						if (bitem->isAI)
						{
							std::cout << "子弹击中AI" <<bitem->AIIndex<< std::endl;
							int index = bitem->ObjCBIndex;
							int bl_index = bitem->AIIndex;
							//处理的不是很好，只有先删除子弹再删除AI
							mRitemLayer[(int)RenderLayer::Opaque].erase(mRitemLayer[(int)RenderLayer::Opaque].begin() + e_index);		//从当前PSO管道删除
							mAllRitems.erase(mAllRitems.begin() + b_index);	//全局删除子弹

							mRitemLayer[(int)RenderLayer::AI].erase(mRitemLayer[(int)RenderLayer::AI].begin() + bl_index);		//从当前PSO管道删除
							mAllRitems.erase(mAllRitems.begin() + index);	//全局删除AI
							//参数： AI的索引 子弹的索引
							DeleteAI(index, b_index);
							//打印游戏信息
							LogPlayerInfo();
							OpaqueObjIndex--;			//全局透明层索引减一
							AIIndex--;					//全局AI层索引减一
							ObjCBIndex -= 2;			//全局物体索引减一
							//遍历全局物体  除了上述被删除的两个物体
							for (auto& bbitem : mAllRitems)
							{
								if (bbitem->isAI)  //是AI就可能更新AIIndex
								{
									//更新AILayer局部的索引
									if (bbitem->AIIndex > bl_index)
										bbitem->AIIndex--;
								}
								else//否则就可能更新OpaqueLayerIndex
								{
									if (bbitem->OpaqueObjIndex > e_index)
										bbitem->OpaqueObjIndex--;

								}
								//更新全局常量缓冲区索引ObjCBIndex  index代表AIIndex   b_index代表子弹Index
								//大于两者(被删除的子弹和AI)中的一个 
								if ((bbitem->ObjCBIndex > index&& bbitem->ObjCBIndex < b_index) || (bbitem->ObjCBIndex < index && bbitem->ObjCBIndex > b_index))
								{
									--bbitem->ObjCBIndex;
								}//大于两者
								else if (bbitem->ObjCBIndex > index&& bbitem->ObjCBIndex > b_index)
								{
									bbitem->ObjCBIndex -= 2;
								}
								else
								{
									bbitem->ObjCBIndex = bbitem->ObjCBIndex;
								}
							}
							break;   //删除完毕，停止遍历，等待下一帧更新
						}
						else  //与非AI角色发生碰撞，只是删除当前子弹
						{
							std::cout << "子弹击中非AI角色" << std::endl;
							//从OpaqueLayer删除Bullet
							mRitemLayer[(int)RenderLayer::Opaque].erase(mRitemLayer[(int)RenderLayer::Opaque].begin() + e_index);		//从当前PSO管道删除
							mAllRitems.erase(mAllRitems.begin() + e->ObjCBIndex);	//全局删除
							--OpaqueObjIndex;
							--ObjCBIndex;
							//这种情况是最后添加子弹  如果不是这种情况则需要更改代码
							for (auto& b : mAllRitems)
							{

								if (b->isBullet)
								{
									b->OpaqueObjIndex--;
									b->ObjCBIndex--;
								}
							}
							break;
						}
					}
				}
				break;
			}




			currObjectCB->CopyData(e->ObjCBIndex, objConstants);
			e->NumFramesDirty--;
		}
	}
}

void DemoMain::UpdateMaterialBuffer(const GameTimer& gt)
{
	auto currMaterialBuffer = mCurrFrameResource->MaterialBuffer.get();
	for (auto& e : mMaterials)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialData matData;
			matData.DiffuseAlbedo = mat->DiffuseAlbedo;
			matData.FresnelR0 = mat->FresnelR0;
			matData.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matData.MatTransform, XMMatrixTranspose(matTransform));
			matData.DiffuseMapIndex = mat->DiffuseSrvHeapIndex;

			currMaterialBuffer->CopyData(mat->MatCBIndex, matData);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void DemoMain::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = mCamera.GetView();
	XMMATRIX proj = mCamera.GetProj();

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);
	XMMATRIX shadowTransform = XMLoadFloat4x4(&mShadowTransform);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	XMStoreFloat4x4(&mMainPassCB.ShadowTransform, XMMatrixTranspose(shadowTransform));

	mMainPassCB.EyePosW = mCamera.GetPosition3f();
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
	//3个环境光
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	mMainPassCB.Lights[0].Direction = mRotatedLightDirections[0];
	mMainPassCB.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
	mMainPassCB.Lights[1].Direction = mRotatedLightDirections[1];
	mMainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	mMainPassCB.Lights[2].Direction = mRotatedLightDirections[2];
	mMainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

	//出生点的聚光灯
	for (int i = 0; i < 6; i++)
	{
		mMainPassCB.Lights[3 + i].Strength = { 1.0f,0.0f,0.0f };
		mMainPassCB.Lights[3 + i].Position = { mAIInitialPos[i].x,mAIInitialPos[i].y+4.0f,mAIInitialPos[i].z };
		mMainPassCB.Lights[3 + i].Direction = { 0.0f,-1.0f,0.0f };
		mMainPassCB.Lights[3 + i].FalloffStart = 1.0f;
		mMainPassCB.Lights[3 + i].FalloffEnd = 1000.f;
	}

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void DemoMain::LoadTextures()
{
	 texNames =
	{
		"bricksDiffuseMap",
		"tileDiffuseMap",
		"defaultDiffuseMap",
		"posterMap",
		"flare",
		"jixieren",
		"mainbg",
		"choosebg",
		"help",
		"container",

		"skyCubeMap",
	};

	 texFilenames =
	{
		L"Textures/bricks2.dds",
		L"Textures/map.dds",
		L"Textures/white1x1.dds",
		L"Textures/poster.dds",
		L"Textures/flare.dds",
		L"fbx/JIXIEPeople/Ani.fbm/male.dds",
		L"Textures/mainbg.dds",
		L"Textures/Choose.dds",
		L"Textures/help.dds",
		L"Textures/container.dds",

		L"Textures/grasscube1024.dds",
	};

	for (int i = 0; i < (int)texNames.size(); ++i)
	{
		auto texMap = std::make_unique<Texture>();
		texMap->Name = texNames[i];
		texMap->Filename = texFilenames[i];
		ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
			mCommandList.Get(), texMap->Filename.c_str(),
			texMap->Resource, texMap->UploadHeap));

		mTextures[texMap->Name] = std::move(texMap);
	}
}

void DemoMain::BuildRootSignature()
{
	//常量缓冲区  passConstant+shadowPassConstant
	CD3DX12_DESCRIPTOR_RANGE texTable0;
	////描述符类型  描述符数量  基准着色器寄存器	寄存器空间
	texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0);

	//物体纹理+天空盒纹理+阴影
	CD3DX12_DESCRIPTOR_RANGE texTable1;
	texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, texNames.size()+1, 2, 0);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[6];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsConstantBufferView(0);				//b0			常量缓冲区
	slotRootParameter[1].InitAsConstantBufferView(1);				//b1			pass缓冲区
	slotRootParameter[2].InitAsConstantBufferView(2);				//b2
	slotRootParameter[3].InitAsShaderResourceView(0, 1);			//t0 space1	    material struct缓冲	
	slotRootParameter[4].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[5].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);
	

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	//slotRootParameter数量5
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(6, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void DemoMain::BuildDescriptorHeaps()
{
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = texNames.size()+1;   //物体纹理+天空纹理+阴影纹理
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	for (size_t i = 0; i < texNames.size()-1; ++i)
	{
		auto ObjectTexture = mTextures[texNames[i]]->Resource;

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = ObjectTexture->GetDesc().MipLevels;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		srvDesc.Format = ObjectTexture->GetDesc().Format;
		md3dDevice->CreateShaderResourceView(ObjectTexture.Get(), &srvDesc, hDescriptor);
		//next descriptor
		hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);

	}

	auto skyCubeMap = mTextures[texNames[texNames.size()-1]]->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = skyCubeMap->GetDesc().MipLevels;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	srvDesc.Format = skyCubeMap->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(skyCubeMap.Get(), &srvDesc, hDescriptor);

	

	mSkyTexHeapIndex = texNames.size() - 1;
	mShadowMapHeapIndex = mSkyTexHeapIndex + 1;

	auto srvCpuStart = mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	auto srvGpuStart = mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	auto dsvCpuStart = mDsvHeap->GetCPUDescriptorHandleForHeapStart();

	mShadowMap->BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, mShadowMapHeapIndex, mCbvSrvUavDescriptorSize),
		CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, mShadowMapHeapIndex, mCbvSrvUavDescriptorSize),
		CD3DX12_CPU_DESCRIPTOR_HANDLE(dsvCpuStart, 1, mDsvDescriptorSize));

}

void DemoMain::BuildShadersAndInputLayout()
{
	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["skyVS"] = d3dUtil::CompileShader(L"Shaders\\Sky.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["skyPS"] = d3dUtil::CompileShader(L"Shaders\\Sky.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["shadowVS"] = d3dUtil::CompileShader(L"Shaders\\Shadows.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["shadowOpaquePS"] = d3dUtil::CompileShader(L"Shaders\\Shadows.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["shadowAlphaTestedPS"] = d3dUtil::CompileShader(L"Shaders\\Shadows.hlsl", alphaTestDefines, "PS", "ps_5_1");

	mShaders["UIVS"] = d3dUtil::CompileShader(L"Shaders\\UIPage.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["UIPS"] = d3dUtil::CompileShader(L"Shaders\\UIPage.hlsl", nullptr, "PS", "ps_5_1");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BONEINDICES", 0, DXGI_FORMAT_R32G32B32A32_SINT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

void DemoMain::BuildShapeGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(40.0f, 60.0f, 60, 40);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);
	//quad类和其他几何的区别在于他直接生成位于齐次裁剪空间的点，或者说我们在shader中不会对他的点进进行变换
	GeometryGenerator::MeshData quad = geoGen.CreateQuad(0.0f, 0.0f, 1.0f, 1.0f, 0.0);

	//
	// We are concatenating all the geometry into one big vertex/index buffer.  So
	// define the regions in the buffer each submesh covers.
	//

	// Cache the vertex offsets to each object in the concatenated vertex buffer.
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
	UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();
	UINT quadVertexOffset = cylinderVertexOffset + (UINT)cylinder.Vertices.size();

	// Cache the starting index for each object in the concatenated index buffer.
	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
	UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();
	UINT quadIndexOffset = cylinderIndexOffset + (UINT)cylinder.Indices32.size();

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;
	BoundingBox::CreateFromPoints(boxSubmesh.Bounds, box.Vertices.size(), &box.Vertices[0].Position, sizeof(GeometryGenerator::Vertex));


	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;
	BoundingBox::CreateFromPoints(gridSubmesh.Bounds, grid.Vertices.size(), &grid.Vertices[0].Position, sizeof(GeometryGenerator::Vertex));

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

	BoundingSphere::CreateFromPoints(sphereSubmesh.Sphere, sphere.Vertices.size(), &sphere.Vertices[0].Position, sizeof(GeometryGenerator::Vertex));
	
	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;
	BoundingBox::CreateFromPoints(cylinderSubmesh.Bounds, cylinder.Vertices.size(), &cylinder.Vertices[0].Position, sizeof(GeometryGenerator::Vertex));
	
	SubmeshGeometry quadSubmesh;
	quadSubmesh.IndexCount = (UINT)quad.Indices32.size();
	quadSubmesh.StartIndexLocation = quadIndexOffset;
	quadSubmesh.BaseVertexLocation = quadVertexOffset;
	
	//
	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	//


	auto totalVertexCount =
		box.Vertices.size() +
		grid.Vertices.size() +
		sphere.Vertices.size() +
		cylinder.Vertices.size()+
		quad.Vertices.size();

	std::vector<Vertex> vertices(totalVertexCount);

	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].TexC = box.Vertices[i].TexC;
	}

	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Normal = grid.Vertices[i].Normal;
		vertices[k].TexC = grid.Vertices[i].TexC;
	}

	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Normal = sphere.Vertices[i].Normal;
		vertices[k].TexC = sphere.Vertices[i].TexC;
	}

	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Normal = cylinder.Vertices[i].Normal;
		vertices[k].TexC = cylinder.Vertices[i].TexC;
	}
	for (size_t i = 0; i < quad.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = quad.Vertices[i].Position;
		vertices[k].Normal = quad.Vertices[i].Normal;
		vertices[k].TexC = quad.Vertices[i].TexC;
	}
	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));
	indices.insert(indices.end(), std::begin(quad.GetIndices16()), std::end(quad.GetIndices16()));

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;
	geo->DrawArgs["quad"] = quadSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}

void DemoMain::BuildSkullGeometry()
{
	std::ifstream fin("Models/skull.txt");

	if (!fin)
	{
		MessageBox(0, L"Models/skull.txt not found.", 0, 0);
		return;
	}

	UINT vcount = 0;
	UINT tcount = 0;
	std::string ignore;

	fin >> ignore >> vcount;
	fin >> ignore >> tcount;
	fin >> ignore >> ignore >> ignore >> ignore;

	XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
	XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

	XMVECTOR vMin = XMLoadFloat3(&vMinf3);
	XMVECTOR vMax = XMLoadFloat3(&vMaxf3);

	std::vector<Vertex> vertices(vcount);
	for (UINT i = 0; i < vcount; ++i)
	{
		fin >> vertices[i].Pos.x >> vertices[i].Pos.y >> vertices[i].Pos.z;
		fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;

		vertices[i].TexC = { 0.0f, 0.0f };

		XMVECTOR P = XMLoadFloat3(&vertices[i].Pos);

		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}

	BoundingBox bounds;
	XMStoreFloat3(&bounds.Center, 0.5f * (vMin + vMax));
	XMStoreFloat3(&bounds.Extents, 0.5f * (vMax - vMin));

	fin >> ignore;
	fin >> ignore;
	fin >> ignore;

	std::vector<std::int32_t> indices(3 * tcount);
	for (UINT i = 0; i < tcount; ++i)
	{
		fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
	}

	fin.close();

	//
	// Pack the indices of all the meshes into one index buffer.
	//

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::int32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "skullGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;
	submesh.Bounds = bounds;

	geo->DrawArgs["skull"] = submesh;

	mGeometries[geo->Name] = std::move(geo);
}

void DemoMain::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	//
	// PSO for opaque objects.
	//
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

	//
	// PSO for sky.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPsoDesc = opaquePsoDesc;

	// The camera is inside the sky sphere, so just turn off culling.
	skyPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	// Make sure the depth function is LESS_EQUAL and not just LESS.  
	// Otherwise, the normalized depth values at z = 1 (NDC) will 
	// fail the depth test if the depth buffer was cleared to 1.
	skyPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	skyPsoDesc.pRootSignature = mRootSignature.Get();
	skyPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["skyVS"]->GetBufferPointer()),
		mShaders["skyVS"]->GetBufferSize()
	};
	skyPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["skyPS"]->GetBufferPointer()),
		mShaders["skyPS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skyPsoDesc, IID_PPV_ARGS(&mPSOs["sky"])));

	//
	//PSO for shadowMap.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC smapPsoDesc = opaquePsoDesc;

	/*以下三个参数是用于控制阴影偏移的深度偏移发生在光栅化期间(裁剪阶段之后）不会对几何体裁剪造成影响*/
	smapPsoDesc.RasterizerState.DepthBias = 100000;
	smapPsoDesc.RasterizerState.DepthBiasClamp = 0.0f;
	smapPsoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;						//斜率缩放偏移量  像素的深度值与偏移值相加
	smapPsoDesc.pRootSignature = mRootSignature.Get();
	smapPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["shadowVS"]->GetBufferPointer()),
		mShaders["shadowVS"]->GetBufferSize()
	};
	smapPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["shadowOpaquePS"]->GetBufferPointer()),
		mShaders["shadowOpaquePS"]->GetBufferSize()
	};

	//ShadowMap渲染午需涉及渲染目标.
	smapPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	smapPsoDesc.NumRenderTargets = 0;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&smapPsoDesc, IID_PPV_ARGS(&mPSOs["shadow_opaque"])));


	D3D12_GRAPHICS_PIPELINE_STATE_DESC UIPsoDesc = opaquePsoDesc;
	UIPsoDesc.pRootSignature = mRootSignature.Get();
	UIPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["UIVS"]->GetBufferPointer()),
		mShaders["UIVS"]->GetBufferSize()
	};
	UIPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["UIPS"]->GetBufferPointer()),
		mShaders["UIPS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&UIPsoDesc, IID_PPV_ARGS(&mPSOs["UI"])));
}

void DemoMain::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		//当前设备 常量数量 物体数量 材质数量
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
			8, (UINT)mAllRitems.size() + 30, 1, (UINT)mMaterials.size(), false));
	}
}

void DemoMain::BuildMaterials()
{
	UINT TotalMaterialCount = 0;
	for (int i = 0; i < texNames.size(); i++)
	{
		auto mat = std::make_unique<Material>();
		mat->Name = texNames[i];
		mat->MatCBIndex = TotalMaterialCount++;
		mat->DiffuseSrvHeapIndex = i;								 //漫反射纹理在SRV堆中的索引
		//normal 暂未指定
		mat->NormalSrvHeapIndex = 0;
		mat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);		 //漫反射反照率
		mat->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);				 //菲涅尔因子
		mat->Roughness = 0.3f;										 //粗糙度
		mMaterials[mat->Name] = std::move(mat);
	}
}

void DemoMain::BuildRenderItems()
{
	auto skyRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&skyRitem->World, XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
	skyRitem->TexTransform = MathHelper::Identity4x4();
	skyRitem->ObjCBIndex = ObjCBIndex++;
	skyRitem->Mat = mMaterials["skyCubeMap"].get();
	skyRitem->Geo = mGeometries["shapeGeo"].get();
	skyRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skyRitem->IndexCount = skyRitem->Geo->DrawArgs["sphere"].IndexCount;
	skyRitem->StartIndexLocation = skyRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
	skyRitem->BaseVertexLocation = skyRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::Sky].push_back(skyRitem.get());
	mAllRitems.push_back(std::move(skyRitem));

	auto leftBox = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&leftBox->World, XMMatrixScaling(6.0f, 4.0f, 60.0f) * XMMatrixTranslation(-17.0f, 2.f, 0.0f));
	XMStoreFloat4x4(&leftBox->TexTransform, XMMatrixScaling(4.0f, 2.0f, 1.0f));
	leftBox->ObjCBIndex = ObjCBIndex++;
	leftBox->OpaqueObjIndex = OpaqueObjIndex++;
	leftBox->Mat = mMaterials["container"].get();
	leftBox->Geo = mGeometries["shapeGeo"].get();
	leftBox->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	leftBox->IndexCount = leftBox->Geo->DrawArgs["box"].IndexCount;
	leftBox->StartIndexLocation = leftBox->Geo->DrawArgs["box"].StartIndexLocation;
	leftBox->BaseVertexLocation = leftBox->Geo->DrawArgs["box"].BaseVertexLocation;
	leftBox->needDetectCollision = 1;
	leftBox->boxType = 1;
	BoundingOrientedBox::CreateFromBoundingBox(leftBox->orBounds, leftBox->Geo->DrawArgs["box"].Bounds);
	leftBox->orBounds.Extents.x *= 6.0f;
	leftBox->orBounds.Extents.y *= 4.0f;
	leftBox->orBounds.Extents.z *= 60.0f;
	leftBox->orBounds.Center.x = -17.0f;
	leftBox->orBounds.Center.y = 2.0f;
	leftBox->orBounds.Center.z = 0.0f;

	mRitemLayer[(int)RenderLayer::Opaque].push_back(leftBox.get());
	mAllRitems.push_back(std::move(leftBox));

	auto rightBox = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&rightBox->World, XMMatrixScaling(6.0f, 4.0f, 60.0f) * XMMatrixTranslation(17.0f, 2.f, 0));
	XMStoreFloat4x4(&rightBox->TexTransform, XMMatrixScaling(4.0f, 2.0f, 1.0f));
	rightBox->ObjCBIndex = ObjCBIndex++;
	rightBox->OpaqueObjIndex = OpaqueObjIndex++;
	rightBox->Mat = mMaterials["container"].get();
	rightBox->Geo = mGeometries["shapeGeo"].get();
	rightBox->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	rightBox->IndexCount = rightBox->Geo->DrawArgs["box"].IndexCount;
	rightBox->StartIndexLocation = rightBox->Geo->DrawArgs["box"].StartIndexLocation;
	rightBox->BaseVertexLocation = rightBox->Geo->DrawArgs["box"].BaseVertexLocation;
	rightBox->needDetectCollision = 1;
	rightBox->boxType = 1;
	BoundingOrientedBox::CreateFromBoundingBox(rightBox->orBounds, rightBox->Geo->DrawArgs["box"].Bounds);
	rightBox->orBounds.Extents.x *= 6.0f;
	rightBox->orBounds.Extents.y *= 4.0f;
	rightBox->orBounds.Extents.z *= 60.0f;
	rightBox->orBounds.Center.x = 17.0f;
	rightBox->orBounds.Center.y = 2.0f;
	rightBox->orBounds.Center.z = 0.0f;

	mRitemLayer[(int)RenderLayer::Opaque].push_back(rightBox.get());
	mAllRitems.push_back(std::move(rightBox));

	auto centreBox = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&centreBox->World, XMMatrixScaling(4.0f, 4.0f, 4.0f) * XMMatrixTranslation(0.0f, 2.f, 0.0f));
	XMStoreFloat4x4(&centreBox->TexTransform, XMMatrixScaling(2.0f, 2.0f, 2.0f));
	centreBox->ObjCBIndex = ObjCBIndex++;
	centreBox->OpaqueObjIndex = OpaqueObjIndex++;
	centreBox->Mat = mMaterials["bricksDiffuseMap"].get();
	centreBox->Geo = mGeometries["shapeGeo"].get();
	centreBox->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	centreBox->IndexCount = centreBox->Geo->DrawArgs["box"].IndexCount;
	centreBox->StartIndexLocation = centreBox->Geo->DrawArgs["box"].StartIndexLocation;
	centreBox->BaseVertexLocation = centreBox->Geo->DrawArgs["box"].BaseVertexLocation;
	centreBox->needDetectCollision = 1;
	centreBox->boxType = 1;
	BoundingOrientedBox::CreateFromBoundingBox(centreBox->orBounds, centreBox->Geo->DrawArgs["box"].Bounds);
	centreBox->orBounds.Extents.x *= 4.0f;
	centreBox->orBounds.Extents.y *= 4.0f;
	centreBox->orBounds.Extents.z *= 4.0f;
	centreBox->orBounds.Center.x = 0.0f;
	centreBox->orBounds.Center.y = 2.0f;
	centreBox->orBounds.Center.z = 0.0f;

	mRitemLayer[(int)RenderLayer::Opaque].push_back(centreBox.get());
	mAllRitems.push_back(std::move(centreBox));

	centreBox = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&centreBox->World, XMMatrixScaling(4.0f, 4.0f, 4.0f) * XMMatrixTranslation(-10.0f, 2.f, 14.0f));
	XMStoreFloat4x4(&centreBox->TexTransform, XMMatrixScaling(2.0f, 2.0f, 2.0f));
	centreBox->ObjCBIndex = ObjCBIndex++;
	centreBox->OpaqueObjIndex = OpaqueObjIndex++;
	centreBox->Mat = mMaterials["bricksDiffuseMap"].get();
	centreBox->Geo = mGeometries["shapeGeo"].get();
	centreBox->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	centreBox->IndexCount = centreBox->Geo->DrawArgs["box"].IndexCount;
	centreBox->StartIndexLocation = centreBox->Geo->DrawArgs["box"].StartIndexLocation;
	centreBox->BaseVertexLocation = centreBox->Geo->DrawArgs["box"].BaseVertexLocation;
	centreBox->needDetectCollision = 1;
	centreBox->boxType = 1;
	BoundingOrientedBox::CreateFromBoundingBox(centreBox->orBounds, centreBox->Geo->DrawArgs["box"].Bounds);
	centreBox->orBounds.Extents.x *= 4.0f;
	centreBox->orBounds.Extents.y *= 4.0f;
	centreBox->orBounds.Extents.z *= 4.0f;
	centreBox->orBounds.Center.x = -10.0f;
	centreBox->orBounds.Center.y = 2.0f;
	centreBox->orBounds.Center.z = 14.0f;

	mRitemLayer[(int)RenderLayer::Opaque].push_back(centreBox.get());
	mAllRitems.push_back(std::move(centreBox));


	centreBox = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&centreBox->World, XMMatrixScaling(4.0f, 4.0f, 4.0f)* XMMatrixTranslation(10.0f, 2.f, -14.0f));
	XMStoreFloat4x4(&centreBox->TexTransform, XMMatrixScaling(2.0f, 2.0f, 2.0f));
	centreBox->ObjCBIndex = ObjCBIndex++;
	centreBox->OpaqueObjIndex = OpaqueObjIndex++;
	centreBox->Mat = mMaterials["bricksDiffuseMap"].get();
	centreBox->Geo = mGeometries["shapeGeo"].get();
	centreBox->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	centreBox->IndexCount = centreBox->Geo->DrawArgs["box"].IndexCount;
	centreBox->StartIndexLocation = centreBox->Geo->DrawArgs["box"].StartIndexLocation;
	centreBox->BaseVertexLocation = centreBox->Geo->DrawArgs["box"].BaseVertexLocation;
	centreBox->needDetectCollision = 1;
	centreBox->boxType = 1;
	BoundingOrientedBox::CreateFromBoundingBox(centreBox->orBounds, centreBox->Geo->DrawArgs["box"].Bounds);
	centreBox->orBounds.Extents.x *= 4.0f;
	centreBox->orBounds.Extents.y *= 4.0f;
	centreBox->orBounds.Extents.z *= 4.0f;
	centreBox->orBounds.Center.x = 10.0f;
	centreBox->orBounds.Center.y = 2.0f;
	centreBox->orBounds.Center.z = -14.0f;

	mRitemLayer[(int)RenderLayer::Opaque].push_back(centreBox.get());
	mAllRitems.push_back(std::move(centreBox));

	//40*60  
	auto gridRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&gridRitem->World, XMMatrixScaling(1.0f, 1.0f, 1.f));
	XMStoreFloat4x4(&gridRitem->TexTransform, XMMatrixScaling(10.0f, 10.0f, 1.0f));
	gridRitem->ObjCBIndex = ObjCBIndex++;
	gridRitem->OpaqueObjIndex = OpaqueObjIndex++;
	gridRitem->Mat = mMaterials["tileDiffuseMap"].get();
	gridRitem->Geo = mGeometries["shapeGeo"].get();
	gridRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	gridRitem->needDetectCollision = 1;
	gridRitem->boxType = 1;
	BoundingOrientedBox::CreateFromBoundingBox(gridRitem->orBounds, gridRitem->Geo->DrawArgs["grid"].Bounds);
	gridRitem->orBounds.Extents.x *= 1.0f;
	gridRitem->orBounds.Extents.y *= 1.0f;
	gridRitem->orBounds.Extents.z *= 1.0f;
	gridRitem->orBounds.Center.x = 0.0f;
	gridRitem->orBounds.Center.y = 0.0f;
	gridRitem->orBounds.Center.z = 0.0f;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(gridRitem.get());
	mAllRitems.push_back(std::move(gridRitem));

	XMMATRIX brickTexTransform = XMMatrixScaling(1.5f, 2.0f, 1.0f);
	for (int i = 0; i < 5; ++i)
	{
		auto leftCylRitem = std::make_unique<RenderItem>();
		auto rightCylRitem = std::make_unique<RenderItem>();
		auto leftSphereRitem = std::make_unique<RenderItem>();
		auto rightSphereRitem = std::make_unique<RenderItem>();

		XMMATRIX leftCylWorld = XMMatrixTranslation(-5.0f, 1.5f, -9.0f + i * 6.0f);
		XMMATRIX rightCylWorld = XMMatrixTranslation(+5.0f, 1.5f, -9.0f + i * 6.0f);

		XMMATRIX leftSphereWorld = XMMatrixTranslation(-5.0f, 3.5f, -9.0f + i * 6.0f);
		XMMATRIX rightSphereWorld = XMMatrixTranslation(+5.0f, 3.5f, -9.0f + i * 6.0f);

		XMStoreFloat4x4(&leftCylRitem->World, rightCylWorld);
		XMStoreFloat4x4(&leftCylRitem->TexTransform, brickTexTransform);
		leftCylRitem->ObjCBIndex = ObjCBIndex++;
		leftCylRitem->OpaqueObjIndex = OpaqueObjIndex++;
		leftCylRitem->Mat = mMaterials["bricksDiffuseMap"].get();
		leftCylRitem->Geo = mGeometries["shapeGeo"].get();
		leftCylRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftCylRitem->IndexCount = leftCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		leftCylRitem->StartIndexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		leftCylRitem->BaseVertexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;
		leftCylRitem->needDetectCollision = 1;
		leftCylRitem->boxType = 1;
		BoundingOrientedBox::CreateFromBoundingBox(leftCylRitem->orBounds, leftCylRitem->Geo->DrawArgs["cylinder"].Bounds);
		leftCylRitem->orBounds.Extents.x *= 1.0f;
		leftCylRitem->orBounds.Extents.y *= 1.0f;
		leftCylRitem->orBounds.Extents.z *= 1.0f;
		leftCylRitem->orBounds.Center.x = -5.0f;
		leftCylRitem->orBounds.Center.y = 1.5f;
		leftCylRitem->orBounds.Center.z = -9.0f + i * 6.0f;


		XMStoreFloat4x4(&rightCylRitem->World, leftCylWorld);
		XMStoreFloat4x4(&rightCylRitem->TexTransform, brickTexTransform);
		rightCylRitem->ObjCBIndex = ObjCBIndex++;
		rightCylRitem->OpaqueObjIndex = OpaqueObjIndex++;
		rightCylRitem->Mat = mMaterials["bricksDiffuseMap"].get();
		rightCylRitem->Geo = mGeometries["shapeGeo"].get();
		rightCylRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightCylRitem->IndexCount = rightCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		rightCylRitem->StartIndexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		rightCylRitem->BaseVertexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;
		rightCylRitem->needDetectCollision = 1;
		rightCylRitem->boxType = 1;
		BoundingOrientedBox::CreateFromBoundingBox(rightCylRitem->orBounds, rightCylRitem->Geo->DrawArgs["cylinder"].Bounds);
		rightCylRitem->orBounds.Extents.x *= 1.0f;
		rightCylRitem->orBounds.Extents.y *= 1.0f;
		rightCylRitem->orBounds.Extents.z *= 1.0f;
		rightCylRitem->orBounds.Center.x = +5.0f;
		rightCylRitem->orBounds.Center.y = 1.5f;
		rightCylRitem->orBounds.Center.z = -9.0f + i * 6.0f;

		XMStoreFloat4x4(&leftSphereRitem->World, leftSphereWorld);
		leftSphereRitem->TexTransform = MathHelper::Identity4x4();
		leftSphereRitem->ObjCBIndex = ObjCBIndex++;
		leftSphereRitem->OpaqueObjIndex = OpaqueObjIndex++;
		leftSphereRitem->Mat = mMaterials["defaultDiffuseMap"].get();
		leftSphereRitem->Geo = mGeometries["shapeGeo"].get();
		leftSphereRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftSphereRitem->IndexCount = leftSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		leftSphereRitem->StartIndexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		leftSphereRitem->BaseVertexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
		leftSphereRitem->needDetectCollision = 1;
		leftSphereRitem->boxType = 2;		//bounding sphere
		leftSphereRitem->obSphere.Center.x =-5.0f;
		leftSphereRitem->obSphere.Center.y = 3.5f;
		leftSphereRitem->obSphere.Center.z = -9.0f + i * 6.0f;
		leftSphereRitem->obSphere.Radius = 0.5f;


		XMStoreFloat4x4(&rightSphereRitem->World, rightSphereWorld);
		rightSphereRitem->TexTransform = MathHelper::Identity4x4();
		rightSphereRitem->ObjCBIndex = ObjCBIndex++;
		rightSphereRitem->OpaqueObjIndex = OpaqueObjIndex++;
		rightSphereRitem->Mat = mMaterials["defaultDiffuseMap"].get();
		rightSphereRitem->Geo = mGeometries["shapeGeo"].get();
		rightSphereRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightSphereRitem->IndexCount = rightSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		rightSphereRitem->StartIndexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		rightSphereRitem->BaseVertexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
		leftSphereRitem->boxType = 2;		//bounding sphere
		leftSphereRitem->obSphere.Center.x = 5.0f;
		leftSphereRitem->obSphere.Center.y = 3.5f;
		leftSphereRitem->obSphere.Center.z = -9.0f + i * 6.0f;
		leftSphereRitem->obSphere.Radius = 0.5f;

		mRitemLayer[(int)RenderLayer::Opaque].push_back(leftCylRitem.get());
		mRitemLayer[(int)RenderLayer::Opaque].push_back(rightCylRitem.get());
		mRitemLayer[(int)RenderLayer::Opaque].push_back(leftSphereRitem.get());
		mRitemLayer[(int)RenderLayer::Opaque].push_back(rightSphereRitem.get());

		mAllRitems.push_back(std::move(leftCylRitem));
		mAllRitems.push_back(std::move(rightCylRitem));
		mAllRitems.push_back(std::move(leftSphereRitem));
		mAllRitems.push_back(std::move(rightSphereRitem));
	}

	auto frontPicture = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&frontPicture->World, XMMatrixScaling(0.7f, 1.0f, 0.1f) *
		XMMatrixRotationX(-MathHelper::Pi * 0.5f) *
		XMMatrixTranslation(0.0f, 3.0f, 30.0f));
	XMStoreFloat4x4(&frontPicture->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	frontPicture->ObjCBIndex = ObjCBIndex++;
	frontPicture->OpaqueObjIndex = OpaqueObjIndex++;
	frontPicture->Mat = mMaterials["posterMap"].get();
	frontPicture->Geo = mGeometries["shapeGeo"].get();
	frontPicture->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	frontPicture->IndexCount = frontPicture->Geo->DrawArgs["grid"].IndexCount;
	frontPicture->StartIndexLocation = frontPicture->Geo->DrawArgs["grid"].StartIndexLocation;
	frontPicture->BaseVertexLocation = frontPicture->Geo->DrawArgs["grid"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::Opaque].push_back(frontPicture.get());
	mAllRitems.push_back(std::move(frontPicture));

	auto backPicture = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&backPicture->World, XMMatrixScaling(0.7f, 1.0f, 0.1f) *
		XMMatrixRotationX(MathHelper::Pi * 0.5f) * XMMatrixRotationZ(-MathHelper::Pi) *
		XMMatrixTranslation(0.0f, 3.0f, -30.0f));
	XMStoreFloat4x4(&backPicture->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	backPicture->ObjCBIndex = ObjCBIndex++;
	backPicture->OpaqueObjIndex = OpaqueObjIndex++;
	backPicture->Mat = mMaterials["posterMap"].get();
	backPicture->Geo = mGeometries["shapeGeo"].get();
	backPicture->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	backPicture->IndexCount = backPicture->Geo->DrawArgs["grid"].IndexCount;
	backPicture->StartIndexLocation = backPicture->Geo->DrawArgs["grid"].StartIndexLocation;
	backPicture->BaseVertexLocation = backPicture->Geo->DrawArgs["grid"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::Opaque].push_back(backPicture.get());
	mAllRitems.push_back(std::move(backPicture));
}

void DemoMain::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();

	// For each render item...
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;

		cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);

		if (ri->isMesh)
		{
			cmdList->DrawInstanced(ri->VertexCount, 1, ri->BaseVertexLocation, 0);
		}
		else
		{
			cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
			cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
		}
	}
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> DemoMain::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC shadow(
		6, // shaderRegister
		D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressW
		0.0f,                               // mipLODBias
		16,                                 // maxAnisotropy
		D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK);

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp,
		shadow
	};
}

/*****************************************************************************
检测所有需要进行碰撞检测的实体，如果发生碰撞，则保存到实体的collisionDetected
结构体矢量中。如果没有碰撞，但在结构体中还有与该实体发生碰撞的信息，则要删除这
些信息。
********************************************************************************/
void DemoMain::DetectCollision()
{
	for (int i = 0; i < mAllRitems.size(); i++)
	{
		for (int j = i + 1; j < mAllRitems.size(); j++)
		{
			if (i == j)
				continue;
			auto& m = mAllRitems[i];
			auto& n = mAllRitems[j];

			//m n两个物体都需要检测碰撞
			if (m->needDetectCollision == 1 && n->needDetectCollision == 1) {
				if ((n->boxType == 1 && m->boxType == 1 && m->orBounds.Contains(n->orBounds)) ||
					(n->boxType == 2 && m->boxType == 1 && m->orBounds.Contains(n->obSphere)) ||
					(n->boxType == 1 && m->boxType == 2 && n->orBounds.Contains(m->obSphere)) ||
					(n->boxType == 2 && m->boxType == 2 && n->obSphere.Contains(m->obSphere)))
				{
					//该物体是否已被标记为与n物体碰撞，如果是则不加入，否则加入
					int k;
					for (k = 0; k < n->collisionDetected.size(); k++) {
						auto& t = n->collisionDetected[k];
						if (t->collisionObjectIndex == m->ObjCBIndex) {
							break;
						}
					}
					//插入一个碰撞的物体的信息
					if (k == n->collisionDetected.size()) {
						auto collision = new CollisionDetect;
						collision->collisionObjectIndex = m->ObjCBIndex;
						n->collisionDetected.push_back(collision);
					}

				}
			}
			else
			{
				//该物体是否此前被标记为与m物体碰撞，如是则删除，否则不处理
				for (int k = 0; k < m->collisionDetected.size(); k++) {
					auto& t = m->collisionDetected[k];
					if (t->collisionObjectIndex == n->ObjCBIndex) {
						m->collisionDetected.erase(m->collisionDetected.begin() + k);
						break;
					}
				}
				//该物体是否此前被标记为与m物体碰撞，如是则删除，否则不处理
				for (int k = 0; k < n->collisionDetected.size(); k++) {
					auto& t = n->collisionDetected[k];
					if (t->collisionObjectIndex == m->ObjCBIndex) {
						n->collisionDetected.erase(n->collisionDetected.begin() + k);
						break;
					}
				}
			}
		}
	}
}

void DemoMain::AddBullet(int x, int y, XMFLOAT3 cameraPos, XMFLOAT3 lookDir)
{
	auto bulletRitem = std::make_unique<RenderItem>();

	XMStoreFloat4x4(&bulletRitem->World, XMMatrixScaling(0.5f, 0.5f, 0.5f) * XMMatrixTranslation(cameraPos.x, cameraPos.y, cameraPos.z));
	bulletRitem->TexTransform = MathHelper::Identity4x4();
	bulletRitem->ObjCBIndex = ObjCBIndex++;
	bulletRitem->OpaqueObjIndex = OpaqueObjIndex++;
	bulletRitem->isBullet = TRUE;
	bulletRitem->Mat = mMaterials["flare"].get();
	bulletRitem->Geo = mGeometries["shapeGeo"].get();
	bulletRitem->needDetectCollision = 1;	//需要检测碰撞
	bulletRitem->boxType = 2;//Sphere
	bulletRitem->obSphere.Center.x = cameraPos.x;
	bulletRitem->obSphere.Center.y = cameraPos.y;
	bulletRitem->obSphere.Center.z = cameraPos.z;
	bulletRitem->obSphere.Radius = 0.5f;
	bulletRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	bulletRitem->IndexCount = bulletRitem->Geo->DrawArgs["sphere"].IndexCount;
	bulletRitem->StartIndexLocation = bulletRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
	bulletRitem->BaseVertexLocation = bulletRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
	bulletRitem->lookDirection = { lookDir.x - cameraPos.x,lookDir.y - cameraPos.y,lookDir.z - cameraPos.z };
	mRitemLayer[(int)RenderLayer::Opaque].push_back(bulletRitem.get());
	mAllRitems.push_back(std::move(bulletRitem));
}

void DemoMain::UpdateBullet(RenderItem* b, const GameTimer& gt)
{
	float PosZ = b->World._43 + 50 * b->lookDirection.z * gt.DeltaTime();  //x=v0+at
	float PosX = b->World._41 + 50 * b->lookDirection.x * gt.DeltaTime();
	float PosY = b->World._42 + 50 * b->lookDirection.y * gt.DeltaTime();
	//float PosY = b->World._42 - 200.f * gt.DeltaTime() * gt.DeltaTime(); //y=1/2*g*t*t

		//对bullet 位移
	XMMATRIX bulletOffset = XMMatrixScaling(0.5f, 0.5f, 0.5f) * XMMatrixTranslation(PosX, PosY, PosZ);
	XMStoreFloat4x4(&b->World, bulletOffset);
	XMStoreFloat3(&b->obSphere.Center, { PosX ,PosY, PosZ });
	//updateBombPos(PosX,PosY,PosZ);

	b->NumFramesDirty = gNumFrameResources;
}

void DemoMain::UpdateShadowTransform(const GameTimer& gt)
{
	//我们只设置第一个光源会导致阴影.
	XMVECTOR lightDir = XMLoadFloat3(&mRotatedLightDirections[0]);
	XMVECTOR lightPos = -2.0f * mSceneBounds.Radius * lightDir;
	XMVECTOR targetPos = XMLoadFloat3(&mSceneBounds.Center);
	XMVECTOR lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	//建立光照camera的观察矩阵
	XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);
	XMStoreFloat3(&mLightPosW, lightPos);

	//将绑定圆转换到灯光坐标系
	XMFLOAT3 sphereCenterLS;
	XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, lightView));

	//因为在灯光坐标系进行正交投影  所以我们的frustum是一个长方体
	float l = sphereCenterLS.x - mSceneBounds.Radius;
	float b = sphereCenterLS.y - mSceneBounds.Radius;
	float n = sphereCenterLS.z - mSceneBounds.Radius;
	float r = sphereCenterLS.x + mSceneBounds.Radius;
	float t = sphereCenterLS.y + mSceneBounds.Radius;
	float f = sphereCenterLS.z + mSceneBounds.Radius;

	mLightNearZ = n;
	mLightFarZ = f;
	//正交投影矩阵
	XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

	// 转换到贴图空间的矩阵 与 视矩阵 投影矩阵结合为viewprojtex transform
	//从坐标范围【-1，1】的NDC空间转换到【0，1】的纹理空间
	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);
	//通过mShadowTransform这个矩阵我们可以将世界坐标系下的物体转换到shadowmap空间。
	XMMATRIX S = lightView * lightProj * T;
	XMStoreFloat4x4(&mLightView, lightView);
	XMStoreFloat4x4(&mLightProj, lightProj);
	XMStoreFloat4x4(&mShadowTransform, S);
}

void DemoMain::DrawSceneToShadow()
{
	mCommandList->RSSetViewports(1, &mShadowMap->Viewport());
	mCommandList->RSSetScissorRects(1, &mShadowMap->ScissorRect());
	// 转换我们的shadowmap的状态为DEPTH_WRITE,因为我们将使用它作为深度渲染目标视图
	mCommandList->ResourceBarrier(1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			mShadowMap->Resource(),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_STATE_DEPTH_WRITE
		)
	);
	UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));
	// 清理后台缓冲区以及深度缓冲区深度缓冲
	mCommandList->ClearDepthStencilView(mShadowMap->Dsv(),
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	//设为dsv，注意这里我们的rtv为nullptr 因为我们只需要渲染深度信息即可
	//设置了0个渲染目标，其实是禁止颜色写入的操作
	mCommandList->OMSetRenderTargets(0, nullptr, false, &mShadowMap->Dsv());
	//绑定 shadowmap 的pass缓冲区.
	auto passCB = mCurrFrameResource->PassCB->Resource();

	//注意地址是+1*passCBByteSize  因为前面是场景中主相机的的PassConstants
	D3D12_GPU_VIRTUAL_ADDRESS passCBAddress = passCB->GetGPUVirtualAddress() + 1 * passCBByteSize;
	mCommandList->SetGraphicsRootConstantBufferView(1, passCBAddress);

	mCommandList->SetPipelineState(mPSOs["shadow_opaque"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

	// 写入完毕后转换状态为可读状态，使我们能够从着色器中读取纹理
	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			mShadowMap->Resource(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			D3D12_RESOURCE_STATE_GENERIC_READ
		)
	);
	mCommandList->SetPipelineState(mPSOs["opaque"].Get());
}
void DemoMain::UpdateShadowPassCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&mLightView);
	XMMATRIX proj = XMLoadFloat4x4(&mLightProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	UINT w = mShadowMap->Width();
	UINT h = mShadowMap->Height();

	XMStoreFloat4x4(&mShadowPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mShadowPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mShadowPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mShadowPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mShadowPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mShadowPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mShadowPassCB.EyePosW = mLightPosW;
	mShadowPassCB.RenderTargetSize = XMFLOAT2((float)w, (float)h);
	mShadowPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / w, 1.0f / h);
	mShadowPassCB.NearZ = mLightNearZ;
	mShadowPassCB.FarZ = mLightFarZ;

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(1, mShadowPassCB);
}

void DemoMain::CreateRtvAndDsvDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));

	// Add +1 DSV for shadow map.
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 2;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));
}

void DemoMain::LoadStaticAIMesh()
{
	fbxFileName = "fbx/JIXIEPeople/rzr.FBX";
	fileLoader.SetReadFile(fbxFileName, FBX_SkinMesh);  //以指定格式读取fbx文件
	fileLoader.ReadFile();	//读取
	vertices.insert(vertices.end(), std::begin(fileLoader.GetSkinnedVertex()), std::end(fileLoader.GetSkinnedVertex())); //读取骨骼顶点到vertices中存储
	mFbxMeshData.m_Skeletons["AIJoint"].insert(mFbxMeshData.m_Skeletons["AIJoint"].end(), std::begin(fileLoader.GetSkeleton()), std::end(fileLoader.GetSkeleton()));
	mFbxMeshData.m_SubMesh.insert(mFbxMeshData.m_SubMesh.end(), std::begin(fileLoader.GetSubmesh()), std::end(fileLoader.GetSubmesh()));
	mFbxMeshData.m_Material.insert(mFbxMeshData.m_Material.end(), std::begin(fileLoader.GetMaterial()), std::end(fileLoader.GetMaterial()));

	UINT vertexCount = (UINT)vertices.size();

	UINT vbByteSize = vertexCount * sizeof(SkinnedVertex);
	UINT indexCount = (UINT)indices.size();
	UINT ibByteSize = indexCount * sizeof(std::uint16_t);
	auto AI_Ritem = std::make_unique<MeshGeometry>();
	AI_Ritem->Name = "AI_01";
	AI_Ritem->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, AI_Ritem->VertexBufferUploader);
	/*Lina->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, Lina->IndexBufferUploader);*/
	AI_Ritem->VertexByteStride = sizeof(SkinnedVertex);
	AI_Ritem->VertexBufferByteSize = vbByteSize;
	AI_Ritem->IndexFormat = DXGI_FORMAT_R16_UINT;
	AI_Ritem->IndexBufferByteSize = ibByteSize;

	for (int i = 0; i < mFbxMeshData.m_SubMesh.size(); ++i)
	{
		BoundingBox::CreateFromPoints(mFbxMeshData.m_SubMesh[i].Bounds, mFbxMeshData.m_SubMesh[i].VertexCount,
			&vertices[mFbxMeshData.m_SubMesh[i].BaseVertexLocation].Pos, sizeof(SkinnedVertex));
		std::ostringstream os;
		os << "submesh" << i;
		AI_Ritem->DrawArgs[os.str()] = mFbxMeshData.m_SubMesh[i];
	}
	mGeometries[AI_Ritem->Name] = std::move(AI_Ritem);
	CleanVertex();
}

void DemoMain::BuildAIItems(int num)
{

	int index[100];
	srand((unsigned int)time(NULL));//初始化种子为随机值
	for (int j = 0; j < num; j++)
	{
		index[j] = rand() % 6;
		for (int i = 0; i < j; i++)
		{
			if (index[j] == index[i])
			{
				i = -1;
				index[j] = rand() % 6;
			}
		}
	}

	for (int j = 0; j < num; j++)
	{
		//角色人物
		for (int i = 0; i < mGeometries["AI_01"]->DrawArgs.size(); ++i)
		{

			auto EnemyRitem = std::make_unique<RenderItem>();
			XMStoreFloat4x4(&EnemyRitem->World,
				XMMatrixScaling(.008f, .008f, .008f) *
				XMMatrixRotationX(MathHelper::Pi / 2) *
				XMMatrixTranslation(mAIInitialPos[index[j]].x, mAIInitialPos[index[j]].y, mAIInitialPos[index[j]].z));
			XMStoreFloat4x4(&EnemyRitem->TexTransform, XMMatrixRotationZ(MathHelper::Pi) * XMMatrixRotationY(MathHelper::Pi));
			EnemyRitem->ObjCBIndex = ObjCBIndex++;
			EnemyRitem->AIIndex = AIIndex++;
			EnemyRitem->Mat = mMaterials["jixieren"].get();; //Todo
			EnemyRitem->Geo = mGeometries["AI_01"].get();
			EnemyRitem->isAI = TRUE;
			EnemyRitem->isMesh = TRUE;
			EnemyRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			std::ostringstream os;
			os << "submesh" << i;
			EnemyRitem->needDetectCollision = 1;	//需要检测碰撞
			EnemyRitem->boxType = 1;//OOBB
			BoundingOrientedBox::CreateFromBoundingBox(EnemyRitem->orBounds, EnemyRitem->Geo->DrawArgs[os.str()].Bounds);
			EnemyRitem->orBounds.Extents.x *= .01f;
			EnemyRitem->orBounds.Extents.y *= .01f;
			EnemyRitem->orBounds.Extents.z *= .01f;
			EnemyRitem->orBounds.Center.x = mAIInitialPos[index[j]].x;
			EnemyRitem->orBounds.Center.y = mAIInitialPos[index[j]].y + 1.38;
			EnemyRitem->orBounds.Center.z = mAIInitialPos[index[j]].z;
			EnemyRitem->BaseVertexLocation = EnemyRitem->Geo->DrawArgs[os.str()].BaseVertexLocation;
			EnemyRitem->VertexCount = EnemyRitem->Geo->DrawArgs[os.str()].VertexCount;
			EnemyRitem->IndexCount = EnemyRitem->Geo->DrawArgs[os.str()].IndexCount;
			EnemyRitem->StartIndexLocation = EnemyRitem->Geo->DrawArgs[os.str()].StartIndexLocation;

			//创建npc信息
			auto enemyRitem = std::make_unique<Enemy>();
			std::ostringstream os2;
			os2 << "npc" << j;
			enemyRitem->name = os2.str();
			enemyRitem->attack = 10;
			enemyRitem->ObjCBIndex = EnemyRitem->ObjCBIndex;
			enemyRitem->velecity = 2;
			mAllEnemy.push_back(std::move(enemyRitem));

			mRitemLayer[(int)RenderLayer::AI].push_back(EnemyRitem.get());
			mAllRitems.push_back(std::move(EnemyRitem));
		}

	}
}
void DemoMain::LoadSkeletonAndAnimation()
{
	CleanVertex();

	std::string fbxFileName = "fbx/JIXIEPeople/rzr.FBX";							//Mesh模型文件路径
	std::string animFileName = "fbx/JIXIEPeople/Ani100.FBX";							//动画文件路径
	FbxLoader fileLoader;
	fileLoader.SetReadFile(fbxFileName, FBX_Type::FBX_SkinMesh);
	fileLoader.ReadFile();

	vertices.insert(vertices.end(), std::begin(fileLoader.GetSkinnedVertex()), std::end(fileLoader.GetSkinnedVertex()));  //顶点数据
	mFbxMeshData.m_Skeletons["JixierenJoint"].insert(mFbxMeshData.m_Skeletons["JixierenJoint"].end(), std::begin(fileLoader.GetSkeleton()), std::end(fileLoader.GetSkeleton()));		//骨骼数据
	mFbxMeshData.m_SubMesh.insert(mFbxMeshData.m_SubMesh.end(), std::begin(fileLoader.GetSubmesh()), std::end(fileLoader.GetSubmesh()));
	mFbxMeshData.m_Material.insert(mFbxMeshData.m_Material.end(), std::begin(fileLoader.GetMaterial()), std::end(fileLoader.GetMaterial()));

	//读取动画信息
	std::vector<BoneAnimation> boneAnimation;
	std::vector<std::vector<BoneAnimation> >subAnimations;

	fileLoader.SetReadFile(animFileName, FBX_Type::FBX_Animation);
	fileLoader.ReadFile();
	boneAnimation = fileLoader.GetBoneAnimation();

	BuildAnimationData(mFbxMeshData, mSkinnedInfo, boneAnimation);

	mSkinnedModelInst = std::make_unique<SkinnedModelInstance>();
	mSkinnedModelInst->SkinnedInfo = &mSkinnedInfo;
	mSkinnedModelInst->FinalTransforms.resize(mSkinnedInfo.BoneCount());
	mSkinnedModelInst->ClipName = "YinIdle";
	mSkinnedModelInst->TimePos = 0.0f;
	UINT vertexCount = (UINT)vertices.size();
	UINT vbByteSize = vertexCount * sizeof(SkinnedVertex);

	UINT indexCount = (UINT)indices.size();
	UINT ibByteSize = indexCount * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "SK";

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);
	/*geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(m_d3dDevice.Get(),
		m_CommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);*/

	geo->VertexByteStride = sizeof(SkinnedVertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	for (int i = 0; i < mFbxMeshData.m_SubMesh.size(); ++i)
	{
		BoundingBox::CreateFromPoints(mFbxMeshData.m_SubMesh[i].Bounds, mFbxMeshData.m_SubMesh[i].VertexCount,
			&vertices[mFbxMeshData.m_SubMesh[i].BaseVertexLocation].Pos, sizeof(SkinnedVertex));
		std::ostringstream os;
		os << "submesh" << i;
		geo->DrawArgs[os.str()] = mFbxMeshData.m_SubMesh[i];
	}

	mGeometries[geo->Name] = std::move(geo);
}

void DemoMain::BuildAnimationData(FbxMeshData& meshData, SkinenedData& skinnedData, std::vector<BoneAnimation>& boneAnimation)
{
	std::vector<XMFLOAT4X4> boneOffsets;//将模型从绑定姿势矩阵

	std::vector<int> boneIndexToParentIndex;

	std::unordered_map<std::string, AnimationClip> animations;
	std::vector<BoneAnimation> newBoneAnimation;
	auto joint = meshData.m_Skeletons["JixierenJoint"];
	int numJoint = (int)joint.size();
	boneOffsets.resize(numJoint);
	boneIndexToParentIndex.resize(numJoint);

	for (int i = 0; i < numJoint; i++)
	{
		boneIndexToParentIndex[i] = joint[i].joint_ParentIndex;
		boneOffsets[i] = joint[i].joint_invBindPose;
	}
	//0-61 frame Leftright
	//62-149  walk

	//201-240 shoot
	//240-300 jump
	//301-340 dead
	//341-390 standup and shoot
	AnimationClip clip;
	clip.BoneAnimations.insert(clip.BoneAnimations.end(), boneAnimation.begin(), boneAnimation.end());
	animations["YinIdle"] = clip;

	skinnedData.Set(boneIndexToParentIndex, boneOffsets, animations);
}

void DemoMain::BuildAniObjItems()
{
	int i = 0;
	//animation mesh
	for (; i < mGeometries["SK"]->DrawArgs.size(); i++)
	{

		std::ostringstream os;

		auto StaticMeshRitem = std::make_unique<RenderItem>();

		XMStoreFloat4x4(&StaticMeshRitem->World, XMMatrixScaling(0.01f, 0.01f, 0.01f) *
			XMMatrixRotationX(MathHelper::Pi / 2) *
			XMMatrixTranslation(-2.0f, 0.0f, -22.0f));

		XMStoreFloat4x4(&StaticMeshRitem->TexTransform, XMMatrixRotationZ(MathHelper::Pi) * XMMatrixRotationY(MathHelper::Pi));

		//StaticMeshRitem->TexTransform = MathHelper::Identity4x4();
		StaticMeshRitem->ObjCBIndex = ObjCBIndex++;
		StaticMeshRitem->OpaqueObjIndex = OpaqueObjIndex++;
		StaticMeshRitem->Mat = mMaterials["jixieren"].get();
		StaticMeshRitem->Geo = mGeometries["SK"].get();
		StaticMeshRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		os << "submesh" << i;
		StaticMeshRitem->needDetectCollision = 1;	//需要检测碰撞
		StaticMeshRitem->boxType = 1;//OOBB
		BoundingOrientedBox::CreateFromBoundingBox(StaticMeshRitem->orBounds, StaticMeshRitem->Geo->DrawArgs[os.str()].Bounds);
		StaticMeshRitem->orBounds.Extents.x *= .01f;
		StaticMeshRitem->orBounds.Extents.y *= .01f;
		StaticMeshRitem->orBounds.Extents.z *= .01f;
		StaticMeshRitem->orBounds.Center.x = -2.0f;
		StaticMeshRitem->orBounds.Center.y = 0.0f;
		StaticMeshRitem->orBounds.Center.z = -22.0f;
		StaticMeshRitem->BaseVertexLocation = StaticMeshRitem->Geo->DrawArgs[os.str()].BaseVertexLocation;
		StaticMeshRitem->VertexCount = StaticMeshRitem->Geo->DrawArgs[os.str()].VertexCount;
		StaticMeshRitem->IndexCount = StaticMeshRitem->Geo->DrawArgs[os.str()].IndexCount;
		StaticMeshRitem->StartIndexLocation = StaticMeshRitem->Geo->DrawArgs[os.str()].StartIndexLocation;
		StaticMeshRitem->isMesh = true;
		StaticMeshRitem->isAnimation = true;
		StaticMeshRitem->isAI = false;
		mRitemLayer[(int)RenderLayer::Opaque].push_back(StaticMeshRitem.get());
		mAllRitems.push_back(std::move(StaticMeshRitem));
	}
	
}

void DemoMain::UpdateAnimation(const GameTimer& gt)
{
	auto currSkinnedCB = mCurrFrameResource->SkinnedCB.get();


	mSkinnedModelInst->UpdateSkinnedAnimation(gt.DeltaTime());

	SkinnedConstants skinnedConstants;
	std::copy(
		std::begin(mSkinnedModelInst->FinalTransforms),
		std::end(mSkinnedModelInst->FinalTransforms),
		&skinnedConstants.BoneTransforms[0]);

	currSkinnedCB->CopyData(0, skinnedConstants);
}

void DemoMain::updateEnemyPath(const GameTimer& gt)
{

	int startx, starty;			//起点节点坐标			三维转化到二维计算
	int endx, endy;				//终点节点坐标
	/**************计算玩家当前位置的网格***********************/
	endy = 9 + int(mPlayer->position.x) / 2 + (mPlayer->position.x >= 0 ? 1 : 0);			//强制转换为了处理浮点数精度的的问题
	endx = 14 + int(mPlayer->position.z) / 2 + (mPlayer->position.z >= 0 ? 1 : 0);
	Point end(endx, endy);   //结束点
	for (auto& enemy : mAllEnemy)
	{
		/**************计算每个敌人NPC当前位置的网格***********************/
		for (auto& ritem : mAllRitems)
		{
			if (ritem->ObjCBIndex == enemy->ObjCBIndex)
			{
				float posx = ritem->World._41;
				float posz = ritem->World._43;
				starty = 9 + int(posx) / 2 + (posx >= 0 ? 1 : 0);
				startx = 14 + int(posz) / 2 + (posz >= 0 ? 1 : 0);
				Point start(startx, starty); //起始点	
				path.clear();
				mEnemyPath3D.clear();
				//A*算法找寻路径 
				path = astar.GetPath(start, end, false);
				for (auto& p : path)
				{
					//将二维网格点转化为三维坐标，每个坐标处于网格中心
					mEnemyPath3D.push_back({ -20.0f + p->y * 2 + 1,0.0f,-30.0f + p->x * 2 + 1 });
				}
				if (mEnemyPath3D.size() > 1)
				{
					mEnemyPath3D.pop_front();		//删除起始节点
					UpdateEnemyPos(ritem.get(), mEnemyPath3D, gt);
				}
			}
		}
	}
}
void DemoMain::UpdateEnemyPos(RenderItem* enemy, list<XMFLOAT3> posList, const GameTimer& gt)
{
	XMFLOAT3 nextPos = posList.front();
	float k = (nextPos.x - enemy->World._41) / (nextPos.z - enemy->World._43);
	float PosX = enemy->World._41;
	float PosZ = enemy->World._43;
	//更新X,Z坐标   这里做一下if判断是为了处理移动过程中左右移动的问题  
	//这里设置了一个阈值0.1,当对应轴向距离小于0.1时便停止对应轴向距离的更新
	if (abs(nextPos.x - PosX) > 0.1f && abs(nextPos.z - PosZ) > 0.1f)
	{
		PosX = enemy->World._41 + 0.5f * (nextPos.x - enemy->World._41 > 0 ? 1 : -1)* gt.DeltaTime();
		PosZ = enemy->World._43 + 0.5f * (nextPos.z - enemy->World._43 > 0 ? 1 : -1)* gt.DeltaTime();
	}
	else if (abs(nextPos.x - PosX) > 0.1f && abs(nextPos.z - PosZ) <= 0.1f)
	{
		PosX = enemy->World._41 + 0.5f * (nextPos.x - enemy->World._41 > 0 ? 1 : -1)* gt.DeltaTime();
	}
	else if (abs(nextPos.x - PosX) <= 0.1f && abs(nextPos.z - PosZ) > 0.1f)
	{
		PosZ = enemy->World._43 + 0.5f * (nextPos.z - enemy->World._43 > 0 ? 1 : -1)* gt.DeltaTime();
	}
	else
	{
		posList.pop_front();
	}

	XMMATRIX AIOffset = XMMatrixScaling(.008f, .008f, .008f) * XMMatrixRotationX(MathHelper::Pi / 2) * XMMatrixTranslation(PosX, 0.0, PosZ);
	XMStoreFloat4x4(&enemy->World, AIOffset);
	XMStoreFloat3(&enemy->orBounds.Center, { PosX ,1.38f, PosZ });
	enemy->NumFramesDirty = gNumFrameResources;
}

void DemoMain::BuildPlayerInfo()
{
	mPlayer = std::make_unique<Player>();
	mPlayer->name = "AI";
	mPlayer->age = 35;
	mPlayer->attack = 30;
	mPlayer->blood = 100;
	mPlayer->boxType = 1;
	mPlayer->position = { 0.0f,0.0f,0.0f };
	mPlayer->velecity = 6;
	mPlayer->rotation = { 0.0f,0.0f,0.0f };
}

void DemoMain::BuildGameInfo()
{
	mGameInfo = std::make_unique<GameInfo>();
	mGameInfo->currentLevel = 1;
	mGameInfo->name = "穿越防线";
	mGameInfo->currentScore = 0;
	mGameInfo->remainBulletNum = 30;
	mGameInfo->totalGoldNum = 0;
	mGameInfo->version = "0.0.1";
}
//更新玩家的信息  更细位置以及碰撞盒
void DemoMain::UpdatePlayerInfo(const GameTimer& gt)
{
	mPlayer->position = mCamera.GetPosition3f();
	mPlayer->rotation = mCamera.GetLook3f();
	mPlayer->orBounds.Center = mCamera.cmbounds.Center;
	mPlayer->orBounds.Extents = mCamera.cmbounds.Extents;
}


//打印游戏信息  控制台
void DemoMain::LogPlayerInfo()
{
	std::cout << "********************************************************************************** " << std::endl;
	std::cout << "********************************************************************************** " << std::endl;
	std::cout << "******************			玩家血量：" << mPlayer->blood << "		******************" << std::endl;
	std::cout << "******************			玩家分数：" << mGameInfo->currentScore << "		******************" << std::endl;
	std::cout << "******************			玩家金币：" << mGameInfo->totalGoldNum << "		******************" << std::endl;
	std::cout << "******************			剩余子弹：" << mGameInfo->remainBulletNum << "		******************" << std::endl;
	std::cout << "******************			当前关卡：" << mGameInfo->currentLevel << "		******************" << std::endl;
	std::cout << "********************************************************************************** " << std::endl;
	std::cout << "********************************************************************************** " << std::endl;
}
//从存有敌人AI信息的集合删除某个AI ----------  参数： AI的索引 子弹的索引
void DemoMain::DeleteAI(UINT b_index, UINT c_index)
{
	for (vector<std::unique_ptr<Enemy>>::iterator iter = mAllEnemy.begin(); iter != mAllEnemy.end(); )
	{
		if ((*iter)->ObjCBIndex == b_index)
		{
			mAllEnemy.erase(iter);
			//更新游戏分数
			mGameInfo->currentScore += 30;
			break;
		}
		else
		{
			iter++;
		}
	}
	// 更新mAllEnemy中其他元素的索引
	for (auto& enemy : mAllEnemy)
	{
		if (enemy->ObjCBIndex > b_index&& enemy->ObjCBIndex > c_index)
		{
			enemy->ObjCBIndex -= 2;
		}
		else if (enemy->ObjCBIndex > b_index&& enemy->ObjCBIndex < c_index)
		{
			enemy->ObjCBIndex -= 1;
		}
		else if (enemy->ObjCBIndex < b_index && enemy->ObjCBIndex > c_index)
		{
			enemy->ObjCBIndex -= 1;
		}
		else
		{
			enemy->ObjCBIndex = enemy->ObjCBIndex;
		}
	}
}
void DemoMain::ChangeUIPages()
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for (auto& e : mRitemLayer[(int)RenderLayer::UI])
	{
		if (e->isVisible)
		{
			switch (mPagesNum)
			{
			case MAINPAGE:
				e->Mat = mMaterials["mainbg"].get();
				e->NumFramesDirty = gNumFrameResources;
				break;
			case CHOOSEMODE:
				e->Mat = mMaterials["choosebg"].get();
				e->NumFramesDirty = gNumFrameResources;
				break;
			case HELP:
				e->Mat = mMaterials["help"].get();
				e->NumFramesDirty = gNumFrameResources;
				break;
			default:
				break;
			}
		}

	}

}

void DemoMain::BuildUIMainPages()
{
	//Main Quad
	auto quadRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&quadRitem->World, XMMatrixScaling(2.f, 2.0f, 0.8f) * XMMatrixTranslation(-1.f, 1.f, 0.1f));
	XMStoreFloat4x4(&quadRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	quadRitem->ObjCBIndex = ObjCBIndex++;

	quadRitem->Mat = mMaterials["mainbg"].get();
	quadRitem->Geo = mGeometries["shapeGeo"].get();
	quadRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	quadRitem->IndexCount = quadRitem->Geo->DrawArgs["quad"].IndexCount;
	quadRitem->StartIndexLocation = quadRitem->Geo->DrawArgs["quad"].StartIndexLocation;
	quadRitem->BaseVertexLocation = quadRitem->Geo->DrawArgs["quad"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::UI].push_back(quadRitem.get());
	mAllRitems.push_back(std::move(quadRitem));
}

void DemoMain::DrawUIItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	// For each render item...
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];
		if (!gameStart)
		{
			if (ri->isVisible)
			{
				cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
				cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

				D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
				cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);
				if (ri->isMesh)
				{
					cmdList->DrawInstanced(ri->VertexCount, 1, ri->BaseVertexLocation, 0);
				}
				else
				{
					cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
					cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
				}
			}
		}
		else
		{
			if (!ri->isVisible)
			{
				cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
				cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

				D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + (UINT)ri->ObjCBIndex * objCBByteSize;
				cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);
				if (ri->isMesh)
				{
					cmdList->DrawInstanced(ri->VertexCount, 1, ri->BaseVertexLocation, 0);
				}
				else
				{
					cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
					cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
				}
			}
		}


	}
}

bool DemoMain::InitSound(std::string fileName)
{
	if (!D3DApp::InitSound(fileName))
		return false;
	return true;
}