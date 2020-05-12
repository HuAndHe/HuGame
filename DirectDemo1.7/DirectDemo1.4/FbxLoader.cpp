#include"FbxLoader.h"

FbxLoader::FbxLoader()
{
	m_FbxManger = FbxManager::Create();
	if (!m_FbxManger)
	{
		std::cerr << "Unable to create FBX Manager!" << std::endl;
		/*
			exit��0�����������г����˳�����

			exit��1�������������е����˳�����
		*/
		exit(1);
	}
	//����IO����
	FbxIOSettings* ios = FbxIOSettings::Create(m_FbxManger, IOSROOT);
	m_FbxManger->SetIOSettings(ios);


	//���ز��
	FbxString lPath = FbxGetApplicationDirectory();
	m_FbxManger->LoadPluginsDirectory(lPath.Buffer());

	//����FBX Scene
	m_FbxScene = FbxScene::Create(m_FbxManger, "MyScene");
	if(!m_FbxScene)
	{
		std::cerr << "Unable to Create FBX Scene!" << std::endl;
		exit(1);
	}

}
FbxLoader::~FbxLoader()
{
	if (m_FbxManger)
	{
		m_FbxManger->Destroy();
	}
}
void FbxLoader::SetReadFile(const std::string& filename, const FBX_Type type)
{
	//���������Ϣ
	m_Material.clear();
	m_Material.resize(0);
	m_Skeleton.clear();
	m_Skeleton.resize(0);
	m_SubMesh.clear();
	m_SubMesh.resize(0);
	m_Vertices.clear();
	m_Vertices.resize(0);
	m_BoneAnimation.clear();
	m_BoneAnimation.resize(0);

	if (m_FbxScene)
	{
		//�����scene��Ϣ
		m_FbxScene->Clear();
	}

	//�����ļ���ȡ��
	FbxImporter* importer = FbxImporter::Create(m_FbxManger, IOSROOT);
	if (!importer->Initialize(filename.c_str(), -1, m_FbxManger->GetIOSettings()))
	{
		std::cerr << "fbx�ļ���ȡʧ��" << std::endl;
		return;
	}
	importer->Import(m_FbxScene);
	importer->Destroy();

	//ת��ģ�ͣ���ģ�Ͱ��ղ��ʷָ���������в��ʲ���
	FbxGeometryConverter converter(m_FbxManger);
	converter.SplitMeshesPerMaterial(m_FbxScene, true);

	//��ȡ���ڵ�
	m_RootNode = m_FbxScene->GetRootNode();
	if (!m_RootNode)
	{
		std::cerr << "��ȡrootNodeʧ�ܣ�" << std::endl;
		return;
	}
	m_Type = type;
}
void FbxLoader::ProcessMeshNode(FbxNode* node)
{
	//�˱������ڿ��ƶ�ȡ����ʱ�������
	bool isLoadBindPoseInv = false;
	FbxMesh* mesh = nullptr;
	//�����ڵ����ԣ��Բ�ͬ���Խ��в�ͬ�Ĵ���
	for (UINT i = 0; i < node->GetNodeAttributeCount(); i++)
	{
		switch (node->GetNodeAttributeByIndex(i)->GetAttributeType())
		{
			//����mesh
		case FbxNodeAttribute::eMesh:
			mesh = (FbxMesh*)node->GetNodeAttributeByIndex(i);
			if (!isLoadBindPoseInv)
			{
				ReadMeshTransform(node);
				LoadJointData(mesh);
				loadMeshMaterial(mesh);
				isLoadBindPoseInv = true;
			}
			ProcessMesh(mesh);//����mesh����
			break;
		default:
			break;

		}
	}
	isLoadBindPoseInv = false;
	for (UINT i = 0; i < node->GetChildCount(); i++)
	{
		ProcessMeshNode(node->GetChild(i));
	}
}
void FbxLoader::ProcessMesh(FbxMesh* mesh)
{
	//��ǰmesh�ڵ�Ķ����������������
	int vertexCounter = 0;

	/*��ȡ������Ϣ*/
	//�˱�����ȡ��ǰ�Ѿ���ȡ��mesh�ڵ�һ���ж��ٶ��㣬��Ϊһ��fbx�ļ������ж��mesh�ڵ�
	UINT PreVertexCount = (UINT)m_Vertices.size();
	UINT CurrentVertexCount = (UINT)mesh->GetPolygonVertexCount();
	m_Vertices.resize(PreVertexCount + CurrentVertexCount);
	//���ﴦ���ģ�Ͷ�������ģ�ͣ����Լ����Ϊ��ȡ�����ε�������
	int polyCount = mesh->GetPolygonCount();

	//get submesh message submesh��������Ҫ���������ֲ�ͬ����ʹ�õĲ�ͬ����
	SubmeshGeometry submesh;
	submesh.BaseVertexLocation = PreVertexCount;
	submesh.VertexCount = CurrentVertexCount;
	m_SubMesh.push_back(submesh);

	//��ȡskinģ�͵Ķ�������Ϣ
	std::vector<SkinBlendInfo> conPointBlendInfo = loadSkinData(mesh);

	CalcTangent(mesh);


	//�𶥵��ȡ��Ϣ
	for (int i = 0; i < polyCount; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			int ctrlPointIndex = (int)mesh->GetPolygonVertex(i, j);

			int vertexIndex = 0;
			switch (j)
			{

			case 0:
				vertexIndex = i * 3 + 0 + PreVertexCount;
				break;

			case 1:
				vertexIndex = i * 3 + 2 + PreVertexCount;
				break;

			case 2:
				vertexIndex = i * 3 + 1 + PreVertexCount;
				break;
			default:
				break;
			}

			//get skinblend info
			ReadBlendInfo(ctrlPointIndex, m_Vertices[vertexIndex], conPointBlendInfo);

			//get position
			ReadPosition(mesh, ctrlPointIndex, m_Vertices[vertexIndex].Pos);

			//get normal
			ReadNormal(mesh, ctrlPointIndex, vertexCounter, m_Vertices[vertexIndex].Normal);

			//get uv
			for (int k = 0; k < 1; ++k)
			{
				ReadUV(mesh, ctrlPointIndex, (int)mesh->GetTextureUVIndex(i, j), k, m_Vertices[vertexIndex].TexC);
			}

			//get Targent
			ReadTargent(mesh, ctrlPointIndex, vertexCounter, m_Vertices[vertexIndex].TangentU);

		}



	}


}

