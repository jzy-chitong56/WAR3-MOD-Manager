#ifndef THREADBASE_H
#define THREADBASE_H

#include "_moddata.h"
#include <QObject>

class QWaitCondition;
class QMutex;

class ThreadAction {
public:  enum Action { NoAction, Mount, Unmount, ModData, Scan, ScanEx, Add, Delete, Shortcut };
         enum Result { Success, Failed, Missing, Result_Size };

private: std::array<int, size_t(Result_Size)> results{};
         bool isAborted = false; //, isForced = false;

public:  const QString PROCESSING,
                       modName;
         const Action  action;

     /* Q_DECLARE_METATYPE requires a public default constructor, copy constructor and destructor
      * --> Hence the (unused) Action::NoAction enum as default value for constructor
      * --> Copy constructor and destructor are implicit */
         explicit ThreadAction(const Action &action=NoAction, const QString &modName=QString());

     // action
         bool operator==(const Action &other) const { return action == other; }
         bool operator!=(const Action &other) const { return action != other; }

     // results
         bool errors() const
         { return results[Failed] || results[Missing] || isAborted; }

         bool success() const
         { return /*(isForced && !isAborted) || (*/
                  (results[Success] || action == Unmount || action == Delete) && (!errors() || action == Add);  }

         bool filesProcessed() const { return results[Success] || results[Failed] || results[Missing]; }

         int  get(const Result &result) const { return results[result]; }
         void add(const Result &result, const int amount=1) { results[result] += amount; }

     // isAborted, isForced
         void abort() { isAborted = true; }
         /*void force()
         {
             results = {};
             isAborted = false;
             isForced = true;
         }*/
         bool aborted() const { return isAborted; }
         //bool forced() const { return isForced; }
};

Q_DECLARE_METATYPE(ThreadAction)

class ThreadBase : public QObject
{
    Q_OBJECT

protected:
           QWaitCondition *confirmWait=nullptr;
           QMutex         *mutex=nullptr;
           ThreadBase() : QObject() {}

signals:   void modDataReady(const md::modData &modData, const QStringList &modNames);
           void scanModUpdate(const QString &modName, const QString &modSize, const QString &fileCount, const qint64 size);
           void scanModReady (const QString &modName);
           void modAdded     (const QString &modName, const int row, const bool addData=true);
           void modDeleted   (const QString &modName);
           void resultReady(const ThreadAction &action);
           void shortcutReady();
};

#endif // THREADBASE_H
