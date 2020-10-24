// Simple class for level management
// Works for now...
#include "GameTime.h"
#pragma once

namespace Library
{
	class DemoLevel
	{
	public:
		virtual void Destroy();
		virtual void Create();
		virtual void UpdateLevel(const GameTime& time);
		virtual void DrawLevel(const GameTime& time);
		virtual bool IsComponent() = 0;
	};

}
