#pragma once

#include "Common.h"

namespace EveryRay_Core
{
	class ER_Core;
	class ER_CoreTime;

	class ER_CoreComponent : public RTTI
	{
		RTTI_DECLARATIONS(ER_CoreComponent, RTTI)

	public:
		ER_CoreComponent();
		ER_CoreComponent(ER_Core& game);

		virtual ~ER_CoreComponent();

		ER_Core* GetCore();
		void SetCore(ER_Core& game);
		bool Enabled() const;
		void SetEnabled(bool enabled);

		virtual void Initialize();
		virtual void Update(const ER_CoreTime& gameTime);
		
	protected:
		ER_Core * mCore;
		bool mEnabled;

	private:
		ER_CoreComponent(const ER_CoreComponent& rhs);
		ER_CoreComponent& operator=(const ER_CoreComponent& rhs);

	};
}
