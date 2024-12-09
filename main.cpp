#include <Windows.h>
#include <cstdint>
#include <string>
#include <dxgidebug.h>
#include <format>
#include <fstream>
#include <sstream>
#include "Matrix.h"
#include "Input.h"
#include "WinApp.h"
#include "DirectXCommon.h"
#include "Logger.h"
#include "StringUtility.h"
#include "externals/DirectXTex/DirectXTex.h"
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")

class ResourceObject {
public:
	ResourceObject(ID3D12Resource* resource)
		:resource_(resource)
	{}
	~ResourceObject() {
		if (resource_) {
			resource_->Release();
		}
	}
	ID3D12Resource* Get() { return resource_; }
private:
	ID3D12Resource* resource_;
};

//4次元ベクトルを表す
struct Vector4 {
	float x;
	float y;
	float z;
	float w;
};

//2次元ベクトルを表す
struct Vector2 {
	float x;
	float y;
};

struct Transform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

struct VertexData {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
};

//MaterialData構造体
struct MaterialData {
	std::string textureFilePath;
};

//ModelData構造体
struct ModelData {
	std::vector<VertexData> vertices;
	MaterialData material;
};

struct D3DResourceLeakChecker {
	~D3DResourceLeakChecker() 
	{
		//リソースリークチェック
		Microsoft::WRL::ComPtr<IDXGIDebug1> debug;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
			debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
			debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
			debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
		}
	}
};

DirectX::ScratchImage LoadTexture(const std::string& filePath)
{
	//テクスチャファイルを読んでプログラムで扱えるようにする
	DirectX::ScratchImage image{};
	std::wstring filePathW = StringUtility::ConvertString(filePath);
	HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));

	//ミニマップの作成
	DirectX::ScratchImage mipImages{};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
	assert(SUCCEEDED(hr));

	//ミニマップ付きのデータを渡す
	return mipImages;
}

ID3D12Resource* CreateBufferResource(ID3D12Device* device, size_t sizeInBytes) {
	//頂点リソース用のヒープの設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;  //UploadHeapを使う
	//頂点リソースの設定
	D3D12_RESOURCE_DESC vertexResourceDesc{};
	//バッファリソース
	vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertexResourceDesc.Width = sizeInBytes;  //リソースのサイズ
	//バッファの場合はこれらは1にする決まり
	vertexResourceDesc.Height = 1;
	vertexResourceDesc.DepthOrArraySize = 1;
	vertexResourceDesc.MipLevels = 1;
	vertexResourceDesc.SampleDesc.Count = 1;
	//バッファの場合はこれにする決まり
	vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	//実際に頂点リソースを作る
	ID3D12Resource* resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE,
		&vertexResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));
	return resource;
}

Microsoft::WRL::ComPtr<ID3D12Resource>
CreateTextureResource(Microsoft::WRL::ComPtr<ID3D12Device> device, const DirectX::TexMetadata& metadata) 
{
	//1.metadataを基にResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width);  //Textureの幅
	resourceDesc.Height = UINT(metadata.height);  //Textureの高さ
	resourceDesc.MipLevels = UINT16(metadata.mipLevels);  //mipmapの数
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);  //奥行き or 配列Textureの配列数
	resourceDesc.Format = metadata.format;  //TextureのFormat
	resourceDesc.SampleDesc.Count = 1;  //サンプリングカウント。1固定。
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);  //Textureの次元数。
	//2.利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM;  //細かい設定を行う
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;  //WriteBackポリシーでCPUアクセス可能
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;  //プロセッサの近くに配置
	//3.Resourceを生成する
	ID3D12Resource* resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,  //Heapの設定
		D3D12_HEAP_FLAG_NONE,  //Heapの特殊な設定。
		&resourceDesc,  //Resourceの設定
		D3D12_RESOURCE_STATE_GENERIC_READ,  //初回のResourceState。
		nullptr,  //Clear最適地。
		IID_PPV_ARGS(&resource));  //作成するResourceポインタへのポインタ
	assert(SUCCEEDED(hr));
	return resource;
}

