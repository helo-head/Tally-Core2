#include <M5Core2.h>
#include <Fonts/EVA_20px.h>
#include <stdio.h>
#include <WiFi.h>
#include <EEPROM.h>


//Version 6 includes reading the eeprom for config data
//Version 7 added EEPROM config capability and CRC32 checking
//Version 8 added EEPROM config encyption

// Config File Reader header file from
// https://github.com/arduino-libraries/Arduino_CRC32
#include <Arduino_CRC32.h>

// Config File Reader header file from
// https://github.com/bneedhamia/sdconfigfile
#include <SDConfigFile.h>


//Cipher header/cpp files from 
// https://github.com/josephpal/esp32-Encrypt
#include <Cipher.h>


// ATEM header files from 
// https://github.com/kasperskaarhoj/SKAARHOJ-Open-Engineering/tree/master/ArduinoLibs
#include <SkaarhojPgmspace.h>
#include <ATEMstd.h>

//Define Cipher pre-key
#define CIPHER_PKEY "M5C2" 


//Create adruino crc32 object
Arduino_CRC32 crc32;

//Config file name, function, config file max length and return value
boolean postConfig = false; //Global boolen to allow event handler to determin how to handle button events
long fileNumber = -1; //Global variable to hold selected file array offset
const uint8_t CONFIG_LINE_LENGTH = 120; //Max config line lenght

boolean readSDconfig(String configFile,int configFileLength);
boolean readEEconfig(int configFileLength);
char   *readEEPROMString(int baseAddress, int stringNumber);
boolean addToEEPROM(int baseAddress, const char *text);
boolean writeEEconfig();
boolean didReadConfig;

// First address of EEPROM to write to.
const int START_ADDRESS = 0;

// Marks the end of the data written to EEPROM
const byte EEPROM_END_MARK = 255;

// Maximum bytes (including null) of any string we support.
const int EEPROM_MAX_STRING_LENGTH = 120;

//Define starting point for EEPROM write
int nextEEPROMaddress = START_ADDRESS;

//define if eeprom is to be written to when SD card missing or config file missing
boolean weeProm = false;

// Configuration data 
// Client ID and Network info and switch ip address defined in config file
const char* cfgVer;
const char* M5id;
const char* ssid;
const char* password;
const char* atemIp;

// Debug wait times defined in config file
boolean waitEnable = false;
int waitMS = 0;

//Place holder for ip conversion
uint8_t ip[4];
unsigned int tip[4];

//Defined current running marco and tallyStates array
int currentRunningMacro = -1;
int tallyStates[4];

/* macroOffset - Used to determin macro number based on button number + offset value
 * 0 for page 1, values 0 thru 3
 * 4 for page 2, values 4 thru 7
 * 8 for page 3, values 8 thru 11
 */
int macroOffset = 0;

#define CAMERA_OFF 0
#define CAMERA_PREVIEW 1
#define CAMERA_PROGRAM 2



//Defined button sizes and locations
#define BUTTON_SIZE 78
#define BUTTON_SPACE 2
#define CAMERA_BUTTON_Y 20 // 42
#define MACRO_BUTTON_Y 130 // 150

int buttonOneLocationX = BUTTON_SPACE;
int buttonTwoLocationX = (BUTTON_SPACE * 2)+ BUTTON_SIZE;
int buttonThreeLocationX = (BUTTON_SPACE * 3) + (BUTTON_SIZE * 2);
int buttonFourLocationX = (BUTTON_SPACE * 4) + (BUTTON_SIZE * 3);

//Create ATEM object  
ATEMstd AtemSwitcher;

void HaltProgram() {
  //M5.Lcd.fillScreen(BLACK); 
  M5.Lcd.fillRect(1,(M5.Lcd.height()/2) - (M5.Lcd.fontHeight()/2),M5.Lcd.width(),50,TFT_BLACK);
  M5.Lcd.drawCentreString("Shutting Down", (M5.Lcd.width()/2), (M5.Lcd.height()/2) - (M5.Lcd.fontHeight()/2), 1);

 for(int x=10; x > 0; x--) {
  M5.Lcd.fillRect(10,180,300,50,TFT_BLACK);
  M5.Lcd.progressBar(10,180,300,50,x*10);
  delay(1000);
 }
  
 M5.shutdown();
}


// Begining SD readFiles funtion
int readFiles(File dir, String fileNames[], int sizeOfArray, String extension)
{
int fileCount = 0;
String fileName[1];

while (fileCount < sizeOfArray)
  {
    File entry =  dir.openNextFile();
    if (! entry) return (fileCount);

    fileName[0] = entry.name();

    if (fileName[0].endsWith(extension))
    {
      fileNames[ fileCount ] = entry.name();
      fileCount++;
    }

  }
  return (fileCount);
}
// End readFiles funtion


