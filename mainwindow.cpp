#include "_dic.h"
#include "_utils.h"
#include "thread.h"
#include "main_core.h"
#include "mainwindow.h"
#include "dg_shortcuts.h"
#include "dg_settings.h"

#include <QMenuBar>
#include <QToolBar>
#include <QLayout>
#include <QCheckBox>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QStatusBar>
#include <QLabel>
#include <QDirIterator>
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QTimer>
#include <QLineEdit>

#include <winerror.h>

#include <QDebug>

/********************************************************************/
/*      MOD TABLE       *********************************************/
/********************************************************************/
    ModTable::ModTable() : QTableWidget(0, 3)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setContextMenuPolicy(Qt::ActionsContextMenu);
        setEditTriggers(QAbstractItemView::NoEditTriggers);
        setDragEnabled(false);
        setAlternatingRowColors(true);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setSelectionBehavior(QAbstractItemView::SelectRows);
        QFont tableFont = font();
        tableFont.setPointSizeF(tableFont.pointSize()*1.2);
        setFont(tableFont);

            verticalHeader()->hide();
            setHorizontalHeaderLabels({ d::MOD, d::SIZE, d::FILES });
            horizontalHeader()->setStretchLastSection(true);
            horizontalHeaderItem(0)->setTextAlignment(Qt::AlignVCenter|Qt::AlignLeft);
            horizontalHeaderItem(1)->setTextAlignment(Qt::AlignVCenter|Qt::AlignRight);
            horizontalHeaderItem(2)->setTextAlignment(Qt::AlignVCenter|Qt::AlignLeft);
    }

    QLabel* ModTable::cellLabel(const int row, const int column) const
    { return dynamic_cast<QLabel *>(cellWidget(row, column)); }

    void ModTable::resizeCR(const int row)
    {
        resizeColumnToContents(0);
        resizeColumnToContents(1); // Last column (2) is stretched
        if(row >=0 && row < rowCount()) resizeRowToContents(row);
    }

    void ModTable::addMod(const QString &modName, const int row, const bool addData)
    {
        if(addData)
        {
            modNames.insert(row, modName);
            modData.insert({ modName, md::newData(row) });
            for(int i=row+1; i < modNames.length(); ++i) // Renumber mods following added
                md::setRow(modData[modNames[i]], i);
        }

        insertRow(row);

        QLabel *nameLbl = new QLabel(modName),
               *sizeLbl = new QLabel(d::ZERO_MB),
               *filesLbl = new QLabel(d::ZERO_FILES);
        nameLbl->setContentsMargins(3, 0, 3, 0);
        setCellWidget(row, 0, nameLbl);
        sizeLbl->setContentsMargins(3, 0, 3, 0);
        sizeLbl->setAlignment(Qt::AlignVCenter|Qt::AlignRight);
        setCellWidget(row, 1, sizeLbl);
        filesLbl->setContentsMargins(3, 0, 3, 0);
        setCellWidget(row, 2, filesLbl);
        resizeCR(row);
    }

    void ModTable::deleteMod(const QString &modName)
    {
        if(md::exists(modData, modName))
        {
            const int row = this->row(modName);

            modNames.removeAt(row);
            modData.erase(modName);
            for(int i=row; i < modNames.length(); ++i) // Renumber mods following deleted
                md::setRow(modData[modNames[i]], i);

            removeRow(row);
            resizeCR();
        }
    }

    void ModTable::updateMod(const QString &modName, const QString &modSize, const QString &fileCount, const qint64 size)
    {
        if(md::exists(modData, modName))
        {
            const int row = this->row(modName);

            QLabel *sizeLbl = cellLabel(row, 1),
                   *fileLbl = cellLabel(row, 2);

            const QString &oldSize = sizeLbl->text(),
                          &oldCount = fileLbl->text();

            bool resize = oldSize.length() != modSize.length() || oldCount.length() != fileCount.length(),
                 focus = !hasFocus() && currentRow() == row;
            
            setSize(modName, size);
            sizeLbl->setText(modSize);
            fileLbl->setText(fileCount);

            if(isRowHidden(row))
            {
                if(modSize != d::ZERO_MB || fileCount != d::ZERO_FILES)
                {
                    showRow(row);
                    resize = true;
                }
                else focus = false;
            }
            else focus = focus && oldSize == d::ZERO_MB && oldCount == d::ZERO_FILES;

            if(resize) resizeCR(row);
            if(focus) setFocus();
        }
    }

    void ModTable::renameMod(const QString &modName, const QString &newName)
    {
        if(md::exists(modData, modName))
        {
            const int row = this->row(modName);

            modNames[row] = newName;

            const md::data &data = modData[modName];
            modData.erase(modName);
            modData.insert({ newName, data });

            cellLabel(row, 0)->setText(newName);
            resizeCR();
        }
    }

