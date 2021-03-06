#include <fstream>
#include <iostream>
#include <iomanip>
#include "src/model/stoch_oil/StochOilMethod.hpp"

#include "adolc/sparse/sparsedrivers.h"
#include "adolc/drivers/drivers.h"

using namespace stoch_oil;

StochOilMethod::StochOilMethod(Model* _model) : AbstractMethod<Model>(_model)
{
	const int strNum0 = model->cellsNum;
	y0 = new double[strNum0];
	ind_i0 = new int[Mesh::stencil * strNum0];
	ind_j0 = new int[Mesh::stencil * strNum0];
	//cols = new int[strNum];
	a0 = new double[Mesh::stencil * strNum0];
	ind_rhs0 = new int[strNum0];
	rhs0 = new double[strNum0];

	const int strNum1 = model->cellsNum;
	y1 = new double[strNum1];
	ind_i1 = new int[Mesh::stencil * strNum1];
	ind_j1 = new int[Mesh::stencil * strNum1];
	//cols = new int[strNum];
	a1 = new double[Mesh::stencil * strNum1];
	ind_rhs1 = new int[strNum1];
	rhs1 = new double[strNum1];

	//options[0] = 0;          /* sparsity pattern by index domains (default) */
	//options[1] = 0;          /*                         safe mode (default) */
	//options[2] = 0;          /*              not required if options[0] = 0 */
	//options[3] = 0;          /*                column compression (default) */

	pvd.open("snaps/StochOil.pvd", std::ofstream::out);
	pvd << "<VTKFile type = \"Collection\" version = \"1.0\" byte_order = \"LittleEndian\" header_type = \"UInt64\">\n";
	pvd << "\t<Collection>\n";

	plot_P.open("snaps/P.dat", std::ofstream::out);
	plot_Q.open("snaps/Q.dat", std::ofstream::out);
};
StochOilMethod::~StochOilMethod()
{
	delete[] y0, y1;

	delete[] ind_i0, ind_j0, ind_rhs0;
	delete[] a0, rhs0;

	delete[] ind_i1, ind_j1, ind_rhs1;
	delete[] a1, rhs1;

	plot_P.close();
	plot_Q.close();
	pvd << "\t</Collection>\n";
	pvd << "</VTKFile>\n";
	pvd.close();
};
void StochOilMethod::writeData()
{
	plot_Q << cur_t * t_dim / 3600.0;
	plot_P << cur_t * t_dim / 3600.0;

	for (const auto& well : model->wells)
	{
		plot_Q << "\t" << model->getRate(well) * model->Q_dim * 86400.0 << "\t" << sqrt(model->getRateVar(well, step_idx)) * model->Q_dim * 86400.0;
		plot_P << "\t" << model->getPwf(well) * model->P_dim / BAR_TO_PA << "\t" << sqrt(model->getPwfVar(well, step_idx)) * model->P_dim / BAR_TO_PA;
	}

	plot_Q << std::endl;
	plot_P << std::endl;

	pvd << "\t\t<DataSet part=\"0\" timestep=\"" + std::to_string(cur_t * t_dim / 3600.0) +
		"0\" file=\"StochOil_" + std::to_string(step_idx) + ".vtu\"/>\n";

    model->writeCPS(step_idx);
}
void StochOilMethod::control()
{
	writeData();

	if (cur_t >= model->wells[0].period[curTimePeriod])
	{
		curTimePeriod++;
		model->ht = model->ht_min;
		model->setPeriod(curTimePeriod);
	}

	model->ht *= 2.0;
	//if (model->ht <= model->ht_max && iterations < 6)
	//	model->ht = model->ht * 1.5;
	//else if (iterations > 6 && model->ht > model->ht_min)
	//	model->ht = model->ht / 1.5;

	if (cur_t + model->ht > model->wells[0].period[curTimePeriod])
		model->ht = model->wells[0].period[curTimePeriod] - cur_t;

	cur_t += model->ht;
}
void StochOilMethod::start()
{
	step_idx = 0;

	fillIndices();
	solver0.Init(model->cellsNum, 1.e-15, 1.e-15);
	solver1.Init(model->cellsNum, 1.e-15, 1.e-15);

	model->setPeriod(curTimePeriod);
	while (cur_t < Tt)
	{
		control();
		model->snapshot_all(step_idx++);
		doNextStep();
		copyTimeLayer();
		cout << "---------------------NEW TIME STEP---------------------" << std::endl;
		cout << std::setprecision(6);
		cout << "time = " << cur_t << std::endl;
	}

	model->snapshot_all(step_idx);
	writeData();
}
void StochOilMethod::fillIndices()
{
    int counter = 0;

    for (int i = 0; i < model->cellsNum; i++)
    {
        auto& cell = mesh->cells[i];
        getMatrixStencil(cell);

        for (size_t i = 0; i < var_size; i++)
            for (const int idx : cell.stencil)
                for (size_t j = 0; j < var_size; j++)
                {
                    ind_i0[counter] = ind_i1[counter] = var_size * cell.id + i;
                    ind_j0[counter] = ind_j1[counter] = var_size * idx + j;
                    counter++;
                }
        ind_rhs0[i] = ind_rhs1[i] = i;
    }
    elemNum0 = elemNum1 = counter;
};

