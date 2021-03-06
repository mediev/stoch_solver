#include "src/model/stoch_oil/StochOil.hpp"

#include <valarray>

#include <assert.h>
#include <boost/math/special_functions/expint.hpp>
#include <boost/math/constants/constants.hpp>

using namespace stoch_oil;
using std::valarray;
using boost::math::expint;

StochOil::StochOil()
{
    inv_cond_cov = NULL;
}
StochOil::~StochOil()
{
	delete[] x, h;
}
void StochOil::loadPermAvg(const std::string fileName)
{
    std::ifstream file(fileName.c_str(), std::ifstream::in);
    std::string buf;
    std::string::size_type sz;
    while(buf != "PERMX")
        file >> buf;
    file >> buf;
    while (buf != "/")
    {
        props_sk.perm_grd.push_back(MilliDarcyToM2(std::stod(buf, &sz)));
        file >> buf;
    }
    file.close();
}
void StochOil::writeCPS(const int i)
{
    const double minX = R_dim * mesh->hx / mesh->num_x / 2.0;
    const double minY = R_dim * mesh->hy / mesh->num_y / 2.0;
    const double maxX = R_dim * mesh->hx - minX;
    const double maxY = R_dim * mesh->hy - minY;

    auto head = [&](std::ofstream& file)
    {
        file << "FSASCI 0 1 \"COMPUTED \" 0 " << -999.0 << "\n";
        file << "FSATTR 0 0\n";
        file << "!Grid generated by ResViewII - 3D.6.9.137\n";
        file << "FSLIMI " << minX << " " << maxX << " " << minY << " " << maxY << " 0.0 " << R_dim * mesh->cells[0].hz << "\n";
        file << "FSNROW " << mesh->num_x << " " << mesh->num_y << "\n";
        file << "FSXINC " + std::to_string(R_dim * mesh->hx / mesh->num_x) + " " + std::to_string(R_dim * mesh->hy / mesh->num_y) + "\n";
    };
    auto writePres = [&](const int i)
    {
        const std::string filename = "snaps/Pres_" + std::to_string(i) + ".cps";
        std::ofstream file(filename.c_str(), std::ofstream::out);
        head(file);

        int counter = 0;
        for (const auto& cell : mesh->cells)
        {
            if (cell.type == elem::QUAD)
            {
                //const int y_id = cell.id % (mesh->num_y + 2);
                //const int x_id = cell.id / (mesh->num_y + 2);
                file << "\t" << P_dim * (p0_next[cell.id] + p2_next[cell.id]) / BAR_TO_PA;
                counter++;

                if (counter % 5 == 0)
                    file << "\n";
            }
        }
        file.close();
    };
    auto writePresStd = [&](const int i)
    {
        const std::string filename = "snaps/Pres_std_" + std::to_string(i) + ".cps";
        std::ofstream file(filename.c_str(), std::ofstream::out);
        head(file);

        int counter = 0;
        for (const auto& cell : mesh->cells)
        {
            if (cell.type == elem::QUAD)
            {
                //const int y_id = cell.id % (mesh->num_y + 2);
                //const int x_id = cell.id / (mesh->num_y + 2);
                file << "\t" << P_dim * sqrt(fmax(Cp_next[i][cell.id * cellsNum + cell.id], 0.0)) / BAR_TO_PA;
                counter++;

                if (counter % 5 == 0)
                    file << "\n";
            }
        }
        file.close();
    };
    auto writePerm = [&]()
    {
        const std::string filename = "snaps/Perm.cps";
        std::ofstream file(filename.c_str(), std::ofstream::out);
        head(file);

        int counter = 0;
        for (const auto& cell : mesh->cells)
        {
            if (cell.type == elem::QUAD)
            {
                //const int y_id = cell.id % (mesh->num_y + 2);
                //const int x_id = cell.id / (mesh->num_y + 2);
                file << "\t" << R_dim * R_dim * M2toMilliDarcy(getPerm(cell));
                counter++;

                if (counter % 5 == 0)
                    file << "\n";
            }
        }
        file.close();
    };
    auto writePermStd = [&]()
    {
        const std::string filename = "snaps/Perm_std.cps";
        std::ofstream file(filename.c_str(), std::ofstream::out);
        head(file);

        double buf1, buf2;
        int counter = 0;
        for (const auto& cell : mesh->cells)
        {
            if (cell.type == elem::QUAD)
            {
                //const int y_id = cell.id % (mesh->num_y + 2);
                //const int x_id = cell.id / (mesh->num_y + 2);
                buf1 = getPerm(cell);
                buf2 = (exp(getSigma2f(cell)) - 1.0) * buf1 * buf1;
                file << "\t" << R_dim * R_dim * M2toMilliDarcy(sqrt(fmax(buf2, 0.0)));
                counter++;

                if (counter % 5 == 0)
                    file << "\n";
            }
        }
        file.close();
    };
    auto writeGeomPerm = [&]()
    {
        const std::string filename = "snaps/Perm_geom.cps";
        std::ofstream file(filename.c_str(), std::ofstream::out);
        head(file);

        double buf1, buf2;
        int counter = 0;
        for (const auto& cell : mesh->cells)
        {
            if (cell.type == elem::QUAD)
            {
                //const int y_id = cell.id % (mesh->num_y + 2);
                //const int x_id = cell.id / (mesh->num_y + 2);
                file << "\t" << R_dim * R_dim * M2toMilliDarcy(getGeomPerm(cell));
                counter++;

                if (counter % 5 == 0)
                    file << "\n";
            }
        }
        file.close();
    };

    writePres(i);
    writePresStd(i);
    if (i == 0)
    {
        writePerm();
        writePermStd();
        writeGeomPerm();
    }
}
void StochOil::setProps(const Properties& props)
{
	R_dim = props.R_dim;
	t_dim = props.t_dim;
	Q_dim = R_dim * R_dim * R_dim / t_dim;

	possible_steps_num = props.possible_steps_num;
	start_time_simple_approx = props.start_time_simple_approx;
	ht = props.ht;
	ht_min = props.ht_min;
	ht_max = props.ht_max;

	props_sk = props.props_sk;
	props_sk.perm = MilliDarcyToM2(props_sk.perm);
    //loadPermAvg("props/perm41");

	props_oil = props.props_oil;
	props_oil.visc = cPToPaSec(props_oil.visc);

    conditions = props.conditions;
    for(auto& cond : conditions)
        cond.perm = MilliDarcyToM2(cond.perm);

	wells = props.wells;
	for (auto& well : wells)
		for (auto& rate : well.rate)
			rate /= 86400.0;

	mesh = std::make_shared<Mesh>(*new Mesh(props.num_x, props.num_y, props.hx / R_dim, props.hy / R_dim, props.hz / R_dim));
	Volume = mesh.get()->V;
	cellsNum = mesh.get()->num;

	p0_prev.resize(cellsNum);	
	p0_iter.resize(cellsNum);	
	p0_next.resize(cellsNum);

	Cfp.resize(possible_steps_num);
	for (auto& cfp : Cfp)
		cfp.resize(cellsNum * cellsNum, 0.0);
	Cfp_prev = &Cfp[0][0];	Cfp_next = &Cfp[1][0];

	p2_prev.resize(cellsNum);	
	p2_iter.resize(cellsNum);	
	p2_next.resize(cellsNum);

	Cp_prev.resize(possible_steps_num);	
	Cp_next.resize(possible_steps_num);
	for (size_t i = 0; i < possible_steps_num; i++)
	{
		Cp_prev[i].resize(cellsNum * cellsNum, 0.0);
		Cp_next[i].resize(cellsNum * cellsNum, 0.0);
	}

	x = new adouble[cellsNum];		
	h = new adouble[cellsNum]; 

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
	props_sk.l_f /= R_dim;
    for (auto& perm : props_sk.perm_grd)
        perm /= R_dim * R_dim;

    for (auto& cond : conditions)
        cond.perm /= R_dim * R_dim;

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
		p0_prev[i] = p0_iter[i] = p0_next[i] = props_sk.p_init;
		p2_prev[i] = p2_iter[i] = p2_next[i] = 0.0;
	}

	for (size_t i = 0; i < possible_steps_num; i++)
		Cfp[i] = Cp_prev[i] = Cp_next[i] = 0.0;

    Favg.resize(cellsNum, 0.0);
    Cf.resize(cellsNum);
    std::for_each(Cf.begin(), Cf.end(), [&](std::vector<double>& vec) { vec.resize(cellsNum, 0.0); });
    for (int i = 0; i < cellsNum; i++)
    {
        const Cell& cell1 = mesh->cells[i];
        Favg[i] = getFavg_prior(cell1);
        for (int j = 0; j < cellsNum; j++)
            Cf[i][j] = getCf_prior(cell1, mesh->cells[j]);
    }
    // Conditioning
    calculateConditioning();

    // WI calculation
    for (auto& well : wells)
    {
        const Cell& cell = mesh->cells[well.cell_id];
        well.r_peaceman = 0.28 * sqrt(cell.hx * cell.hx + cell.hy * cell.hy) / 2.0;
        well.WI = 2.0 * M_PI * well.perm * cell.hz / log(well.r_peaceman / well.rw);
    }
}
void StochOil::setPeriod(const int period)
{
	for (auto& well : wells)
	{
		well.cur_period = period;
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
        if(well.isCond)
            return well.WI * (p_cell - well.cur_pwf) / props_oil.visc;
        else
        {
            const auto& cell = mesh->cells[well.cell_id];
            return well.WI / well.perm * getKg(cell) * (exp(getSigma2f(cell) / 2.0) * (p_cell - well.cur_pwf) + Cfp_next[well.cell_id * cellsNum + well.cell_id]);
        }
	}
}
double StochOil::getRateVar(const Well& well, const int step_idx) const
{
    if (well.cur_bound)
        return 0.0;
    else
    {
        const Cell& cell = mesh->cells[well.cell_id];
        double Cp0 = Cp_next[step_idx][well.cell_id * cellsNum + well.cell_id];
        double tmp = well.WI / well.perm * getKg(cell);
        if (well.isCond)
            return tmp * tmp * Cp0;
        else
        {
            double dp = p0_next[well.cell_id] - well.cur_pwf;
            double Cyp0 = Cfp[step_idx][well.cell_id * cellsNum + well.cell_id];
            double buf = exp(getSigma2f(cell));
            return tmp * tmp * (Cp0 + 2.0 * dp * Cyp0 + dp * dp * buf * (buf - 1.0));
        }
    }
}
double StochOil::getPwf(const Well& well) const
{
	if (well.cur_bound)
	{
        const Cell& cell = mesh->cells[well.cell_id];
		double p_cell = p0_next[well.cell_id];
        if(well.isCond)
            return p_cell + well.cur_rate * props_oil.visc / well.WI;
        else
            return p_cell + well.cur_rate * well.perm / well.WI / getKg(cell) * exp(getSigma2f(cell) / 2.0);
	}
	else
		return well.cur_pwf;
}
double StochOil::getPwfVar(const Well& well, const int step_idx) const
{
    if (well.cur_bound)
    {
        const Cell& cell = mesh->cells[well.cell_id];
        double Cp0 = Cp_next[step_idx][well.cell_id * cellsNum + well.cell_id];
        if (well.isCond)
            return Cp0;
        else
        {
            double tmp = well.cur_rate * well.perm / well.WI / getKg(cell);
            double Cyp0 = Cfp[step_idx][well.cell_id * cellsNum + well.cell_id];
            return Cp0 - 2.0 * tmp * Cyp0 + tmp * tmp * getSigma2f(cell);
        }
    }
    else
        return 0.0;
}

