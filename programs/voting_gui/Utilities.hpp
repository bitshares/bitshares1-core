#pragma once

#include <QObject>
#include <QUrl>

class Utilities : public QObject {
    Q_OBJECT

public:
    Utilities(QObject *parent = nullptr) : QObject(parent) {}
    ~Utilities() {}

    Q_INVOKABLE static void copy_to_clipboard(QString string);
    Q_INVOKABLE static void open_in_external_browser(QUrl url);
    Q_INVOKABLE static QString prompt_user_to_open_file(QString dialogCaption);
    Q_INVOKABLE static void log_message(QString message);

    Q_INVOKABLE QString make_temporary_directory();
};
