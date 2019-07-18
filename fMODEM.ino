/*

  fMODEM

  A Fake modem for retro-computers.
  Connects to wifi, and passes data over RS232.
  Requires Arduino UNO WiFi with TTL to RS232 Serial port card on TX/RX pins.

  Configure WiFi:
  AT ssid=Your WiFI Name e.g. AT ssid=MYHOTSPOT
  AT password=Your Wifi password e.g. AT passowrd=SecretPW
  AT password=OPEN is a special case for networks with no password
  AT SAVE

  Connect to WiFi:
  AT WIFI
  (Once you have done an AT SAVE it will store details in EEPROM and use them next power up)

  Connect to server:
  AT TELNET=telnet.server.address e.g. AT TELNET=towel.blinkenlights.nl
  AT TELNET=telnet.server.address,port e.g. AT TELNET=towel.blinkenlights.nl,23


  Other commands
  AT LOGOUT to cancel connection
  AT DROP to drop WiFi connection
  AT LIST to list available access points


  Based on Ardunio examples..

  This example connects to an unencrypted Wifi network.
  Then it prints the  MAC address of the Wifi module,
  the IP address obtained, and other network details.

  created 13 July 2010
  by dlf (Metodo2 srl)
  modified 31 May 2012
  by Tom Igoe


*/

#include <SPI.h>
#include <WiFiNINA.h>
#include <EEPROM.h>
#include <WiFiUdp.h>

// I wired up four LEDs, for power, RX, TX and AUX.
// AUX is used for active WiFi connection.

#define LED_POWER 7
#define LED_RX 4
#define LED_TX 6
#define LED_AUX 5

// Used to store user input
char buffer[80];
char buffer_count = 0;

// Comment this out when you deploy to the device, otherwise output will be sent to Serial, which is the USB Serial port.
// Great for debugging, not so great when you want the RS232 connected computer to see what is going on.
#define DEBUG


// A pointer to the serial port, to make a debug and deploy version.
HardwareSerial *SerialPort;


// Used by the Network Time Service. Yes, now your retro computer can get access to a RTC
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

// WiFi connection stuff
int status = WL_IDLE_STATUS;          // the Wifi radio's status
char ssid[16];                        // your network SSID (name)
char pass[16];                        // your network password (use for WPA, or use as key for WEP)
WiFiUDP Udp;
WiFiClient client;
IPAddress server(82, 68, 254, 162);  // Default server (a BBS)

// Status
bool connected_to_web = false;
bool connected_to_wifi = false;



void setup() {

#ifdef DEBUG
  SerialPort = &Serial;
#else
  SerialPort = &Serial1;
#endif

  clearBuffer();

  pinMode(LED_POWER, OUTPUT);
  pinMode(LED_RX, OUTPUT);
  pinMode(LED_TX, OUTPUT);
  pinMode(LED_AUX, OUTPUT);

  digitalWrite(LED_POWER, HIGH);
  digitalWrite(LED_TX, HIGH);
  digitalWrite(LED_RX, HIGH);
  digitalWrite(LED_AUX, HIGH);


  //Initialize serial and wait for port to open:


#ifdef DEBUG
  SerialPort->begin(9600);

  while (!SerialPort) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
#else
  SerialPort->begin(9600);
#endif

  SerialPort->println("");
  SerialPort->println( "         ##     ##  #######  ########  ######## ##     ## ");
  SerialPort->println( "  ####   ###   ### ##     ## ##     ## ##       ###   ### ");
  SerialPort->println( "##       #### #### ##     ## ##     ## ##       #### #### ");
  SerialPort->println( "######   ## ### ## ##     ## ##     ## ######   ## ### ## ");
  SerialPort->println( "##       ##     ## ##     ## ##     ## ##       ##     ## ");
  SerialPort->println( "##       ##     ## ##     ## ##     ## ##       ##     ## ");
  SerialPort->println( "##       ##     ##  #######  ########  ######## ##     ## ");
  SerialPort->println( "##                                                    ");
  SerialPort->println( "   fMODEM v0.4  July 2019");
  SerialPort->println("");

  digitalWrite(LED_POWER, HIGH);
  digitalWrite(LED_TX, LOW);
  digitalWrite(LED_RX, LOW);
  digitalWrite(LED_AUX, LOW);

  SerialPort->println("fMODEM is active. AT HELP for options.");
  SerialPort->println("");
  SerialPort->print("Restoring settings..");

  // Read previously stored settings
  if (ReadData())
  {
    SerialPort->print("Loaded SSID: [");
    SerialPort->print(ssid);
    SerialPort->println("] and Password: [********]");
    SerialPort->println("OK");

    ConnectToWifi();
  }
  else
  {
    SerialPort->println("No settings have been saved.");

    // Clear SSID and Passwords to zero
    for (int i = 0; i < 16; i++)
    {
      pass[i] = 0;
      ssid[i] = 0;
    }
  }
}