// Choose Chonfiguration funtion
String chooseConfigFile()
{
  #define FILE_EXTENSION ".cfg"   // Define config file extension
  #define ARRAYSIZE 4 //maximum size of array to hold file names read from sd card
  #define FBUTTON_XSIZE 300 //filename button x size
  #define FBUTTON_YSIZE 50 //filename button y size
  #define FBUTTON_XLOC 10 //filename buttons x starting location
  #define FBUTTON_YLOC 60 //filename buttons y location multiplier 
  #define FBUTTON_RADIUS 6 // filename button corner radius size
  
  int font = 1; //text font number
  
  String results[ARRAYSIZE]; //array to hold filenames
  int count= 0; //used to hold filename count returned from readFiles function

  M5.begin(true, true, false, true); //Start the M5 subsystem
  M5.Lcd.fillScreen(BLACK); // Clear the screen

  M5.Lcd.setTextSize(3); // set text size to 30mm
  M5.Lcd.setTextColor(WHITE); // Set color to white

   //Defined the buttons
  TouchButton f0 = TouchButton(FBUTTON_XLOC, 1, FBUTTON_XSIZE, FBUTTON_YSIZE, "f0");
  TouchButton f1 = TouchButton(FBUTTON_XLOC, FBUTTON_YLOC, FBUTTON_XSIZE, FBUTTON_YSIZE, "f1");
  TouchButton f2 = TouchButton(FBUTTON_XLOC, (FBUTTON_YLOC*2), FBUTTON_XSIZE, FBUTTON_YSIZE, "f2");
  TouchButton f3 = TouchButton(FBUTTON_XLOC, (FBUTTON_YLOC*3), FBUTTON_XSIZE, FBUTTON_YSIZE, "f3");

   // Read the SD card
  File root = SD.open("/"); //Open the root file system
  count=readFiles(root, results, ARRAYSIZE, FILE_EXTENSION); // read the filenames that end with defined extension  

switch (count) {
    case 0:
      // No config files found on SD card
       return("");
    case 1:
      //If there is only one config file forgo the menu and just set fileNumber
      fileNumber = 0;
      break;
    case 2:
      // Two files found on disk so create a two file menu system
      M5.Lcd.fillRoundRect(FBUTTON_XLOC, 1, FBUTTON_XSIZE, FBUTTON_YSIZE, FBUTTON_RADIUS, TFT_DARKGREY);
      M5.Lcd.drawCentreString(results[0], FBUTTON_XLOC+(FBUTTON_XSIZE/2), 1+12, font);
      M5.Lcd.fillRoundRect(FBUTTON_XLOC, FBUTTON_YLOC, FBUTTON_XSIZE, FBUTTON_YSIZE, FBUTTON_RADIUS, TFT_DARKGREY);
      M5.Lcd.drawCentreString(results[1], FBUTTON_XLOC+(FBUTTON_XSIZE/2), FBUTTON_YLOC+12, font);
      break;
    case 3:
      // Three files found on disk so create a three file menu system 
      M5.Lcd.fillRoundRect(FBUTTON_XLOC, 1, FBUTTON_XSIZE, FBUTTON_YSIZE, FBUTTON_RADIUS, TFT_DARKGREY);
      M5.Lcd.drawCentreString(results[0], FBUTTON_XLOC+(FBUTTON_XSIZE/2), 1+12, font);
      M5.Lcd.fillRoundRect(FBUTTON_XLOC, FBUTTON_YLOC, FBUTTON_XSIZE, FBUTTON_YSIZE, FBUTTON_RADIUS, TFT_DARKGREY);
      M5.Lcd.drawCentreString(results[1], FBUTTON_XLOC+(FBUTTON_XSIZE/2), FBUTTON_YLOC+12, font);
      M5.Lcd.fillRoundRect(FBUTTON_XLOC, FBUTTON_YLOC*2, FBUTTON_XSIZE, FBUTTON_YSIZE, FBUTTON_RADIUS, TFT_DARKGREY);
      M5.Lcd.drawCentreString(results[2], FBUTTON_XLOC+(FBUTTON_XSIZE/2), (FBUTTON_YLOC*2)+12, font);
      break;
    default: 
      // Covers cases where count is >= 4. In this case we limit the list to the first 4
      M5.Lcd.fillRoundRect(FBUTTON_XLOC, 1, FBUTTON_XSIZE, FBUTTON_YSIZE, FBUTTON_RADIUS, TFT_DARKGREY);
      M5.Lcd.drawCentreString(results[0], FBUTTON_XLOC+(FBUTTON_XSIZE/2), 1+12, font);
      M5.Lcd.fillRoundRect(FBUTTON_XLOC, FBUTTON_YLOC, FBUTTON_XSIZE, FBUTTON_YSIZE, FBUTTON_RADIUS, TFT_DARKGREY);
      M5.Lcd.drawCentreString(results[1], FBUTTON_XLOC+(FBUTTON_XSIZE/2), FBUTTON_YLOC+12, font);
      M5.Lcd.fillRoundRect(FBUTTON_XLOC, FBUTTON_YLOC*2, FBUTTON_XSIZE, FBUTTON_YSIZE, FBUTTON_RADIUS, TFT_DARKGREY);
      M5.Lcd.drawCentreString(results[2], FBUTTON_XLOC+(FBUTTON_XSIZE/2), (FBUTTON_YLOC*2)+12, font);
      M5.Lcd.fillRoundRect(FBUTTON_XLOC, FBUTTON_YLOC*3, FBUTTON_XSIZE, FBUTTON_YSIZE, FBUTTON_RADIUS, TFT_DARKGREY);
      M5.Lcd.drawCentreString(results[3], FBUTTON_XLOC+(FBUTTON_XSIZE/2), (FBUTTON_YLOC*3)+12, font);
      break;
  }

 while (fileNumber == -1) 
 {
    delay(50);
    M5.update();
 } 

M5.Lcd.fillScreen(BLACK);
M5.Lcd.drawCentreString(results[fileNumber], FBUTTON_XLOC+(FBUTTON_XSIZE/2), (M5.Lcd.height()/2) - (M5.Lcd.fontHeight()/2), font);
delay(500);

return(results[fileNumber]);

}// end chooseConfig function


