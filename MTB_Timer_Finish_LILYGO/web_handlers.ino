void handleCancel() {
  cancelRun(); server.sendHeader("Location", "/"); server.send(303);
}

void handleSetTime() {
  if (server.hasArg("ts")) {
    int64_t rxTs = (int64_t)atoll(server.arg("ts").c_str());
    timeOffsetMs = rxTs - (int64_t)millis();
    timeIsSynced = true; lastSyncAt = millis();
  }
  server.send(204, "text/plain", "");
}

void saveSettings() {
  prefs.begin("mtb-cfg2-l", false);
  prefs.putUInt("debounce",   cfg_debounce_ms);
  prefs.putUInt("result",     cfg_result_show_ms);
  prefs.putUInt("timeout",    cfg_run_timeout_ms);
  prefs.putUInt("loracomp",   cfg_lora_comp_ms);
  prefs.putUInt("retryiv",    cfg_retry_interval);
  prefs.putUInt("batmah",     cfg_bat_mah);
  prefs.putUChar("maxretry",  cfg_max_retries);
  prefs.putUChar("contrast",  cfg_contrast);
  prefs.putUChar("platepin",  cfg_plate_pin);
  prefs.putBool("platenc",    cfg_plate_nc);
  if (prefs.getUChar("lorapwr", 14) != cfg_lora_pwr) prefs.putUChar("lorapwr", cfg_lora_pwr);
  prefs.putUChar("btn2pin", cfg_btn2_pin);
  prefs.putUInt("autopage", cfg_page_auto_ms);
  prefs.putString("apssid",   cfg_ap_ssid);
  prefs.putString("appass",   cfg_ap_pass);
  prefs.end();
}

void handleSettingsSave() {
  bool needsRestart = false;
  if (server.hasArg("debounce"))   cfg_debounce_ms    = (uint32_t)server.arg("debounce").toInt();
  if (server.hasArg("result"))     cfg_result_show_ms = (uint32_t)server.arg("result").toInt();
  if (server.hasArg("timeout"))    cfg_run_timeout_ms = (uint32_t)(server.arg("timeout").toInt()*60000);
  if (server.hasArg("loracomp"))   cfg_lora_comp_ms   = (uint32_t)server.arg("loracomp").toInt();
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
  if (server.hasArg("retryiv"))    cfg_retry_interval = (uint32_t)server.arg("retryiv").toInt();
  if (server.hasArg("maxretry"))   cfg_max_retries    = (uint8_t)server.arg("maxretry").toInt();
  if (server.hasArg("batmah"))     cfg_bat_mah        = (uint32_t)server.arg("batmah").toInt();
  if (server.hasArg("contrast")) { cfg_contrast=(uint8_t)server.arg("contrast").toInt(); u8g2.setContrast(cfg_contrast); }
  if (server.hasArg("platepin")) { uint8_t np=(uint8_t)server.arg("platepin").toInt(); if(np!=cfg_plate_pin){cfg_plate_pin=np;needsRestart=true;} }
  if (server.hasArg("platenc")) {
    bool newNc=(server.arg("platenc")=="1");
    if(newNc!=cfg_plate_nc){cfg_plate_nc=newNc;detachInterrupt(digitalPinToInterrupt(cfg_plate_pin));attachInterrupt(digitalPinToInterrupt(cfg_plate_pin),onPlateTrigger,cfg_plate_nc?RISING:FALLING);}
  }
  if (server.hasArg("apssid")) { String s=server.arg("apssid");s.trim(); if(s.length()>0&&s!=String(cfg_ap_ssid)){s.toCharArray(cfg_ap_ssid,sizeof(cfg_ap_ssid));needsRestart=true;} }
  if (server.hasArg("appass")) { String p=server.arg("appass"); if(p.length()==0||p.length()>=8){p.toCharArray(cfg_ap_pass,sizeof(cfg_ap_pass));needsRestart=true;} }
  saveSettings();
  String loc="/?tab=cfg&saved=1"; if(needsRestart)loc+="&restart=1";
  server.sendHeader("Location",loc); server.send(303);
}