/********************************************************************/
/*      MAIN WINDOW     *********************************************/
/********************************************************************/
MainWindow::MainWindow(Core *const core) : QMainWindow(),
    core(core),
    gameIcons({ u::largestIcon(QIcon(":/icons/war3.ico")),      u::largestIcon(QIcon(":/icons/war3x.ico")),
                u::largestIcon(QIcon(":/icons/war3_mod.ico")),  u::largestIcon(QIcon(":/icons/war3x_mod.ico")) }),
    editIcons({ u::largestIcon(QIcon(":/icons/worldedit.ico")), u::largestIcon(QIcon(":/icons/worldedit_mod.ico")) })
{
    core->setParent(this);
    msgr.setParent(this);

    showMsg(d::SETUP_UI___, Msgr::Busy);

    setWindowTitle(d::WC3MM);
    setMinimumSize(420, 350);

    // MENUBAR
        QMenu *fileMenu  = new QMenu(d::aFILE),
              *toolsMenu = new QMenu(d::aTOOLS),
              *aboutMenu = new QMenu(d::aHELP);
        menuBar()->addMenu(fileMenu);
        menuBar()->addMenu(toolsMenu);
        menuBar()->addMenu(aboutMenu);

            QAction *acOpenGameFolder = new QAction(d::OPEN_X.arg(d::X_FOLDER).arg(d::aX).arg(d::GAME)),
                    *acOpenModsFolder = new QAction(d::OPEN_X.arg(d::X_FOLDER).arg(d::aX).arg(d::MODS)),
                    *acOpenShortcuts  = new QAction(d::aX.arg(d::CREATE_uSHORTCUTS)),
                    *acOpenSettings   = new QAction(d::aX.arg(d::SETTINGS)),
                    *acOpenAbout      = new QAction(d::aX.arg(d::ABOUT));
            fileMenu->addActions({ acOpenGameFolder, acOpenModsFolder });
            toolsMenu->addActions({ acOpenShortcuts, acOpenSettings });
            aboutMenu->addAction(acOpenAbout);

    // TOOLBARS
    QToolBar *gameToolBar = new QToolBar(d::GAME_cTOOLBAR),
             *modsToolBar = new QToolBar(d::MODS_cTOOLBAR);
    addToolBar(gameToolBar);
    addToolBarBreak();
    addToolBar(modsToolBar);
    gameToolBar->setIconSize(QSize(32, 32));
    // QAction with iconSize 32x32 appears to have default size of 35x35 (including hover background+border)
    const int tbMargins = 5, gtbItemMargins = 3, gtbMargins = tbMargins-gtbItemMargins; // <--^ so adjust margins/spacing
    gameToolBar->layout()->setSpacing(gtbMargins);
    modsToolBar->layout()->setSpacing(tbMargins);
    gameToolBar->layout()->setContentsMargins(gtbMargins, gtbMargins, gtbMargins, gtbMargins);
    modsToolBar->layout()->setContentsMargins(tbMargins, tbMargins, tbMargins, tbMargins);

        // GAME TOOLBAR WIDGETS
        launchGameAc   = new QAction; // Default visual margin == gtbItemMargins
        launchEditorAc = new QAction;
        gameToolBar->addAction(launchGameAc);
        gameToolBar->addAction(launchEditorAc);

        gameToolBar->addSeparator();

        QWidget *regTools = new QWidget;
        gameToolBar->addWidget(regTools);
        QVBoxLayout *regLayout = new QVBoxLayout;
        regTools->setLayout(regLayout);
        regLayout->setSpacing(0);
        regLayout->setContentsMargins(gtbItemMargins, gtbItemMargins, gtbItemMargins, gtbItemMargins);

            allowFilesCbx  = new QCheckBox(d::ALLOW_aLOCAL_FILES);
            gameVersionCbx = new QCheckBox(d::aX.arg(d::EXPANSION));
            regLayout->addWidget(allowFilesCbx);
            regLayout->addWidget(gameVersionCbx);

        // MODS TOOLBAR WIDGETS
        toggleMountBtn = new QPushButton;
        addModBtn      = new QPushButton(d::aX.arg(d::ADD_uMOD));
        refreshBtn     = new QPushButton(d::aREFRESH);
        modsToolBar->addWidget(toggleMountBtn);
        modsToolBar->addWidget(addModBtn);
        modsToolBar->addWidget(refreshBtn);
        toggleMountBtn->setCheckable(true);
        refreshBtn->setShortcut(QKeySequence(Qt::Key_F5));

    // MOD LIST
    modTable = new ModTable;
    setCentralWidget(modTable);

                toggleMountAc = new QAction;
        QAction *actionOpen   = new QAction(d::OPEN_X.arg(d::FOLDER)),
                *actionRename = new QAction(d::RENAME),
                *actionDelete = new QAction(d::dDELETE);
        modTable->addActions({ toggleMountAc, actionOpen, actionRename, actionDelete });

    // STATUSBAR
    setStatusBar(new QStatusBar);
    statusBar()->setSizeGripEnabled(false);
    statusBar()->setStyleSheet("QLabel { margin: 3; }");

        statusLbl = new QLabel;
        statusBar()->addWidget(statusLbl);

    // initialize conditional UI & fetch mods
    refresh(true);

    // MESSAGES
    connect(core,  &Core::msg, this, &MainWindow::showStatus);
    connect(&msgr, &Msgr::msg, this, &MainWindow::showMsg);

    // MENUBAR
    connect(acOpenGameFolder, &QAction::triggered, this, &MainWindow::openGameFolder);
    connect(acOpenModsFolder, &QAction::triggered, this, &MainWindow::openModsFolder);
    connect(acOpenShortcuts,  &QAction::triggered, this, &MainWindow::openShortcuts);
    connect(acOpenSettings,   &QAction::triggered, this, &MainWindow::openSettings);
    connect(acOpenAbout,      &QAction::triggered, this, &MainWindow::openAbout);
    // TOOLBAR
    connect(launchGameAc,   SIGNAL(triggered()),   core, SLOT(launch()));
    connect(launchEditorAc, &QAction::triggered,   this, &MainWindow::launchEditor);
    connect(allowFilesCbx,  SIGNAL(toggled(const bool)), SLOT(setAllowOrVersion(const bool)));
    connect(gameVersionCbx, &QCheckBox::toggled,   this, &MainWindow::setVersion);
    connect(addModBtn,      &QPushButton::clicked, this, &MainWindow::addMod);
    connect(refreshBtn,     SIGNAL(clicked()),           SLOT(refresh()));
    // MOD LIST
    connect(actionOpen,   &QAction::triggered, this, &MainWindow::openModFolder);
    connect(actionRename, &QAction::triggered, this, &MainWindow::renameMod);
    connect(actionDelete, &QAction::triggered, this, &MainWindow::deleteMod);
}