adouble StochOil::solveInner_p0(const Cell& cell) const
{
	assert(cell.type == elem::QUAD);
	const auto& next = x[cell.id];
	const auto prev = p0_prev[cell.id];
    adouble H, var_plus, var_minus;
	H = getS(cell) * (next - prev) / getKg(cell);

	const auto& beta_y_minus = mesh->cells[cell.stencil[1]];
	const auto& beta_y_plus = mesh->cells[cell.stencil[2]];
	const auto& beta_x_minus = mesh->cells[cell.stencil[3]];
	const auto& beta_x_plus = mesh->cells[cell.stencil[4]];

	const auto& nebr_y_minus = x[cell.stencil[1]];
	const auto& nebr_y_plus = x[cell.stencil[2]];
	const auto& nebr_x_minus = x[cell.stencil[3]];
	const auto& nebr_x_plus = x[cell.stencil[4]];

	H -= ht * ((nebr_x_plus - next) / (beta_x_plus.cent.x - cell.cent.x) -
			(next - nebr_x_minus) / (cell.cent.x - beta_x_minus.cent.x)) / cell.hx;

    //var_plus = linearInterp1d(getSigma2f(beta_x_plus), beta_x_plus.hx / 2.0, getSigma2f(cell), cell.hx / 2.0);
    //var_minus = linearInterp1d(getSigma2f(beta_x_minus), beta_x_minus.hx / 2.0, getSigma2f(cell), cell.hx / 2.0);
	H -= ht * (log(cell.trans[3]) - log(cell.trans[2])) / cell.hx *
			(nebr_x_plus - nebr_x_minus) / (beta_x_plus.cent.x - beta_x_minus.cent.x);

	H -= ht * ((nebr_y_plus - next) / (beta_y_plus.cent.y - cell.cent.y) -
		(next - nebr_y_minus) / (cell.cent.y - beta_y_minus.cent.y)) / cell.hy;

    //var_plus = linearInterp1d(getSigma2f(beta_y_plus), beta_y_plus.hy / 2.0, getSigma2f(cell), cell.hy / 2.0);
    //var_minus = linearInterp1d(getSigma2f(beta_y_minus), beta_y_minus.hy / 2.0, getSigma2f(cell), cell.hy / 2.0);
	H -= ht * (log(cell.trans[1]) - log(cell.trans[0])) / cell.hy *
		    (nebr_y_plus - nebr_y_minus) / (beta_y_plus.cent.y - beta_y_minus.cent.y);

	return H;
}
adouble StochOil::solveBorder_p0(const Cell& cell) const
{
	assert(cell.type == elem::BORDER);
	const auto& beta = mesh->cells[cell.stencil[1]];

	const auto& cur = x[cell.id];
	const auto& nebr = x[cell.stencil[1]];

    return /*(cur - nebr) / P_dim;*/ (cur - (adouble)(props_sk.p_out)) / P_dim;
}
adouble StochOil::solveSource_p0(const Well& well) const
{
	const Cell& cell = mesh->cells[well.cell_id];
	if(well.cur_bound == true)
		return -well.cur_rate * ht / cell.V / getKg(cell);
    else
        return -well.WI / well.perm * (well.cur_pwf - x[cell.id]) * ht / cell.V;
}

