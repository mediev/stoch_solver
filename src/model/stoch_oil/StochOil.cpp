#include "src/model/stoch_oil/StochOil.hpp"

#include <valarray>

#include <assert.h>

using namespace stoch_oil;
using std::valarray;

StochOil::StochOil()
{
}
StochOil::~StochOil()
{
}
void StochOil::setProps(const Properties& props)
{
	R_dim = props.R_dim;
	t_dim = props.t_dim;
	Q_dim = R_dim * R_dim * R_dim / t_dim;
	mesh = std::make_shared<Mesh>(*new Mesh(props.num_x, props.num_y, props.hx / R_dim, props.hy / R_dim, props.hz / R_dim));

	cellsNum = mesh.get()->num;
	p0_prev.resize(var_size * cellsNum);	
	p0_iter.resize(var_size * cellsNum);	
	p0_next.resize(var_size * cellsNum);

	Cfp_prev.resize(var_size * cellsNum * cellsNum);	
	Cfp_iter.resize(var_size * cellsNum * cellsNum);
	Cfp_next.resize(var_size * cellsNum * cellsNum);

	p2_prev.resize(var_size * cellsNum);	
	p2_iter.resize(var_size * cellsNum);	
	p2_next.resize(var_size * cellsNum);

	Cp_prev.resize(var_size * cellsNum * cellsNum);	
	Cp_iter.resize(var_size * cellsNum * cellsNum);	
	Cp_next.resize(var_size * cellsNum * cellsNum);

	x_p0 = new adouble[cellsNum];		
	x_Cfp = new adouble[cellsNum * cellsNum];
	x_p2 = new adouble[cellsNum];
	x_Cp = new adouble[cellsNum * cellsNum];

	h_p0 = new adouble[var_size * cellsNum]; 
	h_Cfp = new adouble[var_size * cellsNum * cellsNum];
	h_p2 = new adouble[var_size * cellsNum];	
	h_Cp = new adouble[var_size * cellsNum * cellsNum];
	Volume = mesh.get()->V;

	ht = props.ht;
	ht_min = props.ht_min;
	ht_max = props.ht_max;

	props_sk = props.props_sk;
	props_sk.perm = MilliDarcyToM2(props_sk.perm);

	props_oil = props.props_oil;
	props_oil.visc = cPToPaSec(props_oil.visc);

	wells = props.wells;
	for (int i = 0; i < wells.back().periodsNum; i++)
	{
		auto& well = wells[i];
		if (well.leftBoundIsRate[i])
			well.rate[i] /= 86400.0;
	}

	makeDimLess();
}
void StochOil::makeDimLess()
{
	P_dim = props_sk.p_init;

	ht /= t_dim;
	ht_min /= t_dim;
	ht_max /= t_dim;

	props_sk.p_init /= P_dim;
	props_sk.p_out /= P_dim;
	props_sk.perm /= R_dim * R_dim;
	props_sk.beta /= (1.0 / P_dim);

	for (auto& well : wells)
	{
		well.rw /= R_dim;
		well.period /= t_dim;
		well.pwf /= P_dim;
		well.rate /= Q_dim;
	}

	props_oil.visc /= P_dim * t_dim;
	props_oil.p_ref /= P_dim;
	props_oil.beta /= (1.0 / P_dim);
	props_oil.rho_stc /= (P_dim * t_dim * t_dim / R_dim / R_dim);
}
void StochOil::setInitialState()
{
	for (size_t i = 0; i < cellsNum; i++)
	{
		const auto& cell = mesh->cells[i];
		p0_prev[i] = p0_iter[i] = p0_iter[i] = props_sk.p_init;
		p2_prev[i] = p2_iter[i] = p2_iter[i] = 0.0;

		const auto sl = std::slice(i * cellsNum, cellsNum, var_size);
		for (size_t j = i * cellsNum; j < (i + 1) * cellsNum; j++)
		{
			Cfp_prev[j] = Cfp_iter[j] = Cfp_next[j] = 0.0;
			Cp_prev[j] = Cp_iter[j] = Cp_next[j] = 0.0;
		}
	}

	// WI calculation
	for (auto& well : wells)
	{
		const Cell& cell = mesh->cells[well.cell_id];
		well.r_peaceman = 0.28 * sqrt(cell.hx * cell.hx + cell.hy * cell.hy) / 2.0;
		well.WI = 2.0 * M_PI * props_sk.perm * cell.hz / log(well.r_peaceman / well.rw);
	}
}
void StochOil::setPeriod(const int period)
{
	for (auto& well : wells)
	{
		well.cur_bound = well.leftBoundIsRate[period];
		if (well.cur_bound)
			well.cur_rate = well.rate[period];
		else
			well.cur_pwf = well.pwf[period];
	}
}
double StochOil::getRate(const Well& well) const
{
	if (well.cur_bound)
		return well.cur_rate;
	else
	{
		double p_cell = p0_next[well.cell_id];
		return well.WI * (p_cell - well.cur_pwf) / props_oil.visc;
	}
}
double StochOil::getPwf(const Well& well) const
{
	if (well.cur_bound)
	{
		double p_cell = p0_next[well.cell_id];
		return p_cell - well.cur_rate * props_oil.visc / well.WI;
	}
	else
		return well.cur_pwf;
}