void MainWindow::show()
{
    QMainWindow::show();
    showMsg(d::READY_, Msgr::Permanent);
    core->closeSplash(this);
}

void MainWindow::showStatus(const QString &msg, const Msgr::Type &msgType)
{
    if(statusLbl && (msgType == Msgr::Permanent || msgType == Msgr::Critical))
        statusLbl->setText(msg);

    statusBar()->showMessage(msg, msgType == Msgr::Busy ? 0 : 10000);
}

void MainWindow::showMsg(const QString &msg, const Msgr::Type &msgType)
{
    showStatus(msg, msgType);
    core->showMsg(msg, msgType, false);
}

bool MainWindow::tryBusy(const QString &modName)
{
    const bool exists = md::exists(modTable->modData, modName);
    if(!exists || !md::busy(modTable->modData[modName]))
    {
        if(exists) std::get<int(md::Busy)>(modTable->modData[modName]) = true;
        return true;
    }
    else
    {
        showMsg(d::X_BUSY.arg(modName), Msgr::Info);
        return false;
    }
}

bool MainWindow::isExternal(const QString &modName)
{
    const QFileInfo &fiMod(core->cfg.pathMods+"/"+modName);
    return modName == md::unknownMod || (!fiMod.isSymLink() && !fiMod.exists());
}

void MainWindow::updateLaunchBtns()
{
    launchGameAc->setEnabled(false);
    launchEditorAc->setEnabled(false);

    const bool modEnabled = !core->mountedMod.isEmpty(),
               exp = gameVersionCbx->isChecked();

    launchGameAc->setIcon(gameIcons[size_t(modEnabled<<1)|exp]);
    launchGameAc->setToolTip(d::LAUNCH_X.arg(modEnabled ? core->mountedMod+" ("+(exp ? d::EXPANSION : d::CLASSIC)+")"
                                                        : exp ? d::TFT : d::ROC));
    launchEditorAc->setIcon(editIcons[modEnabled]);
    launchEditorAc->setToolTip(d::LAUNCH_X.arg(d::WE)+(modEnabled ? " ("+core->mountedMod+")"
                                                                  : QString()));

    launchGameAc->setEnabled(true);
    launchEditorAc->setEnabled(true);
}

