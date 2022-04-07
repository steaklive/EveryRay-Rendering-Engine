#include "Foliage.h"
#include "GameException.h"
#include "Game.h"
#include "MatrixHelper.h"
#include "Utility.h"
#include "Model.h"
#include "Mesh.h"
#include "VertexDeclarations.h"
#include "RasterizerStates.h"
#include "Scene.h"
#include "DirectionalLight.h"
#include "ER_ShadowMapper.h"
#include "ER_PostProcessingStack.h"
#include "Illumination.h"
#include "Camera.h"
#include "ShaderCompiler.h"

namespace Library
{
	FoliageSystem::FoliageSystem(Scene* aScene, DirectionalLight& light) 
	{
		assert(aScene);
		aScene->LoadFoliageZones(mFoliageCollection, light);
	}

	FoliageSystem::~FoliageSystem()
	{
		DeletePointerCollection(mFoliageCollection);
		DeleteObject(FoliageSystemInitializedEvent);
	}

	void FoliageSystem::Initialize()
	{
		for (auto& foliage : mFoliageCollection) {
			foliage->CreateBufferGPU();
		}

		for (auto listener : FoliageSystemInitializedEvent->GetListeners())
			listener();
	}

	void FoliageSystem::Update(const GameTime& gameTime, float gustDistance, float strength, float frequency)
	{
		for (auto& foliage : mFoliageCollection)
		{
			foliage->SetWindParams(gustDistance, strength, frequency);
			foliage->Update(gameTime);
		}
	}

	void FoliageSystem::Draw(const GameTime& gameTime, const ER_ShadowMapper* worldShadowMapper, FoliageRenderingPass renderPass)
	{
		for (auto& object : mFoliageCollection)
			object->Draw(gameTime, worldShadowMapper, renderPass);
	}

	void FoliageSystem::AddFoliage(Foliage* foliage)
	{
		assert(foliage);
		if (foliage)
			mFoliageCollection.emplace_back(foliage);
	}

