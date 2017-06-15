bool GetCredentials(void)
{
	int timeout = 0;
	int SL = 0;
	digitalWrite(led, 0);
	while (!SL && timeout < 70)
	{
		SL = Serial.readBytesUntil(13, ssid, 40);
		MyLcd.print(".");
		MyLcd.display();
		timeout++;
		delay(1000);
	}
	if (timeout == 70)
	{
		Serial.println("Got nothing");
		digitalWrite(led, 1);
		return false;
	}
	Serial.println("Got ssid");
	ssid[SL - 1] = '\0';
	Serial.println(ssid);
	int PL = 0;
	while (!PL) PL=Serial.readBytesUntil(13, password, 40);
	password[PL - 1] = '\0';
	Serial.println("Got pwd");
	Serial.println(password);
	digitalWrite(led, 1);
	return true;


}
bool InitWifi(void)
{
	Serial.print("using ");
	Serial.print(ssid);
	WiFi.begin(ssid, password);
	int timeout = 0;

	while (WiFi.status() != WL_CONNECTED && timeout < 30)
	{
		delay(500);
		MyLcd.clearDisplay();
		MyLcd.print("Connecting ");
		MyLcd.print(timeout);
		MyLcd.display();
		timeout++;

	}
	MyLcd.clearDisplay();
	MyLcd.setFont();
	if (timeout < 60)
	{
		Serial.println("Connected");
		StoreCredentials();
		GetNtpTime();
		Blynk.config(auth);
		return true;

	}
	return false;
}
void loadCredentials() {
	EEPROM.begin(512);
	EEPROM.get(0, ssid);
	EEPROM.get(0 + sizeof(ssid), password);
	char ok[2 + 1];
	EEPROM.get(0 + sizeof(ssid) + sizeof(password), ok);
	EEPROM.end();
	if (String(ok) != String("OK")) {
		ssid[0] = 0;
		password[0] = 0;
	}
}

void StoreCredentials()
{
	EEPROM.begin(512);
	EEPROM.put(0, ssid);
	EEPROM.put(0 + sizeof(ssid), password);
	char ok[2 + 1] = "OK";
	EEPROM.put(0 + sizeof(ssid) + sizeof(password), ok);
	EEPROM.commit();
	EEPROM.end();
}

/* This is a piece to calculate the date/time out of the Unix second
		Myday = currentSecond / (24 * 60 * 60);
		Myyear = (((Myday * 4) + 2) / 1461);
		if (Myyear % 4) leap = 0;
		else leap = 1;
		Myday = Myday - ((Myyear * 1461) + 1) / 4;
		if (Myday > 58 + leap)
		{
			if (leap)
				Myday = Myday + 1;
			else Myday = Myday + 2;
		}
		else Myday = Myday + 0;
		Mymonth = ((Myday * 12) + 6) / 367;
		Myday = Myday + 1 - ((Mymonth * 367) + 5) / 12;
		Mymonth++;
		Myyear = Myyear + 1970;
		Myhour = (currentSecond % 86400L) / 3600;
		Myminute = (currentSecond % 3600) / 60;
		Mysecond = currentSecond % 60;
		*/
