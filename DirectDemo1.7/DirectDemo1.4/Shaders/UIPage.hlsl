

//***************************************************************************************
// Default.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

// 默认光源的数量
#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 6
#endif

// 包含公用的HLSL代码
#include "Common.hlsl"

struct VertexIn
{
	float3 PosL    : POSITION;   //顶点输入位置
    float3 NormalL : NORMAL;	//顶线输入法线
	float2 TexC    : TEXCOORD;	//纹理

};

struct VertexOut
{
	float4 PosH    : SV_POSITION;	//系统Pos 奇次裁剪空间
	//float4 ShadowPosH : POSITION0;
    float3 PosW    : POSITION;		//顶点着色器输出Pos
    float3 NormalW : NORMAL;		//输出法线

	float2 TexC    : TEXCOORD;		//输出纹理
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;

	//获取材质数据
	MaterialData matData = gMaterialData[gMaterialIndex];
	
	//顶点输入坐标转换为世界坐标
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
	//输出PosW 保存世界坐标
    vout.PosW = posW.xyz;   

	//假设非均匀缩放;否则，需要使用世界矩阵的逆转置。 将法线转换到世界空间
    vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);
	

    // 将顶点转换到齐次裁剪空间
    //vout.PosH = mul(posW, gViewProj);
	
	vout.PosH = float4(vout.PosW, 1.0f);
	
	//材质变换
	float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
	vout.TexC = mul(texC, matData.MatTransform).xy;
	//将顶点转换到shadowmap空间 以便与shadowmap进行深度比较
	/*vout.ShadowPosH = mul(posW, gShadowTransform);*/
	
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	//获取材质信息
	MaterialData matData = gMaterialData[gMaterialIndex];
	float4 diffuseAlbedo = matData.DiffuseAlbedo;   //漫反射反照率
	float3 fresnelR0 = matData.FresnelR0;			//菲涅尔因子
	float  roughness = matData.Roughness;			//粗糙度

	uint diffuseMapIndex = matData.DiffuseMapIndex;  //漫反射纹理在SRV堆中的索引
	

	//对法线插值可能使他非规范化，因此要再次对它进行规范化处理	
	pin.NormalW = normalize(pin.NormalW);
	 //如果对象有法线贴图 将使用法线贴图映射
    // 获取贴图
	// Dynamically look up the texture in the array.
	diffuseAlbedo *= gDiffuseMap[diffuseMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);
	
    float3 toEyeW = normalize(gEyePosW - pin.PosW);
    // 光照项
    float4 ambient = gAmbientLight*diffuseAlbedo;

	
	//Alpha通道存储的是逐像素级别上的光泽度
    //const float shininess = (1.0f - roughness) * normalMapSample.a;
	const float shininess = (1.0f - roughness);
    Material mat = { diffuseAlbedo, fresnelR0, 0.1 };
    float3 shadowFactor = 1.0f;
    float4 directLight = ComputeLighting(gLights, mat, pin.PosW,
        pin.NormalW, toEyeW, shadowFactor);

    float4 litColor = ambient + directLight;

	// Add in specular reflections.
	//利用法线图计算镜面反射
	float3 r = reflect(-toEyeW, pin.NormalW);
	float4 reflectionColor = gCubeMap.Sample(gsamLinearWrap, r);
	float3 fresnelFactor = SchlickFresnel(fresnelR0, pin.NormalW, r);
    litColor.rgb += shininess * fresnelFactor * reflectionColor.rgb;
	
    // 从漫反射反照率中获取alpha值得常规方法
    litColor.a = diffuseAlbedo.a;
	
    return litColor;
}
/**********************总结*********************************/

//凹凸法线的向量既可以用于光照计算，又能用在以环境图模拟反射的反射效果计算当中。
//法线图的Alpha通道存储的是光泽度掩码，它控制着逐像素级别上的物体表面的光泽度。
/**********************总结*********************************/
