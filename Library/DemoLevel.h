// Simple class for level management
// Works for now...

#pragma once

namespace Library
{
	class DemoLevel
	{
	public:
		virtual void Destroy();
		virtual void Create();
		virtual bool IsComponent() = 0;
		

	};

}
