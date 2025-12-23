/*
Heavily adopted from Adafruit serial_host_bridge and SH110X examples.
Thanks Adafruit!

Sadly, the RP2040 implementation has no deep-sleep
and as such this code does not provide any sleep/power management
functionality. As a result, it is pretty power consumptive. 

Note- when running RP2040 at 120 MHz there seems to be issues with
dropped bits/bytes via USBhost (SerialHost) connection. Increasing clock speed
to 240 MHz appeared to resolve this.
*/

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <hardware/watchdog.h>
#include "usbh_helper.h" // USBHost is defined in usbh_helper.h

Adafruit_SH1107 display = Adafruit_SH1107(64, 128, &Wire);
// CDC Host object
Adafruit_USBH_CDC SerialHost;

// Define button pins... just for RP2040 feather
const unsigned int BUTTON_A  = 9;
const unsigned int BUTTON_B  = 6;
const unsigned int BUTTON_C  = 5;

//Cursor locations to put text inline with buttons
const unsigned int BUTTON_A_X = 0;
const unsigned int BUTTON_A_Y = 8;
const unsigned int BUTTON_B_X = 0;
const unsigned int BUTTON_B_Y = 27;
const unsigned int BUTTON_C_X = 0;
const unsigned int BUTTON_C_Y = 45;
//Timing constants
const int MENU_DWELL_MS = 1500;
const int BUTTON_DWELL_MS = 200;
const unsigned int INACTIVITY_TIMEOUT_MS = 60000; // general timeout for inactivity
const unsigned int HEAT_TX_TIMEOUT_MS = 17000; // timeout for connecting to Heat Tx
const uint32_t WATCHDOG_MS = 5000; // maximum value for RP2040, runs in background in case anything hangs

// used instead of delay whenever time break is needed
void non_blocking_delay(int delayMillis){
  int start = millis();
  while((millis() - start) < delayMillis){}
  return;
}

// Because of device swapping, we want to clear both
// Rx and Tx Serial buffers on UsbHostSerial. 
//.flush() clears Tx *not Rx*
// and we just read/print to native Serial anything.
void flushUsbSerialHostBufs(){
  SerialHost.flush();
  Serial.println("**** Flushing Serial Host Rx Buffer ****");
  while(SerialHost.available()){
    Serial.println(SerialHost.readString());
  }
  Serial.println("**** Done ****");
  return;
}

void drawTopLevelMenu() {
  display.clearDisplay(); // if you don't do this it will overwrite/mush things together
  display.setTextSize(1); // default is 1
  display.setTextColor(SH110X_WHITE);
  display.setCursor(40,0); // roughly center the cursor on top line
  display.println("UCSD MAE");
  display.setCursor(BUTTON_A_X, BUTTON_A_Y);
  display.println("Basic Tester Program\n");
  display.setCursor(BUTTON_B_X, BUTTON_B_Y);
  display.println("- B: RAMBo test \n");
  display.println("- C: Heat TX");
  display.display(); // actually display all of the above
}

void drawconnectedMenu(){
  display.clearDisplay();
  display.display();
  display.setCursor(40,0);
  display.print("RAMBo");
  display.setCursor(BUTTON_A_X,BUTTON_A_Y);
  display.println("- A: Set WCS (G92)");
  display.setCursor(BUTTON_B_X,BUTTON_B_Y); 
  display.println("- B: Run X/Y Test");
  display.setCursor(BUTTON_C_X,BUTTON_C_Y);
  display.println("- C: Quit");
  display.display();
}

void connectedMenu(){
  flushUsbSerialHostBufs();
  drawconnectedMenu();
  non_blocking_delay(BUTTON_DWELL_MS); // Need to wait for user to release button
  unsigned int start_time = millis();
  while((millis() - start_time) < INACTIVITY_TIMEOUT_MS){
    watchdog_update();
    if(!digitalRead(BUTTON_A)){
      setWCS();
      drawconnectedMenu();
      start_time = millis();
    }
    if(!digitalRead(BUTTON_B)){ 
      runXYTest();
      drawconnectedMenu();
      start_time = millis();
      }
    if(!digitalRead(BUTTON_C)) return;
  }
  return;
}

