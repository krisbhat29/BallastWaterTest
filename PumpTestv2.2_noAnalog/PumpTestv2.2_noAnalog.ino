//!
//!
//! @file      	   2phase HACH pump
//! 
//! @copyright     Hach Confidential
//!                Copyright(c) (2014)
//!                Hach
//!                All Rights Reserved
//!
//! @brief         Implementation of the algorithm for 2phase HACH pump
//! 
//! @detail        This module defines the methods used to drive 2phase HACH pump
//! 
//! @date          20-May-2015 
//! @author        Krishna Bhat
//! 
//! @version       v2.01
//! 
//!

#include<EEPROM.h>

// define pin nos as per schematic
#define PUMPA_PIN1            2 // Takasago Valve
#define PUMPA_PIN2            3 // 3 port solenoid valve
#define PUMPB_PIN1            5
#define PUMPB_PIN2            6
#define PUMPC_PIN1            8
#define PUMPC_PIN2            9
#define PUMPD_PIN1            11
#define PUMPD_PIN2            12
#define PUMPA_SENSE           A0
#define PUMPB_SENSE           A1
#define PUMPC_SENSE           A2
#define PUMPD_SENSE           A3

// define default values for cycletime
#define CYCLETIME_PUMP        200 // default

// No of states  for 2 phase Hach pump
#define NO_OF_STATES          4 

#define SERIAL_BAUDRATE       9600  // default serial baud rate
#define MINIMUM_CYCLETIME     40  //minimum cycletime for pump in ms ; turn on time for each pump is ~ 5ms
#define DEFAULT_INTERVAL      200 // deault interval between each cycle
#define VERSION_NO            201    // divide by 100
#define COUNTER_OVERFLOW      65535
#define EEPROM_ADDRESS        0
#define DEBUG                 0
#define VTGTHRESHOLD_CNT      675 // ADC count equivalent to 3.3V; to change this value use "count = thresholdvtg/0.004882813"

// define the enumeration for the list of serial commands that can be sent over RS232 port
enum SerialCmds
{
  READPROFILE = 0,
  WRITETIME,
  LOGDATA,
  DISPDATA,
  INTERVAL,
  PAUSE,
  RESET,
  VERSION,
  MENU,
  MAX_COMMANDS
};

// cloned string list for serial commands based on the enum SerialCmds
String SerialCommands[]={
  "R", // for reading saved profile in eeprom
  "W", // write new cycletime
  "S", // log current data in EEPROM
  "D", // display active data
  "I", // time interval
  "P", // pause system  
  "X", // reset system
  "V", // software version
  "M" // Show menu
};

// Software Version Number
const float versionNo = VERSION_NO;  

// serial buffer string
String inputString = "";

// serial flag to indicate command request completion
bool stringComplete = false;

// static variables for runtime operation
static unsigned int CycleTime =CYCLETIME_PUMP;
static unsigned int delayVal = 0;
static unsigned int interval = DEFAULT_INTERVAL; 

// active cycle count 0 - 65535
static unsigned int CycleCount = 0;
// overflow counter since uint is 2 bytes ( 0 - 65535 )
static unsigned int OverflowCount = 0;
bool analogSense = true; // set to default case
char discardChar;

// structure to store configuration/log data
struct ConfigData
{
   unsigned int activeCycleCount;
   unsigned int overflowCount;
   unsigned int cycleTime;
};
ConfigData configData;
ConfigData readData;


//!
//!             ShowMenu
//!
//! @brief      Function to display Menu options.
//!
//! @detail     This function displays Menu options to the user.
//!
//! @param[in]  None
//!
//! @param[out] None
//!
//! @retval     None
//!
//!
void ShowMenu()
{
  Serial.println("### 2PHASE CUSTOM ###");
  Serial.println("r; - READ SAVED DATA ");
  Serial.println("w; - WRITE NEW CYCLETIME ");
  Serial.println("s; - SAVE RUNTIME DATA ");
  Serial.println("d; - DISPLAY RUNTIME DATA");
  Serial.println("i; - TIME INTERVAL");
  Serial.println("p; - PAUSE SYSTEM");
  Serial.println("x; - RESET TO DEFAULT");
  Serial.println("v; - S/W VERSION\n\n");
}

