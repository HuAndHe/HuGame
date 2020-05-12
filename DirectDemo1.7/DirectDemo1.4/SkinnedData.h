#pragma once
#include"D3DUtil.h"
#include"MathHelper.h"
//一个骨骼节点在时间点timepos的姿势
struct  Keyframe
{
	Keyframe();
	~Keyframe();

	float TimePos;
	DirectX::XMFLOAT3 Translation;
	DirectX::XMFLOAT3 Scale;
	DirectX::XMFLOAT4 RotationQuat;
};
//一个骨骼在一段时间的姿势集合
struct BoneAnimation
{
	//最早开始的时间
	float GetStartTime() const;
	//最晚停止的结束时间
	float GetEndTime() const;

	
	void Interpolate(float t, DirectX::XMFLOAT4X4& M)const;
	std::vector<Keyframe> Keyframes;
};
//完整骨骼在一段时间的姿势几何
struct AnimationClip
{
	float GetClipStartTime() const;
	float GetClipEndTime() const;

	void Interpolate(float t, std::vector<DirectX::XMFLOAT4X4>& boneTransforms) const;
	std::vector<BoneAnimation> BoneAnimations;
};

struct SkinenedData
{
public :
	UINT BoneCount()const;
	float GetClipStartTime(const std::string& clipName)const;
	float GetClipEndTime(const std::string& clipName)const;

	void Set(
		std::vector<int>& boneHierarchy,
		std::vector<DirectX::XMFLOAT4X4>& boneOffsets,
		std::unordered_map<std::string, AnimationClip>& animations);
	void GetFinalTransforms(const std::string& clipName, float timePos, std::vector<DirectX::XMFLOAT4X4>& finalTransforms)const;
private:
	std::vector<int>mBoneHierarchy;
	std::vector < DirectX::XMFLOAT4X4> mBoneOffsets;
	std::unordered_map<std::string, AnimationClip>mAnimations;
};