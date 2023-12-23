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

		void SetSelectedInEditor(bool value) { mIsSelectedInEditor = value; }
		bool IsSelectedInEditor() { return mIsSelectedInEditor; }

		float mRadius;
		float mEditorColor[3] = { 0.0f, 0.0f, 0.0f };
		float mEditorIntensity = 1.0f;
		float mEditorCurrentTransformMatrix[16] =
		{
			1.f, 0.f, 0.f, 0.f,
			0.f, 1.f, 0.f, 0.f,
			0.f, 0.f, 1.f, 0.f,
			0.f, 0.f, 0.f, 1.f
		};
	protected:
		XMFLOAT3 mPosition;
	private:

		bool mIsSelectedInEditor = false;
	};
}