/*
 * Read configuration settings from EEProm.
 * Returns true if successful, false if it failed.
 * Failures can includ; no data, missing data or failed CRC
 */
boolean readEEconfig() {

uint32_t crc32_results = 0; //Calculated CRC32
uint32_t eeCRC; //CRC

char * eeCRCEncrypted; //Encrypted EEPROM CRC

//Create cipher object, key and working place holders
Cipher * cipher = new Cipher();
char cipherKey[17];

String decryptedString;

//Generate second part of key based on 12 byte mac address
uint64_t chipid = ESP.getEfuseMac(); // The chip ID is essentially its MAC address(length: 6 bytes).
uint16_t chip = (uint16_t)(chipid >> 32);

//Generate fully key based on pre-key + hardware mac address
snprintf(cipherKey, 17, CIPHER_PKEY "%04X%08X", chip, (uint32_t)chipid); 

//Set the key  
cipher->setKey(cipherKey); 

//Read config vaulues from EEPROM
cfgVer = readEEPROMString(START_ADDRESS,CONFIG_LINE_LENGTH, 0);
M5id = readEEPROMString(START_ADDRESS,CONFIG_LINE_LENGTH, 1);
ssid = readEEPROMString(START_ADDRESS,CONFIG_LINE_LENGTH, 2);
password = readEEPROMString(START_ADDRESS,CONFIG_LINE_LENGTH, 3);
atemIp = readEEPROMString(START_ADDRESS,CONFIG_LINE_LENGTH, 4);
eeCRCEncrypted = readEEPROMString(START_ADDRESS,CONFIG_LINE_LENGTH, 5);

 if (cfgVer == 0 || M5id == 0 || ssid == 0 || password == 0 || atemIp == 0 ) {
    Serial.println(F("EEPROM has not yet been initialized or incomplete dataset."));
    return(false);
  } else {


// Defined String to hold decrypted data
String decryptedString;

//Defined new string arrays to hold decrypted data copied from decrypted String. This solves the decrypted String scope problem. 
char * cfgVerDecrypted = new char[strlen(cfgVer)+1];
char * M5idDecrypted = new char[strlen(M5id)+1];
char * ssidDecrypted = new char[strlen(ssid)+1];
char * passwordDecrypted = new char[strlen(password)+1];
char * atemIpDecrypted = new char[strlen(atemIp)+1];
char * eeCRCDecrypted = new char[strlen(eeCRCEncrypted)+1];


//Decrypt the config strings, then copy decrtypedString to string allocated memory then assign global pointer to  allocated memory
    decryptedString = cipher->decryptString(String(cfgVer));
    decryptedString.toCharArray(cfgVerDecrypted, strlen(cfgVerDecrypted));
    cfgVer = cfgVerDecrypted;
    decryptedString = cipher->decryptString(String(M5id));
    decryptedString.toCharArray(M5idDecrypted, strlen(M5idDecrypted));
    M5id = M5idDecrypted;
    decryptedString = cipher->decryptString(String(ssid));
    decryptedString.toCharArray(ssidDecrypted, strlen(ssidDecrypted));
    ssid = ssidDecrypted;
    decryptedString = cipher->decryptString(String(password));
    decryptedString.toCharArray(passwordDecrypted, strlen(passwordDecrypted));
    password = passwordDecrypted;
    decryptedString = cipher->decryptString(String(atemIp));
    decryptedString.toCharArray(atemIpDecrypted, strlen(atemIpDecrypted));
    atemIp = atemIpDecrypted;
    decryptedString = cipher->decryptString(String(eeCRCEncrypted));
    decryptedString.toCharArray(eeCRCDecrypted, strlen(eeCRCDecrypted));
    
  //Convert CRC back to unsigned long after decryption 
    eeCRC = strtoul (eeCRCDecrypted, NULL,0);

    
 //Caldulate CRC values. 
 // Unlike Zlib's crc32_combine this simply adds the values together to provide a reasonable assurance
 // the values were not corruped while writing the EEPROM.  
    crc32_results =  crc32.calc((uint8_t const *)cfgVer, 1); // strlen(cfgVer));
    crc32_results =  crc32_results + crc32.calc((uint8_t const *)M5id, strlen(M5id));
    crc32_results =  crc32_results + crc32.calc((uint8_t const *)ssid, strlen(ssid));
    crc32_results =  crc32_results + crc32.calc((uint8_t const *)password, strlen(password));
    crc32_results =  crc32_results + crc32.calc((uint8_t const *)atemIp, strlen(atemIp));

 
    if (crc32_results != eeCRC) {
    Serial.print(crc32_results);
    Serial.print(" : ");
    Serial.println(eeCRC);
    Serial.println(F("Calculated vs saved CRC not equal"));
    return(false);
    }
 }
 return(true);
}

