#include <openvdb/tools/Composite.h>
#include <openvdb/tools/Interpolation.h>
#include <openvdb/tools/PointAdvect.h> // for velocity integrator

#include "fluid_retimer.h"

using namespace openvdb;

class RetimingOp {
	typedef tools::VelocityIntegrator<VectorGrid> VelIntegrator;
	typedef FloatTree::LeafNodeType FloatLeaf;
	typedef tools::GridSampler<FloatGrid::ConstAccessor, tools::BoxSampler> SamplerF;

	FloatGrid::Ptr m_grid;
	FloatGrid::Accessor m_accessor;
	FloatGrid::ConstAccessor m_accessor_t0, m_accessor_t1;
	VelIntegrator m_vel_field_t0, m_vel_field_t1;

	int m_fwd_steps;
	int m_bck_steps;
	float m_time_scale;
	float m_blur_weights[3];
	float m_fwd_dt[3], m_bwd_dt[3];
	float m_scale;

public:
	RetimingOp(ScalarGrid::Ptr work,
	           ScalarGrid::Ptr gridA,
	           ScalarGrid::Ptr gridB,
	           VectorGrid::Ptr velA,
	           VectorGrid::Ptr velB,
	           int steps, float time_scale, float shutter_speed)
		: m_grid(work)
		, m_accessor(work->getAccessor())
		, m_accessor_t0(gridA->getConstAccessor())
		, m_accessor_t1(gridB->getConstAccessor())
		, m_vel_field_t0(*velA)
		, m_vel_field_t1(*velB)
		, m_fwd_steps(steps)
		, m_bck_steps(MAX_STEPS - steps)
		, m_time_scale(time_scale)
		, m_scale(1.0f / 3.0f)
	{
		/* To counter noise produced by the advection steps we simulate a motion
		 * blur type of effect. In order to avoid more funky code we hardcode the
		 * number of blurring steps to three and use the following constants as
		 * weights.
		 */
		m_blur_weights[0] = time_scale - 0.5f * shutter_speed;
		m_blur_weights[1] = time_scale;
		m_blur_weights[2] = time_scale + 0.5f * shutter_speed;

		/* Precompute forward and backward steps */
		for (int i = 0; i < 3; ++i) {
			m_fwd_dt[i] = (1.0f / m_fwd_steps) * m_blur_weights[i];
			m_bwd_dt[i] = (1.0f / m_bck_steps) * (1.0f - m_blur_weights[i]);
		}
	}

	RetimingOp(RetimingOp &other, tbb::split)
		: m_grid(other.m_grid->deepCopy())
		, m_accessor(m_grid->getAccessor())
		, m_accessor_t0(other.m_accessor_t0)
		, m_accessor_t1(other.m_accessor_t1)
		, m_vel_field_t0(other.m_vel_field_t0)
		, m_vel_field_t1(other.m_vel_field_t1)
		, m_fwd_steps(other.m_fwd_steps)
		, m_bck_steps(other.m_bck_steps)
		, m_time_scale(other.m_time_scale)
		, m_scale(other.m_scale)
	{
		for (int i = 0; i < 3; ++i) {
			m_blur_weights[i] = other.m_blur_weights[i];
			m_fwd_dt[i] = other.m_fwd_dt[i];
			m_bwd_dt[i] = other.m_bwd_dt[i];
		}
	}

	~RetimingOp()
	{}

	typedef tree::IteratorRange<FloatTree::LeafCIter> IterRange;

	void operator()(IterRange &range)
	{
		SamplerF sampler_t0(m_accessor_t0, m_grid->transform());
		SamplerF sampler_t1(m_accessor_t1, m_grid->transform());

		for (; range; ++range) {
			for (FloatLeaf::ValueOnCIter iter(range.iterator()->cbeginValueOn()); iter; ++iter) {
				float val_t0 = 0.0f, val_t1 = 0.0f;
				const math::Coord coord = iter.getCoord();

				for (int i = 0; i < 3; ++i) {
					/* forward advection from t to t+n */
					math::Vec3i coord_t0 = coord.asVec3i();

					for (int j = 0; j < m_fwd_steps; ++j) {
						m_vel_field_t0.rungeKutta<1>(m_fwd_dt[i], coord_t0);
					}
					val_t0 += sampler_t0.isSample(coord_t0);

					/* backward advection from t+1 to t+n */
					math::Vec3i coord_t1 = coord.asVec3i();

					for (int j = 0; j < m_bck_steps; ++j) {
						m_vel_field_t1.rungeKutta<1>(-m_bwd_dt[i], coord_t1);
					}
					val_t1 += sampler_t1.isSample(coord_t1);
				}

				val_t0 = val_t0 * m_scale;
				val_t1 = val_t1 * m_scale;

				m_accessor.setValue(coord, lerp(val_t0, val_t1, m_time_scale));
			}
		}
	}

