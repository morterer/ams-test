#include <bluefruit.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define BUTTON_A    31
#define BUTTON_B    30
#define BUTTON_C    27

// Apple Media Service UUID: 89D3502B-0F36-433A-8EF4-C502AD55F8DC
// Remote Command UUID:      9B3C81D8-57B1-4A8A-B8DF-0E56F7CA51C2 (writeable, notifiable)
// Entity Update UUID:       2F7CABCE-808D-411F-9A0C-BB92BA96C102 (writeable with response, notifiable)


const uint8_t BLEAMS_UUID_SERVICE[] =           {0xDC, 0xF8, 0x55, 0xAD, 0x02, 0xC5, 0xF4, 0x8E, 0x3A, 0x43, 0x36, 0x0F, 0x2B, 0x50, 0xD3, 0x89};
const uint8_t BLEAMS_UUID_CHR_REMOTE_CMD[] =    {0xC2, 0x51, 0xCA, 0xF7, 0x56, 0x0E, 0xDF, 0xB8, 0x8A, 0x4A, 0xB1, 0x57, 0xD8, 0x81, 0x3C, 0x9B};
const uint8_t BLEAMS_UUID_CHR_ENTITY_UPDATE[] = {0x02, 0xC1, 0x96, 0xBA, 0x92, 0xBB, 0x0C, 0x9A, 0x1F, 0x41, 0x8D, 0x80, 0xCE, 0xAB, 0x7C, 0x2F};

BLEClientService        appleMediaService(BLEAMS_UUID_SERVICE);
BLEClientCharacteristic remoteCmdChrt(BLEAMS_UUID_CHR_REMOTE_CMD);
BLEClientCharacteristic entityUpdateChrt(BLEAMS_UUID_CHR_ENTITY_UPDATE);

Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &Wire);

// buffers for artist and song title information
char artist[128];
char title[128];

// used for scrolling title information on OLED screen
int  x, minX;

// track which buttons have been pressed on the OLED featherwing
uint32_t presedButtons;

enum {
  AMS_ENTITY_ID_PLAYER, // app name, playback data, volume
  AMS_ENTITY_ID_QUEUE,  // queue info, shuffle, repeat
  AMS_ENTITY_ID_TRACK   // artist, album, title, length
};

enum {
  AMS_TRACK_ATTRIBUTE_ID_ARTIST,   // name of the artist.
  AMS_TRACK_ATTRIBUTE_ID_ALBUM,    // name of the album.
  AMS_TRACK_ATTRIBUTE_ID_TITLE,    // title of the track.
  AMS_TRACK_ATTRIBUTE_ID_DURATION  // total duration of the track in seconds.
};

void setup() {
  Serial.begin(115200);
  Serial.println("Starting AMS Test");

  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  Bluefruit.begin();
  Bluefruit.setName("AMS Test");
  Bluefruit.Periph.setConnectCallback(connect_callback);

  // Initialize the appleMediaService
  appleMediaService.begin();

  // Initialize the remoteCmdChrt
  remoteCmdChrt.setNotifyCallback(remote_command_callback);
  remoteCmdChrt.begin();

  // Initialize the entityUpdateChrt
  entityUpdateChrt.setNotifyCallback(update_notify_callback);
  entityUpdateChrt.begin();


  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  // Include AMS 128-bit uuid
  Bluefruit.Advertising.addService(appleMediaService);

  // Secondary Scan Response packet (optional)
  // Since there is no room for 'Name' in Advertising packet
  Bluefruit.ScanResponse.addName();

  /* Start Advertising
     - Enable auto advertising if disconnected
     - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
     - Timeout for fast mode is 30 seconds
     - Start(timeout) with timeout = 0 will advertise forever (until connected)

     For recommended advertising interval
     https://developer.apple.com/library/content/qa/qa1931/_index.html
  */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds

  // initialize the display, and configure for white on black text, and don't wrap text to the next line
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Address 0x3C for 128x32
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  display.setTextWrap(false);

  // Button configured
  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);
}

