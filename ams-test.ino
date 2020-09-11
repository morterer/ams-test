#include <bluefruit.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

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

char artist[128];
char title[128];

//int  x, minX;

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

  // initialize the display, and configure for white on black text
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Address 0x3C for 128x32
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
//  display.setTextWrap(false);
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

  display.setCursor(0, 5);
  display.setTextSize(1);
  display.println(artist);
  display.println(title);
  display.display();
}

// https://blog.moddable.com/blog/ams-client/
// https://devzone.nordicsemi.com/f/nordic-q-a/42230/apple-media-service
// https://developer.apple.com/library/archive/documentation/CoreBluetooth/Reference/AppleMediaService_Reference/Specification/Specification.html
//
// * There are important differences between write and write_resp
