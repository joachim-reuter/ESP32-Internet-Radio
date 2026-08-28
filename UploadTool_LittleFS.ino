#include <LittleFS.h>

void setup() {
  Serial.begin(115200);
  LittleFS.begin(true);

  File f = LittleFS.open("/stations.txt", "w");
  f.println("Radio MDR;http://mdr-284290-2.sslcast.mdr.de/mdr/284290/2/mp3/high/stream.mp3");
  f.println("Radio SAW;http://stream.radiosaw.de/saw-anhalt-wittenberg/mp3-192/");
  f.println("MDR Jump;http://mdr-284320-0.cast.mdr.de/mdr/284320/0/mp3/high/stream.mp3");
  f.println("MDR SPUTNIK;http://mdr-284330-0.cast.mdr.de/mdr/284330/0/mp3/high/stream.mp3");
  f.println("MDR KLASSIK;http://mdr-284350-0.cast.mdr.de/mdr/284350/0/mp3/high/stream.mp3");
  f.println("RSA;http://streams.rsa-sachsen.de/rsa-oldies/mp3-192/mediaplayerrsa");
  f.println("Deutschlandfunk;http://st01.dlf.de/dlf/01/128/mp3/stream.mp3");
  f.println("Antenne Brandenburg;http://dispatcher.rndfnk.com/rbb/antennebrandenburg/frankfurt/mp3/mid");
  f.println("Antenne MV;http://streams.antennemv.de/antennemv-live/mp3-192/amv");  
  f.close();

  File g = LittleFS.open("/wifi.txt", "w");  
  g.println("ssid=FRITZ!Box 7590 FI");
  g.println("password=57426211004645725795");  
  g.close();

  Serial.println("OK geschrieben");
}

void loop() {}