// Opens serial connection to rambo
void connectToRambo(){
  watchdog_update();
  flushUsbSerialHostBufs();
  SerialHost.end();
  SerialHost.begin(115200);
  bool connected = false;

  if(SerialHost.connected() ){
    display.clearDisplay();
    display.display();
    display.setCursor(0,0);
    display.println("Connecting to RAMBo\n...");
    display.display();
    SerialHost.println("M115");
    SerialHost.flush();
    flushUsbSerialHostBufs();
    SerialHost.println("M115"); // bad fix for echo:Unknown command: "�M115"
    SerialHost.flush();           // just send it a second time which reliably works
    non_blocking_delay(BUTTON_DWELL_MS); // Need to wait for user to release button
    while(SerialHost.available() > 0){
      String result = SerialHost.readStringUntil('\n');
      Serial.println(result);
      if(result.substring(0,13) == "FIRMWARE_NAME"){
            connected = true;
            display.clearDisplay();
            display.display();
            display.setCursor(0,0);
            display.println("Connected!");
            display.println(result.substring(0,29));
            display.display();
            non_blocking_delay(MENU_DWELL_MS);
            connectedMenu();
      }
    }
  }
  if(!connected){
    display.clearDisplay();
    display.display();
    display.setCursor(0,0);
    display.println("Failed to connect to RAMbo");
    display.display();
    non_blocking_delay(MENU_DWELL_MS);
  }
  return;
}

void setWCS(){
  watchdog_update();
  unsigned int start = millis();
  while((millis() - start) < MENU_DWELL_MS){
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Setting WCS \nto current location\n\nUsing G92 command...");
  display.display();
  flushUsbSerialHostBufs();
  }
  SerialHost.println("G92\n");
  SerialHost.flush();
  non_blocking_delay(BUTTON_DWELL_MS); // Need to wait for user to release button
  String result = SerialHost.readString();
  if (result == "X:0.00 Y:0.00 Z:0.00 E:0.00 Count X:0 Y:0 Z:0"){
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Successfully Set WCS");
    display.display();
    non_blocking_delay(MENU_DWELL_MS);
  }
  flushUsbSerialHostBufs();
  return;
}

void runXYTest(){
  watchdog_update();
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("XY Motion Test:\n");
  display.println("Moving in a \n2.5 cm square...");
  display.display();
  flushUsbSerialHostBufs();
  SerialHost.println("G01 X025 Y0\n");
  SerialHost.println("G01 X025 Y025\n");
  SerialHost.println("G01 X0 Y025\n");
  SerialHost.println("G01 X00 Y00\n");
  SerialHost.flush();
  non_blocking_delay(BUTTON_DWELL_MS); // Need to wait for user to release button
  flushUsbSerialHostBufs();
  non_blocking_delay(MENU_DWELL_MS);
  return;
}

void drawHeatTxDisplay(bool connected, String setpoint, String plateTemp, String airTemp){
  if(!connected){
    display.clearDisplay();
    display.setCursor(0,0);
    display.print("Heat Tx - Tset:");
    display.println(setpoint);
    display.println("\nConnecting...");
  }
  else{
    // Set background color so it overwrites cleanly
    display.setTextColor(1, 0);
    display.setCursor(0,0);
    display.println("Heat Tx"); //move cursor down
    display.print("\nPlate Temp: ");
    display.print(plateTemp);
    display.println("     ");
    display.print("\nAir temp: ");
    display.print(airTemp);
    display.println("    ");
  }
  display.setCursor(BUTTON_C_X, BUTTON_C_Y);
  display.println("- C: Quit");
  display.display();
}