ID3D12Resource* CreateDepthStencilTextureResource(ID3D12Device* device, int32_t width, int32_t height) {
	//生成するResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width;  //Textureの幅
	resourceDesc.Height = height;  //Textureの高さ
	resourceDesc.MipLevels = 1;  //mipmapの数
	resourceDesc.DepthOrArraySize = 1;  //奥行き or 配列Textureの配列数
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;  //DepthStencilとして利用可能なフォーマット
	resourceDesc.SampleDesc.Count = 1;  //サンプリングカウント。
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;  //2次元
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;  //DepthStencilとして使う通知

	//利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;  //VRAM上に作る

	//深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;  //1.0fでクリア
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;  //フォーマット

	//Resourceの生成
	ID3D12Resource* resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,  //Heapの設定
		D3D12_HEAP_FLAG_NONE,  //Heapの特殊な設定。
		&resourceDesc,  //Resourceの設定
		D3D12_RESOURCE_STATE_DEPTH_WRITE,  //深度値を書き込む状態にしておく
		&depthClearValue,  //Clear最適地
		IID_PPV_ARGS(&resource));  //作成するResourceポインタへのポインタ
	assert(SUCCEEDED(hr));
	return resource;
}

void UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages) {
	//Meta情報を取得
	const DirectX::TexMetadata metadata = mipImages.GetMetadata();
	//全MipMapを取得
	for (size_t mipLevel = 0; mipLevel < metadata.mipLevels; ++mipLevel) {
		//MipMapLevelを指定して各Imageを取得
		const DirectX::Image* img = mipImages.GetImage(mipLevel, 0, 0);
		//Textureに転移
		HRESULT hr = texture->WriteToSubresource(
			UINT(mipLevel),
			nullptr,  //全領域へコピー
			img->pixels,  //元データアドレス
			UINT(img->rowPitch),  //1ラインサイズ
			UINT(img->slicePitch)  //1枚サイズ
		);
	}
}

MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename) {
	//1.中で必要となる変数の宣言
	MaterialData materialData;  //検索するMaterialData
	std::string line;  //ファイルから読んだ1行を格納するもの
	//2.ファイルを開く
	std::ifstream file(directoryPath + "/" + filename);  //ファイルを開く
	assert(file.is_open());  //とりあえず開けなかったら止める
	//3.実際にファイルを読み、MaterialDataを検索していく
	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		//identifierに応じた処理
		if (identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;
			//連結してファイルパスにする
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}
	//4.MaterialDataを返す
	return materialData;
}

ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename) {
	// 1.中で必要となる変数の宣言
	ModelData modelData;  //構築するModelData
	std::vector<Vector4> positions;  //位置
	std::vector<Vector3> normals;  //法線
	std::vector<Vector2> texcoords;  //テクスチャ座標
	std::string line;  //ファイルから読んだ1行を格納するもの
	// 2.ファイルを開く
	std::ifstream file(directoryPath + "/" + filename);  //ファイルを開く
	assert(file.is_open());  //とりあえず開けなかったら止める
	// 3.実際にファイルを読み、ModelDataを構築していく
	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;  //先頭の識別子を読む

		// identifierに応じた処理
		if (identifier == "v") {
			Vector4 position = {};
			s >> position.x >> position.y >> position.z;
			position.w = 1.0f;
			positions.push_back(position);
		}
		else if (identifier == "vt") {
			Vector2 texcoord = {};
			s >> texcoord.x >> texcoord.y;
			texcoords.push_back(texcoord);
		}
		else if (identifier == "vn") {
			Vector3 normal = {};
			s >> normal.x >> normal.y >> normal.z;
			normals.push_back(normal);
		}
		else if (identifier == "f") {
			VertexData triangle[3] = {};
			//面は三角形限定。
			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
				std::string vertexDefinition;
				s >> vertexDefinition;
				//Indexを取得する
				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3] = {};
				for (int32_t element = 0; element < 3; ++element) {
					std::string index;
					std::getline(v, index, '/');  //区切りでインデックスを読んでいく
					elementIndices[element] = std::stoi(index);
				}
				//要素へのIndexから、実際の要素を値を取得して、頂点を構築する
				Vector4 position = positions[elementIndices[0] - 1];
				Vector2 texcoord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];
				position.x *= -1.0f;
				texcoord.y = 1.0f - texcoord.y;
				normal.x *= -1.0f;
				triangle[faceVertex] = { position, texcoord, normal };
			}
			//頂点を逆順で登録することで、周り順を逆にする
			modelData.vertices.push_back(triangle[2]);
			modelData.vertices.push_back(triangle[1]);
			modelData.vertices.push_back(triangle[0]);
		}
		else if (identifier == "mtllib") {
			//materialTemplateLibraryファイルの名前を取得する
			std::string materialFilename;
			s >> materialFilename;
			//基本的にobjファイルと同一階層にmtlは存在させるので、ディレクトリ名とファイル名を渡す
			modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
		}
	}
	// 4.ModelDataを返す
	return modelData;
}

