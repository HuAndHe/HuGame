#pragma once
#include<vector>
#include<list>
#include<queue>
const int kCost1 = 20;	//直移一个权值
const int kCost2 = 28;	//斜移一格权值

struct Point
{
	int x, y;			//点的坐标
	int F, G, H;		//估价函数
	Point* parent;		//父节点
	Point(int _x, int _y) :x(_x), y(_y), F(0), G(0), H(0), parent(NULL)			//变量初始化
	{

	}
};
struct greaterr //重写仿函数
	{
	    bool operator() (Point* a, Point* b)
		   {
		         return a->F > b->F; //小顶堆
		    }
	 };
class Astar
{
public:
	void InitAstar(std::vector<std::vector<int>>& _maze);
	std::list<Point*>GetPath(Point& startPoint, Point& endPoint, bool isIgnoreCorner);
private:
	Point* findPath(Point& startPoint, Point& endPoint, bool isIgnoreCorner);
	std::vector<Point*> getSurroundPoints(const Point* point, bool isIgnoreCorner) const;
	bool isCanreach(const Point* point, const Point* target, bool isIgnoreCorner) const; //判断某点是否可以用于下一步判断 
	Point* isInList(const std::list<Point*>& list, const Point* point) const;	//判断开启或者关闭列表中是否包含某点 
	Point* isInQueue( std::priority_queue<Point*, std::vector<Point*>, greaterr>, const Point* point) const;	//判断开启或者关闭列表中是否包含某点 

	Point* getLeastFpoint(); //从开启列表中返回F值最小的节点 
	//计算FGH值 
	int calcG(Point* temp_start, Point* point);
	int calcH(Point* point, Point* end);
	int calcF(Point* point);
private:
	std::vector<std::vector<int>> maze;

	std::list<Point*> openList;  //开启列表 
	std::priority_queue<Point*, std::vector<Point*>, greaterr> openList_prior_queue;
	std::list<Point*> closeList; //关闭列表 
};