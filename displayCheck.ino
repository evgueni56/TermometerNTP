
extern "C" {
#include "user_interface.h"
}
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Time.h>
#include <stdlib.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <EEPROM.h>
#include <BlynkSimpleEsp8266.h>
#include <ESP8266WebServer.h>

// Data wire is plugged into port 2
#define ONE_WIRE_BUS 2

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// arrays to hold device address
DeviceAddress insideThermometer;

#define MINUTES 5
float tempC;
char ssid[] ="Termometer"; // Neame of the access point
char auth[] = "2e46004acfa446649327e04bad56fe22"; // Authentication key to Blynk
String message, Timestring, t_ssdi, t_pw, st, content;
IPAddress currentIP;
unsigned long Myhour, Myminute, Mysecond, Myyear, Mymonth, Myday, leap;

char epromdata[512];

const int led = 0;
int pinValue = 1;
int ReadStatus = 0;
float BatteryV;
//Timer instantiate
BlynkTimer SleepTimer;
unsigned long currentSecond;
byte PWonFlag = 0;
int wifipoints,i,j,numnets,buf_pointer,wifi_cause;
ESP8266WebServer server(80);
String qsid, qpass; //Holds the new credentials from AP

// LCD Object
Adafruit_PCD8544 MyLcd = Adafruit_PCD8544(14, 13, 12, 5, 4); //software SPI - is it better? For hardware: Adafruit_PCD8544(12, 5, 4)

//----------------------------------------------------------------------------------------------------------------------------

void setup()
{
	system_rtc_mem_read(66, &currentSecond, sizeof(currentSecond)); 
	currentSecond += MINUTES * 60;
	setTime(currentSecond); // set internal timer
	Serial.begin(74880);
	EEPROM.begin(512);
	EEPROM.get(0, epromdata);
	numnets = epromdata[0];
	pinMode(led, OUTPUT);
	digitalWrite(led, 1);
	Wire.begin();
	SetupTemeratureSensor();

	// Nokia Display
	MyLcd.begin();
	MyLcd.setContrast(50);
	MyLcd.setTextColor(BLACK);
	MyLcd.clearDisplay();
	MyLcd.setFont();
	MyLcd.setCursor(0, 0);

	int n = WiFi.scanNetworks(); //  Check if any WiFi in grange
	Serial.println(n);
	if (!n)
	{
	// No access points in range - just be a thermometer
		message = "No WiFi around";
		SleepTimer.setInterval(5 * 1000, SleepTFunc);
		wifi_cause = 0;
		return;
	}
// Check for known access points to connect
	wifi_cause = ConnectWiFi();
	Serial.print("Wifi cause: ");
	Serial.println(wifi_cause);
	switch (wifi_cause)
	{
	case 0: //Everything with normal WiFi connection goes here
	{
		MyLcd.clearDisplay();
		MyLcd.setFont();
		GetNtpTime();
		Blynk.config(auth);
	}
	break;
	case 1: //A known network does not connect
	{
		message = "No "+t_ssdi;
		MyLcd.clearDisplay();
		MyLcd.setCursor((84 - 6 * message.length()) / 2, 38);
		MyLcd.print(message);
		MyLcd.display();
		setupAP();
	}
	break;
	case 2: //No known networks
	{
		message = "No known net's";
		MyLcd.setCursor((84 - 6 * message.length()) / 2, 0);
		MyLcd.print(message);
		MyLcd.print("Starting AP");
		MyLcd.setCursor((84 - 6 * 11) / 2, 12);
		MyLcd.display();
		setupAP();
	}
	break;
	}

	//	message = "Credentials:";
	//	MyLcd.setCursor(0, 0);
	//	MyLcd.print(message);
	//	MyLcd.display();
	//	if (GetCredentials())
	//	{
	//		if (!InitWifi())
	//		{
	//			message = "No Connection";
	//			MyLcd.setCursor((84 - 6 * message.length()) / 2, 38);
	//			MyLcd.print(message);
	//			MyLcd.display();
	//			delay(1000);
	//		}
	//	}
	//	else
	//	{
	//		message = "No Connection";
	//		MyLcd.setCursor((84 - 6 * message.length()) / 2, 38);
	//		MyLcd.print(message);
	//		MyLcd.display();
	//		delay(1000);
	//	}
	//}
	
	SleepTimer.setInterval(5 * 1000, SleepTFunc);

}

