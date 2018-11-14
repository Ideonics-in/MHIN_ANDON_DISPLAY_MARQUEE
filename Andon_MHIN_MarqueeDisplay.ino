

#include <EEPROM.h>
#include <Arial14.h>
#include <Arial_black_16.h>
#include <Arial_Black_16_ISO_8859_1.h>
#include <Arial_black_16_UpDown.h>
#include <Bengali.h>
#include <DMDMegaMulti.h>
#include <Font3x5.h>
#include <Hindi.h>
#include <MyBigFont.h>
#include <Odiya.h>
#include <SystemFont5x7.h>


#include <digitalWriteFast.h>

#include <SPI.h>        //SPI.h must be included as DMD is written by SPI (the IDE complains otherwise)

#include "TimerOne.h"


#define DISPLAYS_ACROSS 12
#define DISPLAYS_DOWN 1
#define DISPLAYS_BPP 1
#define WHITE 0xFF
#define BLACK 0
#define STATICMSGBUFFER_SIZE  25
#define DISPLAY_ADDRESS 1



enum
{
	PARSE_IGNORE = 0,
	PARSE_SUCCESS = 1,
	PARSE_FAILURE = 2
};

enum
{
	COM_DEVICE_ADDRESS_INDEX = 0,
	COM_TX_CODE_INDEX = 1,
	COM_DISPLAY_CODE_INDEX = 2,
	COM_TX_DATA_START_INDEX = 3,
	COM_RX_DATA_START_INDEX = 2
};
//state for communication
enum
{
	COM_RESET = 0,
	COM_START = 1,
	COM_IN_PACKET_COLLECTION = 2,
	COM_IN_PARSE_DATA = 3
};

#define RX_BUFFSIZE 64
#define TX_BUFFSIZE 32
#define MAX_ISSUES 12
#define MAX_LINES	15
#define __SERIAL_DEBUG__


#define EEPROM_TYPE_1_BASE	0
#define EEPROM_TYPE_2_BASE	EEPROM_TYPE_1_BASE + 20*32
#define EEPROM_TYPE_3_BASE	EEPROM_TYPE_2_BASE + 20*32
#define EEPROM_LINE_BASE	EEPROM_TYPE_3_BASE + 20*32

typedef struct _COMM
{
	unsigned char state;
	unsigned char prevState;

	unsigned char rxData[RX_BUFFSIZE];
	unsigned char rxDataIndex;
	unsigned char txData[TX_BUFFSIZE];

}COMM;

COMM comm = { 0 };
char SOP = 0xAA;
char EOP = 0xBB;

unsigned long curAppTime = 0;
unsigned long prevAppTime = 0;
unsigned char timeout = 500;


typedef enum
{
	TYPE_1 = 1,
	TYPE_2 = 2,
	TYPE_3
}STATION_TYPE;


unsigned int CurrentLine = 0;

typedef struct LINE
{
	unsigned char ID[3];
	char NAME[40];
	STATION_TYPE Type;
	int Issues[16];
	int IssueCount;
	bool StateChanged;

};

char TYPE_1_MESSAGES[15][28] =
{
	
	"Machine Problem",
	"Robot Problem",
	"R/M conveying Problem",
	"Chiller B/D",
	"Crane B/D",
	"Mold B/D",
	"Quality Approval/Issues",
	"No Manpower",
	"No Packing Material",
	"Non availibilty of Tools",
	"ISSUE ",
	"ISSUE ",
	"ISSUE ",
	"ISSUE ",
	"ISSUE "
	
};




char TYPE_2_MESSAGES[15][28] =
{
	
	"Machine Problem",
	"Fixture B/D",
	"No Packing Material",
	"Quality Approval",
	"Quality Issues",
	"Tools Unavailable",
	"ISSUE ",
	"ISSUE ",
	"ISSUE ",
	"ISSUE ",
	"ISSUE ",
	"ISSUE ",
	"ISSUE ",
	"ISSUE "
	
};

char TYPE_3_MESSAGES[15][28] =
{
	
	"Machine Problem",
	"No Child Parts",
	"Fixture B/D",
	"No Packing Material",
	"Quality Approval",
	"Quality Issues",
	"Non availibilty of Tools",
	"ISSUE ",
	"ISSUE ",
	"ISSUE ",
	"ISSUE ",
	"ISSUE ",
	"ISSUE ",
	"ISSUE ",
	"ISSUE "

};













/*
OFCA Line
Fleating Machine
Round Filter Dosing - 1
Round Filter Dosing - 2
AOS Wrapping Machine
AOS Gluing machine - 1
AOS Gluing machine - 2
AOS Welding Line
Vibration Welding Line
*/

