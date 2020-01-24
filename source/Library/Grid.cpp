#include "stdafx.h"

#include "Grid.h"
#include "Game.h"
#include "GameException.h"
#include "Camera.h"
#include "ColorHelper.h"
#include "VectorHelper.h"
#include "MatrixHelper.h"
#include "Utility.h"
#include "VertexDeclarations.h"
#include <fstream>
#include <iostream>

namespace Library
{
	RTTI_DEFINITIONS(Grid)

	const UINT Grid::DefaultSize = 64;
	const UINT Grid::DefaultScale = 64;
	const XMFLOAT4 Grid::DefaultColor = XMFLOAT4(0.961f, 0.871f, 0.702f, 1.0f);

	Grid::Grid(Game& game, Camera& camera)
		: DrawableGameComponent(game), mEffect(nullptr), mTechnique(nullptr), mPass(nullptr), mWvpVariable(nullptr), mInputLayout(nullptr), mVertexBuffer(nullptr),
		mPosition(Vector3Helper::Zero), mSize(DefaultSize), mScale(DefaultScale), mColor(DefaultColor), mWorldMatrix(MatrixHelper::Identity)
	{
		mCamera = &camera;
	}

	Grid::Grid(Game& game, Camera& camera, UINT size, UINT scale, XMFLOAT4 color)
		: DrawableGameComponent(game), mEffect(nullptr), mTechnique(nullptr), mPass(nullptr), mWvpVariable(nullptr), mInputLayout(nullptr), mVertexBuffer(nullptr),
		mPosition(Vector3Helper::Zero), mSize(size), mScale(scale), mColor(color), mWorldMatrix(MatrixHelper::Identity)
	{
		mCamera = &camera;
	}

	Grid::~Grid()
	{
		ReleaseObject(mWvpVariable);
		ReleaseObject(mPass);
		ReleaseObject(mTechnique);
		ReleaseObject(mEffect);
		ReleaseObject(mInputLayout);
		ReleaseObject(mVertexBuffer);
	}

	const XMFLOAT3& Grid::Position() const
	{
		return mPosition;
	}

	const XMFLOAT4& Grid::Color() const
	{
		return mColor;
	}

	const UINT Grid::Size() const
	{
		return mSize;
	}

	const UINT Grid::Scale() const
	{
		return mScale;
	}

	void Grid::SetPosition(const XMFLOAT3& position)
	{
		mPosition = position;

		XMMATRIX translation = XMMatrixTranslation(mPosition.x, mPosition.y, mPosition.z);
		XMStoreFloat4x4(&mWorldMatrix, translation);
	}

	void Grid::SetPosition(float x, float y, float z)
	{
		mPosition.x = x;
		mPosition.y = y;
		mPosition.z = z;

		XMMATRIX translation = XMMatrixTranslation(mPosition.x, mPosition.y, mPosition.z);
		XMStoreFloat4x4(&mWorldMatrix, translation);
	}

	void Grid::SetColor(const XMFLOAT4& color)
	{
		mColor = color;
		InitializeGrid();
	}

	void Grid::SetSize(UINT size)
	{
		mSize = size;
		InitializeGrid();
	}

	void Grid::SetScale(UINT scale)
	{
		mScale = scale;
		InitializeGrid();
	}

