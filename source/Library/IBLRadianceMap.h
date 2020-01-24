#pragma once
#include "Common.h"
#include "DrawableGameComponent.h"
#include "ConstantBuffer.h"

namespace Library {

	class Effect;
	class IBLRadianceMapMaterial;
	class QuadRenderer;
	class IBLCubemap;

	struct IBLConstantBuffer
	{
		float Face;		
		float MipIndex;	
	};

	class IBLRadianceMap: public DrawableGameComponent {

	public:
		IBLRadianceMap(Game& game, const std::wstring& cubemapFilename);
		~IBLRadianceMap();

		void Create(Game& game);
		virtual void Initialize() override;

		ID3D11ShaderResourceView** GetShaderResourceViewAddress();


	private:
		Game* myGame;

		void DrawFace(ID3D11Device* device, ID3D11DeviceContext* deviceContext,	int faceIndex, int mipIndex, ID3D11RenderTargetView* renderTarget);

		std::wstring mCubeMapFileName;

		ID3D11ShaderResourceView* mEnvMapShaderResourceView;

		std::unique_ptr<IBLCubemap> mIBLCubemap;
		std::unique_ptr<QuadRenderer> mQuadRenderer;
		ConstantBuffer<IBLConstantBuffer> mConstantBuffer;

	};
}