void MainWindow::updateAllowOrVersion(const bool version)
{
    if(version) gameVersionCbx->setEnabled(false);
    else allowFilesCbx->setEnabled(false);

    HKEY hKey;
    if(!Config::regOpenWC3(KEY_READ, hKey)) showMsg(d::FAILED_TO_OPEN_REGK_, Msgr::Error);
    else
    {
        DWORD type = REG_DWORD, size = 1024, value;
        LSTATUS result = RegQueryValueEx(hKey, version ? Core::regGameVersion : Core::regAllowFiles,
                                         nullptr, &type, LPBYTE(&value), &size);

        if(result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) // ERROR_FILE_NOT_FOUND: value does not exist
            showMsg(d::FAILED_TO_GET_X_.arg(version ? d::GAME_VERSION : d::X_SETTING.arg(d::ALLOW_FILES)), Msgr::Error);

        else if(version) gameVersionCbx->setChecked(QString::number(value) == Config::vOn);
        else             allowFilesCbx->setChecked(QString::number(value) == Config::vOn);
    }

    RegCloseKey(hKey);

    updateLaunchBtns();
    if(version) gameVersionCbx->setEnabled(true);
    else allowFilesCbx->setEnabled(true);
}

void MainWindow::updateMountState(const QString &modName, const bool enableBtn)
{
    if(enableBtn) toggleMountBtn->setEnabled(false);

    disconnect(toggleMountBtn, &QPushButton::clicked, this, &MainWindow::unmountMod);
    disconnect(toggleMountBtn, &QPushButton::clicked, this, &MainWindow::mountMod);
    disconnect(toggleMountAc,  &QAction::triggered,   this, &MainWindow::unmountMod);
    disconnect(toggleMountAc,  &QAction::triggered,   this, &MainWindow::mountMod);

    if(core->mountedMod.isEmpty())
    {
        toggleMountAc->setText(d::aX.arg(d::MOUNT));
        toggleMountBtn->setText(d::aX.arg(d::MOUNT));
        toggleMountBtn->setToolTip(QString());
        toggleMountBtn->setChecked(false);
        connect(toggleMountBtn, &QPushButton::clicked, this, &MainWindow::mountMod);
        connect(toggleMountAc,  &QAction::triggered,   this, &MainWindow::mountMod);

        if(!modName.isEmpty() && md::exists(modTable->modData, modName))
        {
            const int row = modTable->row(modName);

            QWidget *nameItem  = modTable->cellWidget(row, 0),
                    *sizeItem  = modTable->cellWidget(row, 1),
                    *filesItem = modTable->cellWidget(row, 2);
            if(nameItem->styleSheet() != QString())
            {
                nameItem->setStyleSheet(QString());
                sizeItem->setStyleSheet(QString());
                filesItem->setStyleSheet(QString());
                nameItem->setFont(modTable->font());
                sizeItem->setFont(modTable->font());
                filesItem->setFont(modTable->font());

                modTable->resizeCR(row);
            }
        }
    }
    else
    {
        toggleMountAc->setText(d::UNaMOUNT);
        toggleMountBtn->setText(d::UNaMOUNT);
        toggleMountBtn->setToolTip(d::UNMOUNT+" "+core->mountedMod);
        toggleMountBtn->setChecked(true);
        connect(toggleMountBtn, &QPushButton::clicked, this, &MainWindow::unmountMod);
        connect(toggleMountAc,  &QAction::triggered,   this, &MainWindow::unmountMod);

        if(md::exists(modTable->modData, modName.isEmpty() ? core->mountedMod : modName))
        {
            const int row = modTable->row(modName.isEmpty() ? core->mountedMod : modName);

            QWidget *nameItem  = modTable->cellWidget(row, 0),
                    *sizeItem  = modTable->cellWidget(row, 1),
                    *filesItem = modTable->cellWidget(row, 2);

            if(nameItem->styleSheet() == QString())
            {
                const QString &style = "border: 2px dashed #f7f500; background-color: #555; color: #eee;";

                nameItem->setStyleSheet(style+"border-right: 0;");
                sizeItem->setStyleSheet(style+"border-left: 0; border-right: 0;");
                filesItem->setStyleSheet(style+"border-left: 0;");
                nameItem->setFont(modTable->font());
                sizeItem->setFont(modTable->font());
                filesItem->setFont(modTable->font());
                
                modTable->resizeCR(row);
            }
        }
    }

    if(!modName.isEmpty()) modTable->setFocus();
    updateLaunchBtns();
    if(enableBtn) toggleMountBtn->setEnabled(true);
}

