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
	
	//将顶点变换到世界空间
	float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);

	//将顶点变换至齐次裁剪空间
	vout.PosH = mul(posW, gViewProj);

	//为三角形插值输出顶点属性
	float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
	vout.TexC = mul(texC, matData.MatTransform).xy;

	return vout;
}

//这段代码仅用于需要进行alpha裁剪的几何图形。以此使阴影正确的显现出来
//如果待处理的集几何图形无需执行此操作，则在深度渲染过程中使用无内容(null）的像素着色器
void PS(VertexOut pin)
{
	//获取材质数据
	MaterialData matData = gMaterialData[gMaterialIndex];
	float4 diffuseAlbedo = matData.DiffuseAlbedo;
	uint diffuseMapIndex = matData.DiffuseMapIndex;

	//在数组中动态查找纹理
	diffuseAlbedo *= gDiffuseMap[diffuseMapIndex].Sample(gsamAnisotropicWrap,pin.TexC);

#ifdef ALPHA_TEST
	//如果像素的alpha值<0.1,丢弃该像素。应该在着色器尽早的执行此项测试，以满足条件的像素尽可能早地退出着色器，以此来跳过没必要执行的后续代码
	clip(diffuseAlbedo.a - 0.1f);
#endif
}