void loop()
{
	if(wifi_cause)
	{
		server.handleClient();
	}
	else
	{
		SleepTimer.run();
		Blynk.run();
		//	digitalWrite(led, 0);
		ShowDisplay();
		BatteryV = float(analogRead(A0) / float(1024)*8.86);
	}
}

void SetupTemeratureSensor()
{
	sensors.begin();
	sensors.getDeviceCount();
	sensors.getAddress(insideThermometer, 0);
	sensors.setResolution(insideThermometer, 12);
}

unsigned long UnixTimeSec ()
{
	int cb = 0;
	WiFiUDP udp;
	IPAddress timeServerIP; // time.nist.gov NTP server address
	const char* ntpServerName = "bg.pool.ntp.org";
	const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
	byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
	unsigned int localPort = 2390;      // local port to listen for UDP packets
	udp.begin(localPort);
	//get a random server from the pool
	WiFi.hostByName(ntpServerName, timeServerIP);

	while (!udp.parsePacket())
	{
		// send an NTP packet to a time server
		// wait to see if a reply is available
		// set all bytes in the buffer to 0
		memset(packetBuffer, 0, NTP_PACKET_SIZE);
		// Initialize values needed to form NTP request
		// (see URL above for details on the packets)
		packetBuffer[0] = 0b11100011;   // LI, Version, Mode
		packetBuffer[1] = 0;     // Stratum, or type of clock
		packetBuffer[2] = 6;     // Polling Interval
		packetBuffer[3] = 0xEC;  // Peer Clock Precision
								 // 8 bytes of zero for Root Delay & Root Dispersion
		packetBuffer[12] = 49;
		packetBuffer[13] = 0x4E;
		packetBuffer[14] = 49;
		packetBuffer[15] = 52;

		// all NTP fields have been given values, now
		// you can send a packet requesting a timestamp:
		udp.beginPacket(timeServerIP, 123); //NTP requests are to port 123
		udp.write(packetBuffer, NTP_PACKET_SIZE);
		udp.endPacket();
		if (++cb > 15) return 0;
		delay(1000);
	}


	// We've received a packet, read the data from it
	udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

												//the timestamp starts at byte 40 of the received packet and is four bytes,
												// or two words, long. First, esxtract the two words:

	unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
	unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
	// combine the four bytes (two words) into a long integer
	// this is NTP time (seconds since Jan 1 1900):
	unsigned long secsSince1900 = highWord << 16 | lowWord;
	// now convert NTP time into everyday time:
	// Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
	const unsigned long seventyYears = 2208988800UL;
	// subtract seventy years:
	unsigned long epoch = secsSince1900-seventyYears;
	return epoch;
}

void GetNtpTime(void)
{
	currentIP = WiFi.localIP();
	message = "Connected";
	system_rtc_mem_read(65, &PWonFlag, 1);

	if (PWonFlag != 7) // Either power on or no NTP time yet
	{
		currentSecond = UnixTimeSec() + 3600 * 3; // Unix Seconds adjusted for the local time
		if (currentSecond == 3600 * 3)
		{
			return; // No connection to NTP
		}
		setTime(currentSecond); // set internal timer
		PWonFlag = 7;
		system_rtc_mem_write(65, &PWonFlag, 1);
	}
	// Populate date/time variables
	Mysecond = second();
	Myminute = minute();
	Myhour = hour();
	Myday = day();
	Mymonth = month();
	Myyear = year();
}

