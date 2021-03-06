#include <iomanip>
#include <iostream>
#include <iterator>

#include "src/model/AbstractMethod.hpp"

#include "src/model/oil/Oil.hpp"
#include "src/model/stoch_oil/StochOil.hpp"
#include "src/model/dual_stoch_oil/DualStochOil.hpp"

using namespace std;

template <class modelType>
AbstractMethod<modelType>::AbstractMethod(modelType* _model) : model(_model), mesh(_model->getMesh()), 
	size(_model->getCellsNum()), Tt(model->wells.back().period[model->wells.back().periodsNum - 1])
{
	cur_t = cur_t_log = 0.0;
	curTimePeriod = 0;

	t_dim = model->t_dim;
	repeat = 0;
}
template <class modelType>
AbstractMethod<modelType>::~AbstractMethod()
{
}
template <class modelType>
void AbstractMethod<modelType>::start()
{
	int counter = 0;

	model->setPeriod(curTimePeriod);
	while (cur_t < Tt)
	{
		control();
		model->snapshot_all(counter++);
		doNextStep();
		copyTimeLayer();
		cout << "---------------------NEW TIME STEP---------------------" << endl;
		cout << setprecision(6);
		cout << "time = " << cur_t << endl;
	}
	model->snapshot_all(counter++);
	writeData();
}
template <class modelType>
void AbstractMethod<modelType>::doNextStep()
{
	solveStep();
}

template <class modelType>
void AbstractMethod<modelType>::copyIterLayer()
{
	model->u_iter = model->u_next;
}
template <class modelType>
void AbstractMethod<modelType>::copyTimeLayer()
{
	model->u_prev = model->u_iter = model->u_next;
}
void AbstractMethod<stoch_oil::StochOil>::copyIterLayer()
{
}
void AbstractMethod<stoch_oil::StochOil>::copyTimeLayer()
{
}

template <class modelType>
double AbstractMethod<modelType>::convergance(int& ind, int& varInd)
{
	double relErr = 0.0;
	double cur_relErr = 0.0;

	varInd = 0;
	auto diff = std::abs((model->u_next - model->u_iter) / model->u_next);
	auto max_iter = std::max_element(std::begin(diff), std::end(diff));
	ind = std::distance(std::begin(diff), max_iter);

	return *max_iter;
}
template <class modelType>
void AbstractMethod<modelType>::averValue(std::array<double, var_size>& aver)
{
	std::fill(aver.begin(), aver.end(), 0.0);

	for (int i = 0; i < var_size; i++)
	{
		const auto var = static_cast<std::valarray<double>>(model->u_next[std::slice(i, model->cellsNum, var_size)]);
		int cell_idx = 0;
		for (const auto& cell : mesh->cells)
			aver[i] += var[cell_idx++] * cell.V;
	}
	for (auto& val : aver)
		val /= mesh->V;
}
double AbstractMethod<stoch_oil::StochOil>::convergance(int& ind, int& varInd)
{
	double relErr = 0.0;
	double cur_relErr = 0.0;

	varInd = 0;
	auto diff = std::abs((model->p0_next - model->p0_iter) / model->p0_next);
	auto max_iter = std::max_element(std::begin(diff), std::end(diff));
	ind = std::distance(std::begin(diff), max_iter);

	return *max_iter;
}
void AbstractMethod<stoch_oil::StochOil>::averValue(std::array<double, var_size>& aver)
{
	std::fill(aver.begin(), aver.end(), 0.0);

	for (int i = 0; i < var_size; i++)
	{
		const auto var = static_cast<std::valarray<double>>(model->p0_next[std::slice(i, model->cellsNum, var_size)]);
		int cell_idx = 0;
		for (const auto& cell : mesh->cells)
			aver[i] += var[cell_idx++] * cell.V;
	}
	for (auto& val : aver)
		val /= mesh->V;
}

template <class modelType>
AbstractDualGridMethod<modelType>::AbstractDualGridMethod(modelType* _model) : model(_model), cell_mesh(_model->getCellMesh()), node_mesh(_model->getNodeMesh()),
cells_size(_model->getCellsNum()), nodes_size(_model->getNodesNum()), Tt(model->wells.back().period[model->wells.back().periodsNum - 1])
{
    cur_t = cur_t_log = 0.0;
    curTimePeriod = 0;

    t_dim = model->t_dim;
    repeat = 0;
}
template <class modelType>
AbstractDualGridMethod<modelType>::~AbstractDualGridMethod()
{
}
template <class modelType>
void AbstractDualGridMethod<modelType>::start()
{
    int counter = 0;

    model->setPeriod(curTimePeriod);
    while (cur_t < Tt)
    {
        control();
        model->snapshot_all(counter++);
        doNextStep();
        copyTimeLayer();
        cout << "---------------------NEW TIME STEP---------------------" << endl;
        cout << setprecision(6);
        cout << "time = " << cur_t << endl;
    }
    model->snapshot_all(counter++);
    writeData();
}
template <class modelType>
void AbstractDualGridMethod<modelType>::doNextStep()
{
    solveStep();
}

template <class modelType>
void AbstractDualGridMethod<modelType>::copyIterLayer()
{
    model->u_iter = model->u_next;
}
template <class modelType>
void AbstractDualGridMethod<modelType>::copyTimeLayer()
{
    model->u_prev = model->u_iter = model->u_next;
}
void AbstractDualGridMethod<dual_stoch_oil::DualStochOil>::copyIterLayer()
{   
}
void AbstractDualGridMethod<dual_stoch_oil::DualStochOil>::copyTimeLayer()
{
}

template <class modelType>
double AbstractDualGridMethod<modelType>::convergance(int& ind, int& varInd)
{
    double relErr = 0.0;
    double cur_relErr = 0.0;

    varInd = 0;
    auto diff = std::abs((model->p0_next - model->p0_iter) / model->p0_next);
    auto max_iter = std::max_element(std::begin(diff), std::end(diff));
    ind = std::distance(std::begin(diff), max_iter);

    return *max_iter;
}
template <class modelType>
void AbstractDualGridMethod<modelType>::averValue(std::array<double, var_size>& aver)
{
    std::fill(aver.begin(), aver.end(), 0.0);

    for (int i = 0; i < var_size; i++)
    {
        const auto var = static_cast<std::valarray<double>>(model->p0_next[std::slice(i, model->cellsNum, var_size)]);
        int cell_idx = 0;
        for (const auto& cell : cell_mesh->cells)
            aver[i] += var[cell_idx++] * cell.V;
    }
    for (auto& val : aver)
        val /= cell_mesh->V;
}

template class AbstractMethod<stoch_oil::StochOil>;
template class AbstractDualGridMethod<dual_stoch_oil::DualStochOil>;