/*
 * Author: Michal Fularz
 */

#ifndef TRANSFER_CONTROL_HPP
#define TRANSFER_CONTROL_HPP

#include <xaxidma.h>		// for axidma types

class TransferControl
{
private:
	u32 numberOfBds;
	s32 processedBdCount;
	s32 numberOfEnquedOperations;

public:
	u32* BdSpacePtr;
	XAxiDma_BdRing* RingPtr;
	XAxiDma_Bd* BdPtr;

	void Init(u32 numberOfBuffers)
	{
		this->numberOfBds = numberOfBuffers;
		this->processedBdCount = 0;
		this->numberOfEnquedOperations = 0;

		this->BdSpacePtr = NULL;
		this->RingPtr = NULL;
		this->BdPtr = NULL;
	}

	u32 GetNumberOfBds(void)
	{
		return this->numberOfBds;
	}

	void ClearProcessedData(void)
	{
		this->processedBdCount = 0;
		this->numberOfEnquedOperations = 0;
	}

	bool AreAllOperationsFinished(void)
	{
		bool status = true;

		if(this->processedBdCount < this->numberOfEnquedOperations)
		{
			status = false;
		}

		return status;
	}

	void IncreaseNumberOfProcessedBds(s32 valueToIncrease)
	{
		this->processedBdCount += valueToIncrease;
	}

	s32 GetNumberOfProcessedBds(void)
	{
		return this->processedBdCount;
	}

	void IncreaseNumberOfEnquedOperations(void)
	{
		this->numberOfEnquedOperations++;
	}

	TransferControl(void)
	{
		Init(0);
	}

	~TransferControl(void)
	{
		if(this->BdSpacePtr != NULL)
		{
			aligned_free(this->BdSpacePtr);
		}
	}
};

#endif /* TRANSFER_CONTROL_HPP */
