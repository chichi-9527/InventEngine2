#include "RegisterTest.h"

#if 1

namespace INVENT
{
	IRegister::~IRegister() = default;
	IRegister::IRegister() = default;

	void IRegister::StaticTestF()
	{}

	void IRegister::TakeDamage(float amount)
	{}

	int IRegister::Add(int a, int b)
	{
		return a + b;
	}

	float IRegister::Add(float a, float b)
	{
		return a + b;
	}

	void IRegister::Test11() const
	{}



}

#endif
