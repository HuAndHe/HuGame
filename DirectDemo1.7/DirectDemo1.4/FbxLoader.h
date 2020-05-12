#pragma once
#include<iostream>
#include<ostream>
#include"D3DUtil.h"
#include"FrameResource.h"
#include"SkinnedData.h"
#include "../fbxsdk/2019.2/include/fbxsdk.h"
#include "../fbxsdk/2019.2/include/fbxsdk/scene/fbxaxissystem.h"
#pragma comment(lib,"../fbxsdk/2019.2/lib/vs2017/x86/debug/libfbxsdk-md.lib")
#pragma comment(lib,"../fbxsdk/2019.2/lib/vs2017/x86/debug/libxml2-md.lib")
#pragma comment(lib,"../fbxsdk/2019.2/lib/vs2017/x86/debug/zlib-md.lib")

using namespace DirectX;
//蒙皮网格信息类型
typedef std::vector<std::pair<UINT16, float>>SkinBlendInfo;

//关节结构体
struct Joint
{
	XMFLOAT4X4 joint_invBindPose;
	std::string joint_name;
	UINT joint_ParentIndex;
};
//不同类型的文件
enum FBX_Type
{
	FBX_SkinMesh = 0,
	FBX_Animation
};
struct MeshTransform
{
	XMFLOAT3 Translation;
	XMFLOAT3 Rotation;
	XMFLOAT3 Scaling;
};

//FBX网格数据
struct FbxMeshData
{
	std::vector<SubmeshGeometry>m_SubMesh;
	std::vector<Material>m_Material;
	std::unordered_map < std::string, std::vector<Joint>>m_Skeletons;
	std::vector<MeshTransform>m_MeshTransform;

};

//蒙皮顶点格式
struct  SkinnedVertex;

class FbxLoader
{
public:
	FbxLoader();
	~FbxLoader();
	FbxLoader(const FbxLoader& rhs) = delete;
	FbxLoader& operator=(const FbxLoader& rhs) = delete;
	void SetReadFile(const std::string& filename, const FBX_Type type);

public:
	//处理mesh信息
	void ProcessMeshNode(FbxNode* node);
	void ProcessMesh(FbxMesh* mesh);
	void ReadPosition(FbxMesh* mesh, int ctrlPointIndex, XMFLOAT3& vertex);//读取Position信息
	void ReadBlendInfo(const int& ctrlPointIndex, SkinnedVertex& vertex, const std::vector<SkinBlendInfo>& blendInfo);//读取skin顶点的blendInfo
	void ReadUV(FbxMesh* mesh, int ctrlPointIndex, int textureIndex, int uvlayer, XMFLOAT2& pUV);//读取UV
	void ReadNormal(FbxMesh* mesh, int ctrlPontIndex, int vertexCounter, XMFLOAT3& pNormal);//读取法线
	void ReadTargent(FbxMesh* mesh, int ctrlPointIndex, int vertexCounter, XMFLOAT3& tangent);//读取Targent
	//读取骨骼层次信息 inDepth参数仅用于调试  readSkeleton，use inDepth Para for Debug
	void processSkeletonHirearchyRecursively(FbxNode* inNode, int inDepth, int myIndex, int inParentIndex);
	//读取蒙皮信息 read skinBlendInfo
	std::vector<SkinBlendInfo> loadSkinData(FbxMesh* mesh);
	//读取关节绑定姿势之逆矩阵
	void LoadJointData(FbxMesh* mesh);

	int findJointIndexByName(const std::string& name);

	//读取材质信息
	void loadMeshMaterial(FbxMesh* mesh);

	//读取动画信息
	void ReadAnimationData(FbxScene* scene);

	void ReadFile();

	void ReadMeshTransform(FbxNode* node);

	void CalcTangent(FbxMesh* mesh);

public:
	const std::vector<Joint>& GetSkeleton();
	const std::vector<SkinnedVertex>& GetSkinnedVertex();
	const std::vector<SubmeshGeometry>& GetSubmesh();
	const std::vector<Material>& GetMaterial();
	FbxNode* GetRootNode();
	const std::vector<BoneAnimation> GetBoneAnimation();
	const std::vector<MeshTransform>& GetMeshTransform();
private:
	FbxManager* m_FbxManger = nullptr;
	FbxScene* m_FbxScene = nullptr;
	FbxNode* m_RootNode = nullptr;
	FBX_Type m_Type;
	std::vector<Joint> m_Skeleton;
	std::vector<SkinnedVertex> m_Vertices;
	std::vector<SubmeshGeometry> m_SubMesh;
	std::vector<Material> m_Material;
	std::vector<BoneAnimation> m_BoneAnimation;
	std::vector<MeshTransform> m_MeshTransform;

	//计算tangent 
	std::vector<XMFLOAT3> TargentArray;
};