void FbxLoader::ReadPosition(FbxMesh* mesh, int ctrlPointIndex, XMFLOAT3& vertex)
{
	//����DirectXʹ����������ϵ�Ĺ�ϵ��������Ҫ��zֵȡ��
	FbxVector4 conPoint = mesh->GetControlPointAt(ctrlPointIndex);

	vertex.x = (float)conPoint.mData[0];
	vertex.y = (float)conPoint.mData[1];
	vertex.z = -(float)conPoint.mData[2];
}

void FbxLoader::ReadBlendInfo(const int& ctrlPointIndex, SkinnedVertex& vertex, const std::vector<SkinBlendInfo>& blendInfo)
{
	//size������ʾһ������ᱻ���ٸ��ؽ�Ӱ�죬�������Ӱ�춥��Ĺؽ������8��
	//�����ڶ�����ʹ������������洢��Щ��Ϣ��
	int size = (int)blendInfo[ctrlPointIndex].size();
	for (size_t i = 0; i < size; ++i)
	{

		switch (i)
		{
		case 0:
			vertex.BoneIndices.x = (INT32)blendInfo[ctrlPointIndex][i].first;
			vertex.BoneWeights.x = blendInfo[ctrlPointIndex][i].second;
			break;
		case 1:
			vertex.BoneIndices.y = (INT32)blendInfo[ctrlPointIndex][i].first;
			vertex.BoneWeights.y = blendInfo[ctrlPointIndex][i].second;
			break;
		case 2:
			vertex.BoneIndices.z = (INT32)blendInfo[ctrlPointIndex][i].first;
			vertex.BoneWeights.z = blendInfo[ctrlPointIndex][i].second;
			break;
		case 3:
			vertex.BoneIndices.w = (INT32)blendInfo[ctrlPointIndex][i].first;
			vertex.BoneWeights.w = blendInfo[ctrlPointIndex][i].second;
			break;
		/*case 4:
			vertex.BoneIndices1.x = (INT32)blendInfo[ctrlPointIndex][i].first;
			vertex.BoneWeights1.x = blendInfo[ctrlPointIndex][i].second;
			break;
		case 5:
			vertex.BoneIndices1.y = (INT32)blendInfo[ctrlPointIndex][i].first;
			vertex.BoneWeights1.y = blendInfo[ctrlPointIndex][i].second;
			break;
		case 6:
			vertex.BoneIndices1.z = (INT32)blendInfo[ctrlPointIndex][i].first;
			vertex.BoneWeights1.z = blendInfo[ctrlPointIndex][i].second;
			break;
		case 7:
			vertex.BoneIndices1.w = (INT32)blendInfo[ctrlPointIndex][i].first;
			vertex.BoneWeights1.w = blendInfo[ctrlPointIndex][i].second;
			break;*/
		default:
			break;
		}
	}
}