void handleSleep() {
  server.send(200,"text/html; charset=utf-8","<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><style>body{background:#0a0a0a;color:#aaa;font-family:Arial;display:flex;align-items:center;justify-content:center;height:100vh;margin:0;text-align:center}p{font-size:1.1em}</style></head><body><p>&#128274; Ger&auml;t schaltet ab...</p></body></html>");
  delay(400); goToSleep();
}
void handleRestart() {
  server.send(200,"text/html; charset=utf-8","<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><meta http-equiv='refresh' content='6;url=/'><style>body{background:#0a0a0a;color:#aaa;font-family:Arial;display:flex;align-items:center;justify-content:center;height:100vh;margin:0;text-align:center}p{font-size:1.1em}</style></head><body><p>&#128260; Neustart...<br><small style='color:#333'>Seite l&auml;dt in 6 s</small></p></body></html>");
  delay(400); ESP.restart();
}
void handleManualPing() {
  int state = radio.transmit("POG");
  if (state==RADIOLIB_ERR_NONE){loraTxCount++;}else{loraTxFail++;}
  server.send(200,"application/json","{\"sent\":true}");
}
void handleOtaPage() {
  server.send(200,"text/html; charset=utf-8","<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>OTA – ZIEL LILYGO</title><style>body{font-family:Arial;background:#0a0a0a;color:#eee;padding:20px;max-width:420px}h2{color:#f0a500}input[type=file]{width:100%;margin:10px 0}.btn{background:#1a3a1a;color:#4caf50;border:none;padding:10px 20px;border-radius:8px;cursor:pointer;margin-top:8px}.warn{color:#ff9800;font-size:.85em}</style></head><body><h2>&#128640; Firmware-Update (ZIEL LILYGO)</h2><p class='warn'>&#9888; Ger&auml;t startet nach dem Update neu.</p><form method='POST' action='/update' enctype='multipart/form-data'><label>Firmware-Datei (.bin):</label><input type='file' name='firmware' accept='.bin' required><br><button class='btn' type='submit'>&#11014; Hochladen</button></form><br><a href='/'>&#8592; Zur&uuml;ck</a></body></html>");
}
void handleOtaUpload() {
  server.sendHeader("Connection","close");
  server.send(200,"text/html; charset=utf-8",Update.hasError()?"<h2 style='color:#f44'>Fehler!</h2>":"<h2 style='color:#4caf50'>OK!</h2><p>Neustart...</p>");
  delay(500); ESP.restart();
}
void handleOtaStream() {
  HTTPUpload& u=server.upload();
  if(u.status==UPLOAD_FILE_START){if(!Update.begin(UPDATE_SIZE_UNKNOWN))Update.printError(Serial);}
  else if(u.status==UPLOAD_FILE_WRITE){if(Update.write(u.buf,u.currentSize)!=u.currentSize)Update.printError(Serial);}
  else if(u.status==UPLOAD_FILE_END){if(Update.end(true))Serial.printf("[OTA] %u Bytes\n",u.totalSize);else Update.printError(Serial);}
}
void handleExport() {
  if (sdPresent) {
    String csv = sdReadCsv();
    if (csv.length() > 0) {
      server.sendHeader("Content-Disposition","attachment; filename=\"ziel_history_sd.csv\"");
      server.send(200,"text/csv; charset=utf-8",csv); return;
    }
  }
  uint8_t si[MAX_HISTORY]; uint8_t vc=0;
  for(uint8_t i=0;i<historyCnt;i++){uint8_t pi=histPhys(i);if(history[pi]>0)si[vc++]=pi;}
  for(uint8_t i=1;i<vc;i++){uint8_t k=si[i];int8_t j=i-1;while(j>=0&&history[si[j]]>history[k]){si[j+1]=si[j];j--;}si[j+1]=k;}
  String csv="Rang,Name,Zeit_ms,Zeit,Datum\r\n";
  for(uint8_t r=0;r<vc;r++){
    uint8_t i=si[r]; char tbuf[12]; fmtTime(history[i],tbuf);
    String nm=String(historyNames[i]); nm.replace(",",";");
    char db[20]="";
    if(historyTimestamp[i]>0){time_t t2=(time_t)(historyTimestamp[i]/1000LL);struct tm* tm2=gmtime(&t2);snprintf(db,sizeof(db),"%02d.%02d.%04d %02d:%02d",tm2->tm_mday,tm2->tm_mon+1,tm2->tm_year+1900,tm2->tm_hour,tm2->tm_min);}
    csv+=String(r+1)+","+nm+","+String(history[i])+","+String(tbuf)+","+String(db)+"\r\n";
  }
  server.sendHeader("Content-Disposition","attachment; filename=\"ziel_history.csv\"");
  server.send(200,"text/csv; charset=utf-8",csv);
}
void handleReset() {
  bestTimeMs=0; historyCnt=0; histHead=0;
  memset(history,0,sizeof(history)); memset(historyNames,0,sizeof(historyNames)); memset(historyTimestamp,0,sizeof(historyTimestamp));
  currentRiderName[0]='\0'; sdClear();
  server.sendHeader("Location","/?tab=hist"); server.send(303);
}

void handleSdFormat() {
  if (!sdPresent) { server.send(200,"application/json","{\"ok\":false,\"msg\":\"Keine SD\"}"); return; }
  sdClear();
  server.send(200,"application/json","{\"ok\":true}");
}