//EEProm Reader
char *readEEPROMString(int baseAddress, int maxLength, int stringNumber) {
  int start;   // EEPROM address of the first byte of the string to return.
  int length;  // length (bytes) of the string to return, less the terminating null.
  char ch;
  int nextAddress;  // next address to read from EEPROM.
  char *result;     // points to the dynamically-allocated result to return.
  int i;

#if defined(ESP8266) || defined(ESP32)
  EEPROM.begin(512);
#endif

  nextAddress = baseAddress;
  for (i = 0; i < stringNumber; ++i) {

    // If the first byte is an end mark, we've run out of strings too early.
    ch = (char) EEPROM.read(nextAddress++);
    if (ch == (char) EEPROM_END_MARK) {
#if defined(ESP8266) || defined(ESP32)
      EEPROM.end();
#endif
      return (char *) 0;  // not enough strings are in EEPROM.
    }

    // Read through the string's terminating null (0).
    int length = 0;
    while (ch != '\0' && length < maxLength - 1) {
      ++length;
      ch = EEPROM.read(nextAddress++);
    }
  }

  // We're now at the start of what should be our string.
  start = nextAddress;

  // If the first byte is an end mark, we've run out of strings too early.
  ch = (char) EEPROM.read(nextAddress++);
  if (ch == (char) EEPROM_END_MARK) {
#if defined(ESP8266) || defined(ESP32)
    EEPROM.end();
#endif
    return (char *) 0;  // not enough strings are in EEPROM.
  }

  // Count to the end of this string.
  length = 0;
  while (ch != '\0' && length < maxLength - 1) {
    ++length;
    ch = EEPROM.read(nextAddress++);
  }

  // Allocate space for the string, then copy it.
  result = new char[length + 1];
  nextAddress = start;
  for (i = 0; i < length; ++i) {
    result[i] = (char) EEPROM.read(nextAddress++);
  }
  result[i] = '\0';

  return result;

} // End EEProm Reader


boolean writeEEconfig() {

/*  Writes configuration data + crc32 to EEPROM
*/
uint32_t crc32_results = 0;

const char * encryptedData;
String workingData;

//Used to convert crc2_reults to string to be written to EEPROM
char cfgCRC[sizeof(unsigned int)*8+1];

//Create cipher object and key
Cipher * cipher = new Cipher();
char cipherKey[17];

//Generate second part of key based on 12 byte mac address
uint64_t chipid = ESP.getEfuseMac(); // The chip ID is essentially its MAC address(length: 6 bytes).
uint16_t chip = (uint16_t)(chipid >> 32);

//Generate fully key based on pre-key + hardware mac address
snprintf(cipherKey, 17, CIPHER_PKEY "%04X%08X", chip, (uint32_t)chipid); 

//Set the key  
cipher->setKey(cipherKey);

//Generate CRC32 value for each value
// Unlike Zlib's crc32_combine this simply adds the values together to provide a reasonable assurance
// the values were not corruped while writing the EEPROM. 

  crc32_results =  crc32.calc((uint8_t const *)cfgVer, strlen(cfgVer));
  crc32_results =  crc32_results + crc32.calc((uint8_t const *)M5id, strlen(M5id));
  crc32_results =  crc32_results + crc32.calc((uint8_t const *)ssid, strlen(ssid));
  crc32_results =  crc32_results + crc32.calc((uint8_t const *)password, strlen(password));
  crc32_results =  crc32_results + crc32.calc((uint8_t const *)atemIp, strlen(atemIp));
  

//convert crc restults to string to write to eeprom
sprintf(cfgCRC, "%u", crc32_results);

//Write to eprom
    
    workingData = cipher->encryptString(String(cfgVer));
    encryptedData = workingData.c_str();
    if (!addToEEPROM(START_ADDRESS, encryptedData)) {
      Serial.println(F("Write cfgVer failed.  Reset to try again."));
      return(false);
    }
    workingData = cipher->encryptString(String(M5id));
    encryptedData = workingData.c_str();
    if (!addToEEPROM(START_ADDRESS, encryptedData)) {
      Serial.println(F("Write M5id failed.  Reset to try again."));
      return(false);
    }
    workingData = cipher->encryptString(String(ssid));
    encryptedData = workingData.c_str();
    if (!addToEEPROM(START_ADDRESS, encryptedData)) {
      Serial.println(F("Write ssid failed.  Reset to try again."));
      return(false);
    }
    workingData = cipher->encryptString(String(password));
    encryptedData = workingData.c_str();
    if (!addToEEPROM(START_ADDRESS, encryptedData)) {
      Serial.println(F("Write password failed.  Reset to try again."));
      return(false);
    }
    workingData = cipher->encryptString(String(atemIp));
    encryptedData = workingData.c_str();
    if (!addToEEPROM(START_ADDRESS, encryptedData)) {
      Serial.println(F("Write atemIp failed.  Reset to try again."));
      return(false);
    }
    workingData = cipher->encryptString(String(cfgCRC));
    encryptedData = workingData.c_str();   
    if (!addToEEPROM(START_ADDRESS, encryptedData)) {
      Serial.println(F("Write CRC failed.  Reset to try again."));
      return(false);
    }
    if (!addToEEPROM(EEPROM_END_MARK, "EEPROM_END_MARK")) {
      Serial.println(F("Write EEPROM_END_MARK failed.  Reset to try again."));
      return(false);
    }



return(true);

} //End writeEEconfig()

/*
 * Append the given null-terminated string to the EEPROM.
 * Return true if successful; false otherwise.
 */
boolean addToEEPROM(int baseAddress, const char *text) {
  // Unfortunately, we can't know how much EEPROM is left.

static int nextEEPROMaddress = baseAddress;

#if defined(ESP8266) || defined(ESP32)
  /*
   * The ESP8266 and ESP32 EEPROM library differs from
   * the standard Arduino library. It is a cached model,
   * I assume to minimize limited EEPROM write cycles.
   */
  EEPROM.begin(512);
#endif

  if (baseAddress == EEPROM_END_MARK) {
    EEPROM.write(nextEEPROMaddress++, EEPROM_END_MARK);

  } else {  
    do {
        EEPROM.write(nextEEPROMaddress++, (byte) *text);
    }   while (*text++ != '\0');
  }
  
#if defined(ESP8266) || defined(ESP32)
  EEPROM.end();
#endif
  return (true);
} // End addToEEPROM()


