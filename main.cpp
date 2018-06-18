#include <iostream>

#include "src/Scene.hpp"
#include "src/model/oil/OilMethod.hpp"
#include "src/model/stoch_oil/StochOilMethod.hpp"

using namespace std;

namespace issues
{
	template <class modelType, class methodType>
	struct Issue
	{
		typename typedef modelType Model;
		typename typedef methodType Method;
	};

	struct Oil : public Issue<oil::Oil, oil::OilMethod> {};
	struct StochOil : public Issue<stoch_oil::StochOil, stoch_oil::StochOilMethod> {};
}

int main()
{
	stoch_oil::Properties props;
	
	props.t_dim = 3600.0;
	props.ht = props.ht_min = 1000.0;
	props.ht_max = 1000000.0;

	props.hx = props.hy = props.R_dim = 100.0;		props.hz = 1.0;
	props.num_x = props.num_y = 100;
	size_t num = (props.num_x + 2) * (props.num_y + 2);

	props.props_sk.p_init = props.props_sk.p_out = 100.0 * BAR_TO_PA;
	props.props_sk.perm = 100.0;
	props.props_sk.m = 0.1;
	props.props_sk.beta = 4.E-10;

	props.props_oil.visc = 1.0;
	props.props_oil.rho_stc = 887.261;
	props.props_oil.beta = 1.0 * 1.e-9;
	props.props_oil.p_ref = props.props_sk.p_init;

	props.wells.push_back(Well((props.num_y + 2) * int((props.num_x + 2) / 2) + (props.num_y + 2) / 2));
	auto& well = props.wells.back();
	well.periodsNum = 1;
	well.period.resize(well.periodsNum);
	well.period[0] = 100.0 * 3600.0;
	well.rate.resize(well.periodsNum);
	well.rate[0] = 50.0;
	well.leftBoundIsRate.resize(well.periodsNum);
	well.leftBoundIsRate[0] = true;
	well.rw = 0.1;

	Scene<issues::StochOil> scene;
	scene.load(props);
	scene.start();

	return 0;
}