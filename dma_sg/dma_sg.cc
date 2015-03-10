/*
 * Author: Michal Fularz
 */

#include "dma_sg.hpp"

#include <xaxidma.h>		// for axidma types
#include "aligned_mem_functions.h"

XAxiDma* dmaSG::GetAxiDmaInstancePtr(void)
{
	return &(this->axiDma);
}

u8** dmaSG::GetRxBuffers(void)
{
	return this->RxBuffers;
}

u32 dmaSG::GetSizeOfRxBuffers(void)
{
	return this->RxBufferSizeInBytes;
}

void dmaSG::Init_DMA_lowlevel(u32 AXI_DMA_DEVICE_ID)
{
	XAxiDma* AxiDmaPtr = &(this->axiDma);

	int status;
	XAxiDma_Config *Config;

	Config = XAxiDma_LookupConfig(AXI_DMA_DEVICE_ID);
	if (!Config) {
		xil_printf("No config found for %d\r\n", AXI_DMA_DEVICE_ID);
	}

	/* Initialize DMA engine */
	status = XAxiDma_CfgInitialize(AxiDmaPtr, Config);
	if (status != XST_SUCCESS) {
		xil_printf("Initialization failed %d\r\n", status);
	}

	if(!XAxiDma_HasSg(AxiDmaPtr)) {
		xil_printf("Device configured as Simple mode \r\n");
	}
}

void dmaSG::Init_Tx(void)
{
	XAxiDma* AxiDmaInstPtr = &(this->axiDma);
	int BdCount = this->Tx.GetNumberOfBds();

	int status;

	this->Tx.RingPtr = XAxiDma_GetTxRing(AxiDmaInstPtr);
	XAxiDma_BdRing* TxRingPtr = this->Tx.RingPtr;

	/* Disable all TX interrupts before TxBD space setup */
	XAxiDma_BdRingIntDisable(TxRingPtr, XAXIDMA_IRQ_ALL_MASK);

	/* Set TX delay and coalesce */
	int Delay = 0;
	int Coalesce = 1;
	XAxiDma_BdRingSetCoalesce(TxRingPtr, Coalesce, Delay);

	/* Setup TxBD space  */
	u32 numberOfBytesRequireedToCreateBdList = XAxiDma_BdRingMemCalc(XAXIDMA_BD_MINIMUM_ALIGNMENT, BdCount);
	u32* TxBdSpacePtr = (u32*) aligned_malloc(numberOfBytesRequireedToCreateBdList, XAXIDMA_BD_MINIMUM_ALIGNMENT);

	u32 TxBdSpaceAddress = (u32)&TxBdSpacePtr[0];

	status = XAxiDma_BdRingCreate(TxRingPtr, TxBdSpaceAddress, TxBdSpaceAddress, XAXIDMA_BD_MINIMUM_ALIGNMENT, BdCount);
	if (status != XST_SUCCESS)
	{
		xil_printf("failed create BD ring in txsetup\r\n");
	}

	/*
	 * We create an all-zero BD as the template.
	 */
	XAxiDma_Bd BdTemplate;
	XAxiDma_BdClear(&BdTemplate);

	status = XAxiDma_BdRingClone(TxRingPtr, &BdTemplate);
	if (status != XST_SUCCESS)
	{
		xil_printf("failed bdring clone in txsetup %d\r\n", status);
	}

	/* Start the TX channel */
	status = XAxiDma_BdRingStart(TxRingPtr);
	if (status != XST_SUCCESS)
	{
		xil_printf("failed start bdring txsetup %d\r\n", status);
	}

	this->Tx.BdSpacePtr = TxBdSpacePtr;
}

