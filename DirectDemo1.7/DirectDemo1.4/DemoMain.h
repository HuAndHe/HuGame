#pragma once
#include "d3dApp.h"
#include "MathHelper.h"
#include "UploadBuffer.h"
#include "GeometryGenerator.h"
#include "Camera.h"
#include "FrameResource.h"
#include"ShadowMap.h"
#include"FbxLoader.h"
#include"SkinnedData.h"
#include"Astar.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace std;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")


struct GameInfo
{
	GameInfo() = default;
	GameInfo(const GameInfo& rhs) = delete;
	string version = "0.0.1";			//当前版本
	string name = "穿越防线";			//游戏名称
	float currentScore = 0.0f;			//当前得分
	float remainBulletNum = 999;		//剩余子弹
	UINT currentLevel = 1;				//当前关卡
	UINT totalGoldNum = 0;				//当前持有金币
};
struct Player
{
	Player() = default;
	Player(const Player& rhs) = delete;
	string name;				//玩家 name
	UINT age;					//玩家年龄
	UINT blood;					//玩家	血量
	UINT attack;				//玩家攻击力
	UINT velecity = 6;			//玩家移动速度
	XMFLOAT3 rotation;			//玩家  朝向
	XMFLOAT3 position;			//玩家  当前位置
	BoundingOrientedBox orBounds;		 //OBB包围盒
	BoundingSphere obSphere;			//球体包围盒，对于球体比较合适
	UINT boxType;						//包围盒的类型  0:AABB包围盒 1：OOBB包围盒
};

struct Enemy
{
	Enemy() = default;
	Enemy(const Enemy& rhs) = delete;
	string name;				//AI名称
	UINT attack;				//AI攻击力
	INT ObjCBIndex;
	UINT velecity = 6;			//AI移动速度
	//XMFLOAT4 rotation;			//AI  朝向
	XMFLOAT3 position;			//AI  当前位置
	//BoundingOrientedBox orBounds;			//OBB包围盒
	//BoundingSphere obSphere;				//球体包围盒，对于球体比较合适
	//UINT boxType;							//包围盒的类型  0:AABB包围盒 1：OOBB包围盒

};

//碰撞信息结构体
struct CollisionDetect {
	CollisionDetect() = default;
	CollisionDetect(const CollisionDetect& rhs) = delete;
	UINT collisionObjectIndex;			//被碰撞物体的编号
	time_t t;							//被碰撞物体的时间
	XMFLOAT3 collisionPos;				//被碰撞物体对应点的位置
	XMVECTOR v0, V1, V2;				//被碰撞检测的三角形顶点
	UINT indices[3];					//对应顶点的索引
};

//蒙皮网格信息  为了呈现校色动画实例在特定时刻的 动作
struct SkinnedModelInstance
{
	SkinenedData* SkinnedInfo = nullptr;

	//一套完整骨骼在时间点t时的模型空间姿势
	std::vector<XMFLOAT4X4> FinalTransforms;
	std::string ClipName;

	//时间点 如果姿势时间点在数据中没有则内置算法会进行插值。
	float TimePos = 0.0f;

	void UpdateSkinnedAnimation(float dt)
	{
		TimePos += dt;

		//loopanimation
		if (TimePos > SkinnedInfo->GetClipEndTime(ClipName))
		{
			TimePos = 0.0f;
		}

		//计算蒙皮定点的最终变换矩阵
		SkinnedInfo->GetFinalTransforms(ClipName, TimePos, FinalTransforms);

	}
};

struct RenderItem
{
	RenderItem() = default;
	RenderItem(const RenderItem& rhs) = delete;

	// World matrix of the shape that describes the object's local space
	// relative to the world space, which defines the position, orientation,
	// and scale of the object in the world.
	XMFLOAT4X4 World = MathHelper::Identity4x4();

	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	int ObjCBIndex = -1;
	int OpaqueObjIndex = -1;
	int AIIndex = -1;
	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0;
	UINT VertexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;

	//添加碰撞体所需要的属性
	BoundingOrientedBox orBounds;						//OBB包围盒
	BoundingSphere obSphere;							//球体包围盒，对于球体比较合适
	UINT boxType=0;										//包围盒的类型  0:AABB包围盒 1：OOBB包围盒
	std::vector<CollisionDetect*> collisionDetected;	//是否产生碰撞的标志
	BOOL needDetectCollision = false;					//是否需要检测碰撞，只有当有包围盒时才检测


	BOOL isBullet = FALSE;
	BOOL isAI = FALSE;
	BOOL isMesh = FALSE;
	BOOL isAnimation = FALSE;
	BOOL isVisible = TRUE;
	XMFLOAT3 lookDirection;
};

enum class RenderLayer : int
{
	Opaque = 0,
	Sky,
	AI,
	UI,
	Count
};

class DemoMain : public D3DApp
{
public:
	DemoMain(UINT width, UINT height, std::wstring name, HINSTANCE hInstance);
	DemoMain(const DemoMain& rhs) = delete;
	DemoMain& operator=(const DemoMain& rhs) = delete;
	~DemoMain();

	virtual bool Initialize()override;

private:
	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;
	virtual void CreateRtvAndDsvDescriptorHeaps()override;
	virtual bool InitSound(std::string fileName)override;

	void LoadStaticAIMesh();

	void BuildAIItems(int num);

	void LoadSkeletonAndAnimation();

	void BuildAnimationData(FbxMeshData& meshData, SkinenedData& skinnedData, std::vector<BoneAnimation>& boneAnimation);

	void BuildAniObjItems();

	void UpdateAnimation(const GameTimer& gt);

	void updateEnemyPath(const GameTimer& gt);