adouble StochOil::solveInner0(const Cell& cell) const
{
	assert(cell.type == elem::QUAD);

	const auto& next = x_p0[cell.id];
	const auto prev = p0_prev[cell.id];
	
	adouble H;
	H = getS(cell) * (next - prev) / ht / getKg(cell);

	const auto& beta_y_minus = mesh->cells[cell.stencil[1]];
	const auto& beta_y_plus = mesh->cells[cell.stencil[2]];
	const auto& beta_x_minus = mesh->cells[cell.stencil[3]];
	const auto& beta_x_plus = mesh->cells[cell.stencil[4]];

	const auto& nebr_y_minus = x_p0[cell.stencil[1]];
	const auto& nebr_y_plus = x_p0[cell.stencil[2]];
	const auto& nebr_x_minus = x_p0[cell.stencil[3]];
	const auto& nebr_x_plus = x_p0[cell.stencil[4]];

	H -= ((nebr_x_plus - next) / (beta_x_plus.cent.x - cell.cent.x) - 
			(next - nebr_x_minus) / (cell.cent.x - beta_x_minus.cent.x)) / cell.hx;

	H -= (getFavg(beta_x_plus) - getFavg(beta_x_minus)) / (beta_x_plus.cent.x - beta_x_minus.cent.x) *
			(nebr_x_plus - nebr_x_minus) / (beta_x_plus.cent.x - beta_x_minus.cent.x);

	H -= ((nebr_y_plus - next) / (beta_y_plus.cent.y - cell.cent.y) -
		(next - nebr_y_minus) / (cell.cent.y - beta_y_minus.cent.y)) / cell.hy;

	H -= (getFavg(beta_y_plus) - getFavg(beta_y_minus)) / (beta_y_plus.cent.y - beta_y_minus.cent.y) *
		(nebr_y_plus - nebr_y_minus) / (beta_y_plus.cent.y - beta_y_minus.cent.y);

	return H;
}
adouble StochOil::solveBorder0(const Cell& cell) const
{
	assert(cell.type == elem::BORDER);

	const auto& cur = x_p0[cell.id];
	const auto& nebr = x_p0[cell.stencil[1]];
	return (cur - (adouble)(props_sk.p_out)) / P_dim;
}
adouble StochOil::solveSource0(const Well& well) const
{
	const Cell& cell = mesh->cells[well.cell_id];
	return well.cur_rate * props_oil.rho_stc / mesh->hz / getKg(cell);
}

adouble StochOil::solveInner1(const Cell& cell) const
{
	/*assert(cell.type == elem::QUAD);
	const auto& next = x2[cell.id];
	const auto prev = (*this)[cell.id].u_prev2;

	TapeVariable1 H;
	H.p2 = getS(cell) * (next.p2 - prev.p2) / ht / getKg(cell);

	const auto& beta_y_minus = mesh->cells[cell.stencil[1]];
	const auto& beta_y_plus = mesh->cells[cell.stencil[2]];
	const auto& beta_x_minus = mesh->cells[cell.stencil[3]];
	const auto& beta_x_plus = mesh->cells[cell.stencil[4]];

	const auto& nebr_y_minus = x0[cell.stencil[1]];
	const auto& nebr_y_plus = x0[cell.stencil[2]];
	const auto& nebr_x_minus = x0[cell.stencil[3]];
	const auto& nebr_x_plus = x0[cell.stencil[4]];*/

	adouble H;
	return H;
}
adouble StochOil::solveBorder1(const Cell& cell) const
{
	/*assert(cell.type == elem::BORDER);
	TapeVariable1 H;
	const auto& cur = x1[cell.id];
	H.p2 = cur.p2;
	H.Cfp = cur.Cfp;*/
	adouble H;
	return H;
}
adouble StochOil::solveSource1(const Well& cell) const
{
	adouble H;
	return H;
}