void dmaSG::Init_Rx(void)
{
	XAxiDma* AxiDmaInstPtr = &(this->axiDma);
	int BdCount = this->Rx.GetNumberOfBds();

	int status;

	this->Rx.RingPtr = XAxiDma_GetRxRing(AxiDmaInstPtr);
	XAxiDma_BdRing* RxRingPtr = this->Rx.RingPtr;

	/* Disable all RX interrupts before RxBD space setup */
	XAxiDma_BdRingIntDisable(RxRingPtr, XAXIDMA_IRQ_ALL_MASK);

	/* Setup Rx BD space */
	u32 numberOfBytesRequireedToCreateBdList = XAxiDma_BdRingMemCalc(XAXIDMA_BD_MINIMUM_ALIGNMENT, BdCount);
	u32* RxBdSpacePtr = (u32*) aligned_malloc(numberOfBytesRequireedToCreateBdList, XAXIDMA_BD_MINIMUM_ALIGNMENT);

	u32 RxBdSpaceAddress = (u32)&RxBdSpacePtr[0];

	status = XAxiDma_BdRingCreate(RxRingPtr, RxBdSpaceAddress, RxBdSpaceAddress, XAXIDMA_BD_MINIMUM_ALIGNMENT, BdCount);

	if (status != XST_SUCCESS)
	{
		xil_printf("RX create BD ring failed %d\r\n", status);
	}

	/*
	 * Setup an all-zero BD as the template for the Rx channel.
	 */
	XAxiDma_Bd BdTemplate;
	XAxiDma_BdClear(&BdTemplate);

	status = XAxiDma_BdRingClone(RxRingPtr, &BdTemplate);
	if (status != XST_SUCCESS)
	{
		xil_printf("RX clone BD failed %d\r\n", status);
	}

	this->Rx.BdSpacePtr = RxBdSpacePtr;
}

// TODO - co z BdPtr - czy nie powinno byæ pole z klasy?
void dmaSG::AttachRxBuffers(void)
{
	u32 RX_numberOfBytes = this->RxBufferSizeInBytes;

	int status;

	XAxiDma_BdRing* RxRingPtr = this->Rx.RingPtr;

	/* Attach buffers to RxBD ring so we are ready to receive packets */
	u32 FreeBdCount = XAxiDma_BdRingGetFreeCnt(RxRingPtr);

	XAxiDma_Bd *BdPtr;
	status = XAxiDma_BdRingAlloc(RxRingPtr, FreeBdCount, &BdPtr);
	if (status != XST_SUCCESS)
	{
		xil_printf("RX alloc BD failed %d\r\n", status);
	}

	XAxiDma_Bd* BdCurPtr = BdPtr;
	for (u32 i = 0; i < FreeBdCount; ++i)
	{
		u32 RxBufferPtr = (u32) &(this->RxBuffers[i][0]);

		status = XAxiDma_BdSetBufAddr(BdCurPtr, RxBufferPtr);
		if (status != XST_SUCCESS)
		{
			xil_printf("Set buffer addr %x on BD %x failed %d\r\n", RxBufferPtr, (unsigned int)BdCurPtr, status);
		}


		status = XAxiDma_BdSetLength(BdCurPtr, RX_numberOfBytes, RxRingPtr->MaxTransferLen);
		if (status != XST_SUCCESS)
		{
			xil_printf("Rx set length %d on BD %x failed %d\r\n", RX_numberOfBytes, (unsigned int)BdCurPtr, status);
		}

		/* Receive BDs do not need to set anything for the control
		 * The hardware will set the SOF/EOF bits per stream status
		 */
		XAxiDma_BdSetCtrl(BdCurPtr, 0);
		XAxiDma_BdSetId(BdCurPtr, RxBufferPtr);

		BdCurPtr = XAxiDma_BdRingNext(RxRingPtr, BdCurPtr);
	}

	status = XAxiDma_BdRingToHw(RxRingPtr, FreeBdCount, BdPtr);
	if (status != XST_SUCCESS)
	{
		xil_printf("RX submit hw failed %d\r\n", status);
	}

	/* Start RX DMA channel */
	status = XAxiDma_BdRingStart(RxRingPtr);
	if (status != XST_SUCCESS)
	{
		xil_printf("RX start hw failed %d\r\n", status);
	}
}

void dmaSG::PrepareRxBuffers(void)
{
	u32 sizeInBytesOfEachRxBuffer = this->RxBufferSizeInBytes;
	u32 numberOfRxBuffers = this->Rx.GetNumberOfBds();

	this->RxBuffers = (u8**) aligned_malloc(numberOfRxBuffers * sizeof(u8*), ALIGNE_VALUE);

	for(u32 i=0; i<numberOfRxBuffers; ++i)
	{
		this->RxBuffers[i] = (u8*) aligned_malloc(sizeInBytesOfEachRxBuffer, ALIGNE_VALUE);
	}
}

void dmaSG::ReleaseRxBuffers()
{
	u32 numberOfRxBuffers = this->Rx.GetNumberOfBds();

	for(u32 i=0; i<numberOfRxBuffers; ++i)
	{
		aligned_free(this->RxBuffers[i]);
	}

	aligned_free(this->RxBuffers);
}