void loop() {

  // Process user input
  EnterLine();

  if (connected_to_web)
  {
    while (client.available()) {
      digitalWrite(LED_RX, HIGH);
      char c = client.read();
      SerialPort->write(c);
    }

    digitalWrite(LED_RX, LOW);

    // if the server's disconnected, stop the client:
    if (!client.connected()) {
      SerialPort->println("disconnecting from server.");
      client.stop();

      connected_to_web = false;

    }
  }
}


// Process what the user just typed.

void EnterLine()
{
  if (SerialPort->available())
  {

    digitalWrite(LED_TX, HIGH);

    int inByte = SerialPort->read();
    buffer[buffer_count] = inByte;
    SerialPort->write(inByte);
    bool found_command = false;


    if (inByte == 8) // User pressed delete
    {
      buffer_count--;
      if (buffer_count < 0)
      {
        buffer_count = 0;
      }
    }
    else
    {
      buffer_count++;
      if (buffer_count >= 80)
      {
        buffer_count = 79;
      }
    }

    if (inByte == 13) // User pressed return
    {
      // Eat next char, it will be 10
      int extraByte = SerialPort->read();

      // Check for commmands

      // remove extra line feeds
      if (buffer[0] == 10)
      {
        for (int i = 0; i < buffer_count; i++)
        {
          buffer[i] = buffer[i + 1];
        }
      }

      buffer[buffer_count - 1] = 0; // Get rid of the newline character in there


      // Process general AT commands
      if (strncasecmp(buffer, "AT ", 3) == 0)
      {
        AT(String(buffer));
        found_command = true;
      }

      // Process ATx commands.
      if (strncasecmp(buffer, "ATI", 3) == 0)
      {
        printWifiData();
        printCurrentNet();
        found_command = true;
      }


      // All commands that have been recognized have been processed.
      if (found_command == true)
      {
        buffer_count = 0;
        for (int i = 0; i < 80; i++)
          buffer[i] = 0;

        digitalWrite(LED_TX, LOW);
        return;
      }

      // Take a new line on the terminal, so the user knows they pressed return
      SerialPort->println("fMODEM HELP");

      // Send the string to the internet if connected

      if (connected_to_web == true)
      {
        client.write(buffer);
      }

      clearBuffer();
    }

    digitalWrite(LED_TX, LOW);
  }
}


