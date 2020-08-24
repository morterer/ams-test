#include <bluefruit.h>

// Apple Media Service UUID: 89D3502B-0F36-433A-8EF4-C502AD55F8DC
// Entity Update UUID:       2F7CABCE-808D-411F-9A0C-BB92BA96C102 (writeable with response, notifiable)


const uint8_t BLEAMS_UUID_SERVICE[] =           {0xDC, 0xF8, 0x55, 0xAD, 0x02, 0xC5, 0xF4, 0x8E, 0x3A, 0x43, 0x36, 0x0F, 0x2B, 0x50, 0xD3, 0x89};
const uint8_t BLEAMS_UUID_CHR_ENTITY_UPDATE[] = {0x02, 0xC1, 0x96, 0xBA, 0x92, 0xBB, 0x0C, 0x9A, 0x1F, 0x41, 0x8D, 0x80, 0xCE, 0xAB, 0x7C, 0x2F};

BLEClientService        appleMediaService(BLEAMS_UUID_SERVICE);
BLEClientCharacteristic entityUpdateChrt(BLEAMS_UUID_CHR_ENTITY_UPDATE);

char buffer[128];

void setup() {
  Serial.begin(115200);
  Serial.println("Starting AMS Test");

  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  Bluefruit.begin();
  Bluefruit.setName("AMS Test");
  Bluefruit.Periph.setConnectCallback(connect_callback);

  // Initialize the appleMediaService
  appleMediaService.begin();

  // Initialize the entityUpdateChrt
  entityUpdateChrt.setNotifyCallback(update_notify_callback);
  entityUpdateChrt.begin();


  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  // Include ANCS 128-bit uuid
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

  // discover entityUpdateChrt
  entityUpdateChrt.discover();
  if (entityUpdateChrt.discovered()) {
    Serial.println("Characteristic discovered");
  } else {
    Serial.println("Characteristic not discovered");
  }

  Bluefruit.requestPairing(conn_handle);
  if (Bluefruit.connPaired(conn_handle)) {
    Serial.println("Pairing successful");
  } else {
    Serial.println("Pairing failed");
  }

  // enable notifications on the entityUpdateChrt
  if (entityUpdateChrt.enableNotify()) {
    Serial.println("Notifications enabled");
  } else {
    Serial.println("Notifications NOT enabled");
  }

  Serial.println("Writing to entityUpdateChrt");
  // EntityIDTrack TrackAttributeIDTitle
  uint8_t command[] = {2, 0,2};
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
  Serial.println("Got a notification");
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
    EntityUpdateFlags: A bitmask whose set bits give the MR specific information about the notification. For example, an MR could be informed that the data had to be truncated in order to fit into the GATT notification.
    Value: A string which corresponds to the value associated with the given attribute.
  */
  //  Serial.print("Data:  "); Serial.println(data);
  Serial.print("EntityID:    "); Serial.println(data[0]);
  Serial.print("AttributeID: "); Serial.println(data[1]);
  Serial.print("Truncated:   "); Serial.println(data[2]);
  memcpy(buffer, data+3, len-3);
  Serial.println(buffer);
//  for (int i = 3; i < len; i++) {
//    Serial.print((char)data[i]);
//  }
//  Serial.println();

  
}

void loop() {
  // put your main code here, to run repeatedly:

}