/*
 * Read our settings from our SD configuration file.
 * Returns true if successful, false if it failed.
 */
boolean readSDconfig(String configFile, int configLineLength) {

  // Defined config file object
  SDConfigFile cfg;

  //convert string to constant charater 
  const char * cfName = configFile.c_str();

  //Define keyCount count;
  int keyCount = 0;

  // Open the configuration file.
  if (!cfg.begin(cfName, configLineLength)) {
    Serial.print("Failed to open configuration file: ");
    Serial.println(cfName);
    return (false);
  }

 
  // Read each setting from the file.
  while (cfg.readNextSetting()) {

    if (cfg.nameIs("weeProm")) {
      weeProm = cfg.getBooleanValue();
    
    } else if (cfg.nameIs("cfgVer")) {
      cfgVer = cfg.copyValue();
      
    } else if (cfg.nameIs("waitEnable")) {
      waitEnable = cfg.getBooleanValue();
        
      // waitMS integer
    } else if (cfg.nameIs("waitMS")) {
      waitMS = cfg.getIntValue();

      // Tally ID (char *)
    } else if (cfg.nameIs("M5id")) {
      M5id = cfg.copyValue();
    
      // ssid string (char *)
    } else if (cfg.nameIs("ssid")) {
      ssid = cfg.copyValue();
      keyCount++;

      // password string (char *)
    } else if (cfg.nameIs("password")) {
      password = cfg.copyValue();
      keyCount++;
      
      // switch string (char *)
    } else if (cfg.nameIs("atemIp")) {
      atemIp = cfg.copyValue();
      keyCount++;

    } else {
      // report unrecognized names.
      Serial.print("Unknown key name in config: ");
      Serial.println(cfg.getName());
    }
  }

  // clean up
  cfg.end();

 //Required key config values is three (3)
 if (keyCount < 3) return(false); else return(true);
 
} // end readSDConfig

// Function to convert IPaddress to strings
String ip2Str(IPAddress ip){
  String s="";
  for (int i=0; i<4; i++) {
    s += i  ? "." + String(ip[i]) : String(ip[i]);
  }
  return s;
} // end of ip2Str

// Button press function
void buttonWasPressed(TouchEvent& e) {
  TouchButton& tbtn = *e.button;
  char buttonType = tbtn.name[0];
  int buttonNumber = tbtn.name[1]-'0';

if (!postConfig) {
  fileNumber = tbtn.name[1]-48; //Hack to convert ascii char to long
  M5.Lcd.fillRect(tbtn.x, tbtn.y, tbtn.w, tbtn.h, tbtn.isPressed() ? WHITE : BLACK);

} else {
  
  if(buttonType == 'c') {
    AtemSwitcher.changeProgramInput(buttonNumber);
    } else {
    updateMacroButton(buttonNumber,macroOffset, tbtn.isPressed(), false);
    AtemSwitcher.setMacroAction(buttonNumber+macroOffset, 0); 
    }
  }
} //end buttonWasPressed

// Draw camera buttons
void drawCameraButton(int buttonNumber, int state) {
  int font = 1;
  M5.Lcd.setTextSize(6);
  
  M5.Lcd.setTextColor(state == CAMERA_OFF ? WHITE : BLACK);

  int buttonColor = TFT_DARKGREY;
  
  switch(state) 
  {
    case CAMERA_OFF:
      buttonColor = TFT_DARKGREY; break;
    case CAMERA_PREVIEW:
      buttonColor = TFT_GREEN; break;
    case CAMERA_PROGRAM:
      buttonColor = TFT_RED; break;
  }

  switch(buttonNumber) 
  {
    case 1:
      M5.Lcd.fillRoundRect(buttonOneLocationX, CAMERA_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, 6, buttonColor);
      M5.Lcd.drawCentreString("1", 2+buttonOneLocationX+(BUTTON_SIZE/2), CAMERA_BUTTON_Y+18, font);
      break;
    case 2:
      M5.Lcd.fillRoundRect(buttonTwoLocationX, CAMERA_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, 6, buttonColor);
      M5.Lcd.drawCentreString("2", 2+buttonTwoLocationX+(BUTTON_SIZE/2), CAMERA_BUTTON_Y+18, font);
      break;
    case 3:
      M5.Lcd.fillRoundRect(buttonThreeLocationX, CAMERA_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, 6, buttonColor);
      M5.Lcd.drawCentreString("3", 2+buttonThreeLocationX+(BUTTON_SIZE/2), CAMERA_BUTTON_Y+18, font);
      break;
    case 4:
      M5.Lcd.fillRoundRect(buttonFourLocationX, CAMERA_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, 6, buttonColor);
      M5.Lcd.drawCentreString("4", 2+buttonFourLocationX+(BUTTON_SIZE/2), CAMERA_BUTTON_Y+18, font);
      break;
  }
} // end drawCameraButton