// Process the various pseudo AT messages
void AT(const String& message) {

  char option[80];

  message.toCharArray(option, 80);

  SerialPort->println("");

  if (strncasecmp(option, "AT LIST", 4) == 0)
  {
    listNetworks();
  }

  if (strncasecmp(buffer, "AT SAVE", 7) == 0)
  {
    SerialPort->print("Saving..");
    WriteData();
    SerialPort->println("OK");
  }

  if (strncasecmp(buffer, "AT LOAD", 7) == 0)
  {
    //AT("HELP");
    SerialPort->print("Loading..");
    if (ReadData())
    {
      printWifiData();
      printCurrentNet();
    }
    else
    {
      SerialPort->println("Unable to read data.");
    }

    SerialPort->println("OK");
  }

  if (strncasecmp(buffer, "AT DROP", 7) == 0)
  {
    AT_drop();
  }

  if (strncasecmp(buffer, "AT TIME", 7) == 0)
  {
    AT_getTime();
  }

  if (strncasecmp(buffer, "AT HELP", 7) == 0)
  {
    SerialPort->println("");
    SerialPort->println("Welcome to fMODEM");
    SerialPort->println("");
    SerialPort->println("Configure WiFi:");
    SerialPort->println("AT ssid=Your WiFI Name e.g. AT ssid=MYHOTSPOT");
    SerialPort->println("AT password=Your Wifi password e.g. AT passowrd=SecretPW");
    SerialPort->println("AT password=OPEN is a special case for networks with no password");
    SerialPort->println("AT SAVE ");
    SerialPort->println("");
    SerialPort->println("Connect to WiFi:");
    SerialPort->println("AT WIFI");
    SerialPort->println("(Once you have done an AT SAVE it will store details in EEPROM and use them next power up)");
    SerialPort->println("");
    SerialPort->println("Connect to server:");
    SerialPort->println("AT TELNET=telnet.server.address e.g. AT TELNET=towel.blinkenlights.nl");
    SerialPort->println("AT TELNET=telnet.server.address,port e.g. AT TELNET=towel.blinkenlights.nl,port");
    SerialPort->println("");
    SerialPort->println("Other commands");
    SerialPort->println("AT LOGOUT to cancel connection");
    SerialPort->println("AT DROP to drop WiFi connection");
    SerialPort->println("AT LIST to list available access points");
  }


  if ((strncasecmp(option, "AT WIFI", 7) == 0) || (strncasecmp(option, "AT BRIDGE", 9) == 0))
  {
    SerialPort->println("Looking for WiFi..");
    ConnectToWifi();
  }

  if (strncasecmp(option, "AT SSID=", 8) == 0)
  {
    String(option).substring(8).toCharArray(ssid, 16);
    SerialPort->print("OK, SSID defined as: ");
    SerialPort->println(ssid);
  }

  if (strncasecmp(option, "AT PASSWORD=", 12) == 0)
  {
    String(option).substring(12).toCharArray(pass, 16);
    SerialPort->print("OK, Password defined as: ");
    SerialPort->println(pass);
  }

  if (strncasecmp(option, "AT TELNET=", 10) == 0)
  {
    AT_Telnet(String(option).substring(10));
  }


  if (strncasecmp(option, "AT LOGIN", 6) == 0)
  {
    if (connected_to_wifi)
    {
      AT_Login();
    }
    else
    {
      SerialPort->println("Connect to WiFi first.");
    }
  }

  if ((strncasecmp(option, "AT LOGOUT", 9) == 0) || (strncasecmp(option, "AT HANGUP", 9) == 0))
  {
    if (!connected_to_wifi)
    {
      SerialPort->println("Connect to WiFi first.");
    }
    else
    {
      if (connected_to_web)
      {
        AT_Logout();
      }
      else
      {
        SerialPort->println("Log in first.");
      }
    }
  }

  SerialPort->println("OK");

}

void ConnectToWifi()
{

  digitalWrite(LED_TX, HIGH);
  digitalWrite(LED_RX, LOW);
  digitalWrite(LED_AUX, LOW);

  int timeout = 0;

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    SerialPort->println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    SerialPort->println("Please upgrade the firmware");
  }

  digitalWrite(LED_TX, LOW);
  digitalWrite(LED_RX, LOW);
  digitalWrite(LED_AUX, LOW);

  // attempt to connect to Wifi network:
  while (status != WL_CONNECTED && timeout < 4) {
    SerialPort->print("Attempting to connect to WiFi..");
#ifdef DEBUG
    SerialPort->print("[");
    SerialPort->print(ssid);
    SerialPort->print("][");
    SerialPort->print(pass);
    SerialPort->println("]");
#endif
    // Connect to WPA/WPA2 network:

    // Make char array of ssid and password as short as possible, as I think this is the cause of a bug
    // that stops WiFi.begin from working with a char array of fixed size. Tobe safe, this icky code is also
    // used when reading settings stored in EEPROM.

    String s_ssid = String(ssid);
    int l_ssid = s_ssid.length() + 1;
    char c_ssid[l_ssid];
    s_ssid.toCharArray(c_ssid, l_ssid);

    String s_pass = String(pass);
    int l_pass = s_pass.length() + 1;
    char c_pass[l_pass];
    s_pass.toCharArray(c_pass, l_pass);

    if (s_pass == "OPEN")
    {
      status = WiFi.begin(c_ssid);
    }
    else
    {
      status = WiFi.begin(c_ssid, c_pass);
    }

    delay(4000);

    timeout++;

    // Blink the LEDs for no reason other than it's cool.
    for (int j = 0; j < 3; j++)
    {
      for (int i = 0; i < 3; i++)
      {

        if (i == 0)
        {
          digitalWrite(LED_TX, HIGH);
          digitalWrite(LED_RX, LOW);
          digitalWrite(LED_AUX, LOW);
        }

        if (i == 1)
        {
          digitalWrite(LED_TX, LOW);
          digitalWrite(LED_RX, LOW);
          digitalWrite(LED_AUX, HIGH);
        }

        if (i == 2)
        {
          digitalWrite(LED_TX, LOW);
          digitalWrite(LED_RX, HIGH);
          digitalWrite(LED_AUX, LOW);
        }

        delay(200);
      }
    }

  }

  digitalWrite(LED_AUX, HIGH);
  digitalWrite(LED_TX, LOW);
  digitalWrite(LED_RX, LOW);

  if (timeout < 4)
  {

    connected_to_wifi = true;
    // you're connected now, so print out the data:
    SerialPort->println("Connected to the network.");
  }
  else
  {
    SerialPort->println("Could not connect :-( . Check SSID and PASSWORD settings.");
    digitalWrite(LED_AUX, LOW);
  }

  SerialPort->println("OK");

}

