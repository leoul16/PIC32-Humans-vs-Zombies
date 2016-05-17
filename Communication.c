#include "Communication.h"    /*!< Communication definitions */
#include "ADXL345.h"

#include <plib.h>
/******************************************************************************/
/************************ Functions Definitions *******************************/
/******************************************************************************/
//deviceID e5
SpiChannel connected_channel;

int SpiMasterInit(int channel){
    int success=-1;
    if (channel == 3)
    {
        connected_channel=SPI_CHANNEL3;
     //   SPI3CON |= 0 << 16; //clear on Bit
   //     mPORTDSetBits(BIT_14);    //SS bit
        SpiChnOpen(connected_channel, SPI_OPEN_CKP_HIGH|SPI_OPEN_MSTEN|SPI_OPEN_MODE8|SPI_OPEN_ENHBUF|SPI_OPEN_ON , 20); //Open SPI: idle-high, master mode, 8 bit transfer, enhanced buffer, on bit set

        mPORTDSetPinsDigitalOut(BIT_14); //MOSI
        // mPORTFSetPinsDigitalIn(BIT_8);  //MISO
        success=0;  //configured successfully
    }
    else if (channel == 4){
        connected_channel= SPI_CHANNEL4;
        //SPI3CON |= 0 << 16; //clear on Bit
        //mPORTFSetBits(BIT_12);    //SS bit
        SpiChnOpen(connected_channel, SPI_OPEN_CKP_HIGH|SPI_OPEN_MSTEN|SPI_OPEN_MODE8|SPI_OPEN_ENHBUF|SPI_OPEN_ON , 20); //Open SPI: idle-high, master mode, 8 bit transfer, enhanced buffer, on bit set

        mPORTFSetPinsDigitalOut(BIT_12); //MOSI
      //  mPORTFSetPinsDigitalIn(BIT_5);  //MISO
        success=0;  //configured successfully
    }
    if((channel!=3) && (channel!=4))
        success=-1;

return success;
}

int SpiMasterIO(char bytes[], int numWriteBytes, int numReadBytes){

    int WriteByte=0;   //byte index
    int ReadByte=numWriteBytes;
    int status=0;
    int arraysize;
    arraysize=numWriteBytes+numReadBytes;

        if(connected_channel==SPI_CHANNEL3){
            mPORTDClearBits(BIT_14);
            status=0;
        }
        else if(connected_channel== SPI_CHANNEL4){
            mPORTFClearBits(BIT_12);
            status=0;
        }

        while(WriteByte<numWriteBytes){
            SpiChnPutC(connected_channel, bytes[WriteByte]);
            SpiChnGetC(connected_channel);
            WriteByte++;
        }

        while(ReadByte<arraysize){
            SpiChnPutC(connected_channel, 0);
            bytes[ReadByte]=SpiChnGetC(connected_channel);
            ReadByte++;
        }

        if(connected_channel== SPI_CHANNEL3)
            mPORTDSetBits(BIT_14);
        else if(connected_channel== SPI_CHANNEL4)
            mPORTFSetBits(BIT_12);


    if ((connected_channel!=SPI_CHANNEL3) && (connected_channel!=SPI_CHANNEL4))
        status=-1;

    return status;
}