//Update Macro button function
void updateMacroButton(int buttonNumber, int macroOffset, bool isPressed, bool isRunning) {
  int font = 1;
  M5.Lcd.setTextSize(6);
  
  M5.Lcd.setTextColor(isPressed || isRunning ? BLACK : WHITE);
  int buttonColor = isPressed ? WHITE : (isRunning ? TFT_RED : TFT_DARKGREY);

  buttonNumber = buttonNumber + macroOffset;
  
  switch(buttonNumber) {
    case 0:
      M5.Lcd.fillRoundRect(buttonOneLocationX, MACRO_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, 6, buttonColor);
      M5.Lcd.drawCentreString("0", 2+buttonOneLocationX+(BUTTON_SIZE/2), MACRO_BUTTON_Y+18, font);
      break;
    case 1:
      M5.Lcd.fillRoundRect(buttonTwoLocationX, MACRO_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, 6, buttonColor);
      M5.Lcd.drawCentreString("1", 2+buttonTwoLocationX+(BUTTON_SIZE/2), MACRO_BUTTON_Y+18, font);
      break;
    case 2:
      M5.Lcd.fillRoundRect(buttonThreeLocationX, MACRO_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, 6, buttonColor);
      M5.Lcd.drawCentreString("2", 2+buttonThreeLocationX+(BUTTON_SIZE/2), MACRO_BUTTON_Y+18, font);
      break;
    case 3:
      M5.Lcd.fillRoundRect(buttonFourLocationX, MACRO_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, 6, buttonColor);
      M5.Lcd.drawCentreString("3", 2+buttonFourLocationX+(BUTTON_SIZE/2), MACRO_BUTTON_Y+18, font);
      break;
    case 4:
      M5.Lcd.fillRoundRect(buttonOneLocationX, MACRO_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, 6, buttonColor);
      M5.Lcd.drawCentreString("4", 2+buttonOneLocationX+(BUTTON_SIZE/2), MACRO_BUTTON_Y+18, font);
      break;
    case 5:
      M5.Lcd.fillRoundRect(buttonTwoLocationX, MACRO_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, 6, buttonColor);
      M5.Lcd.drawCentreString("5", 2+buttonTwoLocationX+(BUTTON_SIZE/2), MACRO_BUTTON_Y+18, font);
      break;
    case 6:
      M5.Lcd.fillRoundRect(buttonThreeLocationX, MACRO_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, 6, buttonColor);
      M5.Lcd.drawCentreString("6", 2+buttonThreeLocationX+(BUTTON_SIZE/2), MACRO_BUTTON_Y+18, font);
      break;
    case 7:
      M5.Lcd.fillRoundRect(buttonFourLocationX, MACRO_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, 6, buttonColor);
      M5.Lcd.drawCentreString("7", 2+buttonFourLocationX+(BUTTON_SIZE/2), MACRO_BUTTON_Y+18, font);
      break;
    case 8:
      M5.Lcd.fillRoundRect(buttonOneLocationX, MACRO_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, 6, buttonColor);
      M5.Lcd.drawCentreString("8", 2+buttonOneLocationX+(BUTTON_SIZE/2), MACRO_BUTTON_Y+18, font);
      break;
    case 9:
      M5.Lcd.fillRoundRect(buttonTwoLocationX, MACRO_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, 6, buttonColor);
      M5.Lcd.drawCentreString("9", 2+buttonTwoLocationX+(BUTTON_SIZE/2), MACRO_BUTTON_Y+18, font);
      break;
    case 10:
      M5.Lcd.fillRoundRect(buttonThreeLocationX, MACRO_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, 6, buttonColor);
      M5.Lcd.drawCentreString("10", 2+buttonThreeLocationX+(BUTTON_SIZE/2), MACRO_BUTTON_Y+18, font);
      break;
    case 11:
      M5.Lcd.fillRoundRect(buttonFourLocationX, MACRO_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, 6, buttonColor);
      M5.Lcd.drawCentreString("11", 2+buttonFourLocationX+(BUTTON_SIZE/2), MACRO_BUTTON_Y+18, font);
      break;
      }
} // end updateMacroButton