void dmaSG::Init(u32 AXI_DMA_DEVICE_ID, u32 numberOfTxBuffers, u32 numberOfRxBuffers, u32 sizeInBytesOfEachRxBuffer)
{
	this->Tx.Init(numberOfTxBuffers);
	this->Rx.Init(numberOfRxBuffers);
	this->RxBufferSizeInBytes = sizeInBytesOfEachRxBuffer;

	Init_DMA_lowlevel(AXI_DMA_DEVICE_ID);

	Init_Tx();
	Init_Rx();

	PrepareRxBuffers();

	AttachRxBuffers();
}

void dmaSG::Send(u8* dataToSend, u32 sizeOfDataToSendInBytes)
{
	// TODO: remove number of packets (used when the input data was partitioned into smaller transfers)
	u32 numberOfPacketsSend = 1;
	this->Tx.IncreaseNumberOfEnquedOperations();
	this->Rx.IncreaseNumberOfEnquedOperations();

	int status;
	XAxiDma_BdRing* TxRingPtr = this->Tx.RingPtr;

	/* Allocate a BD */
	status = XAxiDma_BdRingAlloc(TxRingPtr, 1, &(this->Tx.BdPtr));
	if (status != XST_SUCCESS) {
		xil_printf("Unable to allocate Tx buffers\r\n");
	}

	XAxiDma_Bd* CurBdPtr = this->Tx.BdPtr;
	for(u32 i=0; i<numberOfPacketsSend; ++i)
	{
		const u32 TX_address = (u32) &dataToSend[0];
		/* Set up the BD using the information of the packet to transmit */
		status = XAxiDma_BdSetBufAddr(CurBdPtr, TX_address);
		if (status != XST_SUCCESS) {
			xil_printf("Tx set buffer addr %x on BD %x failed %d\r\n",
					TX_address, (unsigned int)CurBdPtr, status);
		}

		const u32 TX_numberOfBytes = sizeOfDataToSendInBytes;
		status = XAxiDma_BdSetLength(CurBdPtr, TX_numberOfBytes,
					TxRingPtr->MaxTransferLen);
		if (status != XST_SUCCESS) {
			xil_printf("Tx set length %d on BD %x failed %d\r\n",
					TX_numberOfBytes, (unsigned int)CurBdPtr, status);
		}

		u32 CrBits = 0;
		if (i == 0)
		{
			CrBits |= XAXIDMA_BD_CTRL_TXSOF_MASK;
		}

		if (i == (numberOfPacketsSend - 1))
		{
			CrBits |= XAXIDMA_BD_CTRL_TXEOF_MASK;
			XAxiDma_BdSetCtrl(CurBdPtr, XAXIDMA_BD_CTRL_TXEOF_MASK);
		}

		//	/* For single packet, both SOF and EOF are to be set
		//	 */
		//	XAxiDma_BdSetCtrl(BdPtr, XAXIDMA_BD_CTRL_TXEOF_MASK |
		//						XAXIDMA_BD_CTRL_TXSOF_MASK);

		XAxiDma_BdSetCtrl(CurBdPtr, CrBits);

		XAxiDma_BdSetId(CurBdPtr, TX_address);

		CurBdPtr = XAxiDma_BdRingNext(TxRingPtr, CurBdPtr);
	}

	/* Give the BD to DMA to kick off the transmission. */
	status = XAxiDma_BdRingToHw(TxRingPtr, numberOfPacketsSend, this->Tx.BdPtr);
	if (status != XST_SUCCESS) {
		xil_printf("Bringing Tx buffer to hw failed %d\r\n", status);
	}
}

void dmaSG::WaitForTxCompleteAndFree(void)
{
	/* Wait until all BD TX transaction is done */
	while ( !this->Tx.AreAllOperationsFinished() )
	{
		int ProcessedBdCount = XAxiDma_BdRingFromHw(this->Tx.RingPtr, XAXIDMA_ALL_BDS, &(this->Tx.BdPtr));

		/* Free all processed TX BDs for future transmission */
		int status = XAxiDma_BdRingFree(this->Tx.RingPtr, ProcessedBdCount, this->Tx.BdPtr);
		if (status != XST_SUCCESS) {
			xil_printf("Failed to free %d tx BDs %d\r\n",
					ProcessedBdCount, status);
		}

		this->Tx.IncreaseNumberOfProcessedBds(ProcessedBdCount);
	}

	this->Tx.ClearProcessedData();
}

