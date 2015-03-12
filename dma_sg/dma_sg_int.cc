/*
 * Author: Michal Fularz
 */

#include "dma_sg.hpp"

#include <xaxidma.h>		// for axidma types
#include "aligned_mem_functions.h"

// special include for interrupts
#include "xscugic.h"
#include "xparameters.h"
#include "xil_exception.h"

// THIS CODE IS WORK IN PROGRESS
// currently I don't know how to merge interrupts with classes to be elegant

void TxIntrHandler2(XAxiDma_BdRing * TxRingPtr)
{

}

void RxIntrHandler2(XAxiDma_BdRing * RxRingPtr)
{

}

void dmaSG::TxCallback(XAxiDma_BdRing * TxRingPtr)
{
	int BdCount;
	u32 BdSts;
	XAxiDma_Bd *BdPtr;
	XAxiDma_Bd *BdCurPtr;
	int Status;
	int Index;

	/* Get all processed BDs from hardware */
	BdCount = XAxiDma_BdRingFromHw(TxRingPtr, XAXIDMA_ALL_BDS, &BdPtr);

	/* Handle the BDs */
	BdCurPtr = BdPtr;
	for (Index = 0; Index < BdCount; Index++) {

		/*
		 * Check the status in each BD
		 * If error happens, the DMA engine will be halted after this
		 * BD processing stops.
		 */
		BdSts = XAxiDma_BdGetSts(BdCurPtr);
		if ((BdSts & XAXIDMA_BD_STS_ALL_ERR_MASK) ||
		    (!(BdSts & XAXIDMA_BD_STS_COMPLETE_MASK))) {

			this->Error = 1;
			break;
		}

		/*
		 * Here we don't need to do anything. But if a RTOS is being
		 * used, we may need to free the packet buffer attached to
		 * the processed BD
		 */

		/* Find the next processed BD */
		BdCurPtr = XAxiDma_BdRingNext(TxRingPtr, BdCurPtr);
	}

	/* Free all processed BDs for future transmission */
	Status = XAxiDma_BdRingFree(TxRingPtr, BdCount, BdPtr);
	if (Status != XST_SUCCESS) {
		this->Error = 1;
	}

	if(!this->Error) {
		this->TxDone += BdCount;
	}
}

void dmaSG::RxCallback(XAxiDma_BdRing * RxRingPtr)
{
	int BdCount;
	XAxiDma_Bd *BdPtr;
	XAxiDma_Bd *BdCurPtr;
	u32 BdSts;
	int Index;

	/* Get finished BDs from hardware */
	BdCount = XAxiDma_BdRingFromHw(RxRingPtr, XAXIDMA_ALL_BDS, &BdPtr);

	BdCurPtr = BdPtr;
	for (Index = 0; Index < BdCount; Index++) {

		/*
		 * Check the flags set by the hardware for status
		 * If error happens, processing stops, because the DMA engine
		 * is halted after this BD.
		 */
		BdSts = XAxiDma_BdGetSts(BdCurPtr);
		if ((BdSts & XAXIDMA_BD_STS_ALL_ERR_MASK) ||
		    (!(BdSts & XAXIDMA_BD_STS_COMPLETE_MASK))) {
			this->Error = 1;
			break;
		}

		/* Find the next processed BD */
		BdCurPtr = XAxiDma_BdRingNext(RxRingPtr, BdCurPtr);
		this->RxDone += 1;
	}
}