void AT_Telnet(String server) {

  server.toCharArray(buffer, 80);
  SerialPort->println("Opening connection to server...");
  // if you get a connection, report back via serial:

  int comma = server.indexOf(",");

  if ( comma > 0)
  {
    String address = server.substring(0, comma);
    int port = server.substring(comma + 1).toInt();
    address.toCharArray(buffer, comma + 1);

 
    if (client.connect(buffer, port)) {
      SerialPort->println("Connected to server");
  
      connected_to_web = true;
    }

  }
  else // no port specified, use default for telnet - 23
  {

    if (client.connect(buffer, 23)) {
      SerialPort->println("connected to server");
     
      connected_to_web = true;
    }
  }


}

void AT_Login() {
  SerialPort->println("Opening connection to server...");
  // if you get a connection, report back via serial:
  if (client.connect(server, 464)) {
    SerialPort->println("connected to server");
   
    connected_to_web = true;
  }
}

// Drop Wifi
void AT_drop()
{
  if (connected_to_wifi)
  {
    WiFi.disconnect();
    SerialPort->println("Disconnected from WiFi.");
    connected_to_wifi = false;
    digitalWrite(LED_AUX, LOW);
  }
}

void AT_Logout()
{
  if (connected_to_web)
  {
    client.stop();
    SerialPort->println("Disconnected");
    connected_to_web = false;
  }
}

void listNetworks() {
  // scan for nearby networks:

  if (!connected_to_wifi)
  {
    SerialPort->println("Must be connected to WiFi first.");
    return;
  }

  SerialPort->println("Scanning for WiFi networks..");
  int numSsid = WiFi.scanNetworks();
  if (numSsid == -1) {
    SerialPort->println("Couldn't get a wifi connection");
    while (true);
  }

  // print the list of networks seen:
  SerialPort->print("number of available networks:");
  SerialPort->println(numSsid);

  // print the network number and name for each network found:
  for (int thisNet = 0; thisNet < numSsid; thisNet++) {
    SerialPort->print(thisNet);
    SerialPort->print(") ");
    SerialPort->print(WiFi.SSID(thisNet));
    SerialPort->print("\tSignal: ");
    SerialPort->print(WiFi.RSSI(thisNet));
    SerialPort->print(" dBm");
    SerialPort->print("\tEncryption: ");
    byte encryption = WiFi.encryptionType(thisNet);
    SerialPort->println(String(encryption));
  }
}