void dmaSG::WaitForTxComplete(void)
{
	/* Wait until all BD TX transaction is done */
	while ( !this->Tx.AreAllOperationsFinished() )
	{
		int ProcessedBdCount = XAxiDma_BdRingFromHw(this->Tx.RingPtr, XAXIDMA_ALL_BDS, &(this->Tx.BdPtr));
		this->Tx.IncreaseNumberOfProcessedBds(ProcessedBdCount);
	}
}

void dmaSG::FreeProcessedTx(void)
{
	/* Free all processed TX BDs for future transmission */
	int status = XAxiDma_BdRingFree(this->Tx.RingPtr, this->Tx.GetNumberOfProcessedBds(), this->Tx.BdPtr);
	if (status != XST_SUCCESS) {
		xil_printf("Failed to free %d tx BDs %d\r\n",
				this->Tx.GetNumberOfProcessedBds(), status);
	}

	this->Tx.ClearProcessedData();
}

void dmaSG::WaitForRxCompleteAndFree(void)
{
	/* Wait until alle the data has been received by the Rx channel */
	while ( !this->Rx.AreAllOperationsFinished() )
	{
		int ProcessedBdCount = XAxiDma_BdRingFromHw(this->Rx.RingPtr, XAXIDMA_ALL_BDS, &(this->Rx.BdPtr));

		/* Free all processed RX BDs for future transmission */
		int status = XAxiDma_BdRingFree(this->Rx.RingPtr, ProcessedBdCount, this->Rx.BdPtr);
		if (status != XST_SUCCESS) {
			xil_printf("Failed to free %d rx BDs %d\r\n",
					ProcessedBdCount, status);
		}

		/* Return processed BDs to RX channel so we are ready to receive new
		 * packets:
		 *    - Allocate all free RX BDs
		 *    - Pass the BDs to RX channel
		 */
		int FreeBdCount = XAxiDma_BdRingGetFreeCnt(this->Rx.RingPtr);

		status = XAxiDma_BdRingAlloc(this->Rx.RingPtr, FreeBdCount, &(this->Rx.BdPtr));
		if (status != XST_SUCCESS) {
			xil_printf("bd alloc failed\r\n");
		}

		status = XAxiDma_BdRingToHw(this->Rx.RingPtr, FreeBdCount, this->Rx.BdPtr);
		if (status != XST_SUCCESS) {
			xil_printf("Submit %d rx BDs failed %d\r\n", FreeBdCount, status);
		}

		this->Rx.IncreaseNumberOfProcessedBds(ProcessedBdCount);
	}

	this->Rx.ClearProcessedData();
}

// TODO: merge WaitForTx and WiatForRx as the functions are the same, appropriate TransferControl object should be passed (as reference)
void dmaSG::WaitForRxComplete(void)
{
	/* Wait until alle the data has been received by the Rx channel */
	while ( !this->Rx.AreAllOperationsFinished() )
	{
		int ProcessedBdCount = XAxiDma_BdRingFromHw(this->Rx.RingPtr, XAXIDMA_ALL_BDS, &(this->Rx.BdPtr));
		this->Rx.IncreaseNumberOfProcessedBds(ProcessedBdCount);
	}
}

void dmaSG::FreeProcessedRx(void)
{
	/* Free all processed RX BDs for future transmission */
	int status = XAxiDma_BdRingFree(this->Rx.RingPtr, this->Rx.GetNumberOfProcessedBds(), this->Rx.BdPtr);
	if (status != XST_SUCCESS) {
		xil_printf("Failed to free %d rx BDs %d\r\n",
				this->Rx.GetNumberOfProcessedBds(), status);
	}

	/* Return processed BDs to RX channel so we are ready to receive new
	 * packets:
	 *    - Allocate all free RX BDs
	 *    - Pass the BDs to RX channel
	 */
	int FreeBdCount = XAxiDma_BdRingGetFreeCnt(this->Rx.RingPtr);

	status = XAxiDma_BdRingAlloc(this->Rx.RingPtr, FreeBdCount, &(this->Rx.BdPtr));
	if (status != XST_SUCCESS) {
		xil_printf("bd alloc failed\r\n");
	}

	status = XAxiDma_BdRingToHw(this->Rx.RingPtr, FreeBdCount, this->Rx.BdPtr);
	if (status != XST_SUCCESS) {
		xil_printf("Submit %d rx BDs failed %d\r\n", FreeBdCount, status);
	}

	this->Rx.ClearProcessedData();
}

