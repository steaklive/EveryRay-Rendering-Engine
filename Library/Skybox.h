#pragma once
#include "Common.h"
#include "DrawableGameComponent.h"

namespace Library
{
	class Effect;
	class SkyboxMaterial;

	class Skybox : public DrawableGameComponent
	{
		RTTI_DECLARATIONS(Skybox, DrawableGameComponent)

	public:
		Skybox(Game& game, Camera& camera, const std::wstring& cubeMapFileName, float scale);
		~Skybox();

		virtual void Initialize() override;
		virtual void Update(const GameTime& gameTime) override;
		virtual void Draw(const GameTime& gameTime) override;

	private:
		Skybox();
		Skybox(const Skybox& rhs);
		Skybox& operator=(const Skybox& rhs);

		std::wstring mCubeMapFileName;
		Effect* mEffect;
		SkyboxMaterial* mMaterial;
		ID3D11ShaderResourceView* mCubeMapShaderResourceView;
		ID3D11Buffer* mVertexBuffer;
		ID3D11Buffer* mIndexBuffer;
		UINT mIndexCount;

		XMFLOAT4X4 mWorldMatrix;
		XMFLOAT4X4 mScaleMatrix;
	};
}