//Draw Macro button function
void drawMacroButtons(int macroOffset) {
  int font = 1;
  
  M5.Lcd.setTextSize(6);
  M5.Lcd.setTextColor(WHITE);
  
  switch(macroOffset) {
    case 0:
      M5.Lcd.fillRoundRect(buttonOneLocationX, MACRO_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, 6, TFT_DARKGREY);  
      M5.Lcd.drawCentreString("0", 2+buttonOneLocationX+(BUTTON_SIZE/2), MACRO_BUTTON_Y+18, font);
      M5.Lcd.fillRoundRect(buttonTwoLocationX, MACRO_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, 6, TFT_DARKGREY);
      M5.Lcd.drawCentreString("1", 2+buttonTwoLocationX+(BUTTON_SIZE/2), MACRO_BUTTON_Y+18, font);
      M5.Lcd.fillRoundRect(buttonThreeLocationX, MACRO_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, 6, TFT_DARKGREY);
      M5.Lcd.drawCentreString("2", 2+buttonThreeLocationX+(BUTTON_SIZE/2), MACRO_BUTTON_Y+18, font); 
      M5.Lcd.fillRoundRect(buttonFourLocationX, MACRO_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, 6, TFT_DARKGREY);
      M5.Lcd.drawCentreString("3", 2+buttonFourLocationX+(BUTTON_SIZE/2), MACRO_BUTTON_Y+18, font);
      break;
    case 4:
      M5.Lcd.fillRoundRect(buttonOneLocationX, MACRO_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, 6, TFT_DARKGREY);
      M5.Lcd.drawCentreString("4", 2+buttonOneLocationX+(BUTTON_SIZE/2), MACRO_BUTTON_Y+18, font);
      M5.Lcd.fillRoundRect(buttonTwoLocationX, MACRO_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, 6, TFT_DARKGREY);
      M5.Lcd.drawCentreString("5", 2+buttonTwoLocationX+(BUTTON_SIZE/2), MACRO_BUTTON_Y+18, font);
      M5.Lcd.fillRoundRect(buttonThreeLocationX, MACRO_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, 6, TFT_DARKGREY);
      M5.Lcd.drawCentreString("6", 2+buttonThreeLocationX+(BUTTON_SIZE/2), MACRO_BUTTON_Y+18, font);
      M5.Lcd.fillRoundRect(buttonFourLocationX, MACRO_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, 6, TFT_DARKGREY);
      M5.Lcd.drawCentreString("7", 2+buttonFourLocationX+(BUTTON_SIZE/2), MACRO_BUTTON_Y+18, font);
      break;
    case 8:
      M5.Lcd.fillRoundRect(buttonOneLocationX, MACRO_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, 6, TFT_DARKGREY);
      M5.Lcd.drawCentreString("8", 2+buttonOneLocationX+(BUTTON_SIZE/2), MACRO_BUTTON_Y+18, font);
      M5.Lcd.fillRoundRect(buttonTwoLocationX, MACRO_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, 6, TFT_DARKGREY);
      M5.Lcd.drawCentreString("9", 2+buttonTwoLocationX+(BUTTON_SIZE/2), MACRO_BUTTON_Y+18, font);
      M5.Lcd.fillRoundRect(buttonThreeLocationX, MACRO_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, 6, TFT_DARKGREY);
      M5.Lcd.drawCentreString("10", 2+buttonThreeLocationX+(BUTTON_SIZE/2), MACRO_BUTTON_Y+18, font);
      M5.Lcd.fillRoundRect(buttonFourLocationX, MACRO_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, 6, TFT_DARKGREY);
      M5.Lcd.drawCentreString("11", 2+buttonFourLocationX+(BUTTON_SIZE/2), MACRO_BUTTON_Y+18, font);
      break;
  }
} //end drawMacroButtons

// Setup button function
void setupButtons() {
  
  M5.Lcd.fillScreen(BLACK);

  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.drawString("CAMERAS", 6, CAMERA_BUTTON_Y-20, 1);
  M5.Lcd.drawString("MACROS", 6, MACRO_BUTTON_Y-20, 1);

  drawMacroButtons(macroOffset);
  
  drawCameraButton(1, CAMERA_OFF);
  drawCameraButton(2, CAMERA_OFF);
  drawCameraButton(3, CAMERA_OFF);
  drawCameraButton(4, CAMERA_OFF);

  M5.Lcd.fillCircle(52, 230, 7, TFT_WHITE);
  M5.Lcd.drawCircle(158, 230, 7, TFT_WHITE);
  M5.Lcd.drawCircle(264, 230, 7, TFT_WHITE);

} // end setupButtons


// INITIAL SETUP
void setup() {
  
  //Start the M5 system
  M5.begin(true, true, false, true);

  M5.Lcd.fillScreen(BLACK); // Clear the screen
  M5.Lcd.setRotation(1); // Set rotation on its side so reasonable sized message text will fit
  M5.Lcd.setTextSize(3); // Set text size to mid level
  
  Serial.begin(115200);
  //Define the touch handler
  M5.Touch.addHandler(buttonWasPressed, TE_BTNONLY + TE_TOUCH + TE_RELEASE);
  
  // Setup the SD card 
  if (!SD.begin()) {
    M5.Lcd.drawCentreString("SD Card Failure", (M5.Lcd.width()/2), (M5.Lcd.height()/3) - (M5.Lcd.fontHeight()/2), 1);
    M5.Lcd.drawCentreString("Reading EEProm", (M5.Lcd.width()/2), (M5.Lcd.height()/2) - (M5.Lcd.fontHeight()/2), 1);
    delay(2000);
    if (!readEEconfig()) {
      M5.Lcd.fillRect(1,(M5.Lcd.height()/2) - (M5.Lcd.fontHeight()/2),M5.Lcd.width(),50,TFT_BLACK);
      M5.Lcd.drawCentreString("Invalid EEProm", (M5.Lcd.width()/2), (M5.Lcd.height()/2) - (M5.Lcd.fontHeight()/2), 1);
      delay(3000);
      HaltProgram();
    }

  } else {
     // Choose the configuration file assuming one exists
      String configFile;

      configFile = chooseConfigFile();

     if (configFile == "") {
        M5.Lcd.drawCentreString("No " FILE_EXTENSION " Files!", (M5.Lcd.width()/2), (M5.Lcd.height()/3) - (M5.Lcd.fontHeight()/2), 1);
        delay(3000);
        HaltProgram();
      }
  
     // Read our configuration from the SD card file.
     didReadConfig = readSDconfig(configFile,CONFIG_LINE_LENGTH);
    
     if (!didReadConfig) {
       M5.Lcd.drawCentreString("Bad Config File", (M5.Lcd.width()/2), (M5.Lcd.height()/3) - (M5.Lcd.fontHeight()/2), 1);
        delay(3000);
        HaltProgram();
      }

  }//end if/else

  postConfig = true;

  if(weeProm) {
    if(!writeEEconfig()) {
      M5.Lcd.drawCentreString("Error writing EEPROM", (M5.Lcd.width()/2), (M5.Lcd.height()/3) - (M5.Lcd.fontHeight()/2), 1);
      delay(3000);
      HaltProgram();
    }
  }


 //Assign ATEM IP address. This is a poor mans way to avoid the issues with type conversion between uint and uint8_t.
  sscanf(atemIp, "%u.%u.%u.%u", &tip[0], &tip[1], &tip[2], &tip[3]);
  ip[0] = lowByte(tip[0]);
  ip[1] = lowByte(tip[1]);
  ip[2] = lowByte(tip[2]);
  ip[3] = lowByte(tip[3]);
  

  IPAddress switchIp(ip);

  // Startup Wifi
  WiFi.begin(ssid, password);
  
  M5.Lcd.fillScreen(BLACK); // Clear the screen
  M5.Lcd.setTextDatum(TC_DATUM); // Set default for text to top center 
  M5.Lcd.drawString(M5id, (M5.Lcd.width()/2),20); //  Display name of device
  M5.Lcd.drawString(WiFi.macAddress(), (M5.Lcd.width()/2),60); //Display name of device
  M5.Lcd.drawString("Searching for",(M5.Lcd.width()/2),140); 
  M5.Lcd.drawString(ssid,(M5.Lcd.width()/2),180); // Display ATEM SSID so users know what SSID is being used

  //Wait for Wifi to connect
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  M5.Lcd.fillScreen(BLACK); // Clear the screen after connecting to wifi
  M5.Lcd.drawString(M5id, (M5.Lcd.width()/2),20); //Display name of device
  M5.Lcd.drawString(WiFi.localIP().toString(), (M5.Lcd.width()/2),60); //Display ip address of device
  M5.Lcd.drawString("Connecting to ATEM",(M5.Lcd.width()/2),140);
  M5.Lcd.drawString(ip2Str(switchIp), (M5.Lcd.width()/2),180); //Display ip address of device

        
  // If a debugging wait was enabled and defined use it
    if (waitEnable) {
        delay(waitMS);
    } 

  //Start the ATEM switch and try connect
  AtemSwitcher.begin(switchIp);
  AtemSwitcher.serialOutput(0x80);
  AtemSwitcher.connect(); //Note: it may take up to 10-15 seconds to connect

  //Call the button setup function
  setupButtons();
 
} // end setup