void StochOilMethod::solveStep()
{
	col = offset = NULL;
	dmat = NULL;
	avoidMatrixCalc = false;

	solveStep_p0();
	solveStep_Cfp();
	solveStep_p2();
	solveStep_Cp();
}
void StochOilMethod::solveStep_p0()
{
	int cellIdx, varIdx, iterations;
	double err_newton = 1.0;
	averValPrev = averValue_p0();

	iterations = 0;	err_newton = 1;	dAverVal = 1.0;
	while (err_newton > 1.e-4 && dAverVal > 1.e-7 && iterations < 20)
	{
		copyIterLayer_p0();
		computeJac_p0();
		fill_p0();
		solver0.Assemble(ind_i0, ind_j0, a0, elemNum0, ind_rhs0, rhs0);
		solver0.Solve(PRECOND::ILU_SIMPLE);
		copySolution_p0(solver0.getSolution());

		err_newton = convergance_p0(cellIdx, varIdx);
		averVal = averValue_p0();
		dAverVal = fabs(averVal - averValPrev);
		averValPrev = averVal;

		iterations++;
	}
	std::cout << std::endl << "p0 Iterations = " << iterations << std::endl << std::endl;
}
void StochOilMethod::solveStep_Cfp()
{
	for (const auto& cell : mesh->cells)
	{
		//if (cell.type == elem::QUAD)
		//{
			computeJac_Cfp(cell.id);
			fill_Cfp(cell.id);
			if (!avoidMatrixCalc)
			{
				solver1.getInvert(ind_i1, ind_j1, a1, elemNum1, offset, col, dmat);
				//checkInvertMatrix();
				avoidMatrixCalc = true;
			}
			copySolution_Cfp(cell.id);
			std::cout << "Cfp #" << cell.id << std::endl;
			solver1.SetSameMatrix();
		//}
	}
}
void StochOilMethod::checkInvertMatrix() const 
{
    std::ofstream file("ffile.txt", std::ofstream::out);
	int ind0 = 0, ind1 = 0, end_idx = 0;
	double val;
	for (int i = 0; i < size; i++)
	{
		for (int j = 0; j < size; j++)
		{
			val = 0.0;
			//ind = end_idx;
			for (size_t k = 0; k < size; k++)
			{
                if (ind_i1[ind0] == i && ind_j1[ind0] == k)
                {
                    for (int l = offset[k]; l < offset[k + 1]; l++)
                        if (col[l] == j)
                        {
                            val += a1[ind0] * dmat[ind1];
                            break;
                        }
                    ind0++;
                }
                for (int l = offset[k]; l < offset[k + 1]; l++)
                    if (col[l] == j)
                        ind1++;
			}
            file << i << "\t" << j << "\t" << val << "\n";
			/*if (i == j)
				assert(fabs(val - 1.0) < 1.E-6);
			else
				assert(fabs(val) < 1.E-6);*/

		}
		//end_idx = ind;
	}
    file.close();
}
void StochOilMethod::solveStep_p2()
{
	int cellIdx, varIdx, iterations;
	double err_newton = 1.0;
	averValPrev = averValue_p0();

	iterations = 0;	err_newton = 1;	dAverVal = 1.0;
	while (err_newton > 1.e-4 && dAverVal > 1.e-7 && iterations < 20)
	{
		copyIterLayer_p2();
		computeJac_p2();
		fill_p2();
		solver0.Assemble(ind_i0, ind_j0, a0, elemNum0, ind_rhs0, rhs0);
		solver0.Solve(PRECOND::ILU_SIMPLE);
		copySolution_p2(solver0.getSolution());

		err_newton = convergance_p2(cellIdx, varIdx);
		averVal = averValue_p2();
		dAverVal = fabs(averVal - averValPrev);
		averValPrev = averVal;

		iterations++;
	}
	std::cout << std::endl << "p2 Iterations = " << iterations << std::endl << std::endl;
}
void StochOilMethod::solveStep_Cp()
{
	int start_idx = 1;
	if (step_idx > model->start_time_simple_approx)
		start_idx = step_idx;

	for (int time_step = start_idx; time_step < step_idx + 1; time_step++)
	{
		for (const auto& cell : mesh->cells)
		{
			if (cell.type == elem::QUAD)
			{
				computeJac_Cp(cell.id, time_step);
				fill_Cp(cell.id, time_step);
				copySolution_Cp(cell.id, time_step);
				std::cout << "time step = " << time_step << "\t Cp #" << cell.id << std::endl;
			}
		}
	}
}

