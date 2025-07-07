// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "launcher1compat.h"

#include "pkutils.h"

#include <DDesktopEntry>
#include <DNotifySender>
#include <launcher1adaptor.h> // this is the adapter of daemon.Launcher1

// PackageKit-Qt
#include <Daemon>

// Qt Threading
#include <QThreadPool>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>

// Ends with a slash, this is the install prefix for the trusted launcher app.
// For package maintianers, if your distro install binaries to weird locations,
// you can patch this to a empty string.
// TODO: better approach to check if caller is trusted?
//       pr do we really needs to check the caller?
#define BINDIR_PREFIX "/usr/bin/"

DCORE_USE_NAMESPACE

Launcher1Compat::Launcher1Compat(QObject *parent)
    : QObject(parent)
    , m_daemonLauncher1Adapter(new Launcher1Adaptor(this))
{
    PackageKit::Daemon::setHints(QStringList{"interactive=true"});
}

Launcher1Compat::~Launcher1Compat()
{
    // TODO
}

void sendNotification(const QString & displayName, bool successed)
{
    QString msg;
    if (successed) {
        msg = QString(QObject::tr("%1 removed successfully").arg(displayName));
    } else {
        msg = QString(QObject::tr("Failed to remove the app"));
    }

    DUtil::DNotifySender notifySender(msg);
    notifySender = notifySender.appName("deepin-app-store").appIcon("application-default-icon").timeOut(5000);
    notifySender.call();
}

bool uninstallLinglongBundle(const DDesktopEntry & entry)
{
    const QString appId = entry.rawValue("Exec").section(' ', 2, 2);
    const QStringList args {"uninstall", appId};
    QProcess process;

    int retCode = QProcess::execute("ll-cli", args);
    return retCode == 0;
}

void postUninstallCleanUp(const QString & desktopId)
{
    // Remove the shortcut that we created at user's desktop
    const QString &curDesktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    const QString &appDesktopPath = curDesktop + "/" + desktopId;

    QFile file(appDesktopPath);
    if (file.exists()) {
        file.remove();
    } else {
        qDebug() << appDesktopPath << "doesn't exist, no need to remove.";
    }

    // Remove the autostart entry
    // TODO: the legacy dde-application-manager didn't do this

    // Remove the pinned dock entry
    // TODO: the legacy dde-application-manager didn't do this
}

void UninstallTask::uninstallPackageKitPackage(const QString & pkgDisplayName, const QString & pkPackageId, const QString & desktopFilePath)
{
    qDebug() << "Uninstall" << pkPackageId << "via PackageKit";

    // Use QPointer to safely capture this
    QPointer<UninstallTask> self(this);

    PKUtils::removePackage(pkPackageId).then([pkgDisplayName, desktopFilePath, self](){
        if (self) {
            sendNotification(pkgDisplayName, true);
            QFileInfo fi(desktopFilePath);
            // FIXME: THIS IS NOT DESKTOP ID
            postUninstallCleanUp(fi.fileName());
            emit self->uninstallSuccess(desktopFilePath);
            self->finishTask();
        }
    }, [pkgDisplayName, desktopFilePath, self](const std::exception & e){
        if (self) {
            sendNotification(pkgDisplayName, false);
            PKUtils::PkError::printException(e);
            emit self->uninstallFailed(desktopFilePath, QString::fromStdString(e.what()));
            self->finishTask();
        }
    });
}

void UninstallTask::uninstallDCMPackage(const QString & pkgDisplayName, const QString & uninstallCmd, const QString & desktopFilePath)
{
    qDebug() << "Uninstall DCM package" << pkgDisplayName << "via uninstallCmd";

    // run `pkexec args` and wait for finish
    QStringList args = uninstallCmd.split(' ');
    QProcess process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    process.setProcessEnvironment(env);
    args.prepend("SUDO_USER=" + QString::fromLocal8Bit(qgetenv("USER")));
    args.prepend("env");

    process.start("pkexec", args);
    process.waitForFinished();
    if (process.exitCode() != 0) {
        sendNotification(pkgDisplayName, false);
        emit uninstallFailed(desktopFilePath, QString("DCM package uninstall failed"));
    } else {
        sendNotification(pkgDisplayName, true);
        QFileInfo fi(desktopFilePath);
        // FIXME: THIS IS NOT DESKTOP ID
        postUninstallCleanUp(fi.fileName());
        emit uninstallSuccess(desktopFilePath);
    }
    finishTask();
}