void MainWindow::launchEditor() { core->launch(true); }

void MainWindow::setAllowOrVersion( bool enable,  bool version)
{
    enable = allowFilesCbx->isChecked();
    version = gameVersionCbx->isChecked();
    if(core->setAllowOrVersion(enable, version)) updateLaunchBtns();
    else updateAllowOrVersion(version);
}

void MainWindow::refresh(const bool silent)
{
    refreshBtn->setEnabled(false);

    refreshing = !silent;
    if(!silent) showMsg(d::REFRESHING___, Msgr::Busy);

    core->mountedMod = core->getMounted();
    
    Thread *thr = new Thread(ThreadAction::ModData, QString(), core->cfg.pathMods);
    connect(thr, &Thread::modDataReady, this, &MainWindow::scanMods);
    thr->start(modTable->modData, core->mountedMod);

    updateAllowOrVersion();
    updateAllowOrVersion(true);
}

void MainWindow::scanMods(const md::modData &modData, const QStringList &modNames)
{
    const QString &selectedMod = modTable->modSelected() && modTable->currentRow() < modTable->modNames.length()
                                    ? modTable->modNames[modTable->currentRow()] : QString();
    int selectedRow = -1;
    bool mountedFound = core->mountedMod.isEmpty();

    modTable->setRowCount(0);
    modTable->modData = modData;
    modTable->modNames = modNames;
    scanCount = modTable->modNames.length();

    for(const QString &modName : modTable->modNames)
    {
        const int row = modTable->rowCount();
        bool externalMod = false;

        modTable->addMod(modName, row);

        if(modName == selectedMod) selectedRow = row;

        if(mountedFound || modName != core->mountedMod)
        {
            if(core->cfg.getSetting(Config::kHideEmpty) == Config::vOn) modTable->hideRow(row);
        }
        else
        {
            mountedFound = true;

            if(selectedRow == -1) selectedRow = row;
            
            const QString &modPath = core->cfg.getSetting(Config::kGamePath)+"/"+md::w3mod;
            const QFileInfo &fiMounted(modPath);
            
            externalMod = fiMounted.absolutePath() != core->cfg.pathMods;
            if(externalMod)
            {
                if(fiMounted.exists() && fiMounted.isDir())
                {
                    Thread *thr = new Thread(ThreadAction::ScanEx, modName);
                    connect(thr, &Thread::scanModUpdate, modTable, &ModTable::updateMod);
                    connect(thr, &Thread::scanModReady,  this,     &MainWindow::scanModDone);
                    thr->start(fiMounted.isSymLink() ? fiMounted.symLinkTarget() : modPath);
                }
                else scanModDone(modName);
            }
        }

        if(!externalMod)
        {
            Thread *thr = new Thread(ThreadAction::Scan, modName, core->cfg.pathMods);
            connect(thr, &Thread::scanModUpdate, modTable, &ModTable::updateMod);
            connect(thr, &Thread::scanModReady,  this,     &MainWindow::scanModDone);
            thr->start();
        }
    }

    if(selectedRow >= 0 && selectedRow < modTable->rowCount()) modTable->selectRow(selectedRow);

    if(!core->mountedMod.isEmpty() && !mountedFound)
        showMsg(d::FAILED_TO_FIND_MOUNTED_X_.arg(core->mountedMod), Msgr::Critical);

    updateMountState();
}

void MainWindow::scanModDone(const QString &modName)
{
    if(md::exists(modTable->modData, modName))
    {
        const int row = modTable->row(modName);

        if(row == modTable->currentRow())
        modTable->setFocus();
    }

    if(--scanCount <= 0)
    {
        refreshBtn->setEnabled(true);
        if(refreshing)
        {
            refreshing = false;
            showMsg(d::REFRESHED_);
        }
    }
}

void MainWindow::mountMod()
{
    if(!modTable->modSelected()) showMsg(d::SELECT_MOD_TO_MOUNT_, Msgr::Info);
    else
    {
        const QString &modName = modTable->modNames[modTable->currentRow()];

        if(tryBusy(modName) && core->mountModCheck(modName) == Core::MountReady)
        {
            toggleMountBtn->setEnabled(false);

            core->mountedMod = modName;
            Thread *thr = core->mountModThread(modName);
            connect(thr, &Thread::resultReady, this, &MainWindow::actionDone);
            thr->start();
        }

        updateMountState(modName, false);
    }
}