void FbxLoader::ReadUV(FbxMesh* mesh, int ctrlPointIndex, int textureUVIndex, int uvlayer, XMFLOAT2& pUV)
{
	//����DirectXʹ�õ���������ϵ������v = 1 - v'
	if (uvlayer >= 2 || mesh->GetElementUVCount() <= uvlayer)
	{
		return;
	}
	FbxGeometryElementUV* VertexUV = mesh->GetElementUV(uvlayer);

	switch (VertexUV->GetMappingMode())
	{
	case FbxGeometryElement::eByControlPoint:
	{
		switch (VertexUV->GetReferenceMode())
		{

		case FbxGeometryElement::eDirect:
		{
			pUV.x = (float)VertexUV->GetDirectArray().GetAt(ctrlPointIndex).mData[0];
			pUV.y = (float)(1 - VertexUV->GetDirectArray().GetAt(ctrlPointIndex).mData[1]);
		}
		break;


		case FbxGeometryElement::eIndexToDirect:
		{
			int id = VertexUV->GetIndexArray().GetAt(ctrlPointIndex);
			pUV.x = (float)VertexUV->GetDirectArray().GetAt(id).mData[0];
			pUV.y = (float)(1 - VertexUV->GetDirectArray().GetAt(id).mData[1]);
		}
		break;
		default:
			break;
		}
	}
	break;
	case FbxGeometryElement::eByPolygonVertex:
	{
		switch (VertexUV->GetReferenceMode())
		{

		case FbxGeometryElement::eDirect:
		case FbxGeometryElement::eIndexToDirect:
		{
			pUV.x = (float)VertexUV->GetDirectArray().GetAt(textureUVIndex).mData[0];
			pUV.y = (float)VertexUV->GetDirectArray().GetAt(textureUVIndex).mData[1];
		}
		break;
		default:
			break;
		}
	}
	break;


	default:
		break;
	}
}

