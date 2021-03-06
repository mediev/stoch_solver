#ifndef ELEM_HPP_
#define ELEM_HPP_

#include "src/grid/Point.hpp"

namespace elem
{
	typedef point::Point Point;

	enum Type {QUAD, BORDER, CORNER};
	template <int MaxStencilNum>
	class Element
	{
	protected:
		
	public:
		int id;
		const Type type;
		static const int getCurStencNum()
		{
			if (type == QUAD)
				return MaxStencilNum;
			else if (type == BORDER)
				return 2;
			else if (type == CORNER)
				return 3;
            else 
                return -1;
		};
		std::array<int, MaxStencilNum> stencil;
		std::array<double, MaxStencilNum - 1> trans;

		Element(const Type _type) : type(_type) {};
		Element(const int _id, const Type _type) : id(_id), type(_type) {};
	};
	class Quad : public Element<5>
	{
	public:
		Point cent;

		double hx, hy, hz;
		double V;

		Quad(const int _id, const Type _type) : Element(_id, _type) {};
		Quad(const int _id, const Type _type, const Point _cent, const Point _sizes) : 
			Element(_id, _type), cent(_cent), hx(_sizes.x), hy(_sizes.y), hz(_sizes.z), V(_sizes.x * _sizes.y * _sizes.z) {};
	};
    class DualQuad : public Element<5>
    {
    public:
        Point cent;

        double hx, hy, hz;
        double V;

        DualQuad(const int _id, const Type _type) : Element(_id, _type) {};
        DualQuad(const int _id, const Type _type, const Point _cent, const Point _sizes) :
            Element(_id, _type), cent(_cent), hx(_sizes.x), hy(_sizes.y), hz(_sizes.z), V(_sizes.x * _sizes.y * _sizes.z) {};
    };
    class Node : public Element<5>
    {
    protected:
    public:
        Point cent;
        Node(const int _id, const Type _type) : Element(_id, _type) {};
        Node(const int _id, const Type _type, const Point _cent) : Element(_id, _type), cent(_cent) {};
    };
};

#endif /* ELEM_HPP_ */