IDxcBlob* CompileShader(
	//CompilerするShaderファイルへのパス
	const std::wstring& filePath,
	//Compilerに使用するProfile
	const wchar_t* profile,
	//初期化で生成したものを3つ
	IDxcUtils* dxcUtils,
	IDxcCompiler3* dxcCompiler,
	IDxcIncludeHandler* includeHandler)
{
	//1.hlslファイルを読み込む
	//これからシェーダーをコンパイルする旨をログに出す
	Logger::Log(StringUtility::ConvertString(std::format(L"Begin CompileShader, path:{}, profile:{}\n", filePath, profile)));
	//hlslファイルを読み込む
	IDxcBlobEncoding* shaderSource = nullptr;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	//読めなかったら止める
	assert(SUCCEEDED(hr));
	//読み込んだファイルの内容を設定する
	DxcBuffer shaderSourceBuffer = {};
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;  //UTF8の文字コードであることを通知

	//2.Compileする
	LPCWSTR arguments[] = {
		filePath.c_str(),
		L"-E", L"main",  //コンパイル対象のhlslファイル名
		L"-T", profile,  //ShaderProfileの設定
		L"-Zi", L"-Qembed_debug",  //デバック用の情報を埋め込む
		L"-Od",  //最適解を外しておく
		L"-Zpr",  //メモリレイアウトは行優先
	};
	//実際にshaderをコンパイルする
	IDxcResult* shaderResult = nullptr;
	hr = dxcCompiler->Compile(
		&shaderSourceBuffer,  //読み込んだファイル
		arguments,  //コンパイルオプション
		_countof(arguments),  //コンパイルオプションの数
		includeHandler,  //includeが含まれた諸々
		IID_PPV_ARGS(&shaderResult)  //コンパイル結果
	);
	//コンパイルエラーではなくdxcが起動できないなど致命的な状況
	assert(SUCCEEDED(hr));

	//3.警告・エラーがでていないか確認する
	IDxcBlobUtf8* shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		Logger::Log(shaderError->GetStringPointer());
		assert(false);
	}

	//4.Compile結果を受け取って返す
	//コンパイル結果から実行用のバイナリ部分を取得
	IDxcBlob* shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));
	//成功したログを出す
	Logger::Log(StringUtility::ConvertString(std::format(L"Begin CompileShader, path:{}, profile : {}\n", filePath, profile)));
	//もう使わないリソースを解放
	shaderSource->Release();
	shaderResult->Release();
	//実行用のバイナリを返却
	return shaderBlob;
}

//Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	D3DResourceLeakChecker leakCheck;

	//ポインタ
	Input* input = nullptr;
	WinApp* winApp = nullptr;
	DirectXCommon* dxCommon = nullptr;

	//WindowsAPIの初期化
	winApp = new WinApp();
	winApp->Initialize();

	//入力の初期化
	input = new Input();
	input->Initialize(winApp);

	//DirectXの初期化
	dxCommon = new DirectXCommon();
	dxCommon->Initialize(winApp);

	/*
	//RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	//DescriptorRange
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;  //0から始まる
	descriptorRange[0].NumDescriptors = 1;  //数は1つ
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;  //SRVを使う
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;  //Offsetを自動計算

	//RootParameter作成。
	D3D12_ROOT_PARAMETER rootParameters[3] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;  //CBVを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;  //PixelShaderを使う
	rootParameters[0].Descriptor.ShaderRegister = 0;  //レジスタ番号0とバインド
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;  //CBVを使う
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;  //VertexShaderを使う
	rootParameters[1].Descriptor.ShaderRegister = 0;  //レジスタ番号0を使う
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;  //DescriptorTableを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;  //PixelShaderで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;  //Tableの中身の配列を指定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);  //Tableで利用する数
	descriptionRootSignature.pParameters = rootParameters;  //ルートパラメータ配列へのポインタ
	descriptionRootSignature.NumParameters = _countof(rootParameters);  //配列の長さ

	//Samplerの設定
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;  //バイリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;  //0~1の範囲外をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;  //比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;  //ありったけのMipmapを使う
	staticSamplers[0].ShaderRegister = 0;  //レジスタ番号0を使う
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;  //PixelShaderで使う
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	//シリアライズしてバイナリにする
	ID3DBlob* signatureBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}
	//バイナリを元に生成
	ID3D12RootSignature* rootSignature = nullptr;
	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(hr));

	//InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[2] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	//BlendStateの設定
	D3D12_BLEND_DESC blendDesc{};
	//すべての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask =
		D3D12_COLOR_WRITE_ENABLE_ALL;

	//RasiterzerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	//裏面を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	//三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	//Shaderをコンパイルする
	IDxcBlob* vertexShaderBlob = CompileShader(L"resources/shaders/Object3D.VS.hlsl",
		L"vs_6_0", dxcUtils, dxcCompiler, includeHandler);
	assert(vertexShaderBlob != nullptr);

	IDxcBlob* pixelShaderBlob = CompileShader(L"resources/shaders/Object3D.PS.hlsl",
		L"ps_6_0", dxcUtils, dxcCompiler, includeHandler);
	assert(pixelShaderBlob != nullptr);

	//DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	//Depthの機能を有効化する
	depthStencilDesc.DepthEnable = true;
	//書き込みします
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	//比較関数はLessEqual。
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	//PSOを作成する
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature;  //RootSignature
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;  //InputLayout
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
	vertexShaderBlob->GetBufferSize() };  //VertexShader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),
	pixelShaderBlob->GetBufferSize() };  //PixelShader
	graphicsPipelineStateDesc.BlendState = blendDesc;  //BlendState
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;  //ResterizerState
	//書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	//利用するトロポジのタイプ
	graphicsPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	//どのように画面に色を打ち込むかの設定
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	//DepthStencilの設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	//実際に生成
	ID3D12PipelineState* graphicsPipelineState = nullptr;
	hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc,
		IID_PPV_ARGS(&graphicsPipelineState));
	assert(SUCCEEDED(hr));

	//モデル読み込み
	ModelData modelData = LoadObjFile("resources", "plane.obj");
	//頂点リソースを作る
	ID3D12Resource* vertexResource = CreateBufferResource(device, sizeof(VertexData) * modelData.vertices.size());
	//頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	//リソースの先頭のアドレスから使う
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	//使用するリソースのサイズは頂点のサイズ
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size()); 
	//1頂点あたりのサイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData);  

	//頂点リソースにデータを書き込む
	VertexData* vertexData = nullptr;
	//書き込むためのアドレスを取得
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	//頂点データをリリースにコピー
	std::memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData)* modelData.vertices.size());  

	ID3D12Resource* vertexResourceSprite = CreateBufferResource(device, sizeof(VertexData) * 6);
	//頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
	//リソースの先頭のアドレスから使う
	vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress();
	//使用するリソースのサイズは頂点6つ分のサイズ
	vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 6;
	//1頂点あたりのサイズ
	vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);

	ID3D12Resource* indexResourceSprite = CreateBufferResource(device, sizeof(uint32_t) * 6);
	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
	//リソースの先頭のアドレスから使う
	indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress();
	//使用するリソースのサイズはインデックス6つ分のサイズ
	indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
	//インデックスはuint32_tとする
	indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;
	//インデックスリソースにデータを書き込む
	uint32_t* indexDataSprite = nullptr;
	indexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSprite));
	indexDataSprite[0] = 0;  indexDataSprite[1] = 1;  indexDataSprite[2] = 2;
	indexDataSprite[3] = 1;  indexDataSprite[4] = 3;  indexDataSprite[5] = 2;

	//頂点データを設定する
	VertexData* vertexDataSprite = nullptr;
	vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));
	//1枚目の三角形
	vertexDataSprite[0].position = { 0.0f, 360.0f, 0.0f, 1.0f };  //左下
	vertexDataSprite[0].texcoord = { 0.0f, 1.0f };
	vertexDataSprite[1].position = { 0.0f, 0.0f, 0.0f, 1.0f };  //左上
	vertexDataSprite[1].texcoord = { 0.0f, 0.0f };
	vertexDataSprite[2].position = { 640.0f, 360.0f, 0.0f, 1.0f };  //右下
	vertexDataSprite[2].texcoord = { 1.0f, 1.0f };
	//2度目の三角形
	vertexDataSprite[3].position = { 0.0f, 0.0f, 0.0f, 1.0f };  //左上
	vertexDataSprite[3].texcoord = { 0.0f, 0.0f };
	vertexDataSprite[4].position = { 640.0f, 0.0f, 0.0f, 1.0f };  //右上
	vertexDataSprite[4].texcoord = { 1.0f, 0.0f };
	vertexDataSprite[5].position = { 640.0f, 360.0f, 0.0f, 1.0f };  //右下
	vertexDataSprite[5].texcoord = { 1.0f, 1.0f };

	//Sprite用のTransformationMatrix用のリソースを作る
	ID3D12Resource* transfromationMatrixResourceSprite = CreateBufferResource(device, sizeof(Matrix4x4));
	//データを書き込む
	Matrix4x4* transfromationMatrixDataSprite = nullptr;
	//書き込むためのアドレスを取得
	transfromationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&transfromationMatrixDataSprite));
	//単位行列を書きこんでおく
	*transfromationMatrixDataSprite = MakeIdentityx4x4();

	//マテリアル用のリソースを作る。
	ID3D12Resource* materialResource = CreateBufferResource(device, sizeof(Vector4));
	//マテリアルにデータを書き込む
	Vector4* materialData = nullptr;
	//書き込むためのアドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	//今回は白を書き込んでみる
	*materialData = Vector4(1.0f, 1.0f, 1.0f, 1.0f);

	//WVP用のリソースを作る。
	ID3D12Resource* wvpResource = CreateBufferResource(device, sizeof(Matrix4x4));
	//データを書き込む
	Matrix4x4* wvpData = nullptr;
	//書き込むためのアドレスを取得
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	//単位行列を書き込んでおく
	*wvpData = MakeIdentityx4x4();

	//Textureを読んで転送する
	DirectX::ScratchImage mipImages = LoadTexture("resources/uvChecker.png");
	DirectX::ScratchImage mipImages2 = LoadTexture(modelData.material.textureFilePath);
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	ID3D12Resource* textureResource = CreateTextureResource(device, metadata);
	UploadTextureData(textureResource, mipImages);

	//DepthStencilTextureをウィンドウのサイズで作成
	ID3D12Resource* depthStencilResource = CreateDepthStencilTextureResource(device, WinApp::kClientWidth, WinApp::kClientHeight);

	//metaDataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2Dテクスチャ
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	//SRVを作成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	//先頭はImGuiが使っているのでその次を使う
	textureSrvHandleCPU.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	textureSrvHandleGPU.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	//SRVの生成
	device->CreateShaderResourceView(textureResource, &srvDesc, textureSrvHandleCPU);

	//Transform変数を作る
	Transform transform{ {1.0f,1.0f, 1.0f}, {0.0f, 0.0f, 0.0f }, {0.0f, 0.0f, 0.0f } };
	Transform cameraTransfrom{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f }, {0.0f, 0.0f, -5.0f } };
	Transform transformSprite{ {1.0f,1.0f,1.0},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0} };

	//ウインドウのxボタンが押されるまでループ
	while (true) {
		//WINdowにメッセージが来てたら最優先で処理させる
		if (winApp->ProcessMessage()) {
			//ゲームループを抜ける
			break;
		}
		else {
			//ゲームの処理
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			//ImGui
			ImGui::Begin("Setting");
			//生成
			const char* items[] = { "Sprite","Sphere"};
			static int item_current = 0;
			ImGui::Combo("Model", &item_current, items, IM_ARRAYSIZE(items));
			if (ImGui::Button("Create")) {

			}
			ImGui::Separator();
			//オブジェクト移動
			ImGui::SetNextItemOpen(true, 60);
			if (ImGui::CollapsingHeader("Object")) {
				ImGui::DragFloat3("Object.Translate", &transform.translate.x);
				ImGui::DragFloat3("Object.Rotate", &transform.rotate.x);
				ImGui::DragFloat3("Object.Scale", &transform.scale.x);
				if (ImGui::Button("Object.Delete")) {
					transform.translate.x = 0.0f, transform.translate.y = 0.0f, transform.translate.z = 0.0f;
					transform.rotate.x = 0.0f, transform.rotate.y = 0.0f, transform.rotate.z = 0.0f;
					transform.scale.x = 1.0f, transform.scale.y = 1.0f, transform.scale.z = 1.0f;
				}
				ImGui::Indent();
				if (ImGui::CollapsingHeader("Object.Material")) {
					ImGui::ColorEdit3("color", &materialData->x);
				}
				ImGui::Unindent();
			}
			//スプライト移動
			ImGui::SetNextItemOpen(true, 60);
			if (ImGui::CollapsingHeader("Sprite")) {
				ImGui::DragFloat3("Sprite.Translate", &transformSprite.translate.x);
				ImGui::DragFloat3("Sprite.Rotate", &transformSprite.rotate.x);
				ImGui::DragFloat3("Sprite.Scale", &transformSprite.scale.x);
				if (ImGui::Button("Sprite.Delete")) {
					transformSprite.translate.x = 0.0f, transformSprite.translate.y = 0.0f, transformSprite.translate.z = 0.0f;
					transformSprite.rotate.x = 0.0f, transformSprite.rotate.y = 0.0f, transformSprite.rotate.z = 0.0f;
					transformSprite.scale.x = 1.0f, transformSprite.scale.y = 1.0f, transformSprite.scale.z = 1.0f;
				}
				ImGui::Indent();
				if (ImGui::CollapsingHeader("Sprite.Material")) {
					ImGui::ColorEdit3("color", &materialData->x);
				}
				ImGui::Unindent();
			}
			ImGui::End();

			//入力の更新
			input->Update();

			//三角形回転
			Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
			Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransfrom.scale, cameraTransfrom.rotate, cameraTransfrom.translate);
			Matrix4x4 viewMatrix = Inverse(cameraMatrix);
			Matrix4x4 projectMatrix = MakePerspectiveFovMatrix(0.45f, float(WinApp::kClientWidth) / float(WinApp::kClientHeight), 0.1f, 100.0f);
			Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectMatrix));
			*wvpData = worldViewProjectionMatrix;

			//Sprite用のWorldViewProjectionMatrixを作る
			Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
			Matrix4x4 viewMatrixSprite = MakeIdentityx4x4();
			Matrix4x4 projectMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, float(WinApp::kClientWidth), float(WinApp::kClientHeight), 0.0f, 100.0f);
			Matrix4x4 worldViewProjectionMatrixSprite = Multiply(worldMatrixSprite, Multiply(viewMatrixSprite, projectMatrixSprite));
			*transfromationMatrixDataSprite = worldViewProjectionMatrixSprite;

			//バックバッファのインデックスを取得
			UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

			//TransitionBarrierの設定
			D3D12_RESOURCE_BARRIER barrier{};
			//今回のバリアはTransition
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			//Noneにしておく
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			//バリアを張る対象のリソース。
			barrier.Transition.pResource = swapChainResources[backBufferIndex];
			//遷移前のResourceState
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			//遷移後のResourceState
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			//TransitionBarrierを張る
			commandList->ResourceBarrier(1, &barrier);

			//ImGuiの内部コマンドを生成する
			ImGui::Render();

			//描画先のRTVとDSVを設定する
			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle);
			//指定した色で画面全体をクリアする
			float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };   //青っぽい色。
			commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);
			//指定した深度で画面全体をクリアする
			commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
			//描画用のDescriptorHeapの設定
			ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap };
			commandList->SetDescriptorHeaps(1, descriptorHeaps);
			commandList->RSSetViewports(1, &viewport);  //Viewportの設定
			commandList->RSSetScissorRects(1, &scissorRect);  //Scirssorを設定
			//RootSignatureを設定
			commandList->SetGraphicsRootSignature(rootSignature);
			commandList->SetPipelineState(graphicsPipelineState);  //PSOを設定
			commandList->IASetVertexBuffers(0, 1, &vertexBufferView);  //VBVを設定
			//形状を設定。
			commandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			//マテリアルCBufferの場所を設定
			commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
			//wvp用のCBufferの場所を設定
			commandList->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());
			//SRVのDescriptorTableの先頭を設定。
			commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);
			//描画
			commandList->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);
			//Spriteの描画
			commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite);  //VBVを設定
			commandList->IASetIndexBuffer(&indexBufferViewSprite);  //IBVを設定
			//TransformationMatrixCBufferの場所を設定
			commandList->SetGraphicsRootConstantBufferView(1, transfromationMatrixResourceSprite->GetGPUVirtualAddress());
			//描画
			commandList->DrawInstanced(6, 1, 0, 0);
			commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
			//状態を遷移
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
			//TransitionBarrierをはる
			commandList->ResourceBarrier(1, &barrier);
			//コマンドリストの内容を確定させる。
			hr = commandList->Close();
			assert(SUCCEEDED(hr));

			//GPUにコマンドリストの実行を行わせる
			ID3D12CommandList* commandLists[] = { commandList };
			commandQueue->ExecuteCommandLists(1, commandLists);
			//GPUとOSに画面の交換を行うよう通知する
			swapChain->Present(1, 0);
			//Fenceの値を更新
			fenceValue++;
			//GPUがここまでたどり着いたときに、Fenceの値を指定した値を代入するようにSignalを送る
			commandQueue->Signal(fence, fenceValue);
			//GetCompletedValueの初期化はFence作成時に渡した初期値
			if (fence->GetCompletedValue() < fenceValue)
			{
				//指定したSignalにたどりついていないので、たどり着くまで待つようにイベントを設定する
				fence->SetEventOnCompletion(fenceValue, fenceEvent);
				//イベントを待つ
				WaitForSingleObject(fenceEvent, INFINITE);
			}
			//次のフレーム用のコマンドリストを準備
			hr = commandAllocator->Reset();
			assert(SUCCEEDED(hr));
			hr = commandList->Reset(commandAllocator, nullptr);
			assert(SUCCEEDED(hr));
		}
		CoUninitialize();

	*/

	//ImGuiの終了処理
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	//解放処理
	//CloseHandle(fenceEvent);

	CloseWindow(winApp->GetHwnd());

	//人力解放
	delete input;

	//WindowsAPIの終了処理
	winApp->Finalize();

	//DirectX解放
	delete dxCommon;

	//WindowsAPI解放
	delete winApp;
	winApp = nullptr;

	return 0;
}