void AT_getTime() {

  if (!connected_to_wifi)
  {
    SerialPort->println("Must be connected to WiFi first.");
    return;
  }

  SerialPort->println("Connecting to time server..");
  int numSsid = WiFi.scanNetworks();        // Seems to require some network access first. This works..
  IPAddress timeServer(129, 6, 15, 28);     // time.nist.gov NTP server
  unsigned int localPort = 2390;            // local port to listen for UDP packets
  Udp.begin(localPort);
  sendNTPpacket(timeServer);                  // send an NTP packet to a time server
  // wait to see if a reply is available
  delay(1000);
  if (Udp.parsePacket()) {

    // We've received a packet, read the data from it
    Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    SerialPort->print("Seconds since Jan 1 1900 = ");
    SerialPort->println(secsSince1900);

    // now convert NTP time into everyday time:
    SerialPort->print("Unix time = ");
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;
    // print Unix time:
    SerialPort->println(epoch);

    // print the hour, minute and second:
    SerialPort->print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
    SerialPort->print((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
    SerialPort->print(':');
    if (((epoch % 3600) / 60) < 10) {
      // In the first 10 minutes of each hour, we'll want a leading '0'
      SerialPort->print('0');
    }
    SerialPort->print((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
    SerialPort->print(':');
    if ((epoch % 60) < 10) {
      // In the first 10 seconds of each minute, we'll want a leading '0'
      SerialPort->print('0');
    }
    SerialPort->println(epoch % 60); // print the second
  }
  Udp.stop();
}

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address) {
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}




void printWifiData() {

  digitalWrite(LED_TX, HIGH);

  // Af we connected to the Wifi?

  SerialPort->println();

  if (connected_to_wifi)
  {

    // print your board's IP address:
    IPAddress ip = WiFi.localIP();
    SerialPort->print("IP Address: ");
    SerialPort->println(ip);

  }
  else
  {
    SerialPort->println("IP Address: (not currently assigned)");
  }

  // print your MAC address:
  byte mac[6];
  WiFi.macAddress(mac);
  SerialPort->print("MAC address: ");
  printMacAddress(mac);

  digitalWrite(LED_TX, LOW);
}

void printCurrentNet() {

  digitalWrite(LED_TX, HIGH);

  if (connected_to_wifi)
  {
    // print the SSID of the network you're attached to:
    SerialPort->print("SSID: ");
    SerialPort->println(WiFi.SSID());

    // print the MAC address of the router you're attached to:
    byte bssid[6];
    WiFi.BSSID(bssid);
    SerialPort->print("BSSID: ");
    printMacAddress(bssid);

    // print the received signal strength:
    long rssi = WiFi.RSSI();
    SerialPort->print("signal strength (RSSI):");
    SerialPort->println(String(rssi));

    // print the encryption type:
    byte encryption = WiFi.encryptionType();
    SerialPort->print("Encryption Type: ");
    SerialPort->println(String(encryption));
  }
  else
  {
    SerialPort->print("SSID: [");
    SerialPort->print(ssid);
    SerialPort->println("] (not currently connected)");

    SerialPort->print("Password: [");
    SerialPort->print(pass);
    SerialPort->println("] (not currently connected)");

  }
  digitalWrite(LED_TX, LOW);

}

void printMacAddress(byte mac[]) {
  for (int i = 5; i >= 0; i--) {
    if (mac[i] < 16) {
      SerialPort->print("0");
    }
    SerialPort->print(String(mac[i]));
    if (i > 0) {
      SerialPort->print(":");
    }
  }
  SerialPort->println();
}

void clearBuffer()
{
  // Clear buffer
  for (int i = 0; i < 80; i++)
    buffer[i] = 0;

  buffer_count = 0;
}

bool ReadData()
{
  // 1. Read and check for magic value (42) to say we have something
  // 2. Read SSID and Password if there is something

  byte value = EEPROM.read(0);
  char buf_s[16];
  char buf_p[16];

  if (value != 42) return false;

  for (int i = 0; i < 16; i++)
  {
    buf_s[i] = EEPROM.read(i + 1);
  }

  for (int i = 0; i < 16; i++)
  {
    buf_p[i] = EEPROM.read(i + 1 + 16);
  }

  String str_s = String(buf_s);
  String str_p = String(buf_p);

  int l_s = str_s.length() + 1;
  int l_p = str_p.length() + 1;

  str_s.toCharArray(ssid, l_s);
  str_p.toCharArray(pass, l_p);


  return true;

}

void WriteData()
{
  // 1. Write magic value (42) to say we have something
  // 2. Write SSID () and Password()

  // Erase
  for (int i = 0; i < 33; i++)
  {
    EEPROM.write(i, 0);
  }

  EEPROM.write(0, 42);

  for (int i = 0; i < 16; i++)
    EEPROM.write(i + 1, ssid[i]);

  for (int i = 0; i < 16; i++)
    EEPROM.write(i + 1 + 16, pass[i]);

}
