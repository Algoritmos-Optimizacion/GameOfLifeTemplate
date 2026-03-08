#pragma once

#include <vector>

class GameOfLife
{

public:
	void Initialize(int Rows, int Columns);
	void Deinitialize();

	void FillRandomly();

	void NextStep();

	int GetNumRows() const { return NumRows; }
	int GetNumColumns() const { return NumColumns; }

	void SetCell(int Row, int Column, bool bValue);
	bool GetCell(int Row, int Column) const
	{
		if (Row < 0 || Row >= NumRows || Column < 0 || Column >= NumColumns)
		{
			// Error
			return false;
		}

		return CurrentFrame[Row][Column] != 0;
	}

protected:
	int NumRows = 0;
	int NumColumns = 0;
	std::vector<std::vector<int>> CurrentFrame;

};