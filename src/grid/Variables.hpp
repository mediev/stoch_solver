#ifndef VARIABLES_HPP_
#define VARIABLES_HPP_

#include <valarray>
#include <array>
#include "adolc/adouble.h"

namespace var
{
	namespace containers
	{
		struct DummyStruct {};
		struct Var1phase
		{
			static const int size = 1;
			double& p0;

			Var1phase(double* data) : p0(data[0]) {};
			Var1phase(const double* data) : p0(const_cast<double&>(data[0])) {};
		};
		struct TapeVar1Phase
		{
			static const int size = 1;
			adouble p0;
		};

		struct StochVar1phase1
		{
			static const int size = 2;
			double& p2;
			double& Cfp;

			StochVar1phase1(double* data) : p2(data[0]), Cfp(data[1]) {};
			StochVar1phase1(const double* data) : p2(const_cast<double&>(data[0])), Cfp(const_cast<double&>(data[1])) {};
		};
		struct TapeStochVar1Phase1
		{
			static const int size = 2;
			adouble p2;
			adouble Cfp;
		};
		struct StochVar1phase2
		{
			static const int size = 1;
			double& Cp;

			StochVar1phase2(double* data) : Cp(data[0]) {};
			StochVar1phase2(const double* data) : Cp(const_cast<double&>(data[0])) {};
		};
		struct TapeStochVar1Phase2
		{
			static const int size = 1;
			adouble Cp;
		};
	};

	template <typename TVariable>
	struct BaseVarWrapper
	{
		TVariable u_prev, u_iter, u_next;
	};
	template <typename TVariable, typename=var::containers::DummyStruct, typename=var::containers::DummyStruct>
	struct BasicVariables
	{
		static const int size = TVariable::size;
		typedef BaseVarWrapper<TVariable> Wrap;
		std::valarray<double> u_prev, u_iter, u_next;

		Wrap operator[](const size_t idx)
		{
			return{ TVariable(&u_prev[idx * size]), TVariable(&u_iter[idx * size]), TVariable(&u_next[idx * size]) };
		};
		Wrap operator[](const size_t idx) const
		{
			return{ TVariable(&u_prev[idx * size]), TVariable(&u_iter[idx * size]), TVariable(&u_next[idx * size]) };
		};
	};


	template <typename TVariable0, typename TVariable1, typename TVariable2>
	struct StochVarWrapper
	{
		TVariable0 u_prev0, u_iter0, u_next0;
		TVariable1 u_prev1, u_iter1, u_next1;
		TVariable2 u_prev2, u_iter2, u_next2;
	};
	template <typename TVariable0, typename TVariable1, typename TVariable2>
	struct StochVariables
	{
		static const int size0 = TVariable0::size;
		static const int size1 = TVariable1::size;
		static const int size2 = TVariable2::size;

		typedef StochVarWrapper<TVariable0, TVariable1, TVariable2> Wrap;
		std::valarray<double> u_prev0, u_iter0, u_next0, u_prev1, u_iter1, u_next1, u_prev2, u_iter2, u_next2;

		Wrap operator[](const size_t idx)
		{
			return{ TVariable0(&u_prev0[idx * size0]), TVariable0(&u_iter0[idx * size0]), TVariable0(&u_next0[idx * size0]),
					TVariable1(&u_prev1[idx * size1]), TVariable1(&u_iter1[idx * size1]), TVariable1(&u_next1[idx * size1]),
					TVariable2(&u_prev2[idx * size2]), TVariable2(&u_iter2[idx * size2]), TVariable2(&u_next2[idx * size2]) };
		};
		Wrap operator[](const size_t idx) const
		{
			return{ TVariable0(&u_prev0[idx * size0]), TVariable0(&u_iter0[idx * size0]), TVariable0(&u_next0[idx * size0]),
					TVariable1(&u_prev1[idx * size1]), TVariable1(&u_iter1[idx * size1]), TVariable1(&u_next1[idx * size1]),
					TVariable2(&u_prev2[idx * size2]), TVariable2(&u_iter2[idx * size2]), TVariable2(&u_next2[idx * size2]) };
		};
	};
}

#endif /* VARIABLES_HPP_ */