void MainWindow::unmountMod()
{
    if(tryBusy(core->mountedMod) && core->unmountModCheck())
    {
        toggleMountBtn->setEnabled(false);

        Thread *thr = core->unmountModThread();
        connect(thr, &Thread::resultReady, this, &MainWindow::actionDone);
        thr->start();
    }
    else updateMountState(core->mountedMod);
}

void MainWindow::addMod()
{
    const QString &src = QFileDialog::getExistingDirectory(this, d::ADD_uMOD, QString(),
                                                           QFileDialog::ShowDirsOnly|QFileDialog::HideNameFilterDetails);

    if(!src.isEmpty())
    {
        QMessageBox copyMove(this);
        copyMove.setWindowTitle(d::COPY_MOVEq);
        copyMove.setText(d::COPY_MOVE_LONGq+"\n"+src);
        copyMove.addButton(d::MOVE, QMessageBox::ActionRole);
        QPushButton *copyBtn = copyMove.addButton(d::COPY, QMessageBox::ActionRole);
        copyMove.addButton(QMessageBox::Cancel);
        copyMove.setIcon(QMessageBox::Question);

        copyMove.exec();

        if(copyMove.clickedButton() != copyMove.button(QMessageBox::Cancel))
        {
            const QString &modName = QDir(src).dirName(),
                          &dst     = core->cfg.pathMods+"/"+modName;

            if(QFileInfo().exists(dst)) showMsg(d::MOD_EXISTS_, Msgr::Error);
            else
            {
                showMsg(d::ADDING_X___.arg(modName), Msgr::Busy);

                Thread *thr = new Thread(ThreadAction::Add, modName, core->cfg.pathMods, core->cfg.getSetting(Config::kGamePath));
                connect(thr, &Thread::resultReady,   this,     &MainWindow::actionDone);
                connect(thr, &Thread::modAdded, modTable, &ModTable::addMod);
                connect(thr, &Thread::scanModUpdate, modTable, &ModTable::updateMod);
                thr->start(src, dst, copyMove.clickedButton() == copyBtn);
            }
        }
    }
}

void MainWindow::deleteMod()
{
    if(!modTable->modSelected()) showMsg(d::NO_MOD_X_.arg(d::lSELECTED));
    else
    {
        const QString &modName = modTable->modNames[modTable->currentRow()];

        if(modName == core->mountedMod)
            showMsg(d::CANT_X_MOUNTED_.arg(d::lDELETE), Msgr::Info);
        else if(QMessageBox::warning(this, d::PERM_DELETE_Xq.arg(modName), d::PERM_DELETE_X_LONGq.arg(modName,
                                         modTable->cellLabel(modTable->currentRow(), 1)->text(),
                                         modTable->cellLabel(modTable->currentRow(), 2)->text()),
                                     QMessageBox::Yes|QMessageBox::No) == QMessageBox::Yes
                && tryBusy(modName))
        {
            showMsg(d::DELETING_X___.arg(modName), Msgr::Busy);

            qint64 modSize = 0;
            QString fileCount;
            if(md::exists(modTable->modData, modName))
            {
                modSize = md::size(modTable->modData[modName]);
                fileCount = modTable->cellLabel(modTable->row(modName), 2)->text();
            }

            Thread *thr = new Thread(ThreadAction::Delete, modName, core->cfg.pathMods, core->cfg.getSetting(Config::kGamePath));
            connect(thr, &Thread::resultReady,   this,     &MainWindow::actionDone);
            connect(thr, &Thread::modDeleted,    modTable, &ModTable::deleteMod);
            connect(thr, &Thread::scanModUpdate, modTable, &ModTable::updateMod);
            thr->start(modSize, fileCount);
        }
    }
}

void MainWindow::actionDone(const ThreadAction &action)
{
    if(action == ThreadAction::Mount || action == ThreadAction::Unmount)
    {
        core->actionDone(action);
        updateMountState(action.modName);

        if(action == ThreadAction::Unmount)
        {
            const QFileInfo &fiMod(core->cfg.pathMods+"/"+action.modName);

            if(isExternal(action.modName)) modTable->deleteMod(action.modName);
        }
    }
    else showMsg(Core::a2s(action));

    modTable->setIdle(action.modName);
}