	void Grid::Initialize()
	{
		SetCurrentDirectory(Utility::ExecutableDirectory().c_str());
		std::wstring exeDir = Utility::ExecutableDirectory();
		std::string rxeDir = Utility::CurrentDirectory();
		UINT shaderFlags = 0;

		std::wstring path = Utility::GetFilePath(L"content\\effects\\BasicEffect.fx");

		//std::vector<char> compiledShader;
		//Utility::LoadBinaryFile(L"Content\\Effects\\BasicEffect.cso", compiledShader);

		/*std::ifstream is("C:\\Users\\Gen\\Documents\\Graphics Programming\\EveryRay Rendering Engine\\source\\Library\\Content\\Effects\\BasicEffect.cso", std::ios::binary);
		
		is.seekg(0, std::ios_base::end);
		std::streampos pos = is.tellg();
		is.seekg(0, std::ios_base::beg);
		char * effectBuffer = new char[pos];
		is.read(effectBuffer, pos);*/

		#if defined (DEBUG) || defined (_DEBUG)
				shaderFlags |= D3DCOMPILE_DEBUG;
				shaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
		#endif

		ID3D10Blob* compiledShader = nullptr;
		ID3D10Blob* errorMessages = nullptr;

		HRESULT hr = D3DCompileFromFile(path.c_str(), nullptr, nullptr, nullptr, "fx_5_0", shaderFlags, 0, &compiledShader, &errorMessages );

		//Add throw exception for D3DCompileFromFile in errorMessages

		hr = D3DX11CreateEffectFromMemory(compiledShader->GetBufferPointer(), compiledShader->GetBufferSize(), NULL, mGame->Direct3DDevice(), &mEffect);
		if (FAILED(hr))
		{
			throw GameException("D3DX11CreateEffectFromMemory() failed.", hr);
		}

		// Look up the technique, pass, and WVP variable from the effect
		mTechnique = mEffect->GetTechniqueByName("main11");
		if (mTechnique == nullptr)
		{
			throw GameException("ID3DX11Effect::GetTechniqueByName() could not find the specified technique.", hr);
		}

		mPass = mTechnique->GetPassByName("p0");
		if (mPass == nullptr)
		{
			throw GameException("ID3DX11EffectTechnique::GetPassByName() could not find the specified pass.", hr);
		}

		ID3DX11EffectVariable* variable = mEffect->GetVariableByName("WorldViewProjection");
		if (variable == nullptr)
		{
			throw GameException("ID3DX11Effect::GetVariableByName() could not find the specified variable.", hr);
		}

		mWvpVariable = variable->AsMatrix();
		if (mWvpVariable->IsValid() == false)
		{
			throw GameException("Invalid effect variable cast.");
		}

		// Create the input layout
		D3DX11_PASS_DESC passDesc;
		mPass->GetDesc(&passDesc);

		D3D11_INPUT_ELEMENT_DESC inputElementDescriptions[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};

		if (FAILED(hr = mGame->Direct3DDevice()->CreateInputLayout(inputElementDescriptions, ARRAYSIZE(inputElementDescriptions), passDesc.pIAInputSignature, passDesc.IAInputSignatureSize, &mInputLayout)))
		{
			throw GameException("ID3D11Device::CreateInputLayout() failed.", hr);
		}

		InitializeGrid();
	}

	void Grid::Draw(const GameTime& gameTime)
	{
		ID3D11DeviceContext* direct3DDeviceContext = mGame->Direct3DDeviceContext();
		direct3DDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		direct3DDeviceContext->IASetInputLayout(mInputLayout);

		UINT stride = sizeof(VertexPositionColor);
		UINT offset = 0;
		direct3DDeviceContext->IASetVertexBuffers(0, 1, &mVertexBuffer, &stride, &offset);

		XMMATRIX world = XMLoadFloat4x4(&mWorldMatrix);
		XMMATRIX wvp = world * mCamera->ViewMatrix() * mCamera->ProjectionMatrix();
		mWvpVariable->SetMatrix(reinterpret_cast<const float*>(&wvp));

		mPass->Apply(0, direct3DDeviceContext);

		direct3DDeviceContext->Draw((mSize + 1) * 4, 0);
	}

	void Grid::InitializeGrid()
	{
		ReleaseObject(mVertexBuffer);

		ID3D11Device* direct3DDevice = GetGame()->Direct3DDevice();
		int length = 4 * (mSize + 1);
		int size = sizeof(VertexPositionColor) * length;
		std::unique_ptr<VertexPositionColor> vertexData(new VertexPositionColor[length]);
		VertexPositionColor* vertices = vertexData.get();

		float adjustedScale = mScale * 0.1f;
		float maxPosition = mSize * adjustedScale / 2;

		for (unsigned int i = 0, j = 0; i < mSize + 1; i++, j = 4 * i)
		{
			float position = maxPosition - (i * adjustedScale);

			// Vertical line
			vertices[j] = VertexPositionColor(XMFLOAT4(position, 0.0f, maxPosition, 1.0f), mColor);
			vertices[j + 1] = VertexPositionColor(XMFLOAT4(position, 0.0f, -maxPosition, 1.0f), mColor);

			// Horizontal line
			vertices[j + 2] = VertexPositionColor(XMFLOAT4(maxPosition, 0.0f, position, 1.0f), mColor);
			vertices[j + 3] = VertexPositionColor(XMFLOAT4(-maxPosition, 0.0f, position, 1.0f), mColor);
		}

		D3D11_BUFFER_DESC vertexBufferDesc;
		ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));
		vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		vertexBufferDesc.ByteWidth = size;
		vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA vertexSubResourceData;
		ZeroMemory(&vertexSubResourceData, sizeof(vertexSubResourceData));
		vertexSubResourceData.pSysMem = vertices;

		HRESULT hr;
		if (FAILED(hr = direct3DDevice->CreateBuffer(&vertexBufferDesc, &vertexSubResourceData, &mVertexBuffer)))
		{
			throw GameException("ID3D11Device::CreateBuffer() failed");
		}
	}
}