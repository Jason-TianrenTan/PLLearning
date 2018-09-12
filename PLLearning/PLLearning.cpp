// PLLearning.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "DataInstance.h"

int* labels;
double DistMatrix[3 * m][3 * m] = { 0 };//矩阵表
vector<DataInstance> TrainingData;
unordered_map<int, int> GMatrix[q + 1];
double Gamma[m][q + 1];
vector<int*> bSVec;


/* d:实例空间维度
* q:label集合大小
* m:实例数目
*/

double GetDistance(const DataInstance& d1, const DataInstance& d2)
{
	double dist = 0;
	for (int i = 0; i < d; i++)
		dist += (d1.element[i] - d2.element[i]) * (d1.element[i] - d2.element[i]);
	return sqrt(dist);
}

class SortingItem
{
public:
	int index;
	double dist;
	SortingItem(int i, double d)
	{
		index = i;
		dist = d;
	}
};

bool item_compare(const SortingItem& item1, const SortingItem& item2)
{
	return item1.dist < item2.dist;
}

int* getKNN(int index)
{
	int* result = new int[K];
	vector<SortingItem> items;
	for (int i = 0; i < TrainingData.size(); i++)
	{
		if (i == index)
			continue;
		items.emplace_back(i, DistMatrix[index][i]);
	}
	sort(items.begin(), items.end(), item_compare);
	for (int i = 0; i < K; i++)
		result[i] = items[i].index;
	return result;
}

void GetKDist()
{
	for (int i = 0; i < TrainingData.size(); i++)
	{
		for (int j = 0; j < i; j++) 
			DistMatrix[i][j] = DistMatrix[j][i] = GetDistance(TrainingData[i], TrainingData[j]);
	}

	bSVec.clear();
	for (int i = 0; i < TrainingData.size(); i++)
	{
		int* bS = new int[q + 1];
		for (int j = 1; j <= q; j++)
			bS[j] = 0;
		for (int k=0;k<TrainingData[i].type.size();k++)
			bS[TrainingData[i].type[k]] = 1;
		bSVec.push_back(bS);
	}
	
}



void init()
{
	/*
	* Type of glass: (class attribute)
	-- 1 building_windows_float_processed
	-- 2 building_windows_non_float_processed
	-- 3 vehicle_windows_float_processed
	-- 4 vehicle_windows_non_float_processed (none in this database)
	-- 5 containers
	-- 6 tableware
	-- 7 headlamps
	*/
	labels = new int[q + 1];
	ifstream fin("dataset.txt");
	for (int i = 0; i < m; i++)
	{
		DataInstance dataInstance;
		fin >> dataInstance.index;
		for (int j = 0; j < d; j++)
			fin >> dataInstance.element[j];
		int type;
		fin >> type;
		dataInstance.type = vector<int>{ type };
		TrainingData.push_back(dataInstance);
	}
	//计算每一个xi的k近邻
	GetKDist();
	for (int i = 0; i < m; i++)
	{
		int* Nx = getKNN(i);//KNN数组
		double total = 0;
		for (int p = 0; p < K; p++)
			total += DistMatrix[i][Nx[p]];
		for (int index = 1; index <= q; index++)
		{
			Gamma[i][index] = 0;
			for (int knn_index = 0; knn_index < K; knn_index++)
			{
				int j = Nx[knn_index];
				double top = DistMatrix[i][j];
				
				Gamma[i][index] += (1 - top / total) * bSVec[j][index];
			}
		}

		//寻找最大lambda_j
		int maxIndex = 0;
		double maxGamma = -1;
		for (int index = 1; index <= q; index++)
		{
			if (Gamma[i][index] > maxGamma)
			{
				maxGamma = Gamma[i][index];
				maxIndex = index;
			}
		}

		GMatrix[maxIndex].insert(pair<int, int>(i, maxIndex));
	}

}

class GammaRow
{
public:
	int row_index;
	double gamma_value;
	GammaRow(int index, double val)
	{
		row_index = index;
		gamma_value = val;
	}
};

bool gamma_compare(const GammaRow& row1, const GammaRow& row2)
{
	return row1.gamma_value > row2.gamma_value;
}

void printGMatrices()
{
	for (int j = 1; j <= q; j++)
	{
		cout << "G[" << j << "]" << endl;
		for (unordered_map<int, int>::iterator iter = GMatrix[j].begin(); iter != GMatrix[j].end(); iter++)
			cout << "("<<iter->first << "," << iter->second << ") ";
		cout << endl;
	}
}