void UninstallTask::uninstallPackageByScript(const QString & pkgDisplayName, const QString & packageDesktopFilePath)
{
    // call `/usr/libexec/dde-appwiz-uninstaller.sh <packageDesktopFilePath>` and check the return code.
    qDebug() << "Calling dde-appwiz-uninstaller.sh to uninstall" << pkgDisplayName << packageDesktopFilePath << "via script";
    QProcess process;
    process.start("pkexec", QStringList{"/usr/libexec/dde-appwiz-uninstaller.sh", packageDesktopFilePath});
    process.waitForFinished();

    QString standardOutput = process.readAllStandardOutput();
    QString standardError = process.readAllStandardError();
    qDebug() << "stdout:" << standardOutput;
    qDebug() << "stderr:" << standardError;

    if (process.exitCode() != 0) {
        sendNotification(pkgDisplayName, false);
        emit uninstallFailed(packageDesktopFilePath, standardError);
    } else {
        sendNotification(pkgDisplayName, true);
        QFileInfo fi(packageDesktopFilePath);
        // FIXME: THIS IS NOT DESKTOP ID
        postUninstallCleanUp(fi.fileName());
        emit uninstallSuccess(packageDesktopFilePath);
    }
    finishTask();
}

// the 1st argument is the full path of a desktop file.
void Launcher1Compat::RequestUninstall(const QString & desktop, bool unused)
{
    Q_UNUSED(unused)
    qDebug() << "request hitted" << desktop;

    // TODO: If we go with packagekit, it will ask user to input the password to uninstall application.
    //       Thus this checking will be no longer necessary. We still need this check since we are
    //       still using lastore to uninstall package if it exists.
    QString servicePid = QString::number(QDBusConnection::sessionBus().interface()->servicePid(message().service()));
    QString procfs = QLatin1String("/proc/%1/exe").arg(servicePid);
    QFileInfo procfile(procfs);
    QString realPath = procfile.canonicalFilePath();
#ifndef QT_DEBUG
    if (!realPath.endsWith(BINDIR_PREFIX + QStringLiteral("dde-shell")) &&
        !realPath.endsWith(BINDIR_PREFIX + QStringLiteral("dde-launchpad"))) {
        qWarning() << realPath << " has no right to uninstall " << desktop;
        return;
    }
#endif // !QT_DEBUG

    // Create a new task and submit it to thread pool for concurrent processing
    UninstallTask *task = new UninstallTask(desktop, this);
    connect(task, &UninstallTask::uninstallSuccess, this, &Launcher1Compat::UninstallSuccess);
    connect(task, &UninstallTask::uninstallFailed, this, &Launcher1Compat::UninstallFailed);

    task->setAutoDelete(false);
    QThreadPool::globalInstance()->start(task);
}

UninstallTask::UninstallTask(const QString &desktop, Launcher1Compat *parent)
    : m_desktopFilePath(desktop), m_parent(parent)
{
}