adouble StochOil::solveInner_Cfp(const Cell& cell, const Cell& cur_cell) const
{
	assert(cell.type == elem::QUAD);
    adouble next = x[cell.id];
    const auto prev = Cfp_prev[cur_cell.id * cellsNum + cell.id];
	adouble H, var_plus, var_minus;
    H = getS(cell) * (next - prev) / getKg(cell);

	const int& y_minus = cell.stencil[1];
	const int& y_plus = cell.stencil[2];
	const int& x_minus = cell.stencil[3];
	const int& x_plus = cell.stencil[4];

	const auto& beta_y_minus = mesh->cells[y_minus];
	const auto& beta_y_plus = mesh->cells[y_plus];
	const auto& beta_x_minus = mesh->cells[x_minus];
	const auto& beta_x_plus = mesh->cells[x_plus];

	const auto& nebr_y_minus = x[y_minus];
	const auto& nebr_y_plus = x[y_plus];
	const auto& nebr_x_minus = x[x_minus];
	const auto& nebr_x_plus = x[x_plus];

	H -= ht * ((nebr_x_plus - next) / (beta_x_plus.cent.x - cell.cent.x) -
		(next - nebr_x_minus) / (cell.cent.x - beta_x_minus.cent.x)) / cell.hx;

    //var_plus = linearInterp1d(getSigma2f(beta_x_plus), beta_x_plus.hx / 2.0, getSigma2f(cell), cell.hx / 2.0);
    //var_minus = linearInterp1d(getSigma2f(beta_x_minus), beta_x_minus.hx / 2.0, getSigma2f(cell), cell.hx / 2.0);
	H -= ht * (log(cell.trans[3]) - log(cell.trans[2])) / cell.hx *
		    (nebr_x_plus - nebr_x_minus) / (beta_x_plus.cent.x - beta_x_minus.cent.x);

	H -= ht * ((nebr_y_plus - next) / (beta_y_plus.cent.y - cell.cent.y) -
		(next - nebr_y_minus) / (cell.cent.y - beta_y_minus.cent.y)) / cell.hy;

    //var_plus = linearInterp1d(getSigma2f(beta_y_plus), beta_y_plus.hy / 2.0, getSigma2f(cell), cell.hy / 2.0);
    //var_minus = linearInterp1d(getSigma2f(beta_y_minus), beta_y_minus.hy / 2.0, getSigma2f(cell), cell.hy / 2.0);
	H -= ht * (log(cell.trans[1]) - log(cell.trans[0])) / cell.hy *
		(nebr_y_plus - nebr_y_minus) / (beta_y_plus.cent.y - beta_y_minus.cent.y);

	double H1 = -ht * ((p0_next[x_plus] - p0_next[x_minus]) / (beta_x_plus.cent.x - beta_x_minus.cent.x) *
	(getCf(cur_cell, beta_x_plus) - getCf(cur_cell, beta_x_minus)) / (beta_x_plus.cent.x - beta_x_minus.cent.x) +
					(p0_next[y_plus] - p0_next[y_minus]) / (beta_y_plus.cent.y - beta_y_minus.cent.y) *
	(getCf(cur_cell, beta_y_plus) - getCf(cur_cell, beta_y_minus)) / (beta_y_plus.cent.y - beta_y_minus.cent.y));
	
    double H2 = -getS(cell) / getKg(cell) * (p0_next[cell.id] - p0_prev[cell.id]) * getCf(cur_cell, cell);

    return H + H1 + H2;
}
adouble StochOil::solveBorder_Cfp(const Cell& cell, const Cell& cur_cell) const
{
	assert(cell.type == elem::BORDER || cur_cell.type == elem::BORDER);
	const auto& beta = mesh->cells[cell.stencil[1]];
    return /*(x[cell.id] - x[beta.id]) / P_dim;*/ x[cell.id] / P_dim;
}
adouble StochOil::solveSource_Cfp(const Well& well, const Cell& cur_cell) const
{
	const Cell& cell = mesh->cells[well.cell_id];
    if (well.cur_bound == true)
        return well.cur_rate * ht / cell.V / getKg(cell) * getCf(cur_cell, cell);
    else
        return well.WI / well.perm * x[cell.id] * ht / cell.V;
}

