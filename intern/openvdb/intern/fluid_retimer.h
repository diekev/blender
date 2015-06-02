#ifndef __FLUID_RETIMER_H__
#define __FLUID_RETIMER_H__

#include <openvdb/openvdb.h>

#define MAX_STEPS 10

template<typename T>
T lerp(T a, T b, T dt)
{
	return a * (1.0f - dt) + b * dt;
}

class FluidRetimer {
	int m_num_steps;
	float m_time_scale;
	float m_shutter_speed;
	bool m_threaded;
	openvdb::Vec3SGrid::Ptr m_vel_t0, m_vel_t1;
	openvdb::GridPtrVec m_grids;
	std::vector<std::string> m_ignored_grids;

	void readvectSL(openvdb::FloatGrid::Ptr work,
					openvdb::FloatGrid::Ptr gridA,
					openvdb::FloatGrid::Ptr gridB);

public:
	FluidRetimer();
	FluidRetimer(int steps, float time_scale, float shutter_speed);
	~FluidRetimer();

	void setTimeScale(const float scale);
	void addIgnoredGrid(const std::string &name);
	void retime(const std::string &previous, const std::string &cur, const std::string &to);
	void setThreaded(const bool threaded);
};

#endif /* __FLUID_RETIMER_H__ */
