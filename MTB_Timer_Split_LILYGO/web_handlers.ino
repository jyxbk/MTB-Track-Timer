void handleCancel() { cancelRun(); server.sendHeader("Location","/"); server.send(303); }

void handleSetTime() {
  if(server.hasArg("ts")){
    int64_t rxTs=(int64_t)atoll(server.arg("ts").c_str());
    timeOffsetMs=rxTs-(int64_t)millis(); timeIsSynced=true; lastSyncAt=millis();
  }
  server.send(204,"text/plain","");
}

void saveSettings() {
  prefs.begin("mtb-cfg3-l",false);
  prefs.putUInt("debounce",cfg_debounce_ms); prefs.putUInt("result",cfg_result_show_ms);
  prefs.putUInt("timeout",cfg_run_timeout_ms); prefs.putUInt("loracomp",cfg_lora_comp_ms);
  prefs.putUInt("retryiv",cfg_retry_interval); prefs.putUInt("batmah",cfg_bat_mah);
  prefs.putUChar("maxretry",cfg_max_retries); prefs.putUChar("contrast",cfg_contrast);
  prefs.putUChar("platepin",cfg_plate_pin); prefs.putBool("platenc",cfg_plate_nc);
  if (prefs.getUChar("lorapwr", 14) != cfg_lora_pwr) prefs.putUChar("lorapwr", cfg_lora_pwr);
  prefs.putUChar("btn2pin", cfg_btn2_pin);
  prefs.putUInt("autopage", cfg_page_auto_ms);
  prefs.putString("apssid",cfg_ap_ssid); prefs.putString("appass",cfg_ap_pass);
  prefs.end();
}

void handleSettingsSave() {
  bool needsRestart=false;
  if(server.hasArg("debounce"))cfg_debounce_ms=(uint32_t)constrain(server.arg("debounce").toInt(),50,2000);
  if(server.hasArg("result"))cfg_result_show_ms=(uint32_t)constrain(server.arg("result").toInt(),2000,30000);
  if(server.hasArg("timeout"))cfg_run_timeout_ms=(uint32_t)(constrain(server.arg("timeout").toInt(),1,30)*60000);
  if(server.hasArg("loracomp"))cfg_lora_comp_ms=(uint32_t)constrain(server.arg("loracomp").toInt(),0,500);
  if (server.hasArg("lorapwr")) {
    cfg_lora_pwr = (uint8_t)constrain(server.arg("lorapwr").toInt(), 2, 20);
    radio.setOutputPower(cfg_lora_pwr);
  }
  if (server.hasArg("btn2pin")) {
    uint8_t np = (uint8_t)constrain(server.arg("btn2pin").toInt(), 0, 255);
    if (np != cfg_btn2_pin) { cfg_btn2_pin = np; needsRestart = true; }
  }
  if (server.hasArg("autopage")) {
    int secs = server.arg("autopage").toInt();
    cfg_page_auto_ms = (secs > 0) ? (uint32_t)constrain(secs, 2, 60) * 1000UL : 0;
  }
  if(server.hasArg("retryiv"))cfg_retry_interval=(uint32_t)constrain(server.arg("retryiv").toInt(),500,10000);
  if(server.hasArg("maxretry"))cfg_max_retries=(uint8_t)constrain(server.arg("maxretry").toInt(),1,10);
  if(server.hasArg("batmah"))cfg_bat_mah=(uint32_t)constrain(server.arg("batmah").toInt(),100,10000);
  if(server.hasArg("contrast")){cfg_contrast=(uint8_t)server.arg("contrast").toInt();u8g2.setContrast(cfg_contrast);}
  if(server.hasArg("platepin")){uint8_t np=(uint8_t)server.arg("platepin").toInt();if(np!=cfg_plate_pin){cfg_plate_pin=np;needsRestart=true;}}
  if(server.hasArg("platenc")&&!needsRestart){bool nc=(server.arg("platenc")=="1");if(nc!=cfg_plate_nc){cfg_plate_nc=nc;detachInterrupt(digitalPinToInterrupt(cfg_plate_pin));attachInterrupt(digitalPinToInterrupt(cfg_plate_pin),onPlateTrigger,cfg_plate_nc?RISING:FALLING);}}
  else if(server.hasArg("platenc"))cfg_plate_nc=(server.arg("platenc")=="1");
  if(server.hasArg("apssid")){String s=server.arg("apssid");s.trim();if(s.length()>0&&s!=String(cfg_ap_ssid)){s.toCharArray(cfg_ap_ssid,sizeof(cfg_ap_ssid));needsRestart=true;}}
  if(server.hasArg("appass")){String p=server.arg("appass");if(p.length()==0||p.length()>=8){p.toCharArray(cfg_ap_pass,sizeof(cfg_ap_pass));needsRestart=true;}}
  saveSettings();
  String loc="/?tab=cfg&saved=1";if(needsRestart)loc+="&restart=1";
  server.sendHeader("Location",loc);server.send(303);
}

