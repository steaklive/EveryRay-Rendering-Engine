#pragma once

#include "Common.h"
#include "ER_Light.h"

namespace EveryRay_Core
{
	class ER_PointLight : public ER_Light
	{
		RTTI_DECLARATIONS(ER_PointLight, ER_Light)
	public:
		ER_PointLight(ER_Core& game);
		ER_PointLight(ER_Core& game, const XMFLOAT3& pos, float radius);
		virtual ~ER_PointLight();

		void Update(const ER_CoreTime& time);

		void SetPosition(FLOAT x, FLOAT y, FLOAT z);
		void SetPosition(const XMFLOAT3& position);
		const XMFLOAT3& GetPosition() const { return mPosition; }
		XMVECTOR PositionVector() const;

		void SetRadius(float value);
		float GetRadius() const { return mRadius; }

		void SetSelectedInEditor(bool value) { mIsSelectedInEditor = value; }
		bool IsSelectedInEditor() { return mIsSelectedInEditor; }
	protected:
		XMFLOAT3 mPosition;
	private:
		float mRadius;

		float mEditorColor[3] = { 0.0f, 0.0f, 0.0f };
		float mEditorIntensity = 1.0f;
		float mEditorMatrixTranslation[3];
		float mEditorMatrixRotation[3];
		float mEditorMatrixScale[3];
		float mEditorCurrentTransformMatrix[16] =
		{
			1.f, 0.f, 0.f, 0.f,
			0.f, 1.f, 0.f, 0.f,
			0.f, 0.f, 1.f, 0.f,
			0.f, 0.f, 0.f, 1.f
		};
		bool mIsSelectedInEditor = false;
	};
}