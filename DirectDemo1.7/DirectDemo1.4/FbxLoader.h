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
//��Ƥ������Ϣ����
typedef std::vector<std::pair<UINT16, float>>SkinBlendInfo;

//�ؽڽṹ��
struct Joint
{
	XMFLOAT4X4 joint_invBindPose;
	std::string joint_name;
	UINT joint_ParentIndex;
};
//��ͬ���͵��ļ�
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

//FBX��������
struct FbxMeshData
{
	std::vector<SubmeshGeometry>m_SubMesh;
	std::vector<Material>m_Material;
	std::unordered_map < std::string, std::vector<Joint>>m_Skeletons;
	std::vector<MeshTransform>m_MeshTransform;

};

//��Ƥ�����ʽ
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
	//����mesh��Ϣ
	void ProcessMeshNode(FbxNode* node);
	void ProcessMesh(FbxMesh* mesh);
	void ReadPosition(FbxMesh* mesh, int ctrlPointIndex, XMFLOAT3& vertex);//��ȡPosition��Ϣ
	void ReadBlendInfo(const int& ctrlPointIndex, SkinnedVertex& vertex, const std::vector<SkinBlendInfo>& blendInfo);//��ȡskin�����blendInfo
	void ReadUV(FbxMesh* mesh, int ctrlPointIndex, int textureIndex, int uvlayer, XMFLOAT2& pUV);//��ȡUV
	void ReadNormal(FbxMesh* mesh, int ctrlPontIndex, int vertexCounter, XMFLOAT3& pNormal);//��ȡ����
	void ReadTargent(FbxMesh* mesh, int ctrlPointIndex, int vertexCounter, XMFLOAT3& tangent);//��ȡTargent
	//��ȡ���������Ϣ inDepth���������ڵ���  readSkeleton��use inDepth Para for Debug
	void processSkeletonHirearchyRecursively(FbxNode* inNode, int inDepth, int myIndex, int inParentIndex);
	//��ȡ��Ƥ��Ϣ read skinBlendInfo
	std::vector<SkinBlendInfo> loadSkinData(FbxMesh* mesh);
	//��ȡ�ؽڰ�����֮�����
	void LoadJointData(FbxMesh* mesh);

	int findJointIndexByName(const std::string& name);

	//��ȡ������Ϣ
	void loadMeshMaterial(FbxMesh* mesh);

	//��ȡ������Ϣ
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

	//����tangent 
	std::vector<XMFLOAT3> TargentArray;
};