void FbxLoader::ReadNormal(FbxMesh* mesh, int ctrlPointIndex, int vertexCounter, XMFLOAT3& pNormal)
{

	if (mesh->GetElementNormalCount() < 1)
	{
		//
		std::cerr << "ElementNormalCount < 1" << std::endl;
	}

	FbxGeometryElementNormal* normal = mesh->GetElementNormal(0);

	//����DirectXʹ����������ϵ������normal��zֵӦ��ȡ����
	switch (normal->GetMappingMode())
	{
	case FbxGeometryElement::eByControlPoint:
	{
		switch (normal->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
		{
			pNormal.x = (float)normal->GetDirectArray().GetAt(ctrlPointIndex).mData[0];
			pNormal.y = (float)normal->GetDirectArray().GetAt(ctrlPointIndex).mData[1];
			pNormal.z = -(float)normal->GetDirectArray().GetAt(ctrlPointIndex).mData[2];
		}
		break;


		case FbxGeometryElement::eIndexToDirect:
		{
			int id = normal->GetIndexArray().GetAt(ctrlPointIndex);
			pNormal.x = (float)normal->GetDirectArray().GetAt(id).mData[0];
			pNormal.y = (float)normal->GetDirectArray().GetAt(id).mData[1];
			pNormal.z = -(float)normal->GetDirectArray().GetAt(id).mData[2];
		}
		break;

		default:
			break;
		}
	}
	break;



	case FbxGeometryElement::eByPolygonVertex:
	{
		switch (normal->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
		{
			pNormal.x = (float)normal->GetDirectArray().GetAt(vertexCounter).mData[0];
			pNormal.y = (float)normal->GetDirectArray().GetAt(vertexCounter).mData[1];
			pNormal.z = (float)normal->GetDirectArray().GetAt(vertexCounter).mData[2];
		}
		break;


		case FbxGeometryElement::eIndexToDirect:
		{
			int id = normal->GetIndexArray().GetAt(vertexCounter);
			pNormal.x = (float)normal->GetDirectArray().GetAt(id).mData[0];
			pNormal.y = (float)normal->GetDirectArray().GetAt(id).mData[1];
			pNormal.z = (float)normal->GetDirectArray().GetAt(id).mData[2];
		}
		break;
		}
		break;
	default:
		break;
	}
	}
}

void FbxLoader::ReadTargent(FbxMesh* mesh, int ctrlPointIndex, int vertexCounter, XMFLOAT3& tangent)
{
	if (!TargentArray.size())
	{
		return;
	}
	tangent = TargentArray[ctrlPointIndex];
	tangent.z = -tangent.z;
}

void FbxLoader::processSkeletonHirearchyRecursively(FbxNode* inNode, int inDepth, int myIndex, int inParentIndex)
{

	if (inNode->GetNodeAttribute() && inNode->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton)
	{
		Joint currentJoint;
		currentJoint.joint_ParentIndex = inParentIndex;
		currentJoint.joint_name = inNode->GetName();
		/*������������ں����ȡ*/
		m_Skeleton.push_back(currentJoint);
	}
	//�ݹ��ȡÿһ��Joint
	/*
	ÿ�εݹ����ʱ����һ����εĸ��ڵ�Index���ǵ�ǰ�ڵ��Index������ǰ�ڵ��Index��Ϊ���е�Joint������
	*/
	for (int i = 0; i < inNode->GetChildCount(); ++i)
	{
		processSkeletonHirearchyRecursively(inNode->GetChild(i), inDepth + 1, (int)m_Skeleton.size(), myIndex);
	}
}

std::vector<SkinBlendInfo> FbxLoader::loadSkinData(FbxMesh* mesh)
{
	//��¼�����Ϣ�ı���
	std::pair<UINT16, float> currJointIndexAndWeight;
	std::vector<SkinBlendInfo> jointIndexAndWeight(mesh->GetControlPointsCount());



	//��ȡskin
	FbxSkin* m_skin = (FbxSkin*)mesh->GetDeformer(0);
	if (m_skin)
	{
		//���Խ�ÿ��cluster����һ��joint�ڵ�
		/*
		��������֣����ǽ���ȡÿһ��cluster������controlPoint�ĸ����������������һ�������ڵ���Ӱ��Ķ��������
		Ȼ���ⲿ�ֵ�mesh���е�controlPoint�Ļ����Ϣ�洢�������������ĺ�����Ͻ�skinVertex��������Ϣ��ȡ��
		*/
		for (int i = 0; i < m_skin->GetClusterCount(); i++)
		{
			FbxCluster* cluster = m_skin->GetCluster(i);
			std::string JointName = cluster->GetLink()->GetName();
			int jointIndex = findJointIndexByName(JointName);
			int controlPointIndicesCount = cluster->GetControlPointIndicesCount();



			//blendInfo
			double* weights = cluster->GetControlPointWeights();
			int* controlPointIndices = cluster->GetControlPointIndices();

			for (size_t i = 0; i < controlPointIndicesCount; i++)
			{
				currJointIndexAndWeight.first = jointIndex;
				currJointIndexAndWeight.second = (float)weights[i];
				jointIndexAndWeight[controlPointIndices[i]].push_back(currJointIndexAndWeight);
			}
		}
	}

	return jointIndexAndWeight;
}