void MainWindow::renameMod()
{
    if(!modTable->modSelected()) showMsg(d::NO_MOD_X_.arg(d::lSELECTED));
    else
    {
        const QString &modName = modTable->cellLabel(modTable->currentRow(), 0)->text();

        if(modName == core->mountedMod) showMsg(d::CANT_X_MOUNTED_.arg(d::lRENAME), Msgr::Info);
        else if(tryBusy(modName))
        {
            renameDg = new QDialog(this, Qt::MSWindowsFixedSizeDialogHint);
            renameDg->setWindowTitle(d::RENAME+" "+modName);
            renameDg->setFixedWidth(220);
            QVBoxLayout *renameLayout = new QVBoxLayout;
            renameDg->setLayout(renameLayout);
                renameEdit = new QLineEdit(modName);
                renameLayout->addWidget(renameEdit);
                renameEdit->selectAll();
                QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
                renameLayout->addWidget(buttonBox);

            connect(renameDg,  &QDialog::rejected,          this, &MainWindow::renameModDone);
            connect(buttonBox, &QDialogButtonBox::rejected, this, &MainWindow::renameModDone);
            connect(buttonBox, &QDialogButtonBox::accepted, this, &MainWindow::renameModSave);

            renameDg->exec();
            delete renameDg;
        }
    }
}

void MainWindow::renameModSave()
{
    const QString &newName = renameEdit->text(),
                  &modName = modTable->cellLabel(modTable->currentRow(), 0)->text();

    if(newName.isEmpty() || modName == newName) renameModDone();
    else if(md::exists(modTable->modData, newName)) showMsg(d::MOD_EXISTS_, Msgr::Error);
    else if(!u::isValidFileName(newName)) showMsg(d::INVALID_X.arg(d::lFILENAME)+".\n"+d::CHARACTERS_NOT_ALLOWED, Msgr::Error);
    else if(QFile::rename(core->cfg.pathMods+"/"+modName, core->cfg.pathMods+"/"+newName))
    {
        modTable->renameMod(modName, newName);
        renameModDone();
    }
    else showMsg(d::FAILED_TO_X_.arg(d::lRENAME+" "+modName), Msgr::Error);
}

void MainWindow::renameModDone()
{
    modTable->setIdle(modTable->cellLabel(modTable->currentRow(), 0)->text());
    renameDg->close();
}

void MainWindow::openFolder(const QString &path, const QString &name, QString lName)
{
    if(lName.isEmpty()) lName = name;
    showMsg(d::OPENING_X_FOLDER___.arg(lName), Msgr::Busy);

    if(QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
        showMsg(d::X_FOLDER_OPENED_.arg(name));
    else showMsg(d::FAILED_TO_X_.arg(d::lOPEN_X).arg(d::X_FOLDER).arg(lName), Msgr::Error);
}

void MainWindow::openGameFolder()
{ openFolder(core->cfg.getSetting(Config::kGamePath), d::GAME, d::lGAME); }

void MainWindow::openModsFolder()
{ openFolder(core->cfg.pathMods, d::MODS, d::lMODS); }

void MainWindow::openModFolder()
{
    if(modTable->modSelected())
    {
        const QString &modName = modTable->modNames[modTable->currentRow()];

        if(isExternal(modName)) openFolder(core->cfg.getSetting(Config::kGamePath)+"/"+md::w3mod, modName);
        else openFolder(core->cfg.pathMods+"/"+modName, modName);
    }
    else openModsFolder();
}

void MainWindow::openShortcuts()
{
    Shortcuts shortcuts(this, QStringList(), core->cfg.getSetting(Config::kGamePath), &msgr);
    shortcuts.exec();
}

void MainWindow::openSettings()
{
    Settings settings(this, core->cfg, &msgr);
    if(settings.exec()) refresh(true); //limit refresh: "request" scan in settings when hideempty changed
}

void MainWindow::openAbout()
{
    QDialog about(this, Qt::FramelessWindowHint|Qt::MSWindowsFixedSizeDialogHint);
    about.setWindowTitle(d::ABOUT);
    about.setWindowOpacity(0.9);
    about.setFixedWidth(350);
    about.setStyleSheet("QDialog { background-color: #000; border: 2px solid #fac805; }"
                        "QLabel { color: #bbb; }");

    QVBoxLayout aboutLayout;
    about.setLayout(&aboutLayout);
    aboutLayout.setContentsMargins(0, 0, 0, 0);
    aboutLayout.setSpacing(0);

        QLabel logoLbl;
        aboutLayout.addWidget(&logoLbl);
        logoLbl.setAlignment(Qt::AlignTop);
        logoLbl.setFixedSize(350, 110);
        logoLbl.setPixmap(core->pxLogo);
        QLabel versionLbl(&about);
        versionLbl.setAlignment(Qt::AlignHCenter);
        versionLbl.setFixedSize(350, 13);
        versionLbl.move(0, 83);
        versionLbl.setPixmap(core->pxVersion);

        QHBoxLayout textLayout;
        aboutLayout.addLayout(&textLayout);
        textLayout.setContentsMargins(2, 0, 0, 0);
            textLayout.addItem(new QSpacerItem(40, 0, QSizePolicy::Expanding));

            QGridLayout textGrid;
            textLayout.addLayout(&textGrid);
            textGrid.setVerticalSpacing(0);

                QLabel dlLbl(d::DOWNLOADc), srcLbl(d::SOURCEc), lcnsLbl(d::LICENSEc),
                       dlLink  ("<a style=\"color: #66f;\" href=\"https://www.hiveworkshop.com/threads/wc3-mod-manager.308948/\">"
                                    "Hive Workshop</a>"),
                       srcLink ("<a style=\"color: #66f;\" href=\"https://github.com/EzraZebra/WC3ModManager\">"
                                    "GitHub</a>"),
                       lcnsLink("<a style=\"color: #66f;\" href=\"https://www.gnu.org/licenses/gpl-3.0.html\">"
                                    "GPLv3</a>");
                textGrid.addWidget(&dlLbl,    0, 0);
                textGrid.addWidget(&srcLbl,   1, 0);
                textGrid.addWidget(&lcnsLbl,  2, 0);
                textGrid.addItem(new QSpacerItem(35, 0, QSizePolicy::Expanding), 0, 1);
                textGrid.addWidget(&dlLink,   0, 2);
                textGrid.addWidget(&srcLink,  1, 2);
                textGrid.addWidget(&lcnsLink, 2, 2);
                QFont font("Calibri", 14);
                dlLbl.setFont(font);
                srcLbl.setFont(font);
                lcnsLbl.setFont(font);
                dlLink.setFont(font);
                dlLink.setOpenExternalLinks(true);
                srcLink.setFont(font);
                srcLink.setOpenExternalLinks(true);
                lcnsLink.setFont(font);
                lcnsLink.setOpenExternalLinks(true);

            textLayout.addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding));

        QDialogButtonBox closeBtn(QDialogButtonBox::Close);
        aboutLayout.addWidget(&closeBtn);
        closeBtn.setContentsMargins(0, 0, 12, 12);

    connect(&closeBtn, &QDialogButtonBox::rejected, &about, &QDialog::reject);

    about.exec();
}