adouble StochOil::solveInner_p2(const Cell& cell) const
{
	assert(cell.type == elem::QUAD);

	const auto& next = x[cell.id];
	const auto prev = p2_prev[cell.id];

	adouble H, var_plus, var_minus;
	H = getS(cell) * (next - prev) / getKg(cell);

	const int& y_minus = cell.stencil[1];
	const int& y_plus = cell.stencil[2];
	const int& x_minus = cell.stencil[3];
	const int& x_plus = cell.stencil[4];

	const auto& beta_y_minus = mesh->cells[y_minus];
	const auto& beta_y_plus = mesh->cells[y_plus];
	const auto& beta_x_minus = mesh->cells[x_minus];
	const auto& beta_x_plus = mesh->cells[x_plus];

	const auto& nebr_y_minus = x[y_minus];
	const auto& nebr_y_plus = x[y_plus];
	const auto& nebr_x_minus = x[x_minus];
	const auto& nebr_x_plus = x[x_plus];

	H -= ht * ((nebr_x_plus - next) / (beta_x_plus.cent.x - cell.cent.x) -
		(next - nebr_x_minus) / (cell.cent.x - beta_x_minus.cent.x)) / cell.hx;

    //var_plus = linearInterp1d(getSigma2f(beta_x_plus), beta_x_plus.hx / 2.0, getSigma2f(cell), cell.hx / 2.0);
    //var_minus = linearInterp1d(getSigma2f(beta_x_minus), beta_x_minus.hx / 2.0, getSigma2f(cell), cell.hx / 2.0);
	H -= ht * (log(cell.trans[3]) - log(cell.trans[2])) / cell.hx *
		(nebr_x_plus - nebr_x_minus) / (beta_x_plus.cent.x - beta_x_minus.cent.x);

	H -= ht * ((nebr_y_plus - next) / (beta_y_plus.cent.y - cell.cent.y) -
		(next - nebr_y_minus) / (cell.cent.y - beta_y_minus.cent.y)) / cell.hy;

    //var_plus = linearInterp1d(getSigma2f(beta_y_plus), beta_y_plus.hy / 2.0, getSigma2f(cell), cell.hy / 2.0);
    //var_minus = linearInterp1d(getSigma2f(beta_y_minus), beta_y_minus.hy / 2.0, getSigma2f(cell), cell.hy / 2.0);
	H -= ht * (log(cell.trans[1]) - log(cell.trans[0])) / cell.hy *
		(nebr_y_plus - nebr_y_minus) / (beta_y_plus.cent.y - beta_y_minus.cent.y);

	double H1 = -ht * ((Cfp_next[x_plus * cellsNum + x_plus] - Cfp_next[x_plus * cellsNum + x_minus]) -
						(Cfp_next[x_minus * cellsNum + x_plus] - Cfp_next[x_minus * cellsNum + x_minus])) /
						(beta_x_plus.cent.x - beta_x_minus.cent.x) / (beta_x_plus.cent.x - beta_x_minus.cent.x) - 
				ht * ((Cfp_next[y_plus * cellsNum + y_plus] - Cfp_next[y_plus * cellsNum + y_minus]) -
						(Cfp_next[y_minus * cellsNum + y_plus] - Cfp_next[y_minus * cellsNum + y_minus])) /
						(beta_y_plus.cent.y - beta_y_minus.cent.y) / (beta_y_plus.cent.y - beta_y_minus.cent.y);

	const size_t idx = cell.id * cellsNum + cell.id;
	double H2 = getS(cell) / getKg(cell) * ((p0_next[cell.id] - p0_prev[cell.id]) * getSigma2f(cell) / 2.0 -
							(Cfp_next[idx] - Cfp_prev[idx]));
	return H + H1 + H2;
}
adouble StochOil::solveBorder_p2(const Cell& cell) const
{
	assert(cell.type == elem::BORDER);
	const auto& beta = mesh->cells[cell.stencil[1]];
    return /*(x[cell.id] - x[beta.id]) / P_dim;*/ x[cell.id] / P_dim;
}
adouble StochOil::solveSource_p2(const Well& well) const
{
	const Cell& cell = mesh->cells[well.cell_id];
    if (well.cur_bound == true)
        return -well.cur_rate * ht / cell.V / getKg(cell) * getSigma2f(cell) / 2.0;
    else
        return 0.0;// -well.WI / props_oil.visc * (well.cur_pwf - x[cell.id]) * ht / cell.V / getKg(cell) * getSigma2f(cell) / 2.0;
}

