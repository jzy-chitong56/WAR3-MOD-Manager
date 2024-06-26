#include "_dic.h"
#include "thread.h"
#include "main_core.h"

#include <QSplashScreen>
#include <QLabel>
#include <QTimer>
#include <QMessageBox>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QProcess>
#include <QFileInfo>

#include <winerror.h>

#include <QDebug>

const wchar_t *Core::regAllowFiles  = L"Allow Local Files",
              *Core::regGameVersion = L"Preferred Game Version";

Core::Core() : QObject(),
    splashScreen(new QSplashScreen(pxLogo))
{
    qRegisterMetaType<ThreadAction>("ThreadAction");

    splashScreen->setAttribute(Qt::WA_DeleteOnClose);

    QLabel *vrsLbl = new QLabel(splashScreen);
    vrsLbl->setFixedSize(350, 13);
    vrsLbl->move(0, 80);
    vrsLbl->setPixmap(pxVersion);
    vrsLbl->setAlignment(Qt::AlignHCenter);

    showMsg(d::STARTING___, Msgr::Busy);
    splashScreen->show();

    mountedMod = getMounted();
}

Core::~Core()
{
    if(splashScreen)
    {
        splashScreen->close();
        splashScreen = nullptr;
    }
}

void Core::closeSplash(QWidget *finish)
{
    if(splashScreen)
    {
        QCoreApplication::processEvents();

        finishWgt = finish;
        QTimer::singleShot(2000-bool(finish)*1000, this, SLOT(closeSplashTimed()));

        QCoreApplication::processEvents();
    }
}

void Core::closeSplashTimed()
{
    if(finishWgt)
    {
        splashScreen->finish(finishWgt);
        finishWgt = nullptr;
    }
    else splashScreen->close();
    splashScreen = nullptr;
}

void Core::showMsg(const QString &_msg, const Msgr::Type &msgType, const bool propagate)
{
    if(propagate) emit msg(_msg, msgType);

    if(splashScreen)
    {
        splashScreen->raise();
        QCoreApplication::processEvents();
        splashScreen->showMessage(_msg, Qt::AlignHCenter|Qt::AlignBottom, "#bbb");
    }

    switch(msgType)
    {
    default:
        break;
    case Msgr::Info:     QMessageBox::information(QApplication::activeModalWidget(), d::INFO, _msg);
        break;
    case Msgr::Error:    QMessageBox::warning(QApplication::activeModalWidget(), d::dERROR, _msg);
        break;
    case Msgr::Critical: QMessageBox::critical(QApplication::activeModalWidget(), d::dERROR, _msg);
    }
}

QString Core::getMounted()
{
    const QFileInfo &fiMounted(cfg.getSetting(Config::kGamePath)+"/"+md::w3mod);

    return fiMounted.exists() && fiMounted.isDir() ? fiMounted.isSymLink() ? QFileInfo(fiMounted.symLinkTarget()).fileName()
                                                                           : md::unknownMod
                                                   : QString();
}

void Core::launch(const bool editor, const QString &args)
{
    bool success;
    QString exe = editor ? d::WE_EXE : (setGameVersion ? d::WC3X_EXE : d::WC3_EXE);

    showMsg(d::LAUNCHING_X___.arg(editor ? d::WE : d::GAME), Msgr::Busy);

    if(editor || args.isEmpty())
    {
        success = QDesktopServices::openUrl(QUrl::fromLocalFile(cfg.getSetting(Config::kGamePath)+"/"+exe));
        if (!editor && !success && setGameVersion)
        {
            exe = d::WC3_EXE;
            success = QDesktopServices::openUrl(QUrl::fromLocalFile(cfg.getSetting(Config::kGamePath)+"/"+exe));
            exe = d::WC3R_EXE;
        }
    }
    else
    {
        QProcess war3;
        war3.setProgram(cfg.getSetting(Config::kGamePath)+"/"+exe);
        war3.setNativeArguments(args);
        success = war3.startDetached();
        if (!success && setGameVersion)
        {
            exe = d::WC3_EXE;
            QProcess war3;
            war3.setProgram(cfg.getSetting(Config::kGamePath)+"/"+exe);
            war3.setNativeArguments(args);
            success = war3.startDetached();
            exe = d::WC3R_EXE;
        }
    }
    if(success) showMsg(d::X_LAUNCHED_.arg(editor ? d::WE : d::GAME));
    else showMsg(d::FAILED_TO_X_.arg(d::lLAUNCH_X).arg(exe), Msgr::Error);
}