void StochOilMethod::copySolution_p0(const paralution::LocalVector<double>& sol)
{
	for (int i = 0; i < size; i++)
		model->p0_next[i] += sol[i];
}
/*void StochOilMethod::copySolution_Cfp(const int cell_id, const paralution::LocalVector<double>& sol)
{
	for (int i = 0; i < size; i++)
		model->Cfp_next[cell_id * size + i] += sol[i];
}*/
void StochOilMethod::copySolution_Cfp(const int cell_id)
{
	double s;
	for (int i = 0; i < size; i++)
	{
		s = 0.0;
		for (int j = offset[i]; j < offset[i + 1]; j++)
			s += dmat[j] * rhs1[col[j]];
		model->Cfp_next[cell_id * size + i] += s;
	}
}
void StochOilMethod::copySolution_p2(const paralution::LocalVector<double>& sol)
{
	for (int i = 0; i < size; i++)
		model->p2_next[i] += sol[i];
}
/*void StochOilMethod::copySolution_Cp(const int cell_id, const paralution::LocalVector<double>& sol, const size_t time_step)
{
	for (size_t i = 0; i < size; i++)
		model->Cp_next[time_step][cell_id * size + i] += sol[i];
}*/
void StochOilMethod::copySolution_Cp(const int cell_id, const size_t time_step)
{
	double s;
	for (size_t i = 0; i < size; i++)
	{
		s = 0.0;
		for (size_t j = offset[i]; j < offset[i + 1]; j++)
			s += dmat[j] * rhs1[col[j]];
		model->Cp_next[time_step][cell_id * size + i] += s;
	}
}

void StochOilMethod::computeJac_p0()
{
	trace_on(0);

	for (size_t i = 0; i < size; i++)
		model->x[i] <<= model->p0_next[i * var_size];

	for (int i = 0; i < size; i++)
	{
		const auto& cell = mesh->cells[i];

		if (cell.type == elem::QUAD)
			model->h[i * var_size] = model->solveInner_p0(cell);
		else if (cell.type == elem::BORDER)
			model->h[i * var_size] = model->solveBorder_p0(cell);
	}

    for (const auto& well : model->wells)
        model->h[well.cell_id * var_size] += model->solveSource_p0(well);

	for (int i = 0; i < var_size * size; i++)
		model->h[i] >>= y0[i];

	trace_off();
}
void StochOilMethod::computeJac_Cfp(const int cell_id)
{
	trace_on(1);

	const auto& cur_cell = mesh->cells[cell_id];
	for (size_t i = 0; i < size; i++)
		model->x[i] <<= model->Cfp_next[size * cell_id + i];

	for (int i = 0; i < size; i++)
	{
		const auto& cell = mesh->cells[i];

		if (cell.type == elem::QUAD)
			model->h[i] = model->solveInner_Cfp(cell, cur_cell) / model->P_dim;
		else if (cell.type == elem::BORDER)
			model->h[i] = model->solveBorder_Cfp(cell, cur_cell);
	}
    for (const auto& well : model->wells)
        model->h[well.cell_id] += model->solveSource_Cfp(well, cur_cell) / model->P_dim;

	for (int i = 0; i < size; i++)
		model->h[i] >>= y1[i];

	trace_off();
}
void StochOilMethod::computeJac_p2()
{
	trace_on(2);

	for (size_t i = 0; i < size; i++)
		model->x[i] <<= model->p2_next[i * var_size];

	for (int i = 0; i < size; i++)
	{
		const auto& cell = mesh->cells[i];

		if (cell.type == elem::QUAD)
			model->h[i * var_size] = model->solveInner_p2(cell);
		else if (cell.type == elem::BORDER)
			model->h[i * var_size] = model->solveBorder_p2(cell);
	}

	for (const auto& well : model->wells)
		model->h[well.cell_id * var_size] += model->solveSource_p2(well);

	for (int i = 0; i < var_size * size; i++)
		model->h[i] >>= y0[i];

	trace_off();
}
void StochOilMethod::computeJac_Cp(const int cell_id, const size_t time_step)
{
	trace_on(3);

	const auto& cur_cell = mesh->cells[cell_id];
	for (size_t i = 0; i < size; i++)
		model->x[i] <<= model->Cp_next[time_step][size * cell_id + i];

	for (int i = 0; i < size; i++)
	{
		const auto& cell = mesh->cells[i];

		if (cell.type == elem::QUAD)
			model->h[i] = model->solveInner_Cp(cell, cur_cell, time_step, step_idx) / model->P_dim;
		else if (cell.type == elem::BORDER)
			model->h[i] = model->solveBorder_Cp(cell, cur_cell, time_step);
	}
	for (const auto& well : model->wells)
		model->h[well.cell_id] += model->solveSource_Cp(well, cur_cell, time_step) / model->P_dim;

	for (int i = 0; i < size; i++)
		model->h[i] >>= y1[i];

	trace_off();
}