// TODO - clear all the mess in this code...
int dmaSG::SetupInterrupts(void)
{
	#define INTC			XScuGic
	#define INTC_HANDLER	XScuGic_InterruptHandler

	#define INTC_DEVICE_ID	XPAR_PS7_SCUGIC_0_DEVICE_ID

	// TODO - update hardware project to include interupts so this defines are correct
	#define RX_INTR_ID		0 //XPAR_FABRIC_AXIDMA_0_S2MM_INTROUT_VEC_ID
	#define TX_INTR_ID		0 //XPAR_FABRIC_AXIDMA_0_MM2S_INTROUT_VEC_ID

	INTC * IntcInstancePtr = INTC_DEVICE_ID;

	u16 TxIntrId = TX_INTR_ID;
	u16 RxIntrId = RX_INTR_ID;

	XAxiDma_BdRing *TxRingPtr = this->Tx.RingPtr;
	XAxiDma_BdRing *RxRingPtr = this->Rx.RingPtr;
	int Status;

	#ifdef XPAR_INTC_0_DEVICE_ID

		/* Initialize the interrupt controller and connect the ISRs */
		Status = XIntc_Initialize(IntcInstancePtr, INTC_DEVICE_ID);
		if (Status != XST_SUCCESS) {

			xil_printf("Failed init intc\r\n");
			return XST_FAILURE;
		}

		Status = XIntc_Connect(IntcInstancePtr, TxIntrId,
					   (XInterruptHandler) TxIntrHandler, TxRingPtr);
		if (Status != XST_SUCCESS) {

			xil_printf("Failed tx connect intc\r\n");
			return XST_FAILURE;
		}

		Status = XIntc_Connect(IntcInstancePtr, RxIntrId,
					   (XInterruptHandler) RxIntrHandler, RxRingPtr);
		if (Status != XST_SUCCESS) {

			xil_printf("Failed rx connect intc\r\n");
			return XST_FAILURE;
		}

		/* Start the interrupt controller */
		Status = XIntc_Start(IntcInstancePtr, XIN_REAL_MODE);
		if (Status != XST_SUCCESS) {

			xil_printf("Failed to start intc\r\n");
			return XST_FAILURE;
		}

		XIntc_Enable(IntcInstancePtr, TxIntrId);
		XIntc_Enable(IntcInstancePtr, RxIntrId);

	#else

		XScuGic_Config *IntcConfig;


		/*
		 * Initialize the interrupt controller driver so that it is ready to
		 * use.
		 */
		IntcConfig = XScuGic_LookupConfig(INTC_DEVICE_ID);
		if (NULL == IntcConfig) {
			return XST_FAILURE;
		}

		Status = XScuGic_CfgInitialize(IntcInstancePtr, IntcConfig,
						IntcConfig->CpuBaseAddress);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}


		XScuGic_SetPriorityTriggerType(IntcInstancePtr, TxIntrId, 0xA0, 0x3);

		XScuGic_SetPriorityTriggerType(IntcInstancePtr, RxIntrId, 0xA0, 0x3);
		/*
		 * Connect the device driver handler that will be called when an
		 * interrupt for the device occurs, the handler defined above performs
		 * the specific interrupt processing for the device.
		 */

		// TODO - solve how to add methods to take care of an interrupt

		Status = XScuGic_Connect(IntcInstancePtr, TxIntrId,
					(Xil_InterruptHandler)TxIntrHandler2,
					TxRingPtr);
		if (Status != XST_SUCCESS) {
			return Status;
		}

		Status = XScuGic_Connect(IntcInstancePtr, RxIntrId,
					(Xil_InterruptHandler)RxIntrHandler2,
					RxRingPtr);
		if (Status != XST_SUCCESS) {
			return Status;
		}

		XScuGic_Enable(IntcInstancePtr, TxIntrId);
		XScuGic_Enable(IntcInstancePtr, RxIntrId);
	#endif

	/* Enable interrupts from the hardware */

	Xil_ExceptionInit();
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
			(Xil_ExceptionHandler)INTC_HANDLER,
			(void *)IntcInstancePtr);

	Xil_ExceptionEnable();

	return XST_SUCCESS;
}

void dmaSG::TxIntrHandler(void *Callback)
{
	XAxiDma_BdRing *TxRingPtr = (XAxiDma_BdRing *) Callback;
	u32 IrqStatus;
	int TimeOut;

	/* Read pending interrupts */
	IrqStatus = XAxiDma_BdRingGetIrq(TxRingPtr);

	/* Acknowledge pending interrupts */
	XAxiDma_BdRingAckIrq(TxRingPtr, IrqStatus);

	/* If no interrupt is asserted, we do not do anything
	 */
	if (!(IrqStatus & XAXIDMA_IRQ_ALL_MASK)) {

		return;
	}

	/*
	 * If error interrupt is asserted, raise error flag, reset the
	 * hardware to recover from the error, and return with no further
	 * processing.
	 */
	if ((IrqStatus & XAXIDMA_IRQ_ERROR_MASK)) {
		XAxiDma_BdRingDumpRegs(TxRingPtr);

		Error = 1;

		/*
		 * Reset should never fail for transmit channel
		 */
		XAxiDma_Reset(&(this->axiDma));

		TimeOut = RESET_TIMEOUT_COUNTER;

		while (TimeOut) {
			if (XAxiDma_ResetIsDone(&(this->axiDma))) {
				break;
			}

			TimeOut -= 1;
		}

		return;
	}

	/*
	 * If Transmit done interrupt is asserted, call TX call back function
	 * to handle the processed BDs and raise the according flag
	 */
	if ((IrqStatus & (XAXIDMA_IRQ_DELAY_MASK | XAXIDMA_IRQ_IOC_MASK))) {
		TxCallback(TxRingPtr);
	}
}

void dmaSG::RxIntrHandler(void *Callback)
{
	XAxiDma_BdRing *RxRingPtr = (XAxiDma_BdRing *) Callback;
	u32 IrqStatus;
	int TimeOut;

	/* Read pending interrupts */
	IrqStatus = XAxiDma_BdRingGetIrq(RxRingPtr);

	/* Acknowledge pending interrupts */
	XAxiDma_BdRingAckIrq(RxRingPtr, IrqStatus);

	/*
	 * If no interrupt is asserted, we do not do anything
	 */
	if (!(IrqStatus & XAXIDMA_IRQ_ALL_MASK)) {
		return;
	}

	/*
	 * If error interrupt is asserted, raise error flag, reset the
	 * hardware to recover from the error, and return with no further
	 * processing.
	 */
	if ((IrqStatus & XAXIDMA_IRQ_ERROR_MASK)) {

		XAxiDma_BdRingDumpRegs(RxRingPtr);

		Error = 1;

		/* Reset could fail and hang
		 * NEED a way to handle this or do not call it??
		 */
		XAxiDma_Reset(&(this->axiDma));

		TimeOut = RESET_TIMEOUT_COUNTER;

		while (TimeOut) {
			if(XAxiDma_ResetIsDone(&(this->axiDma))) {
				break;
			}

			TimeOut -= 1;
		}

		return;
	}

	/*
	 * If completion interrupt is asserted, call RX call back function
	 * to handle the processed BDs and then raise the according flag.
	 */
	if ((IrqStatus & (XAXIDMA_IRQ_DELAY_MASK | XAXIDMA_IRQ_IOC_MASK))) {
		RxCallback(RxRingPtr);
	}
}