bool Core::setAllowOrVersion(const bool enable, const bool version)
{
    setGameVersion = version;
    HKEY hKey;
    if(Config::regOpenWC3(KEY_ALL_ACCESS, hKey))
    {
        DWORD value(enable);
        if(RegSetValueEx(hKey, version ? regGameVersion : regAllowFiles, 0, REG_DWORD,
                         reinterpret_cast<const BYTE*>(&value), sizeof(value))
                == ERROR_SUCCESS)
        {
            showMsg("Setting Completed");
            RegCloseKey(hKey);
            return true;
        }
        else showMsg(d::FAILED_TO_SET_X_.arg(version ? d::GAME_VERSION : d::ALLOW_FILES), Msgr::Error);
    }
    else showMsg(d::FAILED_TO_OPEN_REGK_, Msgr::Error);

    RegCloseKey(hKey);
    return false;
}

Core::MountResult Core::mountModCheck(const QString &modName)
{
    if(!mountedMod.isEmpty())
    {
        if(mountedMod == modName) return Mounted;
        else
        {
            showMsg(d::ALREADY_MOUNTEDc_X_.arg(mountedMod), Msgr::Error);
            return OtherMounted;
        }
    }
    else
    {
        const QFileInfo &fiGamePath(cfg.getSetting(Config::kGamePath));
        if(fiGamePath.isSymLink() || !fiGamePath.exists() || !fiGamePath.isDir())
        {
            showMsg(d::INVALID_X.arg(d::X_FOLDER).arg(d::WC3)+".", Msgr::Error);
            return MountFailed;
        }
        else return MountReady;
    }
}

Thread* Core::mountModThread(const QString &modName)
{
    showMsg(d::MOUNTING_X___.arg(modName), Msgr::Busy);

    return new Thread(ThreadAction::Mount, modName, cfg.pathMods, cfg.getSetting(Config::kGamePath));
}

bool Core::unmountModCheck()
{
    if(!mountedMod.isEmpty()) return true;
    else
    {
        showMsg(d::NO_MOD_X_.arg(d::lMOUNTED), Msgr::Error);
        return false;
    }
}

Thread* Core::unmountModThread()
{
    showMsg(d::UNMOUNTING_X___.arg(mountedMod), Msgr::Busy);

    return new Thread(ThreadAction::Unmount, mountedMod, cfg.pathMods, cfg.getSetting(Config::kGamePath));
}

bool Core::actionDone(const ThreadAction &action)
{
    showMsg(a2s(action));

    if(action == ThreadAction::Mount && action.success())
    {
        mountedMod = action.modName;
        return !action.errors();
    }
    else if(action == ThreadAction::Unmount && !action.errors())
    {
        mountedMod = QString();
        return true;
    }
    else return false;
}

QString Core::a2s(const ThreadAction &action)
{
    const d::ac_t &dac = d::ac.find(action.PROCESSING)->second;
    return QStringLiteral(u"%0.%1").arg(
       /* %0 */ action.aborted()           ? d::X_ABORTED.arg(action.PROCESSING+" "+action.modName+": ")
                : !action.filesProcessed() ? d::NO_FILES_TO_X.arg(dac[size_t(d::Ac::lPROCESS)])
                : action.success()         ? dac[size_t(d::Ac::X_PROCESSED)].arg(action.modName)
                                            : d::FAILED_TO_X.arg(dac[size_t(d::Ac::lPROCESS_X)]).arg(action.modName),

       /* %1 */ action.errors() ? QStringLiteral(u" [%0%1%2]")
                                    .arg(dac[size_t(d::Ac::X_PROCESSED)].arg(d::X_FILES).arg(action.get(ThreadAction::Success)),    // %0
                                         action.get(ThreadAction::Failed)
                                            ? ", "+d::X_FAILED.arg(d::X_FILES).arg(action.get(ThreadAction::Failed)) : QString(),   // %1
                                         action.get(ThreadAction::Missing)
                                            ? ", "+d::X_MISSING.arg(d::X_FILES).arg(action.get(ThreadAction::Missing)) : QString()) // %2
                                : QString());
}

QString Core::a2e(const ThreadAction &action)
{
    const d::ac_t &dac = d::ac.find(action.PROCESSING)->second;
    return QStringLiteral(u"%0: %1%2%3").arg(
   /* %0 */   action.aborted()           ? d::X_ABORTED.arg(action.PROCESSING)
              //: action.forced()          ? d::FORCE_X.arg(dac[size_t(d::Ac::lPROCESS)])
              : !action.filesProcessed() ? d::NO_FILES_TO_X.arg(dac[size_t(d::Ac::lPROCESS)])
              : action.success()         ? dac[size_t(d::Ac::PROCESSED)]
                                         : d::X_FAILED.arg(action.PROCESSING),
   /* %1 */   dac[size_t(d::Ac::X_PROCESSED)].arg(d::X_FILES).arg(action.get(ThreadAction::Success)),
   /* %2 */   action.get(ThreadAction::Failed)  ? ", "+d::X_FAILED.arg(d::X_FILES).arg(action.get(ThreadAction::Failed))   : QString(),
   /* %3 */   action.get(ThreadAction::Missing) ? ", "+d::X_MISSING.arg(d::X_FILES).arg(action.get(ThreadAction::Missing)) : QString());
}