void UninstallTask::run()
{
    qDebug() << "Processing uninstall task for" << m_desktopFilePath;

    // Check if passed file is valid
    QFileInfo desktopFileInfo(m_desktopFilePath);
    if (!desktopFileInfo.exists()) {
        qDebug() << "File" << m_desktopFilePath << "doesn't exist.";
        return;
    }

    QString desktopFilePath(desktopFileInfo.isSymLink() ? desktopFileInfo.symLinkTarget() : m_desktopFilePath);
    DDesktopEntry desktopEntry(desktopFilePath);
    if (desktopEntry.status() != DDesktopEntry::NoError) {
        qDebug() << "Desktop file" << m_desktopFilePath << "is invalid.";
        return;
    }

    if (!desktopEntry.stringValue("X-Deepin-PreUninstall").isEmpty()) {
        QFileInfo desktopFileInfo(desktopFilePath);
        bool writable = desktopFileInfo.isWritable();
        if (writable) {
            qDebug() << "Desktop file" << desktopFilePath << "is writable, it might be a user-level .desktop file, avoiding execute the PreUninstall command.";
        } else {
            const QString & preUninstallScript = desktopEntry.stringValue("X-Deepin-PreUninstall");
            // The script is usually a shell script, we need to execute it and check the return code.
            // We don't need pkexec, execute it directly.
            // If error, we should print the stderr and return.
            QStringList args = QProcess::splitCommand(preUninstallScript);
            QProcess process;
            if (args.size() < 1) {
                qDebug() << "Pre-uninstall script" << preUninstallScript << "is invalid, aborting uninstallation for" << desktopFilePath;
                return;
            } else if (args.size() == 1) {
                process.start(args[0]);
            } else {
                process.start(args[0], args.mid(1));
            }
            bool succ = process.waitForFinished(-1);
            if (!succ || process.exitCode() != 0) {
                int exitCode = process.exitCode();
                qDebug() << "Pre-uninstall script" << preUninstallScript << "exited with exit code:" << exitCode << process.error();
                switch (exitCode) {
                case 101:
                    qDebug() << "Which means user canceled uninstallation for" << desktopFilePath;
                    qDebug() << "Thus aborting the uninstallation.";
                    return;
                case 103:
                    qDebug() << "Which means there is a running instance of the pre-uninstall script for" << desktopFilePath;
                    qDebug() << "Thus aborting the uninstallation.";
                    return;
                default:
                    qDebug() << "stderr:" << process.readAllStandardError();
                    qDebug() << "stdout:" << process.readAllStandardOutput();
                    qDebug() << "Will continue uninstallation for" << desktopFilePath;
                }
            } else {
                qDebug() << "Pre-uninstall script" << preUninstallScript << "succeeded.";
            }
        }
    }

    // Check and do uninstallation
    if (desktopFilePath.contains("/persistent/linglong") || desktopFilePath.contains("/var/lib/linglong")) {
        // Uninstall Linglong Bundle
        bool succ = uninstallLinglongBundle(desktopEntry);
        if (!succ) {
            emit uninstallFailed(desktopFilePath, QString());
            sendNotification(desktopEntry.ddeDisplayName(), false);
        } else {
            // FIXME: the filename of the desktop file MIGHT NOT be its desktopId in freedesktop spec.
            //        here is the logic from the legacy dde-application-manager which is INCORRECT in that case.
            QFileInfo fileInfo(desktopFilePath);
            postUninstallCleanUp(fileInfo.fileName());
            emit uninstallSuccess(desktopFilePath);
            sendNotification(desktopEntry.ddeDisplayName(), true);
        }
        finishTask();
    // TODO: check if it's a flatpak or snap bundle and do the uninstallation?
    } else {
        QString packageDisplayName = desktopEntry.ddeDisplayName();

        const QString compatibleDesktopJsonPath("/var/lib/deepin-compatible/compatibleDesktop.json");
        if (QFile::exists(compatibleDesktopJsonPath)) {
            qDebug() << "Found compatibleDesktop.json, checking if" << packageDisplayName << "is a compatible-mode application.";
            // the json uses the following format:
            // {
            //     "environment-name-package-name": {
            //          ...,
            //          "RemoveCommand": "deepin-compatible-ctl app --name environment-name remove -- package-name"
            //     },
            //     "environment-name2-package-name2": {...},
            //     ...
            // }
            // Check if desktopFilePath's file name (without `.desktop` suffix) is in the json file. If so, execute the
            // RemoveCommand via `pkexec`.
            QFile jsonFile(compatibleDesktopJsonPath);
            if (jsonFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonFile.readAll());
                if (jsonDoc.isObject()) {
                    QJsonObject jsonObj = jsonDoc.object();
                    for (const QString & key : jsonObj.keys()) {
                        if (!desktopFilePath.endsWith(key + ".desktop")) continue;
                        QJsonObject obj = jsonObj.value(key).toObject();
                        QString removeCommand = obj.value("RemoveCommand").toString();
                        qDebug() << "Found compatible desktop entry" << packageDisplayName << "in" << compatibleDesktopJsonPath;
                        uninstallDCMPackage(packageDisplayName, removeCommand, desktopFilePath);
                        return;
                    }
                }
            }
        }

        // Uninstall regular package via PackageKit or deepin-store
        if (QFile::exists("/run/ostree-booted")) {
            uninstallPackageByScript(packageDisplayName, desktopFilePath);
        } else {
            // call PackageKit to uninstall
            QPointer<UninstallTask> self(this);
            PKUtils::searchFiles(desktopFilePath, PackageKit::Transaction::FilterInstalled).then([self, packageDisplayName, desktopFilePath](const PKUtils::PkPackages packages) {
                if (!self) return;

                if (packages.size() == 0) {
                    qDebug() << "No matching package found";
                    emit self->uninstallFailed(desktopFilePath, QString("No matching package found"));
                    self->finishTask();
                    return;
                }
                for (const PKUtils::PkPackage & pkg : packages) {
                    QString pkgId;
                    std::tie(std::ignore, pkgId, std::ignore) = pkg;
                    self->uninstallPackageKitPackage(packageDisplayName, pkgId, desktopFilePath);
                }
            }, [self, desktopFilePath](const std::exception & e){
                if (!self) return;
                PKUtils::PkError::printException(e);
                emit self->uninstallFailed(desktopFilePath, QString::fromStdString(e.what()));
                self->finishTask();
            });
        }
    }
}

void UninstallTask::finishTask()
{
    qDebug() << "Finishing uninstall task for" << m_desktopFilePath;
    // Schedule deletion in the main thread
    QMetaObject::invokeMethod(this, [this](){
        this->deleteLater();
    }, Qt::QueuedConnection);
}
