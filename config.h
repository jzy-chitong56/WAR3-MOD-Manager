#ifndef CONFIG_H
#define CONFIG_H

#include "_uo_map_qs.h"
#include <QCoreApplication>

#include <windef.h>  // winbase.h needs to be
#include <winbase.h> // preceded by windef.h
#include <apisetcconv.h>
#include <winreg.h>

class Config
{
         static const QChar   CFG_SEP;
public:  static const QString vOn, vOff, kGamePath, kHideEmpty; //, kMounted, kMountedError;

         const QString     pathMods = QCoreApplication::applicationDirPath()+"/mods";
private: const std::string pathCfg  = QCoreApplication::applicationDirPath().toStdString()+"/config.cfg";

         std::unordered_map<QString, QString> settings;

public:  Config();

         QString getSetting   (const QString &key)
         { return settings.find(key) == settings.end() ? QString() : settings[key]; }

         void    deleteSetting(const QString &key)
         { if(settings.find(key) != settings.end()) settings.erase(key); }

         void    saveSetting  (const QString &key, QString value);
         void    saveConfig()  const;

         static bool regOpenWC3(const REGSAM &accessMode, HKEY &hKey);
};

#endif // CONFIG_H