void StochOilMethod::fill_p0()
{
	sparse_jac(0, model->cellsNum, model->cellsNum, repeat,
		&model->p0_next[0], &elemNum0, (unsigned int**)(&ind_i0), (unsigned int**)(&ind_j0), &a0, options);

	int counter = 0;
	for (int j = 0; j < size; j++)
	{
		const auto& cell = mesh->cells[j];
		rhs0[cell.id] = -y0[cell.id];
	}
}
void StochOilMethod::fill_Cfp(const int cell_id)
{
	if(!avoidMatrixCalc)
		sparse_jac(1, model->cellsNum, model->cellsNum, repeat,
			&model->Cfp_next[cell_id * model->cellsNum], &elemNum1, (unsigned int**)(&ind_i1), (unsigned int**)(&ind_j1), &a1, options);

	int counter = 0;
	for (int j = 0; j < size; j++)
	{
		const auto& cell = mesh->cells[j];
		rhs1[cell.id] = -y1[cell.id];
	}
}
void StochOilMethod::fill_p2()
{
	sparse_jac(2, model->cellsNum, model->cellsNum, repeat,
		&model->p2_next[0], &elemNum0, (unsigned int**)(&ind_i0), (unsigned int**)(&ind_j0), &a0, options);

	int counter = 0;
	for (int j = 0; j < size; j++)
	{
		const auto& cell = mesh->cells[j];
		rhs0[cell.id] = -y0[cell.id];
	}
}
void StochOilMethod::fill_Cp(const int cell_id, const size_t time_step)
{
	if (!avoidMatrixCalc)
		sparse_jac(3, model->cellsNum, model->cellsNum, repeat,
			&model->Cp_next[time_step][cell_id * model->cellsNum], &elemNum1, (unsigned int**)(&ind_i1), (unsigned int**)(&ind_j1), &a1, options);

	int counter = 0;
	for (int j = 0; j < size; j++)
	{
		const auto& cell = mesh->cells[j];
		rhs1[cell.id] = -y1[cell.id];
	}
}

void StochOilMethod::copyTimeLayer()
{
	model->p0_prev = model->p0_iter = model->p0_next;

    if (cur_t < Tt)
    {
        model->Cfp[step_idx + 1] = model->Cfp[step_idx];
        model->Cfp_prev = &model->Cfp[step_idx][0];
        model->Cfp_next = &model->Cfp[step_idx + 1][0];
    }
	model->p2_prev = model->p2_iter = model->p2_next;

	model->Cp_prev = model->Cp_next;
}
void StochOilMethod::copyIterLayer_p0()
{
	model->p0_iter = model->p0_next;
}
void StochOilMethod::copyIterLayer_p2()
{
	model->p2_iter = model->p2_next;
}

double StochOilMethod::convergance_p0(int& ind, int& varInd)
{
	double relErr = 0.0;
	double cur_relErr = 0.0;

	varInd = 0;
	auto diff = std::abs((model->p0_next - model->p0_iter) / model->p0_next);
	auto max_iter = std::max_element(std::begin(diff), std::end(diff));
	ind = std::distance(std::begin(diff), max_iter);

	return *max_iter;
}
double StochOilMethod::convergance_p2(int& ind, int& varInd)
{
	double relErr = 0.0;
	double cur_relErr = 0.0;

	varInd = 0;
	auto diff = std::abs((model->p2_next - model->p2_iter) / model->p2_next);
	auto max_iter = std::max_element(std::begin(diff), std::end(diff));
	ind = std::distance(std::begin(diff), max_iter);

	return *max_iter;
}

double StochOilMethod::averValue_p0() const 
{
	double aver = 0.0;
	for (const auto& cell : mesh->cells)
		aver += model->p0_next[cell.id] * cell.V;
	return aver / model->Volume;
}
double StochOilMethod::averValue_Cfp(const int cell_id) const
{
	double aver = 0.0;
	for (const auto& cell : mesh->cells)
		aver += model->Cfp_next[cell_id * model->cellsNum + cell.id] * cell.V;
	return aver / model->Volume;
}
double StochOilMethod::averValue_p2() const
{
	double aver = 0.0;
	for (const auto& cell : mesh->cells)
		aver += model->p2_next[cell.id] * cell.V;
	return aver / model->Volume;
}
double StochOilMethod::averValue_Cp(const int cell_id, const size_t time_step) const
{
	double aver = 0.0;
	for (const auto& cell : mesh->cells)
		aver += model->Cp_next[time_step][cell_id * model->cellsNum + cell.id] * cell.V;
	return aver / model->Volume;
}