	void join(RetimingOp &other)
	{
		tools::compSum(*m_grid, *other.m_grid);
	}
};

FluidRetimer::FluidRetimer()
{
	m_num_steps = 0;
	m_time_scale = 1.0f;
	m_shutter_speed = 0.0f;
	m_threaded = true;
}

FluidRetimer::FluidRetimer(int steps, float time_scale, float shutter_speed)
	: m_num_steps(steps)
	, m_time_scale(time_scale)
	, m_shutter_speed(shutter_speed)
	, m_threaded(true)
{}

FluidRetimer::~FluidRetimer()
{
	m_grids.clear();
	m_ignored_grids.clear();
}

void FluidRetimer::setTimeScale(const float scale)
{
	m_time_scale = scale;
}

void FluidRetimer::setThreaded(const bool threaded)
{
	m_threaded = threaded;
}

void FluidRetimer::addIgnoredGrid(const std::string &name)
{
	m_ignored_grids.push_back(name);
}

/**
 * This function loads the corresponding grids from the current and next frame.
 * The actual retiming is done in readvectSL().
 *
 * @param previous the file containing the grids for the previous frame
 * @param cur the file containing the grids for the current frame
 * @param to the file where to put the retimed grids
 */
void FluidRetimer::retime(const std::string &previous, const std::string &cur, const std::string &to)
{
	io::File file_p(previous), file_c(cur);
	file_p.open();
	file_c.open();

	FloatGrid::Ptr grid_t0, grid_t1, retimed;
	m_vel_t0 = gridPtrCast<Vec3SGrid>(file_p.readGrid("Velocity"));
	m_vel_t1 = gridPtrCast<Vec3SGrid>(file_c.readGrid("Velocity"));

	io::File::NameIterator grid_names = file_p.beginName();

	for (; grid_names != file_p.endName(); ++grid_names) {
		const std::string name = *grid_names;

		bool ignore_grid = std::find(m_ignored_grids.begin(),
		                             m_ignored_grids.end(),
		                             name) != m_ignored_grids.end();

		if (ignore_grid) {
			m_grids.push_back(file_p.readGrid(name));
			continue;
		}

		GridBase::Ptr grid_t0_tmp = file_p.readGrid(name);
		GridBase::Ptr grid_t1_tmp = file_c.readGrid(name);

		grid_t0 = gridPtrCast<FloatGrid>(grid_t0_tmp);
		grid_t1 = gridPtrCast<FloatGrid>(grid_t1_tmp);

		retimed = FloatGrid::create(grid_t1->background());
		retimed->setGridClass(grid_t1->getGridClass());
		retimed->setIsInWorldSpace(grid_t1->isInWorldSpace());
		retimed->setName(name);
		retimed->setSaveFloatAsHalf(grid_t1->saveFloatAsHalf());
		retimed->setTransform(grid_t1->transform().copy());

		readvectSL(retimed, grid_t0, grid_t1);

		m_grids.push_back(retimed);
	}

	io::File file_to(to);
	file_to.write(m_grids);
	file_to.close();
	file_p.close();
	file_c.close();
	m_grids.clear();
}

/**
 * This does the actual retiming process. We basically do a forward and backward
 * readvection of the fluid field.
 *
 * @param work the retimed grid
 * @param gridA the grid for the current frame
 * @param gridB the grid for the next frame
 */
void FluidRetimer::readvectSL(ScalarGrid::Ptr work,
                              ScalarGrid::Ptr gridA,
                              ScalarGrid::Ptr gridB)
{
	RetimingOp op(
			work, gridA, gridB, m_vel_t0, m_vel_t1,
			m_num_steps, m_time_scale, m_shutter_speed);

	RetimingOp::IterRange range(gridA->tree().cbeginLeaf());

	if (m_threaded) {
		tbb::parallel_reduce(range, op);
	}
	else {
		op(range);
	}
}