//!
//!             ReadConfig
//!
//! @brief      Function to read from EEPROM.
//!
//! @detail     This function reads last saved data from EEPROM and updates active variables.
//!
//! @param[in]  None
//!
//! @param[out] None
//!
//! @retval     None
//!
//!
void ReadConfig()
{
  EEPROM.get(EEPROM_ADDRESS,configData);    // Read configuration data from EEPROM; will happen on every powerup.
  CycleCount = configData.activeCycleCount;
  OverflowCount = configData.overflowCount;
  CycleTime = configData.cycleTime;
  Serial.println("***Saved Cyclic Profile***");
  Serial.println("CycleCount    : "+ String(CycleCount));
  Serial.println("OverflowCount : "+ String(OverflowCount));
  Serial.println("CycleTime     : "+ String(CycleTime) + "ms\n");
}

//!
//!             LogData
//!
//! @brief      Function to log config data.
//!
//! @detail     This function saves the current active counts and variables to EEPROM.
//!
//! @param[in]  None
//!
//! @param[out] None
//!
//! @retval     None
//!
//!
void LogData()
{
  // write config data
  configData.activeCycleCount = CycleCount;
  configData.overflowCount = OverflowCount;           
  // write to EEPROM
  EEPROM.put(EEPROM_ADDRESS,configData);
  Serial.write("Cyclic Profile updated ! \n");
}

//!
//!             PumpStatus
//!
//! @brief      Function to check pump status.
//!
//! @detail     This function logs the last available data and pauses the system in case of pump failure.
//!
//! @param[in]  None
//!
//! @param[out] None
//!
//! @retval     None
//!
//!
void PumpStatus()
{
  if(analogRead(PUMPA_SENSE) < VTGTHRESHOLD_CNT)
    Serial.println("Pump 1 Disconnected\n");
  else if(analogRead(PUMPB_SENSE) < VTGTHRESHOLD_CNT)
    Serial.println("Pump 2 Disconnected\n");
  else if(analogRead(PUMPC_SENSE) < VTGTHRESHOLD_CNT)
    Serial.println("Pump 3 Disconnected\n");
  else if(analogRead(PUMPD_SENSE) < VTGTHRESHOLD_CNT)
    Serial.println("Pump 4 Disconnected\n"); 
  LogData();
  ReadConfig();  
  Serial.println("System Paused !\n");
  while(analogSense == false)
  {
    if((analogRead(PUMPA_SENSE) > VTGTHRESHOLD_CNT) && (analogRead(PUMPB_SENSE) > VTGTHRESHOLD_CNT) && (analogRead(PUMPC_SENSE) > VTGTHRESHOLD_CNT) && (analogRead(PUMPD_SENSE) > VTGTHRESHOLD_CNT))
    {
      analogSense = true;
      Serial.println("System Resume\n");
    }
  }
}

//!
//!             InitSystem
//!
//! @brief      Function to initialize the system.
//!
//! @detail     This function sets all the digital I/O pins to LOW.
//!
//! @param[in]  None
//!
//! @param[out] None
//!
//! @retval     None
//!
//!
void InitSystem()
{
  digitalWrite(PUMPA_PIN1,LOW);
  digitalWrite(PUMPA_PIN2,LOW);
  digitalWrite(PUMPB_PIN1,LOW);
  digitalWrite(PUMPB_PIN2,LOW);
  digitalWrite(PUMPC_PIN1,LOW);
  digitalWrite(PUMPC_PIN2,LOW);
  digitalWrite(PUMPD_PIN1,LOW);
  digitalWrite(PUMPD_PIN2,LOW);
  ShowMenu();
  ReadConfig();
  // calculate delay value based on last read value from EEPROM on PowerUp
  delayVal = CycleTime/NO_OF_STATES;
}


//!
//!             setup
//!
//! @brief      Function to setup Arduino Board.
//!
//! @detail     This function configures serial port, sets pin direction and initializes the system.
//!
//! @param[in]  None
//!
//! @param[out] None
//!
//! @retval     None
//!
//!
void setup() 
{
  // put your setup code here, to run once:
  Serial.begin(SERIAL_BAUDRATE);
  pinMode(PUMPA_PIN1,OUTPUT);
  pinMode(PUMPA_PIN2,OUTPUT);
  pinMode(PUMPB_PIN1,OUTPUT);
  pinMode(PUMPB_PIN2,OUTPUT);
  pinMode(PUMPC_PIN1,OUTPUT);
  pinMode(PUMPC_PIN2,OUTPUT);
  pinMode(PUMPD_PIN1,OUTPUT);
  pinMode(PUMPD_PIN2,OUTPUT);
  pinMode(PUMPA_SENSE,INPUT);
  pinMode(PUMPB_SENSE,INPUT);
  pinMode(PUMPC_SENSE,INPUT);
  pinMode(PUMPD_SENSE,INPUT);  
  InitSystem();
}