void FbxLoader::LoadJointData(FbxMesh* mesh)
{
	//��¼�ؽڰ�����֮�����
	FbxAMatrix transformLinkMatrix;
	FbxAMatrix globalBindposeInverseMatrix;

	FbxSkin* m_skin = (FbxSkin*)mesh->GetDeformer(0);
	if (m_skin)
	{
		//���Խ�ÿ��cluster����һ��joint�ڵ�
		for (int i = 0; i < m_skin->GetClusterCount(); i++)
		{
			FbxCluster* cluster = m_skin->GetCluster(i);
			std::string JointName = cluster->GetLink()->GetName();
			int jointIndex = findJointIndexByName(JointName);
			//��ȡ�����������
			cluster->GetTransformLinkMatrix(transformLinkMatrix);
			globalBindposeInverseMatrix = transformLinkMatrix.Inverse();
			{
				m_Skeleton[jointIndex].joint_invBindPose.m[0][0] = (float)globalBindposeInverseMatrix.Get(0, 0);
				m_Skeleton[jointIndex].joint_invBindPose.m[0][1] = (float)globalBindposeInverseMatrix.Get(0, 1);
				m_Skeleton[jointIndex].joint_invBindPose.m[0][2] = (float)globalBindposeInverseMatrix.Get(0, 2);
				m_Skeleton[jointIndex].joint_invBindPose.m[0][3] = (float)globalBindposeInverseMatrix.Get(0, 3);

				m_Skeleton[jointIndex].joint_invBindPose.m[1][0] = (float)globalBindposeInverseMatrix.Get(1, 0);
				m_Skeleton[jointIndex].joint_invBindPose.m[1][1] = (float)globalBindposeInverseMatrix.Get(1, 1);
				m_Skeleton[jointIndex].joint_invBindPose.m[1][2] = (float)globalBindposeInverseMatrix.Get(1, 2);
				m_Skeleton[jointIndex].joint_invBindPose.m[1][3] = (float)globalBindposeInverseMatrix.Get(1, 3);

				m_Skeleton[jointIndex].joint_invBindPose.m[2][0] = (float)globalBindposeInverseMatrix.Get(2, 0);
				m_Skeleton[jointIndex].joint_invBindPose.m[2][1] = (float)globalBindposeInverseMatrix.Get(2, 1);
				m_Skeleton[jointIndex].joint_invBindPose.m[2][2] = (float)globalBindposeInverseMatrix.Get(2, 2);
				m_Skeleton[jointIndex].joint_invBindPose.m[2][3] = (float)globalBindposeInverseMatrix.Get(2, 3);

				m_Skeleton[jointIndex].joint_invBindPose.m[3][0] = (float)globalBindposeInverseMatrix.Get(3, 0);
				m_Skeleton[jointIndex].joint_invBindPose.m[3][1] = (float)globalBindposeInverseMatrix.Get(3, 1);
				m_Skeleton[jointIndex].joint_invBindPose.m[3][2] = (float)globalBindposeInverseMatrix.Get(3, 2);
				m_Skeleton[jointIndex].joint_invBindPose.m[3][3] = (float)globalBindposeInverseMatrix.Get(3, 3);
			}
		}
	}
}

int FbxLoader::findJointIndexByName(const std::string& name)
{
	for (int i = 0; i < m_Skeleton.size(); i++)
	{
		if (m_Skeleton[i].joint_name == name)
		{
			return i;
		}
	}
	return -1;
}

