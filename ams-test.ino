#include <bluefruit.h>

// Apple Media Service UUID: 89D3502B-0F36-433A-8EF4-C502AD55F8DC
// Entity Update UUID:       2F7CABCE-808D-411F-9A0C-BB92BA96C102 (writeable with response, notifiable)


const uint8_t UUID_SERVICE[] =      {0xDC, 0xF8, 0x55, 0xAD, 0x02, 0xC5, 0xF4, 0x8E, 0x3A, 0x43, 0x36, 0x0F, 0x2B, 0x50, 0xD3, 0x89};
const uint8_t UUID_CHARACTISTIC[] = {0x02, 0xC1, 0x96, 0xBA, 0x92, 0xBB, 0x0C, 0x9A, 0x1F, 0x41, 0x8D, 0x80, 0xCE, 0xAB, 0x7C, 0x2F};

BLEClientService        service(UUID_SERVICE);
BLEClientCharacteristic characteristic(UUID_CHARACTISTIC);

void setup() {
  Serial.begin(115200);
  Serial.println("Starting AMS Test");

  Bluefruit.begin();
  Bluefruit.setName("AMS Test");
  Bluefruit.Periph.setConnectCallback(connect_callback);

  // Initialize the service
  service.begin();

  // Initialize the characteristic
  characteristic.setNotifyCallback(update_notify_callback);
  characteristic.begin();


  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  // Include ANCS 128-bit uuid
  Bluefruit.Advertising.addService(service);

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

  // discover service
  service.discover(conn_handle);
  if(service.discovered()){
    Serial.println("Service discovered");
  } else {
    Serial.println("Service not discovered");
  }

  // discover characteristic
  characteristic.discover();
  if(characteristic.discovered()){
    Serial.println("Characteristic discovered");
  } else {
    Serial.println("Characteristic not discovered");
  }

  Bluefruit.requestPairing(conn_handle);
  if(Bluefruit.connPaired(conn_handle)){
    Serial.println("Pairing successful");
  } else {
    Serial.println("Pairing failed");
  }

  // enable notifications on the characteristic
  if(characteristic.enableNotify()){
    Serial.println("Notifications enabled");
  } else {
    Serial.println("Notifications NOT enabled");
  }

  Serial.println("Writing to characteristic");
  // EntityIDTrack TrackAttributeIDTitle
  uint8_t command[] = {2, 2};
  characteristic.write_resp(&command, sizeof(command));
  
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
  //  Serial.print("Data:  "); Serial.println(data);

}

void loop() {
  // put your main code here, to run repeatedly:

}
