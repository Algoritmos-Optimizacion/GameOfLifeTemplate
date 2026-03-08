
#include "GameOfLife.h"


void GameOfLife::Initialize(int Rows, int Columns)
{
	if (Rows < 0 || Columns < 0)
	{
		// Error
		return;
	}
	
	NumRows = Rows;
	NumColumns = Columns;
	CurrentFrame = std::vector<std::vector<int>>(Rows, std::vector<int>(Columns, 0));
}

void GameOfLife::Deinitialize()
{
}

void GameOfLife::SetCell(int Row, int Column, bool bValue)
{
	if (Row < 0 || Row >= NumRows || Column < 0 || Column >= NumColumns)
	{
		// Error
		return;
	}

	CurrentFrame[Row][Column] = bValue;
}

void GameOfLife::FillRandomly()
{
	for (int Column = 0; Column < NumColumns; ++Column)
	{
		for (int Row = 0; Row < NumRows; ++Row)
		{
			CurrentFrame[Row][Column] = rand() & 1;
		}
	}
}


void GameOfLife::NextStep()
{
	std::vector<std::vector<int>> NewFrame = CurrentFrame;
	for (int Column = 0; Column < NumColumns; ++Column)
	{
		for (int Row = 0; Row < NumRows; ++Row)
		{
			NewFrame[Row][Column] = CurrentFrame[Row][Column] != 0 ? 0 : 1;
		}
	}

	CurrentFrame = NewFrame;
}

