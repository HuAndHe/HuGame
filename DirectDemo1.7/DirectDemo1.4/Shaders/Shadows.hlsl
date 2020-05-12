//***************************************************************************************
//    Date:		2020/5/10
//	  Name:		Shadows.hlsl
// Project:		GameDevelopment
//***************************************************************************************
#include"Common.hlsl"
struct VertexIn
{
	float3 PosL : POSITION;
	float2 TexC : TEXCOORD;
};
struct VertexOut
{
	float4 PosH : SV_POSITION;
	float2 TexC : TEXCOORD;
};
VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;

	MaterialData matData = gMaterialData[gMaterialIndex];
	
	//������任������ռ�
	float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);

	//������任����βü��ռ�
	vout.PosH = mul(posW, gViewProj);

	//Ϊ�����β�ֵ�����������
	float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
	vout.TexC = mul(texC, matData.MatTransform).xy;

	return vout;
}

//��δ����������Ҫ����alpha�ü��ļ���ͼ�Ρ��Դ�ʹ��Ӱ��ȷ�����ֳ���
//���������ļ�����ͼ������ִ�д˲��������������Ⱦ������ʹ��������(null����������ɫ��
void PS(VertexOut pin)
{
	//��ȡ��������
	MaterialData matData = gMaterialData[gMaterialIndex];
	float4 diffuseAlbedo = matData.DiffuseAlbedo;
	uint diffuseMapIndex = matData.DiffuseMapIndex;

	//�������ж�̬��������
	diffuseAlbedo *= gDiffuseMap[diffuseMapIndex].Sample(gsamAnisotropicWrap,pin.TexC);

#ifdef ALPHA_TEST
	//������ص�alphaֵ<0.1,���������ء�Ӧ������ɫ�������ִ�д�����ԣ����������������ؾ���������˳���ɫ�����Դ�������û��Ҫִ�еĺ�������
	clip(diffuseAlbedo.a - 0.1f);
#endif
}