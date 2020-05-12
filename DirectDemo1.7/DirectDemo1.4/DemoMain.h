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
	string version = "0.0.1";			//��ǰ�汾
	string name = "��Խ����";			//��Ϸ����
	float currentScore = 0.0f;			//��ǰ�÷�
	float remainBulletNum = 999;		//ʣ���ӵ�
	UINT currentLevel = 1;				//��ǰ�ؿ�
	UINT totalGoldNum = 0;				//��ǰ���н��
};
struct Player
{
	Player() = default;
	Player(const Player& rhs) = delete;
	string name;				//��� name
	UINT age;					//�������
	UINT blood;					//���	Ѫ��
	UINT attack;				//��ҹ�����
	UINT velecity = 6;			//����ƶ��ٶ�
	XMFLOAT3 rotation;			//���  ����
	XMFLOAT3 position;			//���  ��ǰλ��
	BoundingOrientedBox orBounds;		 //OBB��Χ��
	BoundingSphere obSphere;			//�����Χ�У���������ȽϺ���
	UINT boxType;						//��Χ�е�����  0:AABB��Χ�� 1��OOBB��Χ��
};

struct Enemy
{
	Enemy() = default;
	Enemy(const Enemy& rhs) = delete;
	string name;				//AI����
	UINT attack;				//AI������
	INT ObjCBIndex;
	UINT velecity = 6;			//AI�ƶ��ٶ�
	//XMFLOAT4 rotation;			//AI  ����
	XMFLOAT3 position;			//AI  ��ǰλ��
	//BoundingOrientedBox orBounds;			//OBB��Χ��
	//BoundingSphere obSphere;				//�����Χ�У���������ȽϺ���
	//UINT boxType;							//��Χ�е�����  0:AABB��Χ�� 1��OOBB��Χ��

};

//��ײ��Ϣ�ṹ��
struct CollisionDetect {
	CollisionDetect() = default;
	CollisionDetect(const CollisionDetect& rhs) = delete;
	UINT collisionObjectIndex;			//����ײ����ı��
	time_t t;							//����ײ�����ʱ��
	XMFLOAT3 collisionPos;				//����ײ�����Ӧ���λ��
	XMVECTOR v0, V1, V2;				//����ײ���������ζ���
	UINT indices[3];					//��Ӧ���������
};

//��Ƥ������Ϣ  Ϊ�˳���Уɫ����ʵ�����ض�ʱ�̵� ����
struct SkinnedModelInstance
{
	SkinenedData* SkinnedInfo = nullptr;

	//һ������������ʱ���tʱ��ģ�Ϳռ�����
	std::vector<XMFLOAT4X4> FinalTransforms;
	std::string ClipName;

	//ʱ��� �������ʱ�����������û���������㷨����в�ֵ��
	float TimePos = 0.0f;

	void UpdateSkinnedAnimation(float dt)
	{
		TimePos += dt;

		//loopanimation
		if (TimePos > SkinnedInfo->GetClipEndTime(ClipName))
		{
			TimePos = 0.0f;
		}

		//������Ƥ��������ձ任����
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

	//�����ײ������Ҫ������
	BoundingOrientedBox orBounds;						//OBB��Χ��
	BoundingSphere obSphere;							//�����Χ�У���������ȽϺ���
	UINT boxType=0;										//��Χ�е�����  0:AABB��Χ�� 1��OOBB��Χ��
	std::vector<CollisionDetect*> collisionDetected;	//�Ƿ������ײ�ı�־
	BOOL needDetectCollision = false;					//�Ƿ���Ҫ�����ײ��ֻ�е��а�Χ��ʱ�ż��


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

	virtual void ChangeUIPages() override;	//UI��������л�

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

	//��������Դ
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
	//shadow map ��Դ
	std::unique_ptr<ShaowMap> mShadowMap;
	DirectX::BoundingSphere mSceneBounds;				//��Դ�Ӿ���   ��Դ�۲����������Դ���ӽǹ���
	XMFLOAT3 mLightPosW;
	float mLightNearZ = 0.0f;
	float mLightFarZ = 0.0f;
	XMFLOAT4X4 mLightView = MathHelper::Identity4x4();
	XMFLOAT4X4 mLightProj = MathHelper::Identity4x4();
	XMFLOAT4X4 mShadowTransform = MathHelper::Identity4x4();

	/******************************��̬��Դ��Ϣ*************************************/
	float mLightRotationAngle = 0.0f;					//�����ת�Ƕ�
	XMFLOAT3 mBaseLightDirections[3] = {				//��ʼ��ķ���
		XMFLOAT3(0.57735f, -0.57735f, 0.57735f),
		XMFLOAT3(-0.57735f, -0.57735f, 0.57735f),
		XMFLOAT3(0.0f, -0.707f, -0.707f)
	};
	XMFLOAT3 mRotatedLightDirections[3];				//�����任���ķ���

	/******************************FbX�ļ�������ر���******************************/
	FbxMeshData mFbxMeshData;
	std::vector<SkinnedVertex> vertices;  //��������
	std::vector<std::uint16_t> indices;		//��������
	FbxLoader fileLoader;
	std::string fbxFileName;  //fbx�ļ�·��

	/******************************����AI��ر���***********************************/
	XMFLOAT3 mAIInitialPos[6] = {
		XMFLOAT3(1.0f, 0.f, 25.0f),
		XMFLOAT3(5.f, 0.f,  25.0f),
		XMFLOAT3(-5.f, 0.f, 25.0f),
		XMFLOAT3(3.f, 0.f,7.0f),
		XMFLOAT3(-3.f, 0.f, 7.0f),
		XMFLOAT3(11.f, 0.f, -11.0f)
	};


	/******************************������ر���***********************************/
	UINT mSkinnedSrvHeapStart = 0;
	std::unique_ptr<SkinnedModelInstance> mSkinnedModelInst;
	SkinenedData mSkinnedInfo;


	//��ʼ����ͼ���ö�ά���������Ƭ��ͼ��1��ʾ�ϰ��0��ʾ��ͨ 
	//x��ʾ����  y��ʾ����   20*30
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
	//npc�ƶ�·��3D���꼯��
	list<XMFLOAT3> mEnemyPath3D;
	list<Point*> path;




	std::unique_ptr<Player> mPlayer;
	std::unique_ptr<GameInfo> mGameInfo;
	std::vector<std::unique_ptr<Enemy>> mAllEnemy;										//������Ϣ
	const int AINum = 3;																//npc����
}; 




