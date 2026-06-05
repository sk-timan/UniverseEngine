#pragma once

struct FMath
{
	static constexpr float Pi = 3.14159265358979323846f;
	static constexpr float TwoPi = Pi * 2.0f;
	static constexpr float HalfPi = Pi * 0.5f;
	static constexpr float RadToDeg = 180.0f / Pi;
	static constexpr float DegToRad = Pi / 180.0f;

	static constexpr float DegreesToRadians(float InDegrees)
	{
		return InDegrees * DegToRad;
	}

	static constexpr float RadiansToDegrees(float InRadians)
	{
		return InRadians * RadToDeg;
	}
};