void connect_callback(uint16_t conn_handle) {
  Serial.println("Connected...");

  // discover appleMediaService
  appleMediaService.discover(conn_handle);
  if (appleMediaService.discovered()) {
    Serial.println("Service discovered");
  } else {
    Serial.println("Service not discovered");
  }

  // discover Remote Command characteristic
  remoteCmdChrt.discover();
  if (remoteCmdChrt.discovered()) {
    Serial.println("Remote Command characteristic discovered");
  } else {
    Serial.println("Remote Command characteristic NOT discovered");
  }

  // discover Entity Update characteristic
  entityUpdateChrt.discover();
  if (entityUpdateChrt.discovered()) {
    Serial.println("Entity Update characteristic discovered");
  } else {
    Serial.println("Entity Update characteristic NOT discovered");
  }

  Bluefruit.requestPairing(conn_handle);
  if (Bluefruit.connPaired(conn_handle)) {
    Serial.println("Pairing successful");
  } else {
    Serial.println("Pairing failed");
  }

  // enable notifications on the Remote Command characteristic
  if (remoteCmdChrt.enableNotify()) {
    Serial.println("Remote Command notifications enabled");
  } else {
    Serial.println("Remote Command notifications NOT enabled");
  }

  // enable notifications on the Entity Update characteristic
  if (entityUpdateChrt.enableNotify()) {
    Serial.println("Entity Update notifications enabled");
  } else {
    Serial.println("Entity Update notifications NOT enabled");
  }

  Serial.println("Writing to entityUpdateChrt");
  // EntityIDTrack TrackAttributeIDTitle
  uint8_t command[] = {AMS_ENTITY_ID_TRACK, AMS_TRACK_ATTRIBUTE_ID_ARTIST, AMS_TRACK_ATTRIBUTE_ID_TITLE, AMS_TRACK_ATTRIBUTE_ID_DURATION};
  entityUpdateChrt.write_resp(&command, sizeof(command));

}

/**
   Hooked callback that triggered for a change in entity/attribute pairs
   @param chr   Pointer client characteristic that even occurred,
                in this example it should be hrmc
   @param data  Pointer to received data
   @param len   Length of received data
*/
void update_notify_callback(BLEClientCharacteristic* chr, uint8_t* data, uint16_t len) {
  Serial.println("Got a entity update notification");
  Serial.print("Length:"); Serial.println(len);
  /*
    The format of GATT notifications delivered by the MS is shown below:

    Byte  Value
    -----------------------
    1     EntityID
    2     AttributeID
    3     EntityUpdateFlags
    4...  Value

    A GATT notification delivered through the Entity Update characteristic contains the following information:

    EntityID: The entity to which the subsequent attribute corresponds.
    AttributeID: The attribute whose value is being sent in the notification.
    EntityUpdateFlags: A bitmask whose set bits give the MR specific information about the notification.
    For example, an MR could be informed that the data had to be truncated in order to fit into the GATT notification.
    Value: A string which corresponds to the value associated with the given attribute.
  */

  Serial.print("EntityID:    "); Serial.println(data[0]);
  Serial.print("AttributeID: "); Serial.println(data[1]);
  Serial.print("Truncated:   "); Serial.println(data[2]);

  // only deal with track information, for now
  // TODO: handle truncated data
  if (data[0] == AMS_ENTITY_ID_TRACK) {
    display.clearDisplay();
    switch (data[1]) {
      case AMS_TRACK_ATTRIBUTE_ID_ARTIST:
        // clear the artist buffer before copying in a new value
        memset(artist, 0, sizeof(artist));
        // leave out the leading IDs and flags, and copy the string payload to the buffer
        memcpy(artist, data + 3, len - 3);
        Serial.println(artist);
        break;

      case AMS_TRACK_ATTRIBUTE_ID_TITLE:
        // clear the title buffer before copying in a new value
        memset(title, 0, sizeof(title));
        // leave out the leading IDs and flags, and copy the string payload to the buffer
        memcpy(title, data + 3, len - 3);
        Serial.println(title);
        x = display.width();
        minX = -12 * strlen(title);
        break;

      default:
        Serial.print("Unhandled attribute: "); Serial.println(data[1]);
        break;
    }
  }
}