adouble StochOil::solveInner_Cp(const Cell& cell, const Cell& cur_cell, const size_t step_idx, const size_t cur_step_idx) const
{
	assert(cell.type == elem::QUAD && cur_cell.type == elem::QUAD);
	adouble next = x[cell.id];
	double prev;
	if(step_idx > start_time_simple_approx)
		prev = Cp_prev[step_idx - 1][cur_cell.id * cellsNum + cell.id];
	else
		prev = Cp_next[step_idx - 1][cur_cell.id * cellsNum + cell.id];

	adouble H, var_plus, var_minus;
	H = getS(cell) * (next - prev) / getKg(cell);

	const int& y_minus = cell.stencil[1];
	const int& y_plus = cell.stencil[2];
	const int& x_minus = cell.stencil[3];
	const int& x_plus = cell.stencil[4];

	const auto& beta_y_minus = mesh->cells[y_minus];
	const auto& beta_y_plus = mesh->cells[y_plus];
	const auto& beta_x_minus = mesh->cells[x_minus];
	const auto& beta_x_plus = mesh->cells[x_plus];

	const auto& nebr_y_minus = x[y_minus];
	const auto& nebr_y_plus = x[y_plus];
	const auto& nebr_x_minus = x[x_minus];
	const auto& nebr_x_plus = x[x_plus];

	H -= ht * ((nebr_x_plus - next) / (beta_x_plus.cent.x - cell.cent.x) -
		(next - nebr_x_minus) / (cell.cent.x - beta_x_minus.cent.x)) / cell.hx;

    //var_plus = linearInterp1d(getSigma2f(beta_x_plus), beta_x_plus.hx / 2.0, getSigma2f(cell), cell.hx / 2.0);
    //var_minus = linearInterp1d(getSigma2f(beta_x_minus), beta_x_minus.hx / 2.0, getSigma2f(cell), cell.hx / 2.0);
	H -= ht * (log(cell.trans[3]) - log(cell.trans[2])) / cell.hx *
		(nebr_x_plus - nebr_x_minus) / (beta_x_plus.cent.x - beta_x_minus.cent.x);

	H -= ht * ((nebr_y_plus - next) / (beta_y_plus.cent.y - cell.cent.y) -
		(next - nebr_y_minus) / (cell.cent.y - beta_y_minus.cent.y)) / cell.hy;

    //var_plus = linearInterp1d(getSigma2f(beta_y_plus), beta_y_plus.hy / 2.0, getSigma2f(cell), cell.hy / 2.0);
    //var_minus = linearInterp1d(getSigma2f(beta_y_minus), beta_y_minus.hy / 2.0, getSigma2f(cell), cell.hy / 2.0);
	H -= ht * (log(cell.trans[1]) - log(cell.trans[0])) / cell.hy *
		(nebr_y_plus - nebr_y_minus) / (beta_y_plus.cent.y - beta_y_minus.cent.y);

	double H1 = -ht * ((p0_next[x_plus] - p0_next[x_minus]) / (beta_x_plus.cent.x - beta_x_minus.cent.x) *
		(Cfp[step_idx][x_plus * cellsNum + cur_cell.id] - Cfp[step_idx][x_minus * cellsNum + cur_cell.id]) / (beta_x_plus.cent.x - beta_x_minus.cent.x) +
		(p0_next[y_plus] - p0_next[y_minus]) / (beta_y_plus.cent.y - beta_y_minus.cent.y) *
		(Cfp[step_idx][y_plus * cellsNum + cur_cell.id] - Cfp[step_idx][y_minus * cellsNum + cur_cell.id]) / (beta_y_plus.cent.y - beta_y_minus.cent.y));

	double H2 = -getS(cell) / getKg(cell) * (p0_next[cell.id] - p0_prev[cell.id]) * Cfp[step_idx][cell.id * cellsNum + cur_cell.id];

    return H + H1 + H2;
}
adouble StochOil::solveBorder_Cp(const Cell& cell, const Cell& cur_cell, const size_t step_idx) const
{
	assert(cell.type == elem::BORDER || cur_cell.type == elem::BORDER);
	const auto& beta = mesh->cells[cell.stencil[1]];
    return /*(x[cell.id] - x[beta.id]) / P_dim;*/ x[cell.id] / P_dim;
}
adouble StochOil::solveSource_Cp(const Well& well, const Cell& cur_cell, const size_t step_idx) const
{
	const Cell& cell = mesh->cells[well.cell_id];
    if (well.cur_bound == true)
        return well.cur_rate * ht / cell.V / getKg(cell) * Cfp[step_idx][cell.id * cellsNum + cur_cell.id];
    else
        return 0.0;// well.WI / props_oil.visc * (well.cur_pwf - x[cell.id]) * ht / cell.V / getKg(cell) * Cfp[step_idx][cell.id * cellsNum + cur_cell.id];
}