#include"SkinnedData.h"
using namespace DirectX;

Keyframe::Keyframe() : 
TimePos(0.0f),
Translation(0.0f, 0.0f, 0.0f),
Scale(1.0f, 1.0f, 1.0f),
RotationQuat(0.0f, 0.0f, 0.0f, 1.0f)
{

}
Keyframe::~Keyframe()
{

}

float BoneAnimation::GetStartTime()const
{
	// Keyframes are sorted by time, so first keyframe gives start time.
	return Keyframes.front().TimePos;
}
float BoneAnimation::GetEndTime()const
{
	// Keyframes are sorted by time, so last keyframe gives end time.
	return Keyframes.back().TimePos;
}

void BoneAnimation::Interpolate(float t, XMFLOAT4X4& M) const
{
	if (t <= Keyframes.front().TimePos)
	{
		XMVECTOR S = XMLoadFloat3(&Keyframes.front().Scale);
		XMVECTOR P = XMLoadFloat3(&Keyframes.front().Translation);
		XMVECTOR Q = XMLoadFloat4(&Keyframes.front().RotationQuat);

		XMVECTOR zero = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		//XMMatrixAffineTransformation( Scaling, RotationOrigin, RotationQuaternion, Translation)构建仿射变换矩阵
		XMStoreFloat4x4(&M, XMMatrixAffineTransformation(S, zero, Q, P));

	}
	else if (t >= Keyframes.back().TimePos)
	{
		XMVECTOR S = XMLoadFloat3(&Keyframes.back().Scale);
		XMVECTOR Q = XMLoadFloat4(&Keyframes.back().RotationQuat);
		XMVECTOR P = XMLoadFloat3(&Keyframes.back().Translation);
		XMVECTOR zero = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		XMStoreFloat4x4(&M, XMMatrixAffineTransformation(S, zero, Q, P));
	}
	else
	{
		for (UINT i = 0; i < Keyframes.size() - 1; ++i)
		{
			if (t >= Keyframes[i].TimePos && t <= Keyframes[i + 1].TimePos)
			{
				//线性插值比例
				float LerpPercent = (t - Keyframes[i].TimePos) / (Keyframes[i + 1].TimePos - Keyframes[i].TimePos);
				XMVECTOR s0 = XMLoadFloat3(&Keyframes[i].Scale);
				XMVECTOR s1 = XMLoadFloat3(&Keyframes[i + 1].Scale);

				XMVECTOR p0 = XMLoadFloat3(&Keyframes[i].Translation);
				XMVECTOR p1 = XMLoadFloat3(&Keyframes[i + 1].Translation);

				XMVECTOR q0 = XMLoadFloat4(&Keyframes[i].RotationQuat);
				XMVECTOR q1 = XMLoadFloat4(&Keyframes[i + 1].RotationQuat);

				XMVECTOR S = XMVectorLerp(s0, s1, LerpPercent);
				XMVECTOR Q = XMVectorLerp(q0, q1, LerpPercent);
				XMVECTOR P = XMVectorLerp(p0, p1, LerpPercent);

				XMVECTOR zero = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

				XMStoreFloat4x4(&M, XMMatrixAffineTransformation(S, zero, Q, P));

				break;

			}

		}
	}
}
float AnimationClip::GetClipStartTime()const
{
	float t = MathHelper::Infinity;
	for (UINT i = 0; i < BoneAnimations.size(); ++i)
	{
		t = MathHelper::Min(t, BoneAnimations[i].GetStartTime());
	}
	return t;
}
float AnimationClip::GetClipEndTime()const
{
	float t = 0.0f;
	for (UINT i = 0; i < BoneAnimations.size(); ++i)
	{
		t = MathHelper::Max(t, BoneAnimations[i].GetEndTime());
	}
	return t;
}

void AnimationClip::Interpolate(float t, std::vector<XMFLOAT4X4>& boneTransforms)const
{
	for (UINT i = 0; i < BoneAnimations.size(); i++)
	{
		BoneAnimations[i].Interpolate(t, boneTransforms[i]);
	}
}

float SkinenedData::GetClipStartTime(const std::string& clipName)const
{
	auto clip = mAnimations.find(clipName);
	return clip->second.GetClipStartTime();
}
float SkinenedData::GetClipEndTime(const std::string& clipName)const
{
	auto clip = mAnimations.find(clipName);
	return clip->second.GetClipEndTime();
}
UINT SkinenedData::BoneCount() const
{
	return (UINT)mBoneHierarchy.size();
}
void SkinenedData::Set(std::vector<int>& boneHierarchy,
	std::vector<XMFLOAT4X4>& boneOffsets,
	std::unordered_map<std::string, AnimationClip>& animations)
{
	mBoneHierarchy = boneHierarchy;
	mBoneOffsets = boneOffsets;
	mAnimations = animations;
}
void SkinenedData::GetFinalTransforms(const std::string& clipName, float timePos, std::vector<XMFLOAT4X4>& finalTransforms)const
{
	UINT numBones = (UINT)mBoneOffsets.size();
	std::vector<XMFLOAT4X4> toParentTransforms(numBones);

	auto clip = mAnimations.find(clipName);
	clip->second.Interpolate(timePos, toParentTransforms);

	std::vector<XMFLOAT4X4> toRootTransforms(numBones);
	toRootTransforms[0] = toParentTransforms[0];


	//计算到根顶点的空间
	for (UINT i = 1; i < numBones; ++i)
	{
		XMMATRIX toParent = XMLoadFloat4x4(&toParentTransforms[i]);
		int parentIndex = mBoneHierarchy[i];
		XMMATRIX parentToRoot = XMLoadFloat4x4(&toRootTransforms[parentIndex]);
		XMMATRIX toRoot = XMMatrixMultiply(toParent, parentToRoot);

		XMStoreFloat4x4(&toRootTransforms[i], toRoot);
	}
	//加上偏移变换offset
	for (UINT i = 0; i < numBones; ++i)
	{
		XMMATRIX offset = XMLoadFloat4x4(&mBoneOffsets[i]);
		XMMATRIX toRoot = XMLoadFloat4x4(&toRootTransforms[i]);
		XMMATRIX finalTransform = XMMatrixMultiply(offset, toRoot);


		finalTransform.r[0].m128_f32[2] = -finalTransform.r[0].m128_f32[2];
		finalTransform.r[1].m128_f32[2] = -finalTransform.r[1].m128_f32[2];
		finalTransform.r[3].m128_f32[2] = -finalTransform.r[3].m128_f32[2];

		finalTransform.r[2].m128_f32[0] = -finalTransform.r[2].m128_f32[0];
		finalTransform.r[2].m128_f32[1] = -finalTransform.r[2].m128_f32[1];
		finalTransform.r[2].m128_f32[3] = -finalTransform.r[2].m128_f32[3];

		XMStoreFloat4x4(&finalTransforms[i], XMMatrixTranspose(finalTransform));
	}
}