//!
//!             loop
//!
//! @brief      Function to run main code repeatedly.
//!
//! @detail     This function is an infinite loop that drives the digital I/O pins as 
//!             configured. 
//!
//! @param[in]  None
//!
//! @param[out] None
//!
//! @retval     None
//!
//!
void loop() 
{
  /*
  if((analogRead(PUMPA_SENSE) < VTGTHRESHOLD_CNT) || (analogRead(PUMPB_SENSE) < VTGTHRESHOLD_CNT) || (analogRead(PUMPC_SENSE) < VTGTHRESHOLD_CNT) || (analogRead(PUMPD_SENSE) < VTGTHRESHOLD_CNT))
  {
    analogSense = false;
    PumpStatus();
  }
  else
  {*/
    // Configuration for all Pumps in a single group
    digitalWrite(PUMPA_PIN1,HIGH);      
    digitalWrite(PUMPB_PIN1,HIGH);
    digitalWrite(PUMPC_PIN1,HIGH);
    digitalWrite(PUMPD_PIN1,HIGH);
    delay(delayVal);
    digitalWrite(PUMPA_PIN2,HIGH);
    digitalWrite(PUMPB_PIN2,HIGH);
    digitalWrite(PUMPC_PIN2,HIGH);
    digitalWrite(PUMPD_PIN2,HIGH);
    delay(delayVal);
    digitalWrite(PUMPA_PIN1,LOW);
    digitalWrite(PUMPB_PIN1,LOW);
    digitalWrite(PUMPC_PIN1,LOW);
    digitalWrite(PUMPD_PIN1,LOW);
    delay(delayVal);
    digitalWrite(PUMPA_PIN2,LOW);
    digitalWrite(PUMPB_PIN2,LOW);
    digitalWrite(PUMPC_PIN2,LOW);
    digitalWrite(PUMPD_PIN2,LOW);
   // delay(delayVal);
    delay(interval);
    if(CycleCount == COUNTER_OVERFLOW)
    {
      OverflowCount++;
      //LogData();
    }
    CycleCount++;
  //}

}

//!
//!             ParseCommand
//!
//! @brief      Function to parse serial command list.
//!
//! @detail     This function parses the command list array to return enum position.
//!
//! @param[in]  Cmd: String variable that holds the serial command.
//!             RANGE: Refer to enum SerialCmds for list of commands.
//!
//! @param[out] None
//!
//! @retval     -1 for failure else array index for valid Cmd.
//!
//!
int ParseCommand(String Cmd)
{
  int i;
  for(i=0;i<MAX_COMMANDS;i++)
  {
    if(Cmd == SerialCommands[i])
     return i;
  }
  return -1;
}


