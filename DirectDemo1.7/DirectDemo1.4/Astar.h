#pragma once
#include<vector>
#include<list>
#include<queue>
const int kCost1 = 20;	//ֱ��һ��Ȩֵ
const int kCost2 = 28;	//б��һ��Ȩֵ

struct Point
{
	int x, y;			//�������
	int F, G, H;		//���ۺ���
	Point* parent;		//���ڵ�
	Point(int _x, int _y) :x(_x), y(_y), F(0), G(0), H(0), parent(NULL)			//������ʼ��
	{

	}
};
struct greaterr //��д�º���
	{
	    bool operator() (Point* a, Point* b)
		   {
		         return a->F > b->F; //С����
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
	bool isCanreach(const Point* point, const Point* target, bool isIgnoreCorner) const; //�ж�ĳ���Ƿ����������һ���ж� 
	Point* isInList(const std::list<Point*>& list, const Point* point) const;	//�жϿ������߹ر��б����Ƿ����ĳ�� 
	Point* isInQueue( std::priority_queue<Point*, std::vector<Point*>, greaterr>, const Point* point) const;	//�жϿ������߹ر��б����Ƿ����ĳ�� 

	Point* getLeastFpoint(); //�ӿ����б��з���Fֵ��С�Ľڵ� 
	//����FGHֵ 
	int calcG(Point* temp_start, Point* point);
	int calcH(Point* point, Point* end);
	int calcF(Point* point);
private:
	std::vector<std::vector<int>> maze;

	std::list<Point*> openList;  //�����б� 
	std::priority_queue<Point*, std::vector<Point*>, greaterr> openList_prior_queue;
	std::list<Point*> closeList; //�ر��б� 
};