unsigned int MARQUEE_LINES[MAX_LINES + 1] = { 0 };
unsigned int CurrentMarqueeIndex = 1;
unsigned int NextMarqueeIndex = 1;

#define DISPLAY_UNIT  2
#if  DISPLAY_UNIT == 1



LINE Lines[] = {
	{},
	{ "1","FORD A/C",TYPE_2 },
	{ "2","MHD-1045/1024",TYPE_2 },
	{ "3","TOY-1086",TYPE_2 },
	{ "4","MHD-1023/1029/1069", TYPE_2 },
	{ "5","DHE-1004",TYPE_2 },
	{ "6","MHD-100",TYPE_2 },
	{ "7","LINE_7",TYPE_2 },
	{ "8","LINE_8",TYPE_2 },
	{ "9","LINE_9",TYPE_2 },
	{ "10","LINE_10",TYPE_2 },
	{ "11","CHR-DSD ASSY",TYPE_2 },
	{ "12","TURBO ADAPTOR", TYPE_2 },
	{ "13","CHR-1124 A/C", TYPE_2 },
	{ "14","IMM-500T", TYPE_1 },
	{ "15","IMM-350", TYPE_1 }
	
	

};

#else if DISPLAY_UNIT == 2 //  DISPLAY == 1

LINE Lines[] = {
	{},
	{ "1","OFCA",TYPE_2 },
	{ "2","TATA OES",TYPE_2 },
	{ "3","LINE_3",TYPE_2 },
	{ "4","LINE_4", TYPE_2 },
	{ "5","AOS",TYPE_2 },
	{ "6","LINE_6",TYPE_2 },
	{ "7","LINE_7",TYPE_2 },
	{ "8","LINE_8",TYPE_2 },
	{ "9","VIB WELDING",TYPE_3 },
	{ "10","LINE_10",TYPE_2 },
	{ "11","LINE_11",TYPE_2 },
	{ "12","LINE_12", TYPE_2 },
	{ "13","LINE_13", TYPE_2 },
	{ "14","LINE_14", TYPE_1 },
	{ "15","LINE_15", TYPE_1 }



};


#endif
enum
{
	STATIC = 0,
	SCROLLING = 1
};

boolean ret = true;
int updateInterval = 30;
int updateTime = 0;
DMD dmd(DISPLAYS_ACROSS, DISPLAYS_DOWN);

/*--------------------------------------------------------------------------------------
Interrupt handler for Timer1 (TimerOne) driven DMD refresh scanning, this gets
called at the period set in Timer1.initialize();
--------------------------------------------------------------------------------------*/
void ScanDMD()
{
	dmd.scanDisplayBySPI();
}


int dataIndex = 0;
bool dataReceived = false;
char message[350] = "WELCOME TO MANN AND HUMMEL";


void MakeLineMarquee()
{
	int i;
	memset(message, 0, sizeof(message));
	if (Lines[CurrentLine].IssueCount == 0) return;
	strcpy(message, (const char*)Lines[CurrentLine].NAME);
	strcat(message, ":");

	if (Lines[CurrentLine].StateChanged == true)
	{
		for (i = 0; i < Lines[CurrentLine].IssueCount; i++)
		{
			if (Lines[CurrentLine].Type == TYPE_1)
			{
				strcat(message, (const char*)TYPE_1_MESSAGES[Lines[CurrentLine].Issues[i]]);
				strcat(message, ";");
			}
			else if (Lines[CurrentLine].Type == TYPE_2)
			{
				strcat(message, (const char*)TYPE_2_MESSAGES[Lines[CurrentLine].Issues[i]]);
				strcat(message, ";");
			}
			else if (Lines[CurrentLine].Type == TYPE_2)
			{
				strcat(message, (const char*)TYPE_2_MESSAGES[Lines[CurrentLine].Issues[i]]);
				strcat(message, ";");
			}
		}
	}
}

void setup()
{
#ifdef __FACTORY_CONFIGURATION
	EEPROM.put(EEPROM_TYPE_1_BASE, TYPE_1_MESSAGES);
	EEPROM.put(EEPROM_TYPE_2_BASE, TYPE_2_MESSAGES);
	EEPROM.put(EEPROM_TYPE_3_BASE, TYPE_3_MESSAGES);
#else
/*
	EEPROM.put(EEPROM_TYPE_1_BASE, TYPE_1_MESSAGES);
	EEPROM.put(EEPROM_TYPE_2_BASE, TYPE_2_MESSAGES);
	EEPROM.put(EEPROM_TYPE_3_BASE, TYPE_3_MESSAGES);
	*/
#endif
	/* add setup code here */
	Timer1.initialize(2000 / DISPLAYS_BPP);           //period in microseconds to call ScanDMD. Anything longer than 5000 (5ms) and you can see flicker.
	Timer1.attachInterrupt(ScanDMD);   //attach the Timer1 interrupt to ScanDMD which goes to dmd.scanDisplayBySPI()

									   //clear/init the DMD pixels held in RAM
	dmd.clearScreen(WHITE);   //true is normal (all pixels off), false is negative (all pixels on)
	dmd.selectFont(Arial_Black_16);   // BOLD FONT

	dmd.clearScreen(WHITE);
	dmd.drawMarquee(message, strlen(message), (32 * DISPLAYS_ACROSS) - 1, 0); // set up the marquee


	COM_init();

	Serial.println("Application Started");
	Serial1.println("Application Started");
	Serial2.println("Application Started");
	Serial3.println("Application Started");

}

