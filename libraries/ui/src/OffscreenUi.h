//
//  OffscreenUi.h
//  interface/src/entities
//
//  Created by Bradley Austin Davis on 2015-04-04
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#pragma once
#ifndef hifi_OffscreenUi_h
#define hifi_OffscreenUi_h

#include <gl/OffscreenQmlSurface.h>

#include <QMessageBox>
#include <DependencyManager.h>

#include "OffscreenQmlElement.h"


class OffscreenUi : public OffscreenQmlSurface, public Dependency {
    Q_OBJECT

public:
    OffscreenUi();
    virtual void create(QOpenGLContext* context) override;
    void show(const QUrl& url, const QString& name, std::function<void(QQmlContext*, QObject*)> f = [](QQmlContext*, QObject*) {});
    void toggle(const QUrl& url, const QString& name, std::function<void(QQmlContext*, QObject*)> f = [](QQmlContext*, QObject*) {});
    bool shouldSwallowShortcut(QEvent* event);
    bool navigationFocused();
    void setNavigationFocused(bool focused);

    // Messagebox replacement functions
    using ButtonCallback = std::function<void(QMessageBox::StandardButton)>;
    static ButtonCallback NO_OP_CALLBACK;

    static void messageBox(const QString& title, const QString& text,
        ButtonCallback f,
        QMessageBox::Icon icon,
        QMessageBox::StandardButtons buttons);

    static void information(const QString& title, const QString& text,
        ButtonCallback callback = NO_OP_CALLBACK,
        QMessageBox::StandardButtons buttons = QMessageBox::Ok);

    /// Note: will block until user clicks a response to the question
    static void question(const QString& title, const QString& text,
        ButtonCallback callback = NO_OP_CALLBACK,
        QMessageBox::StandardButtons buttons = QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No));

    /// Same design as QMessageBox::question(), will block, returns result
    static QMessageBox::StandardButton question(void* ignored, const QString& title, const QString& text, 
        QMessageBox::StandardButtons buttons = QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No), 
        QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

    static void warning(const QString& title, const QString& text,
        ButtonCallback callback = NO_OP_CALLBACK,
        QMessageBox::StandardButtons buttons = QMessageBox::Ok);

    /// Same design as QMessageBox::warning(), will block, returns result
    static QMessageBox::StandardButton warning(void* ignored, const QString& title, const QString& text,
        QMessageBox::StandardButtons buttons = QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No),
        QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

    static void critical(const QString& title, const QString& text,
        ButtonCallback callback = NO_OP_CALLBACK,
        QMessageBox::StandardButtons buttons = QMessageBox::Ok);

    static void error(const QString& text);  // Interim dialog in new style
};

#endif