//!
//!             ProcessSerialMessage
//!
//! @brief      Function to process serial command.
//!
//! @detail     This function process the incoming serial command. It verifies if command type is 
//!             Read or Write and accordingly processes it in a switch case method.
//!
//! @param[in]  command: String variable that holds the serial command.
//!             RANGE: Refer to enum SerialCmds for list of commands.
//! @param[in]  newTime: Time value incase of Write command.
//!             RANGE: [40,65535].
//!
//! @param[out] None
//!
//! @retval     bool value 'false' for invalid command else true for success.
//!
//!  
bool ProcessSerialMessage(String command, unsigned int newTime)
{
  int cmd;
  int temp;
  if(command != "")
  {
    // get enum from string
    command.toUpperCase();
    cmd = ParseCommand(command);
    if((cmd == WRITETIME) || (cmd == INTERVAL)) // only for write commands to check time value limit.
    {
      if((newTime < MINIMUM_CYCLETIME) || (newTime > COUNTER_OVERFLOW))
      {
        Serial.write("Value not within required range. 40 >= val <= 65535 \n");
        return true;
      }
    }
    switch(cmd)
    {
      case READPROFILE:
        {
          EEPROM.get(EEPROM_ADDRESS,readData);    // Read configuration data from EEPROM;
          Serial.println("***Saved Cyclic Profile***");
          Serial.println("CycleCount    : "+ String(readData.activeCycleCount));
          Serial.println("OverflowCount : "+ String(readData.overflowCount));
          Serial.println("CycleTime     : "+ String(readData.cycleTime) + "ms\n");
        }
        break;    
      case WRITETIME:
        {
          // display last recorded data and reset profile
          Serial.println("***Current Runtime Data***");
          Serial.println("CycleCount    : " + String(CycleCount));          
          Serial.println("OverflowCount : " + String(OverflowCount));
          Serial.println("CycleTime     : " + String(configData.cycleTime) + "ms");
          //Serial.println("TotalRuntime in hours: ((CycleTime * CycleCount) + (CycleTime * OverflowCount * 65536)) / ( 3600 * 1000 )\n\n");
          temp = delayVal * NO_OF_STATES;
          delayVal = newTime/NO_OF_STATES ;
          configData.cycleTime = delayVal * NO_OF_STATES;
          CycleCount = 0;
          OverflowCount = 0;
          Serial.println("CycleTime changed from " + String(temp) + "ms to " + String(configData.cycleTime) + "ms\n");
          //LogData();
        }
        break; 
      case LOGDATA:
        {
          LogData();
        }  
        break;  
      case DISPDATA:
        {
          // Calculate total runtime duration
          Serial.println("***Current Runtime Data***");
          Serial.println("Current CycleCount    : " + String(CycleCount));          
          Serial.println("Current OverflowCount : " + String(OverflowCount));
          Serial.println("Current CycleTime     : " + String(configData.cycleTime) + "ms");
          Serial.println("T/4 state duration    : " + String(delayVal) + "ms\n");          
          /*
          Serial.println("Runtime duration in seconds: " + String(((configData.cycleTime * CycleCount) + (configData.cycleTime * OverflowCount * (COUNTER_OVERFLOW + 1)))/1000));
          Serial.println("Runtime duration in minutes: " + String(TotalRuntime/60000));
          Serial.println("Runtime duration in hours: " + String(TotalRuntime/3600000)+"\n");*/
        }  
        break;
      case INTERVAL:
        {
          Serial.println("Cooldown Interval updated\n");  
          interval = newTime;
        }
       break;
      case PAUSE:
        {
          // System Pause
          Serial.println("***System Paused***\n");
          Serial.println("Current CycleCount    : " + String(CycleCount));          
          Serial.println("Current OverflowCount : " + String(OverflowCount));
          Serial.println("Current CycleTime     : " + String(configData.cycleTime) + "ms");
          Serial.println("T/4 state duration    : " + String(delayVal) + "ms\n");                 
          while (!Serial.available()) 
          {
          }
          discardChar = (char)Serial.read();
          Serial.println("***System Resume***\n");
        }  
        break;        
       case RESET:
        {
            configData.activeCycleCount = 0;
            configData.overflowCount = 0;
            configData.cycleTime = CYCLETIME_PUMP;
            EEPROM.put(EEPROM_ADDRESS,configData);
            Serial.write("SYSTEM RESET TO DEFAULT SETTINGS. PRESS RESET BUTTON TO RESTART.\n");
        }
        break;
      case VERSION : 
        {
          Serial.println("S/W version no: "+String(versionNo/100)+"\n");
          Serial.println("Analog sense not available\n");
        }
        break;
      case MENU: 
        {
          ShowMenu();
        }
        break;        
      default:  
        return false;
        break;
    }
  }
  else
  {
    return false;
  }
  return true;
}


//!
//!             serialEvent
//!
//! @brief      Function to receive serial data.
//!
//! @detail     This function will receive serial data from USB or Rx pin and store 
//!             data in buffer. 
//!
//! @param[in]  None
//!
//! @param[out] None
//!
//! @retval     None
//!
//!  
void serialEvent() {
  while (Serial.available()) 
  {
    // get the new byte:
    char inChar = (char)Serial.read();
    int value = 0; 
    int delimitter = 0;
    bool rtnval=false;
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if (inChar == '\n') 
    {
      stringComplete = true;

      //split incoming message
      delimitter = inputString.indexOf(';'); 
      if(delimitter == -1)
      {
        Serial.write("INVALID COMMAND\n\n");
      }
      else
      {   
        value = inputString.substring(delimitter+1).toInt();
        inputString = inputString.substring(0,delimitter);
        rtnval = ProcessSerialMessage(inputString, value);
        if(false == rtnval)
          Serial.write("INVALID COMMAND\n\n");
      }
      //clear serial buffer      
      inputString = "";
    } 
    else
    {
      // add it to the inputString:
      inputString += inChar;
    }
  }
}


