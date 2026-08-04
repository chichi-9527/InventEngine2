#pragma once

#include "IRenderThreadBase.h"

namespace INVENT
{
	class IRenderThread : public IRenderThreadBase
	{
		friend class IRenderThreadBase;

		IRenderThread() = default;
	public:
		~IRenderThread() override = default;

		void SetCreateSurfaceFunction(CreateSurfaceFunc func) override { _create_surface = func; }

		bool Start() override;
		void Shutdown() override;

	private:
		CreateSurfaceFunc _create_surface = nullptr;
	};
}
