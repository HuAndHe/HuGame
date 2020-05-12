#include <math.h> 
#include "Astar.h" 

void Astar::InitAstar(std::vector<std::vector<int>>& _maze)
{
	maze = _maze;
}

int Astar::calcG(Point* temp_start, Point* point)
{
	int extraG = (abs(point->x - temp_start->x) + abs(point->y - temp_start->y)) == 1 ? kCost1 : kCost2;
	int parentG = point->parent == NULL ? 0 : point->parent->G; //����ǳ�ʼ�ڵ㣬���丸�ڵ��ǿ� 
	return parentG + extraG;
}

int Astar::calcH(Point* point, Point* end)
{
	//�ü򵥵�ŷ����þ������H�����H�ļ����ǹؼ������кܶ��㷨��û�����о�^_^ 
	return int(sqrt((double)(end->x - point->x) * (double)(end->x - point->x) + (double)(end->y - point->y) * (double)(end->y - point->y)) * kCost1);
}

int Astar::calcF(Point* point)
{
	return point->G + point->H;
}
//b����Openlist�õ���СȨֵ�Ľڵ�
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
	//if (!openList_prior_queue.empty())		//�����б������Ϊ������Ҫ�����
	//{		
	//	openList_prior_queue.clear();
	//	closeList.clear();
	//}
	closeList.clear();
	//openList.push_back(new Point(startPoint.x, startPoint.y)); //�������,��������һ���ڵ㣬������� 
	openList_prior_queue.push(new Point(startPoint.x, startPoint.y));
	while (!openList_prior_queue.empty())
	{
		auto curPoint = getLeastFpoint(); //�ҵ�Fֵ��С�ĵ� 
		//openList.remove(curPoint); //�ӿ����б���ɾ�� 
		openList_prior_queue.pop();
		closeList.push_back(curPoint); //�ŵ��ر��б� 
		//1,�ҵ���ǰ��Χ�˸����п���ͨ���ĸ��� 
		auto surroundPoints = getSurroundPoints(curPoint, isIgnoreCorner);
		for (auto& target : surroundPoints)
		{
			//2,��ĳһ�����ӣ���������ڿ����б��У����뵽�����б����õ�ǰ��Ϊ�丸�ڵ㣬����F G H 
			if (!isInQueue(openList_prior_queue, target))
			{
				target->parent = curPoint;

				target->G = calcG(curPoint, target);
				target->H = calcH(target, &endPoint);
				target->F = calcF(target);

				//openList.push_back(target);
				openList_prior_queue.push(target);
			}
			//3����ĳһ�����ӣ����ڿ����б��У�����Gֵ, �����ԭ���Ĵ�, ��ʲô������, �����������ĸ��ڵ�Ϊ��ǰ��,������G��F 
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
				return resPoint; //�����б���Ľڵ�ָ�룬��Ҫ��ԭ�������endpointָ�룬��Ϊ��������� 
			}
				
		}
	}

	return NULL;
}

std::list<Point*> Astar::GetPath(Point& startPoint, Point& endPoint, bool isIgnoreCorner)
{
	Point* result = findPath(startPoint, endPoint, isIgnoreCorner);
	std::list<Point*> path;
	//����·�������û�ҵ�·�������ؿ����� 
	while (result)
	{
		path.push_front(result);
		result = result->parent;
	}
	return path;
}

Point* Astar::isInList(const std::list<Point*>& list, const Point* point) const
{
	//�ж�ĳ���ڵ��Ƿ����б��У����ﲻ�ܱȽ�ָ�룬��Ϊÿ�μ����б����¿��ٵĽڵ㣬ֻ�ܱȽ����� 
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

//��������ǰ�ڵ㣬��һ�ڵ�
bool Astar::isCanreach(const Point* point, const Point* target, bool isIgnoreCorner) const
{
	//������뵱ǰ�ڵ��غϡ�������ͼ�����ϰ�������ڹر��б��У�����false 
	if (target->x<0 || target->x>maze.size() - 1					//x�ķ�Χ��������ͼ��Χ
		|| target->y<0 && target->y>maze[0].size() - 1				//y�ķ�Χ��������ͼ��Χ
		|| maze[target->x][target->y] == 1							//��һ�ڵ㲻Ϊ�ϰ��ڵ�
		|| target->x == point->x && target->y == point->y			//��һ�ڵ㲻�뵱ǰ�ڵ��غ�
		|| isInList(closeList, target))								//��һ�ڵ�û�д���closeList�ڵ���
		
		return false;
	else
	{
		if (abs(point->x - target->x) + abs(point->y - target->y) == 1) //��б�ǿ��� 
			return true;
		else
		{
			//б�Խ�Ҫ�ж��Ƿ��ס 
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