/*
 * Author: Michal Fularz
 */


#ifndef DMA_SG_HPP
#define DMA_SG_HPP

#include <xaxidma.h>		// for axidma types
#include "aligned_mem_functions.h"

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

class dmaSG
{
private:
	XAxiDma axiDma;

	TransferControl Tx;
	TransferControl Rx;

	u32 RxBufferSizeInBytes;

	u8** RxBuffers;

	static const u32 ALIGNE_VALUE = 64;

	void Init_DMA_lowlevel(u32 AXI_DMA_DEVICE_ID);

	void Init_Tx(void);

	void Init_Rx(void);

	void AttachRxBuffers(void);

	void PrepareRxBuffers(void);

	// TODO - add function that returns RX buffers and then create new ones and attach them to DMA for further processing

	void ReleaseRxBuffers();

public:
	XAxiDma* GetAxiDmaInstancePtr(void);

	void Init(u32 AXI_DMA_DEVICE_ID, u32 numberOfTxBuffers, u32 numberOfRxBuffers, u32 sizeInBytesOfEachRxBuffer);

	void Send(u8* dataToSend, u32 sizeOfDataToSendInBytes);

	void WaitForTxComplete(void);
	void FreeProcessedTx(void);

	void WaitForTxCompleteAndFree(void);

	void WaitForRxComplete(void);
	void FreeProcessedRx(void);

	void WaitForRxCompleteAndFree(void);

	u8** GetRxBuffers(void);
	u32 GetSizeOfRxBuffers(void);

	dmaSG(void)
	{
		this->RxBufferSizeInBytes = 0;
		this->RxBuffers = NULL;

		this->Tx.Init(0);
		this->Rx.Init(0);
	}

	~dmaSG(void)
	{
		// free attached buffers
		ReleaseRxBuffers();

		this->Tx.~TransferControl();
		this->Rx.~TransferControl();
	}
};

#endif /* DMA_SG_HPP */
