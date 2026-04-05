#include <LI.h>
#include <LI/Core/EntryPoint.h>

#include "GameLayer.h"

class ColorfulBlock : public LI::Application
{
public:
	ColorfulBlock()
	{
		PushLayer(std::make_unique<GameLayer>());
	}
};

LI::Application* LI::CreateApplication()
{
	return new ColorfulBlock();
}
