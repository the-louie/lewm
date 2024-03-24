#include <NTPClient.h>
#include <WiFiUdp.h>

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

void array_to_string(byte array[], unsigned int len, char buffer[])
{
    for (unsigned int i = 0; i < len; i++)
    {
        byte nib1 = (array[i] >> 4) & 0x0F;
        byte nib2 = (array[i] >> 0) & 0x0F;
        buffer[i*2+0] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
        buffer[i*2+1] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
    }
    buffer[len*2] = '\0';
}

void FillCstring( char * &cstr, String TheString) {
 TheString.toCharArray(cstr, TheString.length()+1);
}

String getDateTime() {
  int hour = timeClient.getHours();
  String hourStr = String(hour);
  if (hour < 10) {
    hourStr = String("0") + hourStr;
  }
  int minute = timeClient.getMinutes();
  String minuteStr = String(minute);
  if (minute < 10) {
    minuteStr = String("0") + minuteStr;
  }
  int second = timeClient.getSeconds();
  String secondStr = String(second);
  if (second < 10) {
    secondStr = String("0") + secondStr;
  }

  time_t epochTime = timeClient.getEpochTime();

  //Get a time structure
  struct tm *ptm = gmtime ((time_t *)&epochTime);

  int monthDay = ptm->tm_mday;
  String monthDayStr = String(monthDay);
  if (monthDay < 9) {
    monthDayStr = String("0") + String(monthDayStr);
  }

  int currentMonth = ptm->tm_mon+1;
  String currentMonthStr = String(currentMonth);
  if (currentMonth < 10) {
    currentMonthStr = String("0") + String(currentMonthStr);
  }

  int currentYear = ptm->tm_year+1900;

  return String(currentYear) + "-" + currentMonthStr + "-" + monthDayStr + " " + hourStr + ":" + minuteStr + ":" + secondStr;
}

