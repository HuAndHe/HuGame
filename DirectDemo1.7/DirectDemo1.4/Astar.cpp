#include <math.h> 
#include "Astar.h" 

void Astar::InitAstar(std::vector<std::vector<int>>& _maze)
{
	maze = _maze;
}

int Astar::calcG(Point* temp_start, Point* point)
{
	int extraG = (abs(point->x - temp_start->x) + abs(point->y - temp_start->y)) == 1 ? kCost1 : kCost2;
	int parentG = point->parent == NULL ? 0 : point->parent->G; //如果是初始节点，则其父节点是空 
	return parentG + extraG;
}

int Astar::calcH(Point* point, Point* end)
{
	//用简单的欧几里得距离计算H，这个H的计算是关键，还有很多算法，没深入研究^_^ 
	return int(sqrt((double)(end->x - point->x) * (double)(end->x - point->x) + (double)(end->y - point->y) * (double)(end->y - point->y)) * kCost1);
}

int Astar::calcF(Point* point)
{
	return point->G + point->H;
}
//b遍历Openlist得到最小权值的节点
Point* Astar::getLeastFpoint()
{
	if (!openList_prior_queue.empty())
	{
		/*auto resPoint = openList.front();
		for (auto& point : openList)
			if (point->F < resPoint->F)
				resPoint = point;*/
		auto resPoint = openList_prior_queue.top();
		return resPoint;
	}
	return NULL;
}

Point* Astar::findPath(Point& startPoint, Point& endPoint, bool isIgnoreCorner)
{
	//if (!openList_prior_queue.empty())		//开放列表如果不为空则需要先清空
	//{		
	//	openList_prior_queue.clear();
	//	closeList.clear();
	//}
	closeList.clear();
	//openList.push_back(new Point(startPoint.x, startPoint.y)); //置入起点,拷贝开辟一个节点，内外隔离 
	openList_prior_queue.push(new Point(startPoint.x, startPoint.y));
	while (!openList_prior_queue.empty())
	{
		auto curPoint = getLeastFpoint(); //找到F值最小的点 
		//openList.remove(curPoint); //从开启列表中删除 
		openList_prior_queue.pop();
		closeList.push_back(curPoint); //放到关闭列表 
		//1,找到当前周围八个格中可以通过的格子 
		auto surroundPoints = getSurroundPoints(curPoint, isIgnoreCorner);
		for (auto& target : surroundPoints)
		{
			//2,对某一个格子，如果它不在开启列表中，加入到开启列表，设置当前格为其父节点，计算F G H 
			if (!isInQueue(openList_prior_queue, target))
			{
				target->parent = curPoint;

				target->G = calcG(curPoint, target);
				target->H = calcH(target, &endPoint);
				target->F = calcF(target);

				//openList.push_back(target);
				openList_prior_queue.push(target);
			}
			//3，对某一个格子，它在开启列表中，计算G值, 如果比原来的大, 就什么都不做, 否则设置它的父节点为当前点,并更新G和F 
			else
			{
				int tempG = calcG(curPoint, target);
				if (tempG < target->G)
				{
					target->parent = curPoint;

					target->G = tempG;
					target->F = calcF(target);
				}
			}
			Point* resPoint = isInQueue(openList_prior_queue, &endPoint);
			if (resPoint)
			{
				closeList.clear();
				while (!openList_prior_queue.empty())
				{
					openList_prior_queue.pop();
				}
				return resPoint; //返回列表里的节点指针，不要用原来传入的endpoint指针，因为发生了深拷贝 
			}
				
		}
	}

	return NULL;
}

std::list<Point*> Astar::GetPath(Point& startPoint, Point& endPoint, bool isIgnoreCorner)
{
	Point* result = findPath(startPoint, endPoint, isIgnoreCorner);
	std::list<Point*> path;
	//返回路径，如果没找到路径，返回空链表 
	while (result)
	{
		path.push_front(result);
		result = result->parent;
	}
	return path;
}

Point* Astar::isInList(const std::list<Point*>& list, const Point* point) const
{
	//判断某个节点是否在列表中，这里不能比较指针，因为每次加入列表是新开辟的节点，只能比较坐标 
	for (auto p : list)
		if (p->x == point->x && p->y == point->y)
			return p;
	return NULL;
}

Point* Astar::isInQueue( std::priority_queue<Point*, std::vector<Point*>, greaterr> openQueue, const Point* point) const
{
	while (!openQueue.empty())
	{
		auto resPoint = openQueue.top();
		if (resPoint->x == point->x && resPoint->y == point->y)
		{
			return resPoint;
			break;
		}
		openQueue.pop();
	}
	return NULL;
}

//参数：当前节点，下一节点
bool Astar::isCanreach(const Point* point, const Point* target, bool isIgnoreCorner) const
{
	//如果点与当前节点重合、超出地图、是障碍物、或者在关闭列表中，返回false 
	if (target->x<0 || target->x>maze.size() - 1					//x的范围不超过地图范围
		|| target->y<0 && target->y>maze[0].size() - 1				//y的范围不超过地图范围
		|| maze[target->x][target->y] == 1							//下一节点不为障碍节点
		|| target->x == point->x && target->y == point->y			//下一节点不与当前节点重合
		|| isInList(closeList, target))								//下一节点没有处于closeList节点中
		
		return false;
	else
	{
		if (abs(point->x - target->x) + abs(point->y - target->y) == 1) //非斜角可以 
			return true;
		else
		{
			//斜对角要判断是否绊住 
			if (maze[point->x][target->y] == 0 && maze[target->x][point->y] == 0)
				return true;
			else
				return isIgnoreCorner;
		}
	}
}

std::vector<Point*> Astar::getSurroundPoints(const Point* point, bool isIgnoreCorner) const
{
	std::vector<Point*> surroundPoints;

	for (int x = point->x - 1; x <= point->x + 1; x++)
		for (int y = point->y - 1; y <= point->y + 1; y++)
			if (isCanreach(point, new Point(x, y), isIgnoreCorner))
				surroundPoints.push_back(new Point(x, y));

	return surroundPoints;
}