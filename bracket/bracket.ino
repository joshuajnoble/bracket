#include "DataFlash.h"

#include <TinyGPS++.h>
#include <RFduinoBLE.h>
#include <SPI.h>

DataFlash dataflash;

#define DATAPAGE_SIZE 2048
#define CENTROID_PAGES 8
#define TRACT_PAGES 16

///////////////
// do we have an income yet?

// select a flash page that isn't in use (see Memory.h for more info)
#define  MY_FLASH_PAGE  251

// double level of indirection required to get gcc
// to apply the stringizing operator correctly
#define  str(x)   xstr(x)
#define  xstr(x)  #x

///////////////

#define SHOCK_PIN 2
#define SD_PIN 6

uint16_t currentLat, currentLon;
uint16_t distLat, distLon;
unsigned long long id;

uint16_t tractIncome, income;

float tempLat, tempLon;
TinyGPSPlus gps;

bool waitingForIncome = true;

struct location {
  uint16_t lat, lon, id;
};

void setup()
{
   Serial.begin(4800);
   
   pinMode(SHOCK_PIN, OUTPUT);

   // Initialize dataflash, 6 turns on both GPS and flashmem
   dataflash.setup(6);
  
   
   // a flash page is 1K in length, so page 251 starts at address 251 * DATAPAGE_SIZE = 257024 = 3EC00 hex
   uint32_t *p = ADDRESS_OF_PAGE(MY_FLASH_PAGE);
   int rc;
   
   if( rc == 0 && *p != 0 ) // is it 0?
   {
     waitingForIncome = false;
     income = (uint16_t) *p; // if we've saved this from before
     delay(100); // make sure we're all done with flash
     
     dataflash.begin();
   }
   else
   {
      RFduinoBLE.deviceName = "Bracket";
      RFduinoBLE.advertisementData = "Income Data";
      // start the BLE stack
      RFduinoBLE.begin();
     
   }
}

void loop()
{
  
  if(waitingForIncome)
  {    
    delay(10);
    return;
  }
  
  boolean newData = false;
  
  // For one second we parse GPS data and report some key values
  for (unsigned long start = millis(); millis() - start < 1000;)
  {
    while (Serial.available())
    {
      char c = Serial.read();
      if (gps.encode(c)) // Did a new valid sentence come in?
        newData = true;
    }
  }
  
  if(newData)
  {
    if (gps.location.isValid())
    {
      currentLat = gps.location.lat();
      currentLon = gps.location.lng();
    }
  }
  

  boolean found = false;
  
  char buf[16];
  int bufIndex = 0;
  
  uint16_t tmpId;
  
  location currentLoc;
  currentLoc.id = 0;
  currentLoc.lat = 0;
  currentLoc.lon = 0;
  
  uint8_t data[DATAPAGE_SIZE];
  
  int i;
  
  for(i=0; i< CENTROID_PAGES; ++i)
  {
    // this is slower than pageToBuffer() + bufferRead()?
    dataflash.pageRead(i, 0);
    
    int j = 0;
    do {
        data[j] = SPI.transfer(0xff);
        ++j;
    } while(j < DATAPAGE_SIZE);
      
    j = 0;
      
    while(j < DATAPAGE_SIZE)
    {
      char r = data[j];
      
      if(r == '/r' && currentLoc.id == 0) {
        currentLoc.id = atoi(buf);
        bufIndex = 0;
      } else if(r == ' ' && currentLoc.lat == 0) {
        currentLoc.lat = atoi(buf);
        bufIndex = 0;
      } else if(r == ' ' && currentLoc.lon == 0) {
        currentLoc.lon = atoi(buf);
        bufIndex = 0;
      } else if(r == '/r' && currentLoc.id != 0 &&  currentLoc.lat != 0 &&  currentLoc.lon != 0) {
        
        // now check it
        if( currentLoc.lat - currentLat < distLat && currentLoc.lon - currentLon < distLon ) {
          distLat = currentLoc.lat - currentLat;
          distLon = currentLoc.lon - currentLon;
          id = currentLoc.id;
        }
        
        // now reset everything
        currentLoc.id = 0;
        currentLoc.lat = 0;
        currentLoc.lon = 0;
        bufIndex = 0;
        
      } else {
        buf[bufIndex] = r;
      }
      bufIndex++;
      j++;
    }
  }
      
    // got DATAPAGE_SIZE? good, now look through for our string
    
   for(i=CENTROID_PAGES; i< TRACT_PAGES; ++i)
  {
    // this is slower than pageToBuffer() + bufferRead()?
    dataflash.pageRead(i, 0);
    
    int j = 0;
    do {
        data[j] = SPI.transfer(0xff);
        ++j;
    } while(j < DATAPAGE_SIZE);
      
    j = 0;
      
    while(j < DATAPAGE_SIZE)
    {
      char r = data[j];
      
      if( r == ' ' ) {
        
        tmpId = atoi(buf);
        if(tmpId == id)
        {
          found = true;
          bufIndex = 0;
          
          bool haveIncome = false;
          while( !haveIncome )
          {
            char r = data[j];
            if( r == ' ' || r == '/r' ) {
              tractIncome = atoi(buf);
              haveIncome = true;
            }else {
              buf[bufIndex] = r;
            }
            j++;
          }
          
        } else {
          buf[bufIndex] = r;
        }
        bufIndex++;
        j++;
      }
    }
  }
  
  if( abs(income - tractIncome) > 5000 )
  {
    digitalWrite( SHOCK_PIN, HIGH );
    delay(2);
    digitalWrite( SHOCK_PIN, LOW );
  }
  
  delay(30000); // check every 5 minutes?
}

void RFduinoBLE_onConnect()
{
  //
}

void RFduinoBLE_onReceive(char *data, int len)
{
      // just waiting for the income to be set
    int income = atoi(data);
    uint32_t *p = ADDRESS_OF_PAGE(MY_FLASH_PAGE);
    int rc;
    rc = flashWrite(p, income);
    if(rc == 0)
    {
      RFduinoBLE.end(); // not needed any more
      dataflash.begin();
      waitingForIncome = false;
    }
    else
    {
      // hmm?
    }
      
}