void loop()
{

	unsigned char txPacket[10],i,lineID,issues;
	unsigned int j, k;
#if 0
	int i, j;
	for (int i = 0; i < 32 * DISPLAYS_ACROSS; i++)
	{
		for (int j = 0; j < 16; j++)
		{
			dmd.writePixel(i, j, GRAPHICS_NORMAL, 1);
			delay(10);
		}
		delay(200);

	}
	delay(500);
	dmd.clearScreen(WHITE);

	/* add main program code here */
	//
#else
	
	if (ComService() == true)
	{
#ifdef __SERIAL_DEBUG__
		Serial.println("Data Received");
#endif
		switch (comm.rxData[2])
		{
		case 0x90:
#ifdef __SERIAL_DEBUG__
			Serial.println("COMMAND : Update Line State");
#endif      
			lineID = comm.rxData[3];
			Lines[lineID].IssueCount = comm.rxData[1] - 2;

#ifdef __SERIAL_DEBUG__
			Serial.print("Line:");
			Serial.println(lineID);
			Serial.print("IssueCount:");
			Serial.println(Lines[lineID].IssueCount);
#endif      
			Lines[lineID].StateChanged = true;

			if (Lines[lineID].IssueCount > 0)
			{
				for (j = 1; j <= MAX_LINES; j++)
				{
					if (MARQUEE_LINES[j] == lineID) break;
				}
				if (j >= MAX_LINES)
				{
					MARQUEE_LINES[NextMarqueeIndex] = lineID;
					NextMarqueeIndex++;
					if (NextMarqueeIndex > MAX_LINES)
						NextMarqueeIndex = 1;
				}
			}
			else if (Lines[lineID].IssueCount == 0)
			{
				for (j = 1; j <= MAX_LINES; j++)
				{
					if (MARQUEE_LINES[j] == lineID)
					{
						MARQUEE_LINES[j] = 0;
						break;
					}
				}
			}

			for (i = 0; i < Lines[lineID].IssueCount; i++)
			{
				Lines[lineID].Issues[i] = comm.rxData[4 + i];
				
			}
			

			break;

		case 0x91:
#ifdef __SERIAL_DEBUG__
			Serial.println("COMMAND : Update Station Display Line 1");
#endif     
			j = comm.rxData[1] - 1;
			txPacket[0] = 0xAA;
			for (k = 0; k < j; k++)
			{
				txPacket[1+k] = comm.rxData[3 + k];

			}
		
			txPacket[1+k] = 0xBB;
			for (i = 0; i <= k+1; i++)
				Serial2.write(txPacket[i]);
			break;

		case 0x92:
#ifdef __SERIAL_DEBUG__
			Serial.println("COMMAND : Update Station Display Line 2");
#endif
			j = comm.rxData[1] - 1;
			txPacket[0] = 0xAA;
			for (k = 0; k < j; k++)
			{
				txPacket[1 + k] = comm.rxData[3 + k];

			}

			txPacket[1 + k] = 0xBB;
			for (i = 0; i <= k + 1; i++)
				Serial3.write(txPacket[i]);
			break;

		case 0x83:
#ifdef __SERIAL_DEBUG__
			Serial.println("COMMAND : Update Station Name");
#endif           
			strcpy((char*)Lines[comm.rxData[1]].NAME, (const char*)&comm.rxData[2]);
			break;

		case 0x84:
#ifdef __SERIAL_DEBUG__
			Serial.println("COMMAND : Update Issue List");
#endif           
			if(comm.rxData[1] == TYPE_1)
			{
			strcpy((char*)Lines[comm.rxData[1]].NAME, (const char*)&comm.rxData[2]);
			}
			break;
		}
	}


#endif
	if (ret)		//check if scrolling complete
	{
		dmd.clearScreen(WHITE);
		memset(message, 0, sizeof(message));
		int j;
		CurrentMarqueeIndex++;
		if (CurrentMarqueeIndex >= MAX_LINES)
			CurrentMarqueeIndex = 1;

		for (; CurrentMarqueeIndex <= MAX_LINES; CurrentMarqueeIndex++)
		{
			if (MARQUEE_LINES[CurrentMarqueeIndex] != 0)
			{
				CurrentLine = MARQUEE_LINES[CurrentMarqueeIndex];
#ifdef __SERIAL_DEBUG__
				Serial.print("CurrentLine:");
				Serial.println(CurrentLine);
#endif           		
				MakeLineMarquee();

#ifdef __SERIAL_DEBUG__

				Serial.print("Marquee:");
				Serial.println(message);
#endif
				
				break;
			}
		}
		
		int dataCount = strlen(message);
		dmd.drawMarquee(message, dataCount, (32 * DISPLAYS_ACROSS) - 1, 0); // set up the marquee


																	//Serial.println("Cycle Complete");
	}



	int currentTime = millis();

	if ((currentTime - updateTime) > updateInterval)
	{
		ret = dmd.stepMarquee(-1, 0);
		updateTime = currentTime;

	}


}



