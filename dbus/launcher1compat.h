// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDBusContext>
#include <QDBusMessage>
#include <QObject>
#include <QThreadPool>
#include <QRunnable>

class Launcher1Adaptor;
class UninstallTask;

class Launcher1Compat : public QObject, protected QDBusContext
{
    Q_OBJECT
public:
    static Launcher1Compat &instance()
    {
        static Launcher1Compat _instance;
        return _instance;
    }
    ~Launcher1Compat();

// Launcher1Adapter
public:
    void RequestUninstall(const QString &desktop, bool unused);

signals:
    void UninstallFailed(const QString &appId, const QString &errMsg);
    void UninstallSuccess(const QString &appID);

private:
    explicit Launcher1Compat(QObject *parent = nullptr);

    Launcher1Adaptor * m_daemonLauncher1Adapter;
};

class UninstallTask : public QObject, public QRunnable
{
    Q_OBJECT
public:
    explicit UninstallTask(const QString &desktop, Launcher1Compat *parent);
    void run() override;

signals:
    void uninstallFailed(const QString &appId, const QString &errMsg);
    void uninstallSuccess(const QString &appID);

private:
    void uninstallPackageKitPackage(const QString & pkgDisplayName, const QString & pkPackageId, const QString & desktopFilePath);
    void uninstallDCMPackage(const QString & pkgDisplayName, const QString & uninstallCmd, const QString & desktopFilePath);
    void uninstallPackageByScript(const QString & pkgDisplayName, const QString & packageDesktopFilePath);
    void finishTask();

    QString m_desktopFilePath;
    Launcher1Compat *m_parent;
};