	void UpdateEnemyPos(RenderItem* enemy, list<XMFLOAT3> posList, const GameTimer& gt);

	void BuildPlayerInfo();

	void BuildGameInfo();

	void UpdatePlayerInfo(const GameTimer& gt);

	void LogPlayerInfo();

	void DeleteAI(UINT b_index, UINT c_index);

	void BuildUIMainPages();

	virtual void ChangeUIPages() override;	//UI界面材质切换

	void DrawUIItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

	void OnKeyboardInput(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialBuffer(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);

	void LoadTextures();
	void BuildRootSignature();
	void BuildDescriptorHeaps();
	void BuildShadersAndInputLayout();
	void BuildShapeGeometry();
	void BuildSkullGeometry();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildRenderItems();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers();

	void DetectCollision();

	void AddBullet(int x, int y, XMFLOAT3 cameraPos, XMFLOAT3 lookDir);

	void UpdateBullet(RenderItem* b, const GameTimer& gt);

	void UpdateShadowTransform(const GameTimer& gt);

	void DrawSceneToShadow();

	void UpdateShadowPassCB(const GameTimer& gt);

	//清理顶点资源
	void CleanVertex()
	{
		vertices.clear();
		vertices.resize(0);
		indices.clear();
		indices.resize(0);
		mFbxMeshData.m_Skeletons.clear();
		mFbxMeshData.m_SubMesh.clear();
		mFbxMeshData.m_SubMesh.resize(0);
		mFbxMeshData.m_Material.clear();
		mFbxMeshData.m_Material.resize(0);
	}
	

private:

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	UINT mCbvSrvDescriptorSize = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	UINT mSkyTexHeapIndex = 0;
	UINT mShadowMapHeapIndex = 0;

	UINT mNullCubeSrvIndex = 0;
	UINT mNullTexSrvIndex = 0;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mNullSrv;

	PassConstants mMainPassCB;		// index 0 of pass cbuffer.
	PassConstants mShadowPassCB;	// index 1 of pass cbuffer.

	Camera mCamera;

	POINT mLastMousePos;


	UINT ObjCBIndex = 0;
	UINT OpaqueObjIndex = 0;
	UINT AIIndex = 0;
	std::vector<std::string> texNames;
	std::vector<std::wstring> texFilenames;
	//shadow map 资源
	std::unique_ptr<ShaowMap> mShadowMap;
	DirectX::BoundingSphere mSceneBounds;				//光源视景体   光源观察矩阵以主光源的视角构建
	XMFLOAT3 mLightPosW;
	float mLightNearZ = 0.0f;
	float mLightFarZ = 0.0f;
	XMFLOAT4X4 mLightView = MathHelper::Identity4x4();
	XMFLOAT4X4 mLightProj = MathHelper::Identity4x4();
	XMFLOAT4X4 mShadowTransform = MathHelper::Identity4x4();

	/******************************动态光源信息*************************************/
	float mLightRotationAngle = 0.0f;					//光的旋转角度
	XMFLOAT3 mBaseLightDirections[3] = {				//初始光的方向
		XMFLOAT3(0.57735f, -0.57735f, 0.57735f),
		XMFLOAT3(-0.57735f, -0.57735f, 0.57735f),
		XMFLOAT3(0.0f, -0.707f, -0.707f)
	};
	XMFLOAT3 mRotatedLightDirections[3];				//经过变换后光的方向

	/******************************FbX文件解析相关变量******************************/
	FbxMeshData mFbxMeshData;
	std::vector<SkinnedVertex> vertices;  //骨骼顶点
	std::vector<std::uint16_t> indices;		//骨骼索引
	FbxLoader fileLoader;
	std::string fbxFileName;  //fbx文件路径

	/******************************创建AI相关变量***********************************/
	XMFLOAT3 mAIInitialPos[6] = {
		XMFLOAT3(1.0f, 0.f, 25.0f),
		XMFLOAT3(5.f, 0.f,  25.0f),
		XMFLOAT3(-5.f, 0.f, 25.0f),
		XMFLOAT3(3.f, 0.f,7.0f),
		XMFLOAT3(-3.f, 0.f, 7.0f),
		XMFLOAT3(11.f, 0.f, -11.0f)
	};


	/******************************动画相关变量***********************************/
	UINT mSkinnedSrvHeapStart = 0;
	std::unique_ptr<SkinnedModelInstance> mSkinnedModelInst;
	SkinenedData mSkinnedInfo;


	//初始化地图，用二维矩阵代表瓦片地图，1表示障碍物，0表示可通 
	//x表示纵轴  y表示横轴   20*30
	vector<vector<int>> maze = {
		{1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1},
		{1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1},
		{1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1},
		{1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1},
		{1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1},
		{1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1},
		{1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1},
		{1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,0,1,1,1},
		{1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,0,1,1,1},
		{1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1},
		{1,1,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,1,1},
		{1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1},
		{1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1},
		{1,1,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,1,1},
		{1,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,1},
		{1,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1,1},
		{1,1,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,1,1},
		{1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1},
		{1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1},
		{1,1,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,1,1},
		{1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1},
		{1,1,1,0,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1},
		{1,1,1,0,1,1,0,1,0,0,0,0,1,0,0,0,0,1,1,1},
		{1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1},
		{1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1},
		{1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1},
		{1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1},
		{1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1},
		{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
		{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
	};
	Astar astar;
	//npc移动路径3D坐标集合
	list<XMFLOAT3> mEnemyPath3D;
	list<Point*> path;




	std::unique_ptr<Player> mPlayer;
	std::unique_ptr<GameInfo> mGameInfo;
	std::vector<std::unique_ptr<Enemy>> mAllEnemy;										//敌人信息
	const int AINum = 3;																//npc数量
}; 