void heatTxTest(){
  flushUsbSerialHostBufs();
  SerialHost.end(); // Heat TX uses lower baud rate
  int start_time = millis();
  String setpoint = "26.0";
  drawHeatTxDisplay(false, setpoint, "", "");
  non_blocking_delay(BUTTON_DWELL_MS); // Need to wait for user to release button
  SerialHost.begin(9600);
  if(SerialHost.connected() ){
    SerialHost.print(setpoint.toFloat());
    // SerialHost.write('\n');
    SerialHost.flush();
    while((millis() - start_time) < HEAT_TX_TIMEOUT_MS){
      watchdog_update();
      if(!digitalRead(BUTTON_C)) break;
      if ((SerialHost.connected()) && (SerialHost.available())) {
        String result = SerialHost.readStringUntil('\n');
        if((result.indexOf(",")==5) && (result.indexOf(";")==11)){
          drawHeatTxDisplay(true, setpoint, result.substring(0,4), result.substring(6,10));
          non_blocking_delay(BUTTON_DWELL_MS); // Need to wait for user to release button
          start_time = millis();
          flushUsbSerialHostBufs();
        }
        Serial.println(result);
          }
      }
  }
  else{
    display.clearDisplay();
    display.display();
    display.setCursor(0,0);
    display.println("No Device Detected");
    display.display();
    non_blocking_delay(MENU_DWELL_MS);
  }
  SerialHost.end();
  flushUsbSerialHostBufs();
  SerialHost.begin(115200);
}

#if not defined(ARDUINO_ARCH_RP2040)
#error "This code can only be compiled for RP2040 at this time"
#endif
//--------------------------------------------------------------------+
// For RP2040 use both core0 for device stack, core1 for host stack
//--------------------------------------------------------------------+

//------------- Core0 -------------//
void setup() {
  watchdog_enable(WATCHDOG_MS, true); // need watchdog to handle cases where usb flakes out
  watchdog_update();
  Serial.begin(115200);

  Serial.println("MAE USB Tester");
  non_blocking_delay(BUTTON_DWELL_MS); // wait for the OLED to power up
  display.begin(0x3C, true); // Address 0x3C default

  Serial.println("OLED begun");

  // draw main menu for more seamless wdt reset driven reboots
  display.setRotation(1); // Wide orientation
  drawTopLevelMenu();
  non_blocking_delay(1000);

  Serial.println("MAE 170 USB Tester");

  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);
}

void loop() {
  drawTopLevelMenu();
  unsigned int start_time = millis();
  while((millis()- start_time) < INACTIVITY_TIMEOUT_MS){
    if(!digitalRead(BUTTON_B)){
      connectToRambo();
      drawTopLevelMenu();
      non_blocking_delay(BUTTON_DWELL_MS); // Need to wait for user to release button
      non_blocking_delay(BUTTON_DWELL_MS); // Need to wait for user to release button
    }
    if(!digitalRead(BUTTON_C)){
      heatTxTest();
      drawTopLevelMenu();
      non_blocking_delay(BUTTON_DWELL_MS); // Need to wait for user to release button
      non_blocking_delay(BUTTON_DWELL_MS); // Need to wait for user to release button
    }
    if(((millis() - start_time) + WATCHDOG_MS + 500) < INACTIVITY_TIMEOUT_MS ){
      watchdog_update();
    }
  }
  // kill and restart Serial just in case it has been
  // sitting too long and gotten in an indeterminate state

}

//------------- Core1 -------------//
// taken from TinyUSB examples
// thank you hathatch!
void setup1() {
  // configure pio-usb: defined in usbh_helper.h
  rp2040_configure_pio_usb();

  // run host stack on controller (rhport) 1
  // Note: For rp2040 pico-pio-usb, calling USBHost.begin() on core1 will have most of the
  // host bit-banging processing works done in core1 to free up core0 for other works
  USBHost.begin(1);

  // Initialize SerialHost
  SerialHost.begin(115200);
}

void loop1() {
  USBHost.task();
}






//--------------------------------------------------------------------+
// TinyUSB Host callbacks
//--------------------------------------------------------------------+
extern "C" {

// Invoked when a device with CDC interface is mounted
// idx is index of cdc interface in the internal pool.
void tuh_cdc_mount_cb(uint8_t idx) {
  // bind SerialHost object to this interface index
  SerialHost.mount(idx);
  Serial.println("SerialHost is connected to a new CDC device");
}

// Invoked when a device with CDC interface is unmounted
void tuh_cdc_umont_cb(uint8_t idx) {
  SerialHost.umount(idx);
  Serial.println("SerialHost is disconnected");
}

}