void ShowDisplay(void)
{
	String Date=String(day()), Month=String(month()), Hour=String(hour()), Minute=String(minute());
	if (day() < 10) Date = "0" + Date;
	if (month() < 10) Month = "0" + Month;
	if (hour() < 10) Hour = "0" + Hour;
	if (minute() < 10) Minute = "0" + Minute;
	Timestring = Hour + ":" +  Minute + "   " + Date + "." + Month;
	MyLcd.clearDisplay();
	MyLcd.setTextSize(1);
	MyLcd.setFont();
	MyLcd.setCursor(0, 0);
	MyLcd.print(Timestring);
	MyLcd.setFont(&FreeSansBold12pt7b);
	sensors.requestTemperatures();
	tempC = floor(sensors.getTempC(insideThermometer) * 10 + 0.5) / 10;
	MyLcd.setCursor(16, 30);
	if (tempC < 0) MyLcd.setCursor(12, 30);
	MyLcd.print(tempC, 1);
	MyLcd.setFont();
	MyLcd.setCursor(68, 8);
	MyLcd.setTextSize(2);
	MyLcd.print("o");
	MyLcd.setTextSize(1);
	MyLcd.setCursor((84 - 6 * message.length()) / 2, 38);
	MyLcd.print(message);
	MyLcd.display();
}

BLYNK_WRITE(V0)
{
	pinValue = param.asInt(); // assigning incoming value from pin V0 to a variable
	digitalWrite(led, pinValue);
	ReadStatus = 1;
}

BLYNK_CONNECTED()
{
	Blynk.syncAll();
}

void SleepTFunc()
{

	// No inernet connection - just sleep
	if (String(message).startsWith("No"))
	{ 
		ESP.deepSleep(MINUTES * 60 * 1000 * 1000); // deep sleep for MINUTES
		delay(500);
	}
	if (!Blynk.connected()) return; // Not yet connected to server
	// Now push the values
	Blynk.virtualWrite(V5, tempC);
	Blynk.virtualWrite(V4, BatteryV);

	if (!ReadStatus) return; // No value of the button yet
	if (pinValue) // Light is not ON - going to sleep
	{ 
//		ESP.deepSleep(MINUTES * 60 * 1000 * 1000); // deep sleep for 0.5 minute
		currentSecond = now();
		system_rtc_mem_write(66, &currentSecond, sizeof(currentSecond));
		system_deep_sleep_set_option(0);
		system_deep_sleep(MINUTES * 60 * 1000000);
		delay(100);
	}
	return;
}

int ConnectWiFi()
{
	int n = WiFi.scanNetworks();
	buf_pointer = 1;
	if (numnets == 0) return 2;
	for (i = 0; i < numnets; i++)
	{
		t_ssdi = String(epromdata + buf_pointer);
		buf_pointer += t_ssdi.length() + 1;
		t_pw = String(epromdata + buf_pointer);
		buf_pointer += t_pw.length() + 1;
		for (j = 0; j < n; j++)
		{
			if (t_ssdi == String(WiFi.SSID(j)))
			{
				int c = 0;
				WiFi.begin(t_ssdi.c_str(), t_pw.c_str());
				MyLcd.print("Connecting ");
				MyLcd.display();
				while (c < 20)
				{
					if (WiFi.status() == WL_CONNECTED)
					{
						MyLcd.setCursor(0, 0);
						MyLcd.print("Connected to:");
						MyLcd.setCursor(0, 14);
						MyLcd.print(" "); // Instead of "clear"
						MyLcd.setCursor(0, 14);
						MyLcd.print(t_ssdi.c_str());
						MyLcd.display();
						delay(1000);
						return 0;
					}
					MyLcd.setCursor(0, 14);
					MyLcd.print(" "); // Instead of "clear"
					MyLcd.setCursor(0, 14);
					MyLcd.print(c);
					MyLcd.display();
					delay(500);
					c++;
				}
				return 1;
			}

		}

	}
	return 2;
}