void FbxLoader::loadMeshMaterial(FbxMesh* mesh)
{
	FbxNode* node = mesh->GetNode();
	FbxProperty pProperty;
	Material material;
	FbxSurfaceMaterial* mat;

	for (int i = 0; i < node->GetMaterialCount(); i++)
	{
		mat = node->GetMaterial(i);
		const char* name = mat->GetName();

		//build mat name
		std::ostringstream os;
		os << "mat" << m_Material.size();
		std::string matName = os.str();

		material.Name = matName;

		if (mat->GetClassId().Is(FbxSurfacePhong::ClassId))
		{
			FbxDouble3 diffuse = ((FbxSurfacePhong*)mat)->Diffuse;
			material.DiffuseAlbedo.x = (float)diffuse.mData[0];
			material.DiffuseAlbedo.y = (float)diffuse.mData[1];
			material.DiffuseAlbedo.z = (float)diffuse.mData[2];
			FbxDouble3 fresnelRO = ((FbxSurfacePhong*)mat)->Specular;
			material.FresnelR0.x = (float)fresnelRO.mData[0];
			material.FresnelR0.y = (float)fresnelRO.mData[1];
			material.FresnelR0.z = (float)fresnelRO.mData[2];
			FbxDouble shiness = ((FbxSurfacePhong*)mat)->Shininess;
			material.Roughness = (float)(shiness / 10.0);

		}
		else if (mat->GetClassId().Is(FbxSurfaceLambert::ClassId))
		{
			FbxDouble3 diffuse = ((FbxSurfaceLambert*)mat)->Diffuse;
			material.DiffuseAlbedo.x = (float)diffuse.mData[0];
			material.DiffuseAlbedo.y = (float)diffuse.mData[1];
			material.DiffuseAlbedo.z = (float)diffuse.mData[2];
			FbxDouble3 fresnelRO = ((FbxSurfaceLambert*)mat)->Emissive;
			material.FresnelR0.x = (float)fresnelRO.mData[0];
			material.FresnelR0.y = (float)fresnelRO.mData[1];
			material.FresnelR0.z = (float)fresnelRO.mData[2];
			material.Roughness = 0.2f;
		}


		for (int textureLayerIndex = 0; textureLayerIndex < FbxLayerElement::sTypeTextureCount; ++textureLayerIndex)
		{
			pProperty = mat->FindProperty(FbxLayerElement::sTextureChannelNames[textureLayerIndex]);
			if (pProperty.IsValid())
			{
				int textureCount = pProperty.GetSrcObjectCount<FbxTexture>();

				for (int j = 0; j < textureCount; j++)
				{

					FbxTexture* pTexture = pProperty.GetSrcObject<FbxTexture>(j);
					if (pTexture)
					{
						FbxFileTexture* fileTexture = FbxCast<FbxFileTexture>(pTexture);
						FbxProceduralTexture* proceduralTexture = FbxCast<FbxProceduralTexture>(pTexture);
						if (fileTexture)
						{
							std::string name = fileTexture->GetFileName();
							int pos = (int)name.find_last_of('\\');
							name = name.erase(0, pos + 1);
							material.TextureName = name;
						}
					}
				}
			}
		}
		material.MatCBIndex = (int)m_Material.size();
		material.DiffuseSrvHeapIndex = (int)m_Material.size();
		m_Material.push_back(material);
	}
}

