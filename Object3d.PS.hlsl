#include "Object3d.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct Material {
    float32_t4 color;
    int32_t enableLighting;
    float32_t shininess;
};

struct DirectionalLight {
	float32_t4 color;  //ライトの色
	float32_t3 direction;  //ライトの向き
	float intensity;  //輝度
};

struct Camera {
    float32_t3 worldPosition;
}

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2);
struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input){
    float32_t4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    PixelShaderOutput output;

    float32_t3 toEye = normalize(gCamera.worldPosition - input.worldPosition);
    float32_t3 reflectLight = reflect(gDirectionalLight.direction, normalize(input.normal));

    if (gMaterial.enableLighting != 0) {  //Lightingする場合
        // half lambert
        float NdotL = dot(normalize(input.normal), -gDirectionalLight.direction);
        float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
        float RdotE = dot(reflectLight, toEye);
        float specularPow = pow(saturate(RdotE), gMaterial.shininess);  //反射強度
        //拡散反射
        float32_t3 diffuse = 
        gMaterial.color.rgb = textureColor.rgb * gDirectionalLight.rgb * cos * gDirectionKight.intensity;
        //鏡面反射
        floa32_t3 specular =
        gDirectionLight.color.rgb * gDirectionLight.intensity * specularPow * float32_t3(1.0f, 1.0f, 1.0f);
        //拡大反射+鏡面反射
        output.color.rgb = diffuse * specular;
        //アルファは今まで通り
        output.color.a = gMaterial.color.a * textureColor.a;
    } else {  //Lightingしない場合
        output.color = gMaterial.color * textureColor;
    }

    return output;
}