void loop() {
  
  static TouchButton c1 = TouchButton(buttonOneLocationX, CAMERA_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, "c1");
  static TouchButton c2 = TouchButton(buttonTwoLocationX, CAMERA_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, "c2");
  static TouchButton c3 = TouchButton(buttonThreeLocationX, CAMERA_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, "c3");
  static TouchButton c4 = TouchButton(buttonFourLocationX,   CAMERA_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, "c4");

  static TouchButton m0 = TouchButton(buttonOneLocationX, MACRO_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, "m0");
  static TouchButton m1 = TouchButton(buttonTwoLocationX, MACRO_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, "m1");
  static TouchButton m2 = TouchButton(buttonThreeLocationX, MACRO_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, "m2");
  static TouchButton m3 = TouchButton(buttonFourLocationX, MACRO_BUTTON_Y, BUTTON_SIZE, BUTTON_SIZE, "m3");

  //Check M5 button status and update if appropriate
  if (M5.BtnA.wasPressed()) {
    macroOffset = 0;
    drawMacroButtons(macroOffset);
    M5.Lcd.fillRect(0,220,320,20,TFT_BLACK);
    M5.Lcd.fillCircle(52, 230, 7, TFT_WHITE); // button a
    M5.Lcd.drawCircle(158, 230, 7, TFT_WHITE); // button b
    M5.Lcd.drawCircle(264, 230, 7, TFT_WHITE); // button c
  }

  if (M5.BtnB.wasPressed()) {
    macroOffset = 4;
    drawMacroButtons(macroOffset);
    M5.Lcd.fillRect(0,220,320,20,TFT_BLACK);
    M5.Lcd.drawCircle(52, 230, 7, TFT_WHITE); // button a
    M5.Lcd.fillCircle(158, 230, 7, TFT_WHITE); // button b
    M5.Lcd.drawCircle(264, 230, 7, TFT_WHITE); // button c
  }

   if (M5.BtnC.wasPressed()) {
    macroOffset = 8;
    drawMacroButtons(macroOffset);
    M5.Lcd.fillRect(0,220,320,20,TFT_BLACK);
    M5.Lcd.drawCircle(52, 230, 7, TFT_WHITE); // button a
    M5.Lcd.drawCircle(158, 230, 7, TFT_WHITE); // button b
    M5.Lcd.fillCircle(264, 230, 7, TFT_WHITE); // button c
  }

  //Update the M5 subsystem
  M5.update(); 
  
  //Run the ATEM loop
  AtemSwitcher.runLoop(); 

  //Determin ATEM connection status
  if (AtemSwitcher.isConnected() == true) {
    M5.Lcd.fillCircle(312,7, 7, TFT_GREEN); //ATEM connected
  }else {
    M5.Lcd.fillCircle(312,7, 7, TFT_RED); //ATEM not connected
  }

  //Determin Tally State
  for(int i=1; i<=4; i++) {
    int currentTallyState = AtemSwitcher.getProgramTally(i) ? CAMERA_PROGRAM : (AtemSwitcher.getPreviewTally(i) ? CAMERA_PREVIEW : CAMERA_OFF);

    if(currentTallyState != tallyStates[i-1]) {
     drawCameraButton(i, currentTallyState);
    }
    
    tallyStates[i-1] = currentTallyState;
  }
 
}