/*
  When the list of commands supported by the media player changes, the MS generates a
  notification on the Remote Command characteristic containing a list of the currently
  supported commands, using the fomat shown below:

    Byte  Value
    -----------------------
    1     RemoteCommandID 1
    2     RemoteCommandID 2
    ...   other RemoteCommandIDs


    Table A-1  RemoteCommandID values
    Name                              Value
    ---------------------------------------
    RemoteCommandIDPlay                 0
    RemoteCommandIDPause                1
    RemoteCommandIDTogglePlayPause      2
    RemoteCommandIDNextTrack            3
    RemoteCommandIDPreviousTrack        4
    RemoteCommandIDVolumeUp             5
    RemoteCommandIDVolumeDown           6
    RemoteCommandIDAdvanceRepeatMode    7
    RemoteCommandIDAdvanceShuffleMode   8
    RemoteCommandIDSkipForward          9
    RemoteCommandIDSkipBackward        10
    RemoteCommandIDLikeTrack           11
    RemoteCommandIDDislikeTrack        12
    RemoteCommandIDBookmarkTrack       13
    Reserved                           14-255
*/
void remote_command_callback(BLEClientCharacteristic* chr, uint8_t* data, uint16_t len) {
  Serial.println("Got a remote command notification");
  Serial.print("Length:"); Serial.println(len);
  Serial.println("Currently supported commands:");
  for (int i = 0; i < len; i++) {
    Serial.println(data[i]);
  }
  Serial.println();
}

void loop() {
  // put your main code here, to run repeatedly:
  if (Serial.available() > 0) {
    uint8_t readByte = Serial.parseInt();
    Serial.print("Sending command: "); Serial.println(readByte);
    remoteCmdChrt.write8_resp(readByte);
  }

  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println(artist);

  // TODO: only scroll title if it exceeds 10 characters
  display.setTextSize(2);
  display.setCursor(x, 10);
  display.print(title);
  x -= 2;
  if (x < minX) x = display.width();
  //  if (--x < minX) x = display.width();
  display.display();

  // Check buttons
  presedButtons = readPressedButtons();

  if ( presedButtons & bit(BUTTON_A) )
  {
    Serial.println("Toggle Play/Pause");
    remoteCmdChrt.write8_resp(2);
  }
  if ( presedButtons & bit(BUTTON_B) )
  {
    Serial.println("B");
  }
  if ( presedButtons & bit(BUTTON_C) )
  {
    Serial.println("Next Track");
    remoteCmdChrt.write8_resp(3);
  }
}


/**
   copied from Adafruit ancs_oled example


   Check if button A,B,C state are pressed, include some software
   debouncing.

   Note: Only set bit when Button is state change from
   idle -> pressed. Press and hold only report 1 time, release
   won't report as well

   @return Bitmask of pressed buttons e.g If BUTTON_A is pressed
   bit 31 will be set.
*/
uint32_t readPressedButtons(void)
{
  // must be exponent of 2
  enum { MAX_CHECKS = 8, SAMPLE_TIME = 10 };

  /* Array that maintains bounce status/, which is sampled
     10 ms each. Debounced state is regconized if all the values
     of a button has the same value (bit set or clear)
  */
  static uint32_t lastReadTime = 0;
  static uint32_t states[MAX_CHECKS] = { 0 };
  static uint32_t index = 0;

  // Last Debounced state, used to detect changed
  static uint32_t lastDebounced = 0;

  // Too soon, nothing to do
  if (millis() - lastReadTime < SAMPLE_TIME ) return 0;

  lastReadTime = millis();

  // Take current read and masked with BUTTONs
  // Note: Bitwise inverted since buttons are active (pressed) LOW
  uint32_t debounced = ~(*portInputRegister( digitalPinToPort(0) ));
  debounced &= (bit(BUTTON_A) | bit(BUTTON_B) | bit(BUTTON_C));

  // Copy current state into array
  states[ (index & (MAX_CHECKS - 1)) ] = debounced;
  index++;

  // Bitwise And all the state in the array together to get the result
  // This means pin must stay at least MAX_CHECKS time to be realized as changed
  for (int i = 0; i < MAX_CHECKS; i++)
  {
    debounced &= states[i];
  }

  // result is button changed and current debounced is set
  // Mean button is pressed (idle previously)
  uint32_t result = (debounced ^ lastDebounced) & debounced;

  lastDebounced = debounced;

  return result;
}

// https://blog.moddable.com/blog/ams-client/
// https://devzone.nordicsemi.com/f/nordic-q-a/42230/apple-media-service
// https://developer.apple.com/library/archive/documentation/CoreBluetooth/Reference/AppleMediaService_Reference/Specification/Specification.html
// https://www.youtube.com/watch?v=sTYPuDMPva8
//
// * There are important differences between write and write_resp