void handleSleep(){server.send(200,"text/html; charset=utf-8","<!DOCTYPE html><html><head><meta charset='utf-8'><style>body{background:#0a0a0a;color:#aaa;font-family:Arial;display:flex;align-items:center;justify-content:center;height:100vh;margin:0;text-align:center}p{font-size:1.1em}</style></head><body><p>&#128274; Ger&auml;t schaltet ab...</p></body></html>");delay(400);goToSleep();}
void handleRestart(){server.send(200,"text/html; charset=utf-8","<!DOCTYPE html><html><head><meta charset='utf-8'><meta http-equiv='refresh' content='6;url=/'><style>body{background:#0a0a0a;color:#aaa;font-family:Arial;display:flex;align-items:center;justify-content:center;height:100vh;margin:0;text-align:center}p{font-size:1.1em}</style></head><body><p>&#128260; Neustart...</p></body></html>");delay(400);ESP.restart();}
void handleManualPing() {
  loRaSend("PNG");
  server.send(200, "application/json", "{\"sent\":true}");
}

void handleExport() {
  if (sdPresent) {
    String csv=sdReadCsv();
    if(csv.length()>0){server.sendHeader("Content-Disposition","attachment; filename=\"split_history_sd.csv\"");server.send(200,"text/csv; charset=utf-8",csv);return;}
  }
  uint8_t si[MAX_HISTORY];uint8_t vc=0;
  for(uint8_t i=0;i<historyCnt;i++){uint8_t pi=histPhys(i);if(history[pi]>0)si[vc++]=pi;}
  for(uint8_t i=1;i<vc;i++){uint8_t k=si[i];int8_t j=i-1;while(j>=0&&history[si[j]]>history[k]){si[j+1]=si[j];j--;}si[j+1]=k;}
  String csv="Rang,Zeit_ms,Zeit,Datum\r\n";
  for(uint8_t r=0;r<vc;r++){
    uint8_t i=si[r];char tbuf[12];fmtTime(history[i],tbuf);
    char db[20]="";
    if(historyTimestamp[i]>0){time_t t2=(time_t)(historyTimestamp[i]/1000LL);struct tm* tm2=gmtime(&t2);snprintf(db,sizeof(db),"%02d.%02d.%04d %02d:%02d",tm2->tm_mday,tm2->tm_mon+1,tm2->tm_year+1900,tm2->tm_hour,tm2->tm_min);}
    csv+=String(r+1)+","+String(history[i])+","+String(tbuf)+","+String(db)+"\r\n";
  }
  server.sendHeader("Content-Disposition","attachment; filename=\"split_history.csv\"");
  server.send(200,"text/csv; charset=utf-8",csv);
}

void handleReset(){bestTimeMs=0;historyCnt=0;histHead=0;memset(history,0,sizeof(history));memset(historyNames,0,sizeof(historyNames));memset(historyTimestamp,0,sizeof(historyTimestamp));sdClear();server.sendHeader("Location","/?tab=hist");server.send(303);}

void handleSdFormat() {
  if (!sdPresent) { server.send(200,"application/json","{\"ok\":false,\"msg\":\"Keine SD\"}"); return; }
  sdClear();
  server.send(200,"application/json","{\"ok\":true}");
}