#pragma region Communication





void COM_init()
{
	Serial.begin(9600);
	Serial1.begin(9600);
	Serial2.begin(9600);
	Serial3.begin(9600);
}

/*--------------------------------------------------------------------------------
bool ComService(void)
--------------------------------------------------------------------------------**/
bool ComService(void)
{
	bool result = false;
	volatile char serialData = 0;
	curAppTime = millis();

	unsigned char i, j;

	if (prevAppTime != curAppTime)
	{
		if (comm.prevState == comm.state && (comm.state == COM_IN_PACKET_COLLECTION))
		{
			--timeout;
			if (timeout == 0)
			{
				ComReset();
				return result;
			}

		}

		prevAppTime = curAppTime;
	}
	switch (comm.state)
	{
	case COM_START:

		if (Serial1.available() > 0)
		{
			serialData = Serial1.read();
			if (serialData == SOP)
			{
				comm.rxDataIndex = 0;
				comm.state = COM_IN_PACKET_COLLECTION;
			}
		}
		break;
	case COM_IN_PACKET_COLLECTION:

		if (Serial1.available() > 0)
		{
			serialData = Serial1.read();
			if (serialData == EOP)
			{
				char parseResult = 0;
				parseResult = ParsePacket();

				switch (parseResult)
				{
				case PARSE_IGNORE:
					ComReset();
					//Serial2.write(comm.rxData, comm.rxDataIndex);

					break;

				case PARSE_SUCCESS:
					result = true;
					break;

				default:
					break;

				}
				comm.state = COM_START;
			}
			else
			{
				comm.rxData[comm.rxDataIndex] = serialData;
				comm.rxDataIndex++;
			}

		}
		break;
	case COM_RESET:

		ComReset();
		break;

	default:
		ComReset();
		break;
	}
	comm.prevState = comm.state;
	return result;
}


/*--------------------------------------------------------------------------------
void ComReset(void)
--------------------------------------------------------------------------------*/
void ComReset(void)
{
	comm.rxDataIndex = 0;
	comm.state = COM_START;
}
/*--------------------------------------------------------------------------------
char ParsePacket(void)
--------------------------------------------------------------------------------*/
char ParsePacket(void)
{
	char receivedChecksum = comm.rxData[comm.rxDataIndex - 1];
	char genChecksum = 0;

	//Serial.println("inside parse");
//	if (comm.rxData[COM_DISPLAY_CODE_INDEX] != DISPLAY_ADDRESS)
	//	return PARSE_IGNORE;
	//else 
		return PARSE_SUCCESS;
/*
	genChecksum = checksum(comm.rxData, comm.rxDataIndex - 1);
	//Serial.println(genChecksum,HEX);
	//Serial.println(receivedChecksum,HEX);
	if (receivedChecksum == genChecksum)
	{
		--comm.rxDataIndex;
		comm.rxData[comm.rxDataIndex] = '\0'; //remove checksum from packet

		return PARSE_SUCCESS;
	}
	else
	{
		//*respCode = COM_RESP_CHECKSUM_ERROR;
		return PARSE_FAILURE;
	}
	*/
}

/*---------------------------------------------------------------------------------
unsigned char checksum(unsigned char *buffer, unsigned char length)

summry: take bufer value and give checksum value

--------------------------------------------------------------------------------*/
unsigned char checksum(unsigned char *buff, unsigned char length)
{

	char bcc = 0;
	char i, j;

#ifdef __BCC_LRC__

	for (i = 0; i < length; i++)
	{
		bcc += buff[i];
	}
	return bcc;

#elif defined __BCC_XOR__

	for (i = 0; i < length; i++)
	{
		bcc ^= buff[i];
	}
	return bcc;

#endif
}


#pragma endregion