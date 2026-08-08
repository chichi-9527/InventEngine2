#pragma once

#include "IRenderThreadBase.h"

#include <thread>
#include <atomic>
#include <memory>

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
		bool _init_vulkan();

	private:
		CreateSurfaceFunc _create_surface = nullptr;

		std::atomic_bool _running{ false };
	};
}
