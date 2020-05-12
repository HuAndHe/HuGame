#pragma once
#include"D3DUtil.h"
#include"MathHelper.h"
//һ�������ڵ���ʱ���timepos������
struct  Keyframe
{
	Keyframe();
	~Keyframe();

	float TimePos;
	DirectX::XMFLOAT3 Translation;
	DirectX::XMFLOAT3 Scale;
	DirectX::XMFLOAT4 RotationQuat;
};
//һ��������һ��ʱ������Ƽ���
struct BoneAnimation
{
	//���翪ʼ��ʱ��
	float GetStartTime() const;
	//����ֹͣ�Ľ���ʱ��
	float GetEndTime() const;

	
	void Interpolate(float t, DirectX::XMFLOAT4X4& M)const;
	std::vector<Keyframe> Keyframes;
};
//����������һ��ʱ������Ƽ���
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