	void FoliageSystem::SetVoxelizationParams(float* scale, const float* dimensions, XMFLOAT4* voxelCamera)
	{
		for (auto& object : mFoliageCollection)
			object->SetVoxelizationParams(scale, dimensions, voxelCamera);
	}
	
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	Foliage::Foliage(Game& pGame, Camera& pCamera, DirectionalLight& pLight, int pPatchesCount, std::string textureName, float scale, float distributionRadius, const XMFLOAT3& distributionCenter, FoliageBillboardType bType)
		:
		GameComponent(pGame),
		mCamera(pCamera),
		mDirectionalLight(pLight),
		mPatchesCount(pPatchesCount),
		mPatchesCountToRender(pPatchesCount),
		mScale(scale),
		mDistributionRadius(distributionRadius - 0.1f),
		mDistributionCenter(distributionCenter),
		mType(bType)
	{

		{
			ID3DBlob* blob = nullptr;
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Foliage.hlsl").c_str(), "VSMain", "vs_5_0", &blob)))
				throw GameException("Failed to load VSMain from shader: Foliage.hlsl!");
			if (FAILED(GetGame()->Direct3DDevice()->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mVS)))
				throw GameException("Failed to create vertex shader from Foliage.hlsl!");

			D3D11_INPUT_ELEMENT_DESC inputElementDescriptions[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
				{ "WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
				{ "WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
				{ "WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 }
			};

			HRESULT hr = GetGame()->Direct3DDevice()->CreateInputLayout(inputElementDescriptions, ARRAYSIZE(inputElementDescriptions), blob->GetBufferPointer(), blob->GetBufferSize(), &mInputLayout);
			if (FAILED(hr))
				throw GameException("CreateInputLayout() failed when creating foliage's vertex shader.", hr);
			blob->Release();

			blob = nullptr;
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Foliage.hlsl").c_str(), "GSMain", "gs_5_0", &blob)))
				throw GameException("Failed to load GSMain from shader: Foliage.hlsl!");
			if (FAILED(GetGame()->Direct3DDevice()->CreateGeometryShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mGS)))
				throw GameException("Failed to create geometry shader from Foliage.hlsl!");
			blob->Release();

			blob = nullptr;
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Foliage.hlsl").c_str(), "PSMain", "ps_5_0", &blob)))
				throw GameException("Failed to load PSMain from shader: Foliage.hlsl!");
			if (FAILED(GetGame()->Direct3DDevice()->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mPS)))
				throw GameException("Failed to create pixel shader from Foliage.hlsl!");
			blob->Release();

			blob = nullptr;
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Foliage.hlsl").c_str(), "PSMain_gbuffer", "ps_5_0", &blob)))
				throw GameException("Failed to load PSMain_gbuffer from shader: Foliage.hlsl!");
			if (FAILED(GetGame()->Direct3DDevice()->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mPS_GBuffer)))
				throw GameException("Failed to create pixel shader from Foliage.hlsl!");
			blob->Release();

			blob = nullptr;
			if (FAILED(ShaderCompiler::CompileShader(Utility::GetFilePath(L"content\\shaders\\Foliage.hlsl").c_str(), "PSMain_voxelization", "ps_5_0", &blob)))
				throw GameException("Failed to load PSMain_voxelization from shader: Foliage.hlsl!");
			if (FAILED(GetGame()->Direct3DDevice()->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &mPS_Voxelization)))
				throw GameException("Failed to create pixel shader from Foliage.hlsl!");
			blob->Release();
		}

		LoadBillboardModel(mType);

		if (FAILED(DirectX::CreateWICTextureFromFile(mGame->Direct3DDevice(), mGame->Direct3DDeviceContext(), Utility::ToWideString(textureName).c_str(), nullptr, &mAlbedoTexture)))
		{
			std::string message = "Failed to create Foliage Albedo Map: ";
			message += textureName;
			throw GameException(message.c_str());
		}

		CreateBlendStates();
		Initialize();
	}

	Foliage::~Foliage()
	{
		ReleaseObject(mVertexBuffer);
		ReleaseObject(mInstanceBuffer);
		ReleaseObject(mIndexBuffer);
		ReleaseObject(mAlbedoTexture);
		ReleaseObject(mAlphaToCoverageState);
		ReleaseObject(mNoBlendState);
		DeleteObject(mPatchesBufferCPU);
		DeleteObject(mPatchesBufferGPU);
		ReleaseObject(mInputLayout);
		ReleaseObject(mVS);
		ReleaseObject(mGS);
		ReleaseObject(mPS);
		ReleaseObject(mPS_GBuffer);
		ReleaseObject(mPS_Voxelization);
		mFoliageConstantBuffer.Release();
	}

	void Foliage::LoadBillboardModel(FoliageBillboardType bType)
	{
		if (bType == FoliageBillboardType::SINGLE) {
			mIsRotating = true;
			std::unique_ptr<Model> quadSingleModel(new Model(*mGame, Utility::GetFilePath("content\\models\\vegetation\\foliage_quad_single.obj"), true));
			quadSingleModel->Meshes()[0]->CreateVertexBuffer_PositionUvNormal(&mVertexBuffer);
			quadSingleModel->Meshes()[0]->CreateIndexBuffer(&mIndexBuffer);
			mVerticesCount = quadSingleModel->Meshes()[0]->Indices().size();
		}
		else if (bType == FoliageBillboardType::TWO_QUADS_CROSSING) {
			mIsRotating = false;
			std::unique_ptr<Model> quadDoubleModel(new Model(*mGame, Utility::GetFilePath("content\\models\\vegetation\\foliage_quad_double.obj"), true));
			quadDoubleModel->Meshes()[0]->CreateVertexBuffer_PositionUvNormal(&mVertexBuffer);
			quadDoubleModel->Meshes()[0]->CreateIndexBuffer(&mIndexBuffer);
			mVerticesCount = quadDoubleModel->Meshes()[0]->Indices().size();
		}
		else if (bType == FoliageBillboardType::THREE_QUADS_CROSSING) {
			mIsRotating = false;
			std::unique_ptr<Model> quadTripleModel(new Model(*mGame, Utility::GetFilePath("content\\models\\vegetation\\foliage_quad_triple.obj"), true));
			quadTripleModel->Meshes()[0]->CreateVertexBuffer_PositionUvNormal(&mVertexBuffer);
			quadTripleModel->Meshes()[0]->CreateIndexBuffer(&mIndexBuffer);
			mVerticesCount = quadTripleModel->Meshes()[0]->Indices().size();
		}
	}
	void Foliage::Initialize()
	{
		mFoliageConstantBuffer.Initialize(GetGame()->Direct3DDevice());
		InitializeBuffersCPU();
		//InitializeBuffersGPU();
	}

	void Foliage::CreateBlendStates()
	{
		D3D11_BLEND_DESC blendStateDescription;
		ZeroMemory(&blendStateDescription, sizeof(D3D11_BLEND_DESC));

		// Create an alpha enabled blend state description.
		blendStateDescription.AlphaToCoverageEnable = TRUE;
		blendStateDescription.RenderTarget[0].BlendEnable = TRUE;
		blendStateDescription.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		blendStateDescription.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		blendStateDescription.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blendStateDescription.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blendStateDescription.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		blendStateDescription.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendStateDescription.RenderTarget[0].RenderTargetWriteMask = 0x0f;

		// Create the blend state using the description.
		if (FAILED(mGame->Direct3DDevice()->CreateBlendState(&blendStateDescription, &mAlphaToCoverageState)))
			throw GameException("ID3D11Device::CreateBlendState() failed while create alpha-to-coverage blend state for foliage");

		blendStateDescription.RenderTarget[0].BlendEnable = FALSE;
		blendStateDescription.AlphaToCoverageEnable = FALSE;
		if (FAILED(mGame->Direct3DDevice()->CreateBlendState(&blendStateDescription, &mNoBlendState)))
			throw GameException("ID3D11Device::CreateBlendState() failed while create no blend state for foliage");
	}

	void Foliage::InitializeBuffersGPU(int count)
	{
		D3D11_BUFFER_DESC vertexBufferDesc, instanceBufferDesc;
		D3D11_SUBRESOURCE_DATA vertexData, instanceData;
		
		#pragma region QUAD_GENERATION
		//int vertexCount = 6;
		//FoliagePatchData* patchData = new FoliagePatchData[vertexCount];
		//
		//// Load the vertex array with data.
		//patchData[0].pos = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);  // Bottom left.
		//patchData[0].uv = XMFLOAT2(0.0f, 1.0f);
		//
		//patchData[1].pos = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);  // Top left.
		//patchData[1].uv = XMFLOAT2(0.0f, 0.0f);
		//
		//patchData[2].pos = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);  // Bottom right.
		//patchData[2].uv = XMFLOAT2(1.0f, 1.0f);
		//
		//patchData[3].pos = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);  // Bottom right.
		//patchData[3].uv = XMFLOAT2(1.0f, 1.0f);
		//
		//patchData[4].pos = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);  // Top left.
		//patchData[4].uv = XMFLOAT2(0.0f, 0.0f);
		//
		//patchData[5].pos = XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f);  // Top right.
		//patchData[5].uv = XMFLOAT2(1.0f, 0.0f);
		//
		//// Set up the description of the vertex buffer.
		//vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		//vertexBufferDesc.ByteWidth = sizeof(FoliagePatchData) * vertexCount;
		//vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		//vertexBufferDesc.CPUAccessFlags = 0;
		//vertexBufferDesc.MiscFlags = 0;
		//vertexBufferDesc.StructureByteStride = 0;
		//
		//// Give the subresource structure a pointer to the vertex data.
		//vertexData.pSysMem = patchData;
		//vertexData.SysMemPitch = 0;
		//vertexData.SysMemSlicePitch = 0;
		//
		//if (FAILED(mGame->Direct3DDevice()->CreateBuffer(&vertexBufferDesc, &vertexData, &mVertexBuffer)))
		//	throw GameException("ID3D11Device::CreateBuffer() failed while generating vertex buffer of foliage mesh patch");
		//
		//delete patchData;
		//patchData = nullptr;
#pragma endregion

		// instance buffer
		int instanceCount = count;
		mPatchesBufferGPU = new GPUFoliageInstanceData[instanceCount];

		for (int i = 0; i < instanceCount; i++)
		{
			float randomScale = Utility::RandomFloat(mScale - 1.0f, mScale + 1.0f);
			mPatchesBufferCPU[i].scale = randomScale;
			mPatchesBufferGPU[i].worldMatrix = XMMatrixScaling(randomScale, randomScale, randomScale) * XMMatrixTranslation(mPatchesBufferCPU[i].xPos, mPatchesBufferCPU[i].yPos, mPatchesBufferCPU[i].zPos);
			//mPatchesBufferGPU[i].color = XMFLOAT3(mPatchesBufferCPU[i].r, mPatchesBufferCPU[i].g, mPatchesBufferCPU[i].b);
		}

		// Set up the description of the instance buffer.
		instanceBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		instanceBufferDesc.ByteWidth = sizeof(GPUFoliageInstanceData) * instanceCount;
		instanceBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		instanceBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		instanceBufferDesc.MiscFlags = 0;
		instanceBufferDesc.StructureByteStride = 0;

		// Give the subresource structure a pointer to the instance data.
		instanceData.pSysMem = mPatchesBufferGPU;
		instanceData.SysMemPitch = 0;
		instanceData.SysMemSlicePitch = 0;

		if (FAILED(mGame->Direct3DDevice()->CreateBuffer(&instanceBufferDesc, &instanceData, &mInstanceBuffer)))
			throw GameException("ID3D11Device::CreateBuffer() failed while generating instance buffer of foliage patches");
	}

	void Foliage::InitializeBuffersCPU()
	{
		// randomly generate positions and color
		mPatchesBufferCPU = new CPUFoliageData[mPatchesCount];
		
		for (int i = 0; i < mPatchesCount; i++)
		{
			mPatchesBufferCPU[i].xPos = mDistributionCenter.x + ((float)rand() / (float)(RAND_MAX)) * mDistributionRadius - mDistributionRadius /2;
			mPatchesBufferCPU[i].yPos = mDistributionCenter.y;
			mPatchesBufferCPU[i].zPos = mDistributionCenter.z + ((float)rand() / (float)(RAND_MAX)) * mDistributionRadius - mDistributionRadius /2;

			mPatchesBufferCPU[i].r = ((float)rand() / (float)(RAND_MAX)) * 1.0f + 1.0f;
			mPatchesBufferCPU[i].g = ((float)rand() / (float)(RAND_MAX)) * 1.0f + 0.5f;
			mPatchesBufferCPU[i].b = 0.0f;
		}
	}

	void Foliage::Draw(const GameTime& gameTime, const ER_ShadowMapper* worldShadowMapper, FoliageRenderingPass renderPass)
	{
		if(renderPass == TO_VOXELIZATION)
			assert(worldShadowMapper);

		ID3D11DeviceContext* context = GetGame()->Direct3DDeviceContext();

		if (mPatchesCountToRender == 0)
			return;

		float blendFactor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		context->OMSetBlendState(mAlphaToCoverageState, blendFactor, 0xffffffff);

		unsigned int strides[2];
		unsigned int offsets[2];
		ID3D11Buffer* bufferPointers[2];

		strides[0] = sizeof(GPUFoliagePatchData);
		strides[1] = sizeof(GPUFoliageInstanceData);

		offsets[0] = 0;
		offsets[1] = 0;

		// Set the array of pointers to the vertex and instance buffers.
		bufferPointers[0] = mVertexBuffer;
		bufferPointers[1] = mInstanceBuffer;

		context->IASetVertexBuffers(0, 2, bufferPointers, strides, offsets);
		context->IASetIndexBuffer(mIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
		context->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		if (worldShadowMapper)
		{
			for (int cascade = 0; cascade < NUM_SHADOW_CASCADES; cascade++)
				mFoliageConstantBuffer.Data.ShadowMatrices[cascade] = XMMatrixTranspose(worldShadowMapper->GetViewMatrix(cascade) * worldShadowMapper->GetProjectionMatrix(cascade) * XMLoadFloat4x4(&MatrixHelper::GetProjectionShadowMatrix()));
			mFoliageConstantBuffer.Data.ShadowTexelSize = XMFLOAT4{ 1.0f / worldShadowMapper->GetResolution(), 1.0f, 1.0f , 1.0f };
			mFoliageConstantBuffer.Data.ShadowCascadeDistances = XMFLOAT4{ mCamera.GetCameraFarCascadeDistance(0), mCamera.GetCameraFarCascadeDistance(1), mCamera.GetCameraFarCascadeDistance(2), 1.0f };
		}
		
		mFoliageConstantBuffer.Data.World = XMMatrixIdentity();
		mFoliageConstantBuffer.Data.View = XMMatrixTranspose(mCamera.ViewMatrix());
		mFoliageConstantBuffer.Data.Projection = XMMatrixTranspose(mCamera.ProjectionMatrix());
		mFoliageConstantBuffer.Data.SunDirection = XMFLOAT4(-mDirectionalLight.Direction().x, -mDirectionalLight.Direction().y, -mDirectionalLight.Direction().z, 1.0f);
		mFoliageConstantBuffer.Data.SunColor = XMFLOAT4{ mDirectionalLight.GetDirectionalLightColor().x, mDirectionalLight.GetDirectionalLightColor().y, mDirectionalLight.GetDirectionalLightColor().z , 1.0f };
		mFoliageConstantBuffer.Data.AmbientColor = XMFLOAT4{ mDirectionalLight.GetAmbientLightColor().x, mDirectionalLight.GetAmbientLightColor().y, mDirectionalLight.GetAmbientLightColor().z , 1.0f };
		mFoliageConstantBuffer.Data.CameraDirection = XMFLOAT4(mCamera.Direction().x, mCamera.Direction().y, mCamera.Direction().z, 1.0f);
		mFoliageConstantBuffer.Data.CameraPos = XMFLOAT4(mCamera.Position().x, mCamera.Position().y, mCamera.Position().z, 1.0f);
		mFoliageConstantBuffer.Data.WindDirection = XMFLOAT4{ 0.0f, 0.0f, 1.0f , 1.0f };
		mFoliageConstantBuffer.Data.VoxelCameraPos = XMFLOAT4{ mVoxelCameraPos->x, mVoxelCameraPos->y, mVoxelCameraPos->z, 1.0 };
		mFoliageConstantBuffer.Data.RotateToCamera = (mIsRotating) ? 1.0f : 0.0f;;
		mFoliageConstantBuffer.Data.Time = static_cast<float>(gameTime.TotalGameTime());
		mFoliageConstantBuffer.Data.WindStrength = mWindStrength;
		mFoliageConstantBuffer.Data.WindFrequency = mWindFrequency;
		mFoliageConstantBuffer.Data.WindGustDistance = mWindGustDistance;
		mFoliageConstantBuffer.Data.WorldVoxelScale = *mWorldVoxelScale;
		mFoliageConstantBuffer.Data.VoxelTextureDimension = *mVoxelTextureDimension;
		mFoliageConstantBuffer.ApplyChanges(context);
		ID3D11Buffer* CBs[1] = { mFoliageConstantBuffer.Buffer() };
		
		ID3D11SamplerState* SS[2] = { SamplerStates::TrilinearWrap, SamplerStates::ShadowSamplerState };

		context->IASetInputLayout(mInputLayout);
		context->VSSetShader(mVS, NULL, NULL);
		context->VSSetConstantBuffers(0, 1, CBs);

		if (renderPass == TO_VOXELIZATION)
		{
			context->GSSetShader(mGS, NULL, NULL);
			context->GSSetConstantBuffers(0, 1, CBs);

			context->PSSetShader(mPS_Voxelization, NULL, NULL);
		}
		else if (renderPass == TO_GBUFFER)
		{
			context->PSSetShader(mPS_GBuffer, NULL, NULL);
		}
		context->PSSetConstantBuffers(0, 1, CBs);
		context->PSSetSamplers(0, 2, SS);
		
		ID3D11ShaderResourceView* SRs[1 + NUM_SHADOW_CASCADES] = { mAlbedoTexture };
		if (worldShadowMapper)
			for (int i = 0; i < NUM_SHADOW_CASCADES; i++)
				SRs[1 + i] = worldShadowMapper->GetShadowTexture(i);

		context->PSSetShaderResources(0, 1 + NUM_SHADOW_CASCADES, SRs);

		context->DrawIndexedInstanced(mVerticesCount, mPatchesCountToRender, 0, 0, 0);
		context->OMSetBlendState(mNoBlendState, blendFactor, 0xffffffff);

		context->VSSetShader(NULL, NULL, NULL);
		context->PSSetShader(NULL, NULL, NULL);
		if (renderPass == TO_VOXELIZATION)
			context->GSSetShader(NULL, NULL, NULL);

		ID3D11ShaderResourceView* nullSRV[] = { NULL };
		context->PSSetShaderResources(0, 1, nullSRV);

		ID3D11Buffer* nullCBs[] = { NULL };
		context->VSSetConstantBuffers(0, 1, nullCBs);
		context->PSSetConstantBuffers(0, 1, nullCBs);
		if (renderPass == TO_VOXELIZATION)
			context->GSSetConstantBuffers(0, 1, nullCBs);

		ID3D11SamplerState* nullSSs[] = { NULL };
		context->PSSetSamplers(0, 1, nullSSs);
	}

	void Foliage::Update(const GameTime& gameTime)
	{
		ID3D11DeviceContext* context = GetGame()->Direct3DDeviceContext();
		XMFLOAT3 toCam = { mDistributionCenter.x - mCamera.Position().x, mDistributionCenter.y - mCamera.Position().y, mDistributionCenter.z - mCamera.Position().z };
		float distanceToCam = sqrt(toCam.x * toCam.x + toCam.y*toCam.y + toCam.z*toCam.z);

		//UpdateBufferGPU();
		CalculateDynamicLOD(distanceToCam);
	}

	void Foliage::CreateBufferGPU()
	{
		ID3D11DeviceContext* context = GetGame()->Direct3DDeviceContext();

		mPatchesCountVisible = 0;
		for (int i = 0; i < mPatchesCount; i++)
		{
			if (mPatchesBufferCPU[i].yPos != -999.0f /*|| culledPatch */) {
				mPatchesCountVisible++;
			}
		}

		InitializeBuffersGPU(mPatchesCountVisible);
		UpdateBuffersGPU();
	}

	// updating world matrices of visible patches
	void Foliage::UpdateBuffersGPU() 
	{
		ID3D11DeviceContext* context = GetGame()->Direct3DDeviceContext();

		double angle;
		float rotation, windRotation;
		XMMATRIX translationMatrix;

		for (int i = 0; i < mPatchesCountVisible; i++)
		{
			translationMatrix = XMMatrixTranslation(mPatchesBufferCPU[i].xPos, mPatchesBufferCPU[i].yPos, mPatchesBufferCPU[i].zPos);
			mPatchesBufferGPU[i].worldMatrix = XMMatrixScaling(mPatchesBufferCPU[i].scale, mPatchesBufferCPU[i].scale, mPatchesBufferCPU[i].scale) * translationMatrix;
		}

		D3D11_MAPPED_SUBRESOURCE mappedResource;
		GPUFoliageInstanceData* instancesPtr;

		if (FAILED(context->Map(mInstanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
			throw GameException("Map() failed while updating instance buffer of foliage patches");

		instancesPtr = (GPUFoliageInstanceData*)mappedResource.pData;

		memcpy(instancesPtr, (void*)mPatchesBufferGPU, (sizeof(GPUFoliageInstanceData) * /*mPatchesCount*/mPatchesCountVisible));
		context->Unmap(mInstanceBuffer, 0);
	}

	void Foliage::CalculateDynamicLOD(float distanceToCam)
	{
		float factor = (distanceToCam - 150.0f) / mMaxDistanceToCamera;

		if (factor > 1.0f)
			factor = 1.0f;
		else if (factor < 0.0f)
			factor = 0.0f;

		mPatchesCountToRender = /*mPatchesCount*/mPatchesCountVisible * (1.0f - factor);
	}

}