void FbxLoader::ReadAnimationData(FbxScene* scene)
{
	//��ȡAnimStack�ĸ���
	int stackCount = scene->GetSrcObjectCount<FbxAnimStack>();
	FbxString stackName;
	FbxTime start, end;

	for (int i = 0; i < stackCount; i++)
	{
		FbxAnimStack* animStack = scene->GetSrcObject<FbxAnimStack>(i);
		stackName = animStack->GetName();

		//��ȡ������ʼ�ͽ���ʱ��
		FbxTakeInfo* takeInfo = scene->GetTakeInfo(stackName);
		start = takeInfo->mLocalTimeSpan.GetStart();
		end = takeInfo->mLocalTimeSpan.GetStop();

		//���㶯��֡��
		FbxTime::EMode mode = scene->GetGlobalSettings().GetTimeMode();
		auto animationLength = end.GetFrameCount(mode) - start.GetFrameCount(mode) + 1;

		int jointIndex = 0;
		m_BoneAnimation.resize(m_Skeleton.size());
		//�������нڵ���ʱ��Ƭ���sqt����
		for (auto joint : m_Skeleton)
		{
			FbxString jointName = joint.joint_name.c_str();
			FbxNode* node = scene->FindNodeByName(jointName);
			for (FbxLongLong i = start.GetFrameCount(mode); i <= end.GetFrameCount(mode); ++i)
			{
				m_BoneAnimation[jointIndex].Keyframes.resize(animationLength);

				FbxTime currTime;
				currTime.SetFrame(i, mode);
				auto timePos = (float)currTime.GetSecondDouble();
				FbxAMatrix currLocalTransform = node->EvaluateLocalTransform(currTime);
				FbxQuaternion Q = currLocalTransform.GetQ();
				FbxVector4 T = currLocalTransform.GetT();
				FbxVector4 S = currLocalTransform.GetS();
				//��ȡ����ʱ��
				m_BoneAnimation[jointIndex].Keyframes[i].TimePos = timePos;
				//���������Ŷ���
				m_BoneAnimation[jointIndex].Keyframes[i].Scale.x = 1.0f;
				m_BoneAnimation[jointIndex].Keyframes[i].Scale.y = 1.0f;
				m_BoneAnimation[jointIndex].Keyframes[i].Scale.z = 1.0f;
				//��ȡƽ��
				m_BoneAnimation[jointIndex].Keyframes[i].Translation.x = (float)T.mData[0];
				m_BoneAnimation[jointIndex].Keyframes[i].Translation.y = (float)T.mData[1];
				m_BoneAnimation[jointIndex].Keyframes[i].Translation.z = (float)T.mData[2];
				//��Ԫ����ת
				m_BoneAnimation[jointIndex].Keyframes[i].RotationQuat.x = (float)Q.mData[0];
				m_BoneAnimation[jointIndex].Keyframes[i].RotationQuat.y = (float)Q.mData[1];
				m_BoneAnimation[jointIndex].Keyframes[i].RotationQuat.z = (float)Q.mData[2];
				m_BoneAnimation[jointIndex].Keyframes[i].RotationQuat.w = (float)Q.mData[3];

			}
			++jointIndex;
			std::cout << "��ɹ���" << jointIndex << "�������ݶ�ȡ" << std::endl;
		}


	}

}

void FbxLoader::ReadFile()
{
	switch (m_Type)
	{
	case FBX_SkinMesh:
		processSkeletonHirearchyRecursively(m_RootNode, -2, -1, 0);
		ProcessMeshNode(m_RootNode);
		break;


	case FBX_Animation:
		processSkeletonHirearchyRecursively(m_RootNode, -2, -1, 0);
		ReadAnimationData(m_FbxScene);
		break;
	default:
		break;
	}
}

void FbxLoader::ReadMeshTransform(FbxNode* node)
{
	MeshTransform transform;

	FbxDouble3 translation = node->LclTranslation.Get();//��ȡ���node��λ�á���ת������
	transform.Translation.x = translation.mData[0];
	transform.Translation.y = translation.mData[1];
	transform.Translation.z = translation.mData[2];

	FbxDouble3 rotation = node->LclRotation.Get();
	transform.Rotation.x = rotation.mData[0];
	transform.Rotation.y = rotation.mData[1];
	transform.Rotation.z = rotation.mData[2];

	FbxDouble3 scaling = node->LclScaling.Get();
	transform.Scaling.x = scaling.mData[0];
	transform.Scaling.y = scaling.mData[1];
	transform.Scaling.z = scaling.mData[2];
	
	m_MeshTransform.push_back(transform);

}

void FbxLoader::CalcTangent(FbxMesh* mesh)
{
	//���targent ����
	TargentArray.clear();
	TargentArray.resize(0);

	//�������
	UINT ctrlPointCount = mesh->GetControlPointsCount();
	//�����θ���
	UINT polygonCount = mesh->GetPolygonCount();
	FbxVector4* controlPoints = mesh->GetControlPoints();
	FbxGeometryElementUV* VertexUV = mesh->GetElementUV(0);

	if (!VertexUV)
	{
		return;
	}

	TargentArray.resize(ctrlPointCount);

	for (int i = 0; i < polygonCount; i++)
	{
		int ctrlPointIndex0 = mesh->GetPolygonVertex(i, 0);
		int ctrlPointIndex1 = mesh->GetPolygonVertex(i, 1);
		int ctrlPointIndex2 = mesh->GetPolygonVertex(i, 2);

		//��ȡλ��
		FbxVector4 conPoint0 = mesh->GetControlPointAt(ctrlPointIndex0);
		FbxVector4 conPoint1 = mesh->GetControlPointAt(ctrlPointIndex1);
		FbxVector4 conPoint2 = mesh->GetControlPointAt(ctrlPointIndex2);

		//pos
		float s0x = conPoint1.mData[0] - conPoint0.mData[0];
		float s0y = conPoint1.mData[1] - conPoint0.mData[1];
		float s0z = (conPoint1.mData[2] - conPoint0.mData[2]);

		float s1x = conPoint2.mData[0] - conPoint0.mData[0];
		float s1y = conPoint2.mData[1] - conPoint0.mData[1];
		float s1z = (conPoint2.mData[2] - conPoint0.mData[2]);


		//��ȡuv

		float u0 = VertexUV->GetDirectArray().GetAt(ctrlPointIndex1).mData[0] - VertexUV->GetDirectArray().GetAt(ctrlPointIndex0).mData[0];
		float v0 = VertexUV->GetDirectArray().GetAt(ctrlPointIndex1).mData[1] - VertexUV->GetDirectArray().GetAt(ctrlPointIndex0).mData[1];

		float u1 = VertexUV->GetDirectArray().GetAt(ctrlPointIndex2).mData[0] - VertexUV->GetDirectArray().GetAt(ctrlPointIndex0).mData[0];
		float v1 = VertexUV->GetDirectArray().GetAt(ctrlPointIndex2).mData[1] - VertexUV->GetDirectArray().GetAt(ctrlPointIndex0).mData[1];

		//������벿��
		float leftValue = 1.0f / (u0 * v1 - v0 * u1);

		//�����Ұ벿��
		float e2x = v1 * s0x - v0 * s1x;
		float e2y = v1 * s0y - v0 * s1y;
		float e2z = v1 * s0z - v0 * s1z;

		//��������ʽ
		e2x *= leftValue;
		e2y *= leftValue;
		e2z *= leftValue;

		TargentArray[ctrlPointIndex0].x += e2x;
		TargentArray[ctrlPointIndex0].y += e2y;
		TargentArray[ctrlPointIndex0].z += e2z;

		TargentArray[ctrlPointIndex1].x += e2x;
		TargentArray[ctrlPointIndex1].y += e2y;
		TargentArray[ctrlPointIndex1].z += e2z;

		TargentArray[ctrlPointIndex2].x += e2x;
		TargentArray[ctrlPointIndex2].y += e2y;
		TargentArray[ctrlPointIndex2].z += e2z;
	}

	for (size_t i = 0; i < TargentArray.size(); i++)
	{
		XMVECTOR T = XMLoadFloat3(&TargentArray[i]);
		T = XMVector3Normalize(T);
		XMStoreFloat3(&TargentArray[i], T);
	}

}

const std::vector<Joint>& FbxLoader::GetSkeleton()
{
	return m_Skeleton;
}

const std::vector<SkinnedVertex>& FbxLoader::GetSkinnedVertex()
{
	return m_Vertices;
}

const std::vector<SubmeshGeometry>& FbxLoader::GetSubmesh()
{
	return m_SubMesh;
}

const std::vector<Material>& FbxLoader::GetMaterial()
{
	return m_Material;
}

FbxNode* FbxLoader::GetRootNode()
{
	return m_RootNode;
}

const std::vector<BoneAnimation> FbxLoader::GetBoneAnimation()
{
	return m_BoneAnimation;
}

const std::vector<MeshTransform>& FbxLoader::GetMeshTransform()
{
	return m_MeshTransform;
}