void adjustGMatrix()
{ 
	cout << "Before: " << endl;
	printGMatrices();
	for (int j = 1; j <= q; j++)
	{
		vector<int> instances;
		for (int i = 0; i < m; i++)
		{
			bool contains = false;
			for (unordered_map<int, int>::iterator iter = GMatrix[j].begin(); iter != GMatrix[j].end(); iter++)
			{
				if (iter->first == i && iter->second == j)
				{
					contains = true;
					break;
				}
			}
			if (!contains)
				instances.push_back(i);
		}
		//对Gamma矩阵第j列进行排序
		vector<GammaRow> rows;
		for (int i = 0; i < m; i++)
			rows.emplace_back(i, Gamma[i][j]);
		sort(rows.begin(), rows.end(), gamma_compare);
		//补足止Tou
		int pointer = 0;
		while (GMatrix[j].size() < Tou)
		{
			int index = rows[pointer].row_index;
			int _j = GMatrix[j][index];
			if (GMatrix[_j].size() - 1 > Tou)
			{
				for (unordered_map<int, int>::iterator iter = GMatrix[_j].begin(); iter != GMatrix[_j].end(); iter++)
				{
					if (iter->first == index && iter->second == _j)
					{
						iter = GMatrix[_j].erase(iter);
						break;
					}
				}
				rows.erase(rows.begin());
				vector<int>::iterator pos = std::find(instances.begin(), instances.end(), index);
				instances.erase(pos);
				GMatrix[j].insert(pair<int, int>(index, _j));
			}
			else pointer++;
		}

	}

	cout << "After:" << endl;
	printGMatrices();
}

DataInstance GenerateByROS(int j)
{
	srand(unsigned(time(0)));
	int index = rand() % GMatrix[j].size();
	DataInstance nInstanz;
	nInstanz.type = TrainingData[index].type;
	for (int i = 0; i < d; i++)
		nInstanz.element[i] = TrainingData[GMatrix[j][index]].element[i];
	return nInstanz;
}

DataInstance GenerateBySMOTE(int j, int (&omega)[d], int& index, int &r)
{
	srand(unsigned(time(0)));
	index = GMatrix[j][rand() % GMatrix[j].size()];
	DataInstance nInstanz;
	GetKDist();
	int* k_indices = getKNN(index);
	r = k_indices[rand() % K];
	//生成新的x_i
	for (int i = 0; i < d; i++)
	{
		//i = a
		omega[i] = rand() % 2;
		nInstanz.element[i] = TrainingData[index].element[i]
			+ (TrainingData[r].element[i] - TrainingData[index].element[i]) * omega[i];
	}
	nInstanz.type = TrainingData[index].type;
	return nInstanz;
}

void ReplenishTrainingSet(int algorithm)
{
	int maxSize = 0, maxJ = 0;
	for (int j = 1; j <= q; j++)
	{
		if (GMatrix[j].size() > maxSize)
		{
			maxSize = GMatrix[j].size();
			maxJ = j;
		}
	}
	for (int j = 1; j <= q; j++)
	{
		if (j == maxJ)
			continue;
		int count = 0;
		while (count < GMatrix[maxJ].size() - GMatrix[j].size())
		{
			if (algorithm == ROS)
				TrainingData.push_back(GenerateByROS(j));
			else if (algorithm == SMOTE) 
			{
				int omega[d], r, index;
				TrainingData.push_back(GenerateBySMOTE(j, omega, index, r));
			}
			else if (algorithm == POS)
			{
				int omega[d], r, index;
				DataInstance nInstanz = GenerateBySMOTE(j, omega, index, r);
				double multiResult = 0;
				for (int i = 0; i < d; i++)
					multiResult += (TrainingData[r].element[i] - TrainingData[index].element[i]) * omega[i];
				vector<int> LabelS;//Si_hat
				for (int k = 1; k <= q; k++)
				{
					double diff = bSVec[r][k] - bSVec[index][k];
					double val = bSVec[index][k] + diff * multiResult;
					if (val > 0)
						LabelS.push_back(k);
				}
				nInstanz.type = LabelS;
				TrainingData.push_back(nInstanz);
			}
			count++;
		}
	}

	cout << "Result:" << endl;
	printGMatrices();

}




int main()
{
	init();
	adjustGMatrix();

	ReplenishTrainingSet(ROS);
	return 0;
}