/********************************************************************/
/*      OBSOLETE - may be useful later      *************************/
/********************************************************************

    ModDataItem::ModDataItem(const int row, const QString &zero, const bool alignRight) : QFrame(),
        zero(zero), row(row), timer(new QTimer(this))
    {
        timer->setSingleShot(true);

        QGridLayout *layout = new QGridLayout;
        setLayout(layout);
        layout->setSpacing(0);
        layout->setContentsMargins(ModTable::modItemMrgn, ModTable::modItemMrgn, ModTable::modItemMrgn, ModTable::modItemMrgn);

            totalTitle   = new QLabel(d::TOTALc_);
            totalData    = new QLabel(zero);
            mountedTitle = new QLabel(d::MOUNTED+": ");
            mountedData  = new QLabel(zero);
            layout->addWidget(totalTitle,   0, 0);
            layout->addWidget(totalData,    0, 1);
            layout->addWidget(mountedTitle, 1, 0);
            layout->addWidget(mountedData,  1, 1);
            const Qt::AlignmentFlag &align = alignRight ? Qt::AlignRight : Qt::AlignLeft;
            totalTitle->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
            mountedTitle->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
            totalTitle->setAlignment(align|Qt::AlignVCenter);
            totalData->setAlignment(align|Qt::AlignVCenter);
            mountedTitle->setAlignment(align|Qt::AlignVCenter);
            mountedData->setAlignment(align|Qt::AlignVCenter);
            updateView();

        connect(timer, &QTimer::timeout, this, &ModDataItem::updateView);
    }

    void ModDataItem::updateData(const QString &data, const bool dataMounted)
    {
        if(dataMounted) mountedData->setText(data);
        else totalData->setText(data);

        const bool wantVisible = dataMounted ? (totalData->text() != data && data != zero)
                                             : (data != mountedData->text() && mountedData->text() != zero);

        if(detailsVisible != wantVisible)
        {
            detailsVisible = wantVisible;
            if(!timer->isActive())
            {
                timer->start(1000);
                updateView();
            }
        }
    }

    void ModDataItem::updateView()
    {
        if(totalTitle->isVisibleTo(this) != detailsVisible)
        {
            totalTitle->setVisible(detailsVisible);
            mountedTitle->setVisible(detailsVisible);
            mountedData->setVisible(detailsVisible);
            emit viewUpdated(row);
        }
    }

 ********************************************************************/
/********************************************************************/
/********************************************************************/