void setupAP(void)
{
	WiFi.mode(WIFI_STA);
	WiFi.disconnect();
	delay(100);
	int n = WiFi.scanNetworks();
	st = "<ol>";
	for (int i = 0; i < n; ++i)
	{
		// Print SSID and RSSI for each network found
		st += "<li>";
		st += WiFi.SSID(i);
		st += "</li>";
	}
	st += "</ol>";
	delay(100);
	WiFi.softAP(ssid);
	launchWeb();
}

void launchWeb(void)
{
	Serial.println("In Start");
	Serial.println(content);

	server.on("/", []() {
		IPAddress ip = WiFi.softAPIP();
		String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
		content = "<!DOCTYPE HTML>\r\n";
//		content += "<html xmlns = \"http://www.w3.org/1999/xhtml\">\r\n";
		content += "<head>\r\n";
		content += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" />\r\n";
		content += "<title>Точка за достъп</title>";
		content += "<head>\r\n";
		content += ipStr;
		content += "<p>";
		content += st;
		content += "</p><form method='get' action='setting'><label>SSID: </label><input name='ssid' length=32><input name='pass' length=64><input type='submit'></form>";
		content += "</html>";
		server.send(200, "text/html; charset=utf-8", content);
	});
	Serial.println(content.c_str());
	server.on("/setting", []() {
		qsid = server.arg("ssid");
		qpass = server.arg("pass");
		if (qsid.length() > 0 && qpass.length() > 0) 
		{
			// Should write qsid & qpass to EEPROM
			if (wifi_cause == 1) remove_ssdi();
			append_ssdi();
			content = "<!DOCTYPE HTML>\r\n<html>";
			content += "<p>saved to eeprom... reset to boot into new wifi</p></html>";
		}
		else {
			content = "Error";
		}
		server.send(200, "text/html", content);
	});
	Serial.println("In Server");
	Serial.println(content.c_str());

	server.begin();

}

void append_ssdi(void) 
{
	epromdata[0]++;
	for (i = 0; i < qsid.length(); i++)
		epromdata[i + buf_pointer] = qsid[i];
	buf_pointer += qsid.length();
	epromdata[buf_pointer] = 0;
	buf_pointer++;
	for (i = 0; i < qpass.length(); i++)
		epromdata[i + buf_pointer] = qpass[i];
	buf_pointer += qpass.length();
	epromdata[buf_pointer] = 0;
	buf_pointer++;
	for (i = 0; i < 32; i++)
	{
		Serial.print(16 * i, HEX);
		Serial.print("  ");
		for (j = 0; j < 16; j++)
		{
			Serial.print(epromdata[i * 16 + j], HEX);
			Serial.print(" ");
		}
		for (j = 0; j < 16; j++)
		{
			Serial.print(char(epromdata[i * 16 + j]));
		}
		Serial.println();
	}
	EEPROM.put(0, epromdata);
	EEPROM.commit();
	delay(500);
}

void remove_ssdi(void)
{
	epromdata[0]--;
	if (epromdata[0] == 0)
	{
		buf_pointer = 1;
		return; // No saved networks left
	}
	int block = t_ssdi.length() + t_pw.length() + 2;

	int old_pointer = buf_pointer - block; //Dest. pointer - points the ssdi to be removed
	for (i = 0; i < 512-buf_pointer; i++)
		epromdata[old_pointer + i] = epromdata[buf_pointer + i];
// Adjust the pointer
	buf_pointer = 1;
	for (i = 0; i < epromdata[0]; i++)
	{
		t_ssdi = String(epromdata + buf_pointer);
		buf_pointer += t_ssdi.length() + 1;
		t_pw = String(epromdata + buf_pointer);
		buf_pointer += t_pw.length() + 1;
	}
}
