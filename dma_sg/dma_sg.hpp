/*
 * Author: Michal Fularz
 */


#ifndef DMA_SG_HPP
#define DMA_SG_HPP

#include <xaxidma.h>		// for axidma types
#include "aligned_mem_functions.h"
#include "TransferControl.hpp"

class dmaSG
{
private:
	XAxiDma axiDma;

	TransferControl Tx;
	TransferControl Rx;

	u32 RxBufferSizeInBytes;

	u8** RxBuffers;

	static const u32 ALIGNE_VALUE = 64;


	// special flags for interrupt testing
	/*
	 * Flags interrupt handlers use to notify the application context the events.
	 */
	volatile int TxDone;
	volatile int RxDone;
	volatile int Error;

	static const u32 RESET_TIMEOUT_COUNTER = 100;

	void Init_DMA_lowlevel(u32 AXI_DMA_DEVICE_ID);

	void Init_Tx(void);

	void Init_Rx(void);

	void AttachRxBuffers(void);

	void PrepareRxBuffers(void);

	// TODO - add function that returns RX buffers and then create new ones and attach them to DMA for further processing

	void ReleaseRxBuffers(void);

	int SetupInterrupts(void);

	// TODO - after interupts are working clean up and merge their code with old WaitForXXComplete and FreeProcessedXX methods
	void TxCallback(XAxiDma_BdRing * TxRingPtr);
	void RxCallback(XAxiDma_BdRing * RxRingPtr);

public:


	void Init(u32 AXI_DMA_DEVICE_ID, u32 numberOfTxBuffers, u32 numberOfRxBuffers, u32 sizeInBytesOfEachRxBuffer);

	void Send(u8* dataToSend, u32 sizeOfDataToSendInBytes);
	void Send2(u8* dataToSend, u32 numberOfPacketsToSend, u32 sizeOfDataToSendInBytes);

	void WaitForTxComplete(void);
	void FreeProcessedTx(void);

	void WaitForTxCompleteAndFree(void);

	void WaitForRxComplete(void);
	void FreeProcessedRx(void);

	void WaitForRxCompleteAndFree(void);

	void TxIntrHandler(void *Callback);
	void RxIntrHandler(void *Callback);

	XAxiDma* GetAxiDmaInstancePtr(void)
	{
		return &(this->axiDma);
	}
	u8** GetRxBuffers(void)
	{
		return this->RxBuffers;
	}
	u32 GetSizeOfRxBuffers(void)
	{
		return this->RxBufferSizeInBytes;
	}

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
