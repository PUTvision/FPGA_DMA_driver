/*
 * Empty C++ Application
 */


#include <stdio.h>
#include "platform.h"

#include "dma_sg/dma_sg.hpp"

#include "table_functions.h"
#include "zynq_ps7_timer/zynq_ps7_timer.h"


static int CompareTables(u32* table1, u32 table1Size, u32* table2, u32 table2Size, u32 differenceBetweentable1ValueAndTable2Value)
{
	int result = XST_SUCCESS;

	Xil_DCacheInvalidateRange((u32)&table1[0], table1Size*sizeof(u32));
	Xil_DCacheInvalidateRange((u32)&table2[0], table2Size*sizeof(u32));

	PrintTableAs32bitsValues(&table1[0], table1Size);
	PrintTableAs32bitsValues(&table2[0], table2Size);

	if(table1Size != table2Size)
	{
		xil_printf("Error, tables of different sizes!\r\n");
		result = XST_FAILURE;
	}
	else
	{
		int i;
		int const tableSize = table1Size;
		for(i=0; i<tableSize; ++i)
		{
			const int val1 = table1[i];
			const int val2 = table2[i] - differenceBetweentable1ValueAndTable2Value;

			if(val1 != val2)
			{
				xil_printf("Data error %d: %d != %d\r\n", i, val1, val2);
				result = XST_FAILURE;
			}
		}
	}

	return result;
}

#define DIFFERENCE_BETWEEN_SEND_AND_RECV_DATA						5
#define ALIGNE_VALUE												64

// TODO: change it to template so the number of bits is calculated automatically
class Packet32Bits
{
private:
	u32 numberOfWords;
	u32 numberOfBytesPerWord;
	static const u32 sizeOfTableElement = sizeof(u32);

public:
	u32 totalNumberOfBytes;
	u32 tableSize;
	u32* tableData;

	void CreateByNumberOfBytes(u32 numberOfBytes)
	{
		u32 sizeOfWordInBytes = 4;
		CreateByNumberOfWords(numberOfBytes / sizeOfWordInBytes, sizeOfWordInBytes);
	}

	void CreateByNumberOfWords(u32 numberOfWords)
	{
		u32 sizeOfWordInBytes = 4;
		CreateByNumberOfWords(numberOfWords, sizeOfWordInBytes);
	}

	void CreateByNumberOfBytes(u32 numberOfBytes, u32 sizeOfWordInBytes)
	{
		CreateByNumberOfWords(numberOfBytes / sizeOfWordInBytes, sizeOfWordInBytes);
	}

	void CreateByNumberOfWords(u32 numberOfWords, u32 sizeOfWordInBytes)
	{
		this->numberOfBytesPerWord = sizeOfWordInBytes;
		this->numberOfWords = numberOfWords;

		this->totalNumberOfBytes = (this->numberOfWords * this->numberOfBytesPerWord);
		this->tableSize = (this->totalNumberOfBytes / this->sizeOfTableElement);

		this->tableData = (u32*) aligned_malloc(this->totalNumberOfBytes, ALIGNE_VALUE);
	}

	Packet32Bits(void)
	{

	}


	~Packet32Bits(void)
	{
		if(this->tableData != NULL)
		{
			aligned_free(tableData);
		}
	}

};

// drugi test - paczki o tym samym rozmiarze - 32 bajty, ale ró¿na liczba danych wysy³anych

static void DMA_SG_test(void)
{
	dmaSG dma;

	const u32 SIZE_IN_BYTES_OF_DATA_TO_TRANSFER = 8192;
	const u32 MAX_NUMBER_OF_TRANSFERS = 2048;
	const u32 MAX_SIZE_OF_TRANSFER = SIZE_IN_BYTES_OF_DATA_TO_TRANSFER;

	dma.Init(XPAR_AXI_DMA_0_DEVICE_ID, MAX_NUMBER_OF_TRANSFERS, MAX_NUMBER_OF_TRANSFERS, MAX_SIZE_OF_TRANSFER);

	TIMER_init(XPAR_PS7_SCUTIMER_0_DEVICE_ID);

    for(int i=1; i<1024; i=i*2)
    {
    	u32 NUMBER_OF_TRANSFER_OPERATIONS = i;
    	xil_printf("Number of bytes to send: %d \r\nNumber of transfers: %d \r\n", SIZE_IN_BYTES_OF_DATA_TO_TRANSFER, NUMBER_OF_TRANSFER_OPERATIONS);

    	Packet32Bits packetToSend;
		packetToSend.CreateByNumberOfBytes(SIZE_IN_BYTES_OF_DATA_TO_TRANSFER);
		FillDataTableWithIncreasingValue(packetToSend.tableData, packetToSend.tableSize, 0, 1);
		Xil_DCacheFlushRange((u32)packetToSend.tableData, packetToSend.totalNumberOfBytes);

		u32 sizeOfPartInBytes = packetToSend.totalNumberOfBytes / NUMBER_OF_TRANSFER_OPERATIONS;
		u32 numberOfElementsInPart = packetToSend.tableSize / NUMBER_OF_TRANSFER_OPERATIONS;

		u32 timerValue;
		float timeInUsForTxOperation;
		float timeInUsForRxOperation;

		TIMER_start();

		for(u32 i=0; i<NUMBER_OF_TRANSFER_OPERATIONS; ++i)
		{
			dma.Send((u8*)&packetToSend.tableData[i*numberOfElementsInPart], sizeOfPartInBytes);
		}


		dma.WaitForTxCompleteAndFree();
		//dma.WaitForTxComplete();
		//dma.FreeProcessedTx();

		timerValue = TIMER_getCurrentValue();

		dma.WaitForTxCompleteAndFree();

		//dma.WaitForRxComplete();

		/* Check DMA transfer result */
		/*
		u8** rxBuffers = dma.GetRxBuffers();
		u32 RECV_tableSize = dma.GetSizeOfRxBuffers() / sizeof(u32);
		for(u32 i=0; i<NUMBER_OF_TRANSFER_OPERATIONS; ++i)
		{
			u32* RECV_tableData = (u32*) rxBuffers[i];
			int status = CompareTables(&(packetToSend.tableData[i*numberOfElementsInPart]), numberOfElementsInPart, RECV_tableData, RECV_tableSize, DIFFERENCE_BETWEEN_SEND_AND_RECV_DATA);
			xil_printf("AXI DMA SG Polling Test %s\r\n",
					(status == XST_SUCCESS)? "passed":"failed");
		}
		*/

		//dma.FreeProcessedRx();

		TIMER_stop();
		timeInUsForTxOperation = TIMER_getTimeStartToValueInUs(timerValue);
		timeInUsForRxOperation = TIMER_getTimeValueToStopInUs(timerValue);
		printf("TX operation. Time to execute: %f us\r\n", timeInUsForTxOperation);
		printf("RX operation. Time to execute: %f us\r\n", timeInUsForRxOperation);
    }
}

int main()
{
    init_platform();

    xil_printf("Hello World dma_sg_test\r\n");

    DMA_SG_test();

	while(1)
	{
		asm("nop");
	}

    cleanup_platform();

    return 0;
}
