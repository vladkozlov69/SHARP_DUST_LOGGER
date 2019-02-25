#ifndef MOVINGAVERAGE_H
#define MOVINGAVERAGE_H

#include <Arduino.h>

class MovingAverage
{
public:
	MovingAverage(int size)
	{
		_size = size;
		values = (double*) malloc(size * sizeof(double));
		for(int i = 0; i < size; i++) *(values + i) = 0;
	}
	~MovingAverage()
	{
		free(values);
	}

	double process(double value)
	{
		*(values + _current) = value;
		_current = (_current + 1) % _size;
		if (_count < _size) _count++;

		double sum = 0;
		for (int i = 0; i < _count; i++)
		{
			sum = sum + *(values + i);
		}

		return sum /_count;
	}
private:
	double * values;
	int _size;
	int _count = 0;